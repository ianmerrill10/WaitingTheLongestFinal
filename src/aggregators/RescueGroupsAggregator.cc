/**
 * @file RescueGroupsAggregator.cc
 * @brief Implementation of RescueGroups.org API client
 * @see RescueGroupsAggregator.h for documentation
 */

#include "aggregators/RescueGroupsAggregator.h"

#include <ctime>
#include <iomanip>
#include <sstream>

#include "aggregators/DataMapper.h"
#include "aggregators/DuplicateDetector.h"
#include "clients/HttpClient.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/models/Dog.h"
#include "core/models/Shelter.h"

namespace wtl::aggregators {

// ============================================================================
// CONSTRUCTION
// ============================================================================

RescueGroupsAggregator::RescueGroupsAggregator()
    : BaseAggregator("rescuegroups") {

    config_.display_name = "RescueGroups.org";
    config_.description = "Animal shelter and rescue organization database";
    config_.api_base_url = "https://api.rescuegroups.org/http/v2.json";

    species_filter_ = "dogs";

    // Default target states (pilot states)
    target_states_ = {"TX", "CA", "FL", "NY", "PA"};
}

RescueGroupsAggregator::RescueGroupsAggregator(const AggregatorConfig& config)
    : BaseAggregator(config) {

    species_filter_ = "dogs";
    target_states_ = {"TX", "CA", "FL", "NY", "PA"};
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

size_t RescueGroupsAggregator::fetchOrganizations(const std::vector<std::string>& state_codes) {
    reportProgress("Fetching organizations", 10);

    auto search_params = buildOrgSearchParams(state_codes);
    auto request = buildApiRequest("orgs", "publicSearch", search_params);
    auto response = makeApiCall(request);

    if (response.isNull()) {
        return 0;
    }

    SyncStats temp_stats;
    return processOrganizationResponse(response, temp_stats);
}

size_t RescueGroupsAggregator::fetchAnimalsFromOrg(const std::string& org_id,
                                                    const std::string& species) {
    Json::Value search;
    search["animalOrgID"] = org_id;
    search["animalSpecies"] = species;
    search["animalStatus"] = "Available";

    auto params = buildAnimalSearchParams(species);
    params["search"]["filters"].append(Json::Value());
    params["search"]["filters"][0]["fieldName"] = "animalOrgID";
    params["search"]["filters"][0]["operation"] = "equals";
    params["search"]["filters"][0]["criteria"] = org_id;

    auto request = buildApiRequest("animals", "publicSearch", params);
    auto response = makeApiCall(request);

    if (response.isNull()) {
        return 0;
    }

    SyncStats temp_stats;
    return processAnimalResponse(response, temp_stats);
}

size_t RescueGroupsAggregator::searchAnimals(const Json::Value& filters) {
    auto request = buildApiRequest("animals", "publicSearch", filters);
    auto response = makeApiCall(request);

    if (response.isNull()) {
        return 0;
    }

    SyncStats temp_stats;
    return processAnimalResponse(response, temp_stats);
}

Json::Value RescueGroupsAggregator::getOrganization(const std::string& org_id) {
    Json::Value params;
    params["orgID"] = org_id;

    auto request = buildApiRequest("orgs", "view", params);
    return makeApiCall(request);
}

Json::Value RescueGroupsAggregator::getAnimal(const std::string& animal_id) {
    Json::Value params;
    params["animalID"] = animal_id;

    auto request = buildApiRequest("animals", "view", params);
    return makeApiCall(request);
}

// ============================================================================
// BaseAggregator OVERRIDES
// ============================================================================

SyncStats RescueGroupsAggregator::doSync() {
    SyncStats stats;
    stats.source_name = config_.name;
    stats.is_incremental = false;
    stats.markStarted();

    try {
        // Step 1: Fetch organizations
        reportProgress("Fetching organizations from target states", 5);

        auto org_params = buildOrgSearchParams(target_states_);
        int org_page = 1;
        int total_orgs = 0;

        while (!shouldCancel()) {
            org_params["search"]["resultStart"] = (org_page - 1) * 100;
            org_params["search"]["resultLimit"] = 100;

            auto request = buildApiRequest("orgs", "publicSearch", org_params);
            auto response = makeApiCall(request);

            if (response.isNull() || !response.isMember("data")) {
                break;
            }

            size_t orgs_this_page = processOrganizationResponse(response, stats);
            total_orgs += orgs_this_page;

            if (orgs_this_page < 100) {
                break;  // Last page
            }

            org_page++;
            reportProgress("Fetching organizations (page " + std::to_string(org_page) + ")", 10);
        }

        setShelterCount(static_cast<size_t>(total_orgs));

        if (shouldCancel()) {
            return stats;
        }

        // Step 2: Fetch animals
        reportProgress("Fetching animals", 30);

        auto animal_params = buildAnimalSearchParams(species_filter_);
        int animal_page = 1;
        size_t total_animals = 0;

        while (!shouldCancel()) {
            animal_params["search"]["resultStart"] = (animal_page - 1) * animals_per_request_;
            animal_params["search"]["resultLimit"] = animals_per_request_;

            auto request = buildApiRequest("animals", "publicSearch", animal_params);
            auto response = makeApiCall(request);

            if (response.isNull() || !response.isMember("data")) {
                break;
            }

            size_t animals_this_page = processAnimalResponse(response, stats);
            total_animals += animals_this_page;

            // Check for max dogs limit
            if (total_animals >= static_cast<size_t>(config_.max_dogs_per_sync)) {
                stats.addWarning("MAX_DOGS_REACHED",
                    "Reached maximum dogs per sync limit",
                    "", std::to_string(config_.max_dogs_per_sync));
                break;
            }

            if (animals_this_page < static_cast<size_t>(animals_per_request_)) {
                break;  // Last page
            }

            animal_page++;
            int progress = 30 + static_cast<int>((animal_page * 60) / 100);
            reportProgress("Fetching animals (page " + std::to_string(animal_page) + ")",
                          std::min(progress, 90));
        }

        setDogCount(static_cast<size_t>(total_animals));

        reportProgress("Sync complete", 100);

    } catch (const std::exception& e) {
        stats.addError("SYNC_ERROR", std::string("Sync failed: ") + e.what());
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "RescueGroups sync failed: " + std::string(e.what()),
            {{"aggregator", config_.name}}
        );
    }

    return stats;
}

SyncStats RescueGroupsAggregator::doSyncSince(std::chrono::system_clock::time_point since) {
    SyncStats stats;
    stats.source_name = config_.name;
    stats.is_incremental = true;
    stats.markStarted();

    try {
        reportProgress("Starting incremental sync", 5);

        // Fetch animals updated since last sync
        auto animal_params = buildAnimalSearchParams(species_filter_, "Available", std::optional<std::chrono::system_clock::time_point>(since));
        int animal_page = 1;
        int total_animals = 0;

        while (!shouldCancel()) {
            animal_params["search"]["resultStart"] = (animal_page - 1) * animals_per_request_;
            animal_params["search"]["resultLimit"] = animals_per_request_;

            auto request = buildApiRequest("animals", "publicSearch", animal_params);
            auto response = makeApiCall(request);

            if (response.isNull() || !response.isMember("data")) {
                break;
            }

            size_t animals_this_page = processAnimalResponse(response, stats);
            total_animals += animals_this_page;

            if (animals_this_page < static_cast<size_t>(animals_per_request_)) {
                break;
            }

            animal_page++;
            int progress = 5 + static_cast<int>((animal_page * 90) / 100);
            reportProgress("Fetching updated animals (page " + std::to_string(animal_page) + ")",
                          std::min(progress, 95));
        }

        reportProgress("Incremental sync complete", 100);

    } catch (const std::exception& e) {
        stats.addError("SYNC_ERROR", std::string("Incremental sync failed: ") + e.what());
    }

    return stats;
}

bool RescueGroupsAggregator::doTestConnection() {
    try {
        // Simple API call to test connection
        Json::Value params;
        params["search"]["resultLimit"] = 1;

        auto request = buildApiRequest("animals", "publicSearch", params);
        auto response = makeApiCall(request);

        return !response.isNull() && response.isMember("status") &&
               response["status"].asString() == "ok";

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "RescueGroups connection test failed: " + std::string(e.what()),
            {{"aggregator", config_.name}}
        );
        return false;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

Json::Value RescueGroupsAggregator::buildApiRequest(const std::string& object_type,
                                                     const std::string& object_action,
                                                     const Json::Value& search_params) {
    Json::Value request;

    request["apikey"] = config_.api_key;
    request["objectType"] = object_type;
    request["objectAction"] = object_action;

    if (!search_params.isNull()) {
        for (const auto& key : search_params.getMemberNames()) {
            request[key] = search_params[key];
        }
    }

    return request;
}

Json::Value RescueGroupsAggregator::makeApiCall(const Json::Value& request) {
    // RescueGroups uses POST for all API calls
    auto response = apiPost("", request);

    last_sync_stats_.api_requests_made++;

    if (!response.success) {
        last_sync_stats_.api_requests_failed++;
        return Json::Value();
    }

    last_sync_stats_.bytes_received += response.bytes_received;

    auto json = response.json();

    // Check for API errors
    if (json.isMember("status") && json["status"].asString() != "ok") {
        std::string error = json.get("message", "Unknown API error").asString();
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "RescueGroups API error: " + error,
            {{"request_type", request["objectAction"].asString()}}
        );
        return Json::Value();
    }

    return json;
}

size_t RescueGroupsAggregator::processAnimalResponse(const Json::Value& response,
                                                      SyncStats& stats) {
    if (!response.isMember("data") || response["data"].isNull()) {
        return 0;
    }

    const auto& data = response["data"];
    size_t processed = 0;

    // RescueGroups returns data as an object with IDs as keys
    for (const auto& id : data.getMemberNames()) {
        if (shouldCancel()) {
            break;
        }

        const auto& animal_data = data[id];
        stats.dogs_fetched++;

        // Map to our Dog model
        auto mapping_result = DataMapper::getInstance().mapRescueGroupsDog(animal_data);

        if (!mapping_result.success) {
            stats.addWarning("MAPPING_FAILED", mapping_result.error, id, "dog");
            stats.dogs_skipped++;
            continue;
        }

        auto& dog = mapping_result.data;

        // Check for duplicates
        auto dup_result = DuplicateDetector::getInstance().findDuplicate(dog);
        if (dup_result.is_duplicate && dup_result.confidence >= 0.9) {
            stats.dogs_skipped++;
            continue;
        }

        // Save to database
        if (saveDog(dog, stats)) {
            processed++;

            // Register external ID mapping
            if (dog.external_id.has_value()) {
                DuplicateDetector::getInstance().registerMapping(
                    dog.id, *dog.external_id, dog.source);
            }
        }
    }

    return processed;
}

size_t RescueGroupsAggregator::processOrganizationResponse(const Json::Value& response,
                                                            SyncStats& stats) {
    if (!response.isMember("data") || response["data"].isNull()) {
        return 0;
    }

    const auto& data = response["data"];
    size_t processed = 0;

    for (const auto& id : data.getMemberNames()) {
        if (shouldCancel()) {
            break;
        }

        const auto& org_data = data[id];
        stats.shelters_fetched++;

        auto mapping_result = DataMapper::getInstance().mapRescueGroupsShelter(org_data);

        if (!mapping_result.success) {
            stats.addWarning("MAPPING_FAILED", mapping_result.error, id, "shelter");
            continue;
        }

        if (saveShelter(mapping_result.data, stats)) {
            processed++;
        }
    }

    return processed;
}

bool RescueGroupsAggregator::saveDog(const wtl::core::models::Dog& dog, SyncStats& stats) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Check if dog exists (by external_id and source)
        auto existing = txn.exec_params(
            "SELECT id FROM dogs WHERE external_id = $1 AND source = $2",
            dog.external_id.value_or(""), dog.source
        );

        if (existing.empty()) {
            // Insert new dog
            txn.exec_params(
                "INSERT INTO dogs (name, shelter_id, breed_primary, breed_secondary, "
                "size, age_category, age_months, gender, color_primary, color_secondary, "
                "status, is_available, intake_date, description, external_id, source, "
                "urgency_level, is_on_euthanasia_list) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18)",
                dog.name,
                dog.shelter_id,
                dog.breed_primary,
                dog.breed_secondary.value_or(""),
                dog.size,
                dog.age_category,
                dog.age_months,
                dog.gender,
                dog.color_primary,
                dog.color_secondary.value_or(""),
                dog.status,
                dog.is_available,
                std::chrono::system_clock::to_time_t(dog.intake_date),
                dog.description,
                dog.external_id.value_or(""),
                dog.source,
                dog.urgency_level,
                dog.is_on_euthanasia_list
            );
            stats.dogs_added++;
        } else {
            // Update existing dog
            txn.exec_params(
                "UPDATE dogs SET name = $1, breed_primary = $2, breed_secondary = $3, "
                "size = $4, age_category = $5, age_months = $6, status = $7, "
                "is_available = $8, description = $9, updated_at = NOW() "
                "WHERE external_id = $10 AND source = $11",
                dog.name,
                dog.breed_primary,
                dog.breed_secondary.value_or(""),
                dog.size,
                dog.age_category,
                dog.age_months,
                dog.status,
                dog.is_available,
                dog.description,
                dog.external_id.value_or(""),
                dog.source
            );
            stats.dogs_updated++;
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);
        return true;

    } catch (const std::exception& e) {
        stats.addError("DB_ERROR", std::string("Failed to save dog: ") + e.what(),
                      dog.external_id.value_or("unknown"), "dog");
        return false;
    }
}

bool RescueGroupsAggregator::saveShelter(const wtl::core::models::Shelter& shelter,
                                          SyncStats& stats) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Check if shelter exists
        auto existing = txn.exec_params(
            "SELECT id FROM shelters WHERE external_id = $1 AND source = $2",
            shelter.external_id, shelter.source
        );

        if (existing.empty()) {
            txn.exec_params(
                "INSERT INTO shelters (name, city, state_code, zip_code, phone, email, "
                "website, is_kill_shelter, external_id, source) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)",
                shelter.name,
                shelter.city,
                shelter.state_code,
                shelter.zip_code,
                shelter.phone,
                shelter.email,
                shelter.website,
                shelter.is_kill_shelter,
                shelter.external_id,
                shelter.source
            );
            stats.shelters_added++;
        } else {
            txn.exec_params(
                "UPDATE shelters SET name = $1, city = $2, phone = $3, email = $4, "
                "website = $5, updated_at = NOW() "
                "WHERE external_id = $6 AND source = $7",
                shelter.name,
                shelter.city,
                shelter.phone,
                shelter.email,
                shelter.website,
                shelter.external_id,
                shelter.source
            );
            stats.shelters_updated++;
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);
        return true;

    } catch (const std::exception& e) {
        stats.addError("DB_ERROR", std::string("Failed to save shelter: ") + e.what(),
                      shelter.external_id, "shelter");
        return false;
    }
}

Json::Value RescueGroupsAggregator::buildAnimalSearchParams(
    const std::string& species,
    const std::string& status,
    std::optional<std::chrono::system_clock::time_point> since) {

    Json::Value params;

    // Search configuration
    params["search"]["resultSort"] = "animalID";
    params["search"]["resultOrder"] = "asc";
    params["search"]["calcFoundRows"] = "Yes";

    // Fields to return
    params["search"]["fields"] = Json::Value(Json::arrayValue);
    params["search"]["fields"].append("animalID");
    params["search"]["fields"].append("animalName");
    params["search"]["fields"].append("animalOrgID");
    params["search"]["fields"].append("animalBreed");
    params["search"]["fields"].append("animalPrimaryBreed");
    params["search"]["fields"].append("animalSecondaryBreed");
    params["search"]["fields"].append("animalGeneralSizePotential");
    params["search"]["fields"].append("animalGeneralAge");
    params["search"]["fields"].append("animalSex");
    params["search"]["fields"].append("animalColorPrimary");
    params["search"]["fields"].append("animalColorSecondary");
    params["search"]["fields"].append("animalStatus");
    params["search"]["fields"].append("animalDescription");
    params["search"]["fields"].append("animalPictures");
    params["search"]["fields"].append("animalIntakeDate");
    params["search"]["fields"].append("animalBirthdate");
    params["search"]["fields"].append("animalAltered");
    params["search"]["fields"].append("animalHousetrained");
    params["search"]["fields"].append("animalSpecialneeds");
    params["search"]["fields"].append("animalOKWithKids");
    params["search"]["fields"].append("animalOKWithDogs");
    params["search"]["fields"].append("animalOKWithCats");
    params["search"]["fields"].append("animalUpdatedDate");

    // Filters
    params["search"]["filters"] = Json::Value(Json::arrayValue);

    // Species filter
    Json::Value species_filter;
    species_filter["fieldName"] = "animalSpecies";
    species_filter["operation"] = "equals";
    species_filter["criteria"] = species == "dogs" ? "Dog" : species;
    params["search"]["filters"].append(species_filter);

    // Status filter
    Json::Value status_filter;
    status_filter["fieldName"] = "animalStatus";
    status_filter["operation"] = "equals";
    status_filter["criteria"] = status;
    params["search"]["filters"].append(status_filter);

    // Updated since filter (for incremental sync)
    if (since.has_value()) {
        Json::Value since_filter;
        since_filter["fieldName"] = "animalUpdatedDate";
        since_filter["operation"] = "greaterthan";
        since_filter["criteria"] = formatTimestamp(*since);
        params["search"]["filters"].append(since_filter);
    }

    return params;
}

Json::Value RescueGroupsAggregator::buildOrgSearchParams(
    const std::vector<std::string>& state_codes) {

    Json::Value params;

    params["search"]["resultSort"] = "orgID";
    params["search"]["resultOrder"] = "asc";
    params["search"]["calcFoundRows"] = "Yes";

    params["search"]["fields"] = Json::Value(Json::arrayValue);
    params["search"]["fields"].append("orgID");
    params["search"]["fields"].append("orgName");
    params["search"]["fields"].append("orgCity");
    params["search"]["fields"].append("orgState");
    params["search"]["fields"].append("orgPostalcode");
    params["search"]["fields"].append("orgPhone");
    params["search"]["fields"].append("orgEmail");
    params["search"]["fields"].append("orgWebsiteUrl");
    params["search"]["fields"].append("orgType");

    params["search"]["filters"] = Json::Value(Json::arrayValue);

    // State filter
    if (!state_codes.empty()) {
        Json::Value state_filter;
        state_filter["fieldName"] = "orgState";
        state_filter["operation"] = "equals";

        if (state_codes.size() == 1) {
            state_filter["criteria"] = state_codes[0];
        } else {
            state_filter["operation"] = "contains";
            std::string states;
            for (size_t i = 0; i < state_codes.size(); i++) {
                if (i > 0) states += ",";
                states += state_codes[i];
            }
            state_filter["criteria"] = states;
        }
        params["search"]["filters"].append(state_filter);
    }

    return params;
}

std::string RescueGroupsAggregator::formatTimestamp(
    std::chrono::system_clock::time_point tp) const {

    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&time_t_val);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace wtl::aggregators
