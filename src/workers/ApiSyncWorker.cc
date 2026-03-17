/**
 * @file ApiSyncWorker.cc
 * @brief Implementation of ApiSyncWorker
 * @see ApiSyncWorker.h for documentation
 */

#include "ApiSyncWorker.h"

// Standard library includes
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::workers {

using namespace ::wtl::core::debug;
using namespace ::wtl::core::db;

// ============================================================================
// CONSTRUCTION
// ============================================================================

ApiSyncWorker::ApiSyncWorker()
    : BaseWorker("api_sync_worker",
                 "Synchronizes data from external APIs like RescueGroups",
                 std::chrono::seconds(3600),  // Default 1 hour
                 WorkerPriority::NORMAL)
{
    loadConfigFromSettings();
}

ApiSyncWorker::ApiSyncWorker(const ApiSyncConfig& config)
    : BaseWorker("api_sync_worker",
                 "Synchronizes data from external APIs like RescueGroups",
                 config.sync_interval,
                 WorkerPriority::NORMAL)
    , sync_config_(config)
{
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void ApiSyncWorker::setApiSyncConfig(const ApiSyncConfig& config) {
    sync_config_ = config;
    setInterval(config.sync_interval);
}

ApiSyncConfig ApiSyncWorker::getApiSyncConfig() const {
    return sync_config_;
}

void ApiSyncWorker::loadConfigFromSettings() {
    try {
        auto& cfg = wtl::core::utils::Config::getInstance();

        sync_config_.rescuegroups_api_key = cfg.getString("api.rescuegroups.api_key", "");
        sync_config_.rescuegroups_api_url = cfg.getString("api.rescuegroups.url",
            "https://api.rescuegroups.org/http/v2.json");
        sync_config_.sync_interval = std::chrono::seconds(
            cfg.getInt("api.rescuegroups.sync_interval", 3600));
        sync_config_.batch_size = cfg.getInt("api.rescuegroups.batch_size", 100);
        sync_config_.max_pages = cfg.getInt("api.rescuegroups.max_pages", 100);
        sync_config_.sync_dogs = cfg.getBool("api.rescuegroups.sync_dogs", true);
        sync_config_.sync_shelters = cfg.getBool("api.rescuegroups.sync_shelters", true);
        sync_config_.dry_run = cfg.getBool("api.rescuegroups.dry_run", false);

        // Parse target states if configured
        // This would need proper JSON array parsing in a real implementation

        setInterval(sync_config_.sync_interval);

    } catch (const std::exception& e) {
        logWarning("Failed to load API sync config: " + std::string(e.what()));
    }
}

// ============================================================================
// SYNC CONTROL
// ============================================================================

SyncResult ApiSyncWorker::syncNow() {
    SyncResult result;
    auto start_time = std::chrono::steady_clock::now();

    logInfo("Starting manual sync...");

    try {
        if (sync_config_.sync_shelters) {
            syncSheltersFromRescueGroups(result);
        }

        if (sync_config_.sync_dogs) {
            syncDogsFromRescueGroups(result);
        }
    } catch (const std::exception& e) {
        result.errors++;
        result.error_messages.push_back(e.what());
        logError("Sync failed: " + std::string(e.what()), {});
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        last_result_ = result;
        last_sync_time_ = std::chrono::system_clock::now();
    }

    logInfo("Sync completed: " + std::to_string(result.dogs_created) + " dogs created, " +
            std::to_string(result.dogs_updated) + " updated");

    return result;
}

SyncResult ApiSyncWorker::syncDogs() {
    SyncResult result;
    auto start_time = std::chrono::steady_clock::now();

    try {
        syncDogsFromRescueGroups(result);
    } catch (const std::exception& e) {
        result.errors++;
        result.error_messages.push_back(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

SyncResult ApiSyncWorker::syncShelters() {
    SyncResult result;
    auto start_time = std::chrono::steady_clock::now();

    try {
        syncSheltersFromRescueGroups(result);
    } catch (const std::exception& e) {
        result.errors++;
        result.error_messages.push_back(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

SyncResult ApiSyncWorker::getLastSyncResult() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return last_result_;
}

std::chrono::system_clock::time_point ApiSyncWorker::getLastSyncTime() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return last_sync_time_;
}

// ============================================================================
// WORKER IMPLEMENTATION
// ============================================================================

WorkerResult ApiSyncWorker::doExecute() {
    // Check if API key is configured
    if (sync_config_.rescuegroups_api_key.empty()) {
        logWarning("RescueGroups API key not configured, skipping sync");
        return WorkerResult::Success("Skipped - API key not configured", 0);
    }

    SyncResult sync_result = syncNow();

    int total_processed = sync_result.dogs_created + sync_result.dogs_updated +
                          sync_result.shelters_created + sync_result.shelters_updated;

    if (sync_result.errors > 0) {
        WorkerResult result;
        result.success = false;
        result.message = "Sync completed with " + std::to_string(sync_result.errors) + " errors";
        result.items_processed = total_processed;
        result.items_failed = sync_result.errors;
        result.metadata = sync_result.toJson();
        return result;
    }

    WorkerResult result = WorkerResult::Success(
        "Sync completed successfully", total_processed);
    result.metadata = sync_result.toJson();

    return result;
}

void ApiSyncWorker::beforeExecute() {
    logInfo("Starting API sync...");
}

void ApiSyncWorker::afterExecute(const WorkerResult& result) {
    if (result.success) {
        logInfo("API sync completed: " + result.message);
    } else {
        logWarning("API sync finished with issues: " + result.message);
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void ApiSyncWorker::syncDogsFromRescueGroups(SyncResult& result) {
    logInfo("Syncing dogs from RescueGroups...");

    std::unordered_set<std::string> seen_ids;
    int page = 0;

    while (page < sync_config_.max_pages) {
        try {
            // Build API request
            Json::Value params;
            params["apikey"] = sync_config_.rescuegroups_api_key;
            params["objectType"] = "animals";
            params["objectAction"] = "publicSearch";
            params["search"]["resultStart"] = page * sync_config_.batch_size;
            params["search"]["resultLimit"] = sync_config_.batch_size;
            params["search"]["filters"][0]["fieldName"] = "animalSpecies";
            params["search"]["filters"][0]["operation"] = "equals";
            params["search"]["filters"][0]["criteria"] = "Dog";
            params["search"]["filters"][1]["fieldName"] = "animalStatus";
            params["search"]["filters"][1]["operation"] = "equals";
            params["search"]["filters"][1]["criteria"] = "Available";

            // Add state filter if configured
            if (!sync_config_.target_states.empty()) {
                int filter_idx = 2;
                for (const auto& state : sync_config_.target_states) {
                    params["search"]["filters"][filter_idx]["fieldName"] = "animalLocationState";
                    params["search"]["filters"][filter_idx]["operation"] = "equals";
                    params["search"]["filters"][filter_idx]["criteria"] = state;
                    filter_idx++;
                }
            }

            // Make API request
            Json::Value response = makeRescueGroupsRequest("animals", params);

            if (response.isNull() || !response.isMember("data")) {
                logWarning("Empty response from RescueGroups API");
                break;
            }

            auto& data = response["data"];
            if (data.empty()) {
                break;  // No more data
            }

            // Process each animal
            for (const auto& animal_id : data.getMemberNames()) {
                try {
                    Json::Value dog_data = transformDogData(data[animal_id]);
                    std::string external_id = dog_data["external_id"].asString();
                    seen_ids.insert(external_id);

                    if (!sync_config_.dry_run) {
                        upsertDog(dog_data, result);
                    }
                } catch (const std::exception& e) {
                    result.errors++;
                    result.error_messages.push_back(
                        "Failed to process dog: " + std::string(e.what()));
                }
            }

            // Check if there are more pages
            if (static_cast<int>(data.size()) < sync_config_.batch_size) {
                break;
            }

            page++;

        } catch (const std::exception& e) {
            result.errors++;
            result.error_messages.push_back("API request failed: " + std::string(e.what()));
            break;
        }
    }

    // Mark dogs not seen as removed
    if (!sync_config_.dry_run && !seen_ids.empty()) {
        markRemovedDogs(seen_ids, result);
    }

    logInfo("Dog sync complete: " + std::to_string(result.dogs_created) + " created, " +
            std::to_string(result.dogs_updated) + " updated");
}

void ApiSyncWorker::syncSheltersFromRescueGroups(SyncResult& result) {
    logInfo("Syncing shelters from RescueGroups...");

    int page = 0;

    while (page < sync_config_.max_pages) {
        try {
            // Build API request
            Json::Value params;
            params["apikey"] = sync_config_.rescuegroups_api_key;
            params["objectType"] = "orgs";
            params["objectAction"] = "publicSearch";
            params["search"]["resultStart"] = page * sync_config_.batch_size;
            params["search"]["resultLimit"] = sync_config_.batch_size;

            // Make API request
            Json::Value response = makeRescueGroupsRequest("orgs", params);

            if (response.isNull() || !response.isMember("data")) {
                break;
            }

            auto& data = response["data"];
            if (data.empty()) {
                break;
            }

            // Process each organization
            for (const auto& org_id : data.getMemberNames()) {
                try {
                    Json::Value shelter_data = transformShelterData(data[org_id]);

                    if (!sync_config_.dry_run) {
                        upsertShelter(shelter_data, result);
                    }
                } catch (const std::exception& e) {
                    result.errors++;
                    result.error_messages.push_back(
                        "Failed to process shelter: " + std::string(e.what()));
                }
            }

            if (static_cast<int>(data.size()) < sync_config_.batch_size) {
                break;
            }

            page++;

        } catch (const std::exception& e) {
            result.errors++;
            result.error_messages.push_back("Shelter API request failed: " + std::string(e.what()));
            break;
        }
    }

    logInfo("Shelter sync complete: " + std::to_string(result.shelters_created) + " created, " +
            std::to_string(result.shelters_updated) + " updated");
}

Json::Value ApiSyncWorker::makeRescueGroupsRequest(const std::string& endpoint,
                                                    const Json::Value& params) {
    // In a real implementation, this would use an HTTP client library
    // For now, we return an empty response and log the attempt

    logInfo("Making RescueGroups API request to: " + endpoint);

    // This is a placeholder - actual HTTP implementation would use:
    // - libcurl
    // - Drogon's HttpClient
    // - or another HTTP library

    // Return empty response to indicate no data from placeholder
    Json::Value empty_response;
    empty_response["status"] = "placeholder";
    empty_response["message"] = "HTTP client not implemented - using placeholder";
    return empty_response;
}

Json::Value ApiSyncWorker::transformDogData(const Json::Value& api_dog) {
    Json::Value dog;

    // Map RescueGroups fields to our schema
    dog["external_id"] = api_dog.get("animalID", "").asString();
    dog["name"] = api_dog.get("animalName", "Unknown").asString();
    dog["source"] = "rescuegroups";

    // Breed information
    dog["breed_primary"] = api_dog.get("animalBreed", "Mixed").asString();
    if (api_dog.isMember("animalSecondaryBreed") && !api_dog["animalSecondaryBreed"].isNull()) {
        dog["breed_secondary"] = api_dog["animalSecondaryBreed"].asString();
    }

    // Size mapping
    std::string size = api_dog.get("animalSizeCurrent", "").asString();
    if (size == "Small") dog["size"] = "small";
    else if (size == "Medium") dog["size"] = "medium";
    else if (size == "Large") dog["size"] = "large";
    else if (size == "X-Large") dog["size"] = "xlarge";
    else dog["size"] = "medium";

    // Age mapping
    std::string age = api_dog.get("animalGeneralAge", "").asString();
    if (age == "Baby") dog["age_category"] = "puppy";
    else if (age == "Young") dog["age_category"] = "young";
    else if (age == "Adult") dog["age_category"] = "adult";
    else if (age == "Senior") dog["age_category"] = "senior";
    else dog["age_category"] = "adult";

    // Gender
    std::string gender = api_dog.get("animalSex", "").asString();
    dog["gender"] = (gender == "Male") ? "male" : "female";

    // Colors
    dog["color_primary"] = api_dog.get("animalColor", "").asString();

    // Status
    dog["status"] = "available";
    dog["is_available"] = true;

    // Description
    dog["description"] = api_dog.get("animalDescription", "").asString();

    // Photos
    Json::Value photos(Json::arrayValue);
    if (api_dog.isMember("animalPictures") && api_dog["animalPictures"].isArray()) {
        for (const auto& pic : api_dog["animalPictures"]) {
            if (pic.isMember("large") && !pic["large"].isNull()) {
                photos.append(pic["large"].asString());
            }
        }
    }
    dog["photo_urls"] = photos;

    // Shelter reference
    dog["shelter_external_id"] = api_dog.get("animalOrgID", "").asString();

    // Location
    dog["state_code"] = api_dog.get("animalLocationState", "").asString();
    dog["city"] = api_dog.get("animalLocationCity", "").asString();

    return dog;
}

Json::Value ApiSyncWorker::transformShelterData(const Json::Value& api_shelter) {
    Json::Value shelter;

    shelter["external_id"] = api_shelter.get("orgID", "").asString();
    shelter["name"] = api_shelter.get("orgName", "").asString();
    shelter["source"] = "rescuegroups";

    // Location
    shelter["city"] = api_shelter.get("orgCity", "").asString();
    shelter["state_code"] = api_shelter.get("orgState", "").asString();
    shelter["zip_code"] = api_shelter.get("orgPostalcode", "").asString();
    shelter["address"] = api_shelter.get("orgAddress", "").asString();

    // Contact
    shelter["phone"] = api_shelter.get("orgPhone", "").asString();
    shelter["email"] = api_shelter.get("orgEmail", "").asString();
    shelter["website"] = api_shelter.get("orgWebsiteUrl", "").asString();

    // Type
    std::string type = api_shelter.get("orgType", "").asString();
    if (type == "Rescue") shelter["shelter_type"] = "rescue";
    else if (type == "Shelter") shelter["shelter_type"] = "municipal";
    else shelter["shelter_type"] = "private";

    // Default values for fields we can't determine from API
    shelter["is_kill_shelter"] = false;  // Would need manual curation
    shelter["is_verified"] = true;
    shelter["is_active"] = true;

    return shelter;
}

void ApiSyncWorker::upsertDog(const Json::Value& dog_data, SyncResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string external_id = dog_data["external_id"].asString();

        // Check if dog exists
        auto existing = txn.exec_params(
            "SELECT id FROM dogs WHERE external_id = $1 AND source = 'rescuegroups'",
            external_id);

        if (existing.empty()) {
            // Insert new dog
            Json::StreamWriterBuilder writer;
            std::string photo_urls = Json::writeString(writer, dog_data["photo_urls"]);

            txn.exec_params(
                R"(INSERT INTO dogs (
                    name, external_id, source, breed_primary, breed_secondary,
                    size, age_category, gender, color_primary, status, is_available,
                    description, photo_urls, state_code, intake_date
                ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13::jsonb, $14, NOW()))",
                dog_data["name"].asString(),
                external_id,
                dog_data["source"].asString(),
                dog_data["breed_primary"].asString(),
                dog_data.get("breed_secondary", "").asString(),
                dog_data["size"].asString(),
                dog_data["age_category"].asString(),
                dog_data["gender"].asString(),
                dog_data["color_primary"].asString(),
                dog_data["status"].asString(),
                dog_data["is_available"].asBool(),
                dog_data["description"].asString(),
                photo_urls,
                dog_data["state_code"].asString()
            );

            result.dogs_created++;
        } else {
            // Update existing dog
            txn.exec_params(
                R"(UPDATE dogs SET
                    name = $2, breed_primary = $3, breed_secondary = $4,
                    size = $5, age_category = $6, gender = $7, color_primary = $8,
                    status = $9, is_available = $10, description = $11,
                    updated_at = NOW()
                WHERE external_id = $1 AND source = 'rescuegroups')",
                external_id,
                dog_data["name"].asString(),
                dog_data["breed_primary"].asString(),
                dog_data.get("breed_secondary", "").asString(),
                dog_data["size"].asString(),
                dog_data["age_category"].asString(),
                dog_data["gender"].asString(),
                dog_data["color_primary"].asString(),
                dog_data["status"].asString(),
                dog_data["is_available"].asBool(),
                dog_data["description"].asString()
            );

            result.dogs_updated++;
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        result.errors++;
        result.error_messages.push_back("DB error: " + std::string(e.what()));
        throw;
    }
}

void ApiSyncWorker::upsertShelter(const Json::Value& shelter_data, SyncResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string external_id = shelter_data["external_id"].asString();

        // Check if shelter exists
        auto existing = txn.exec_params(
            "SELECT id FROM shelters WHERE external_id = $1",
            external_id);

        if (existing.empty()) {
            // Insert new shelter
            txn.exec_params(
                R"(INSERT INTO shelters (
                    name, external_id, city, state_code, zip_code, address,
                    phone, email, website, shelter_type, is_kill_shelter,
                    is_verified, is_active
                ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13))",
                shelter_data["name"].asString(),
                external_id,
                shelter_data["city"].asString(),
                shelter_data["state_code"].asString(),
                shelter_data["zip_code"].asString(),
                shelter_data["address"].asString(),
                shelter_data["phone"].asString(),
                shelter_data["email"].asString(),
                shelter_data["website"].asString(),
                shelter_data["shelter_type"].asString(),
                shelter_data["is_kill_shelter"].asBool(),
                shelter_data["is_verified"].asBool(),
                shelter_data["is_active"].asBool()
            );

            result.shelters_created++;
        } else {
            // Update existing shelter
            txn.exec_params(
                R"(UPDATE shelters SET
                    name = $2, city = $3, state_code = $4, zip_code = $5,
                    address = $6, phone = $7, email = $8, website = $9,
                    updated_at = NOW()
                WHERE external_id = $1)",
                external_id,
                shelter_data["name"].asString(),
                shelter_data["city"].asString(),
                shelter_data["state_code"].asString(),
                shelter_data["zip_code"].asString(),
                shelter_data["address"].asString(),
                shelter_data["phone"].asString(),
                shelter_data["email"].asString(),
                shelter_data["website"].asString()
            );

            result.shelters_updated++;
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        result.errors++;
        result.error_messages.push_back("Shelter DB error: " + std::string(e.what()));
        throw;
    }
}

void ApiSyncWorker::markRemovedDogs(const std::unordered_set<std::string>& seen_ids,
                                     SyncResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Build IN clause for seen IDs
        std::ostringstream id_list;
        bool first = true;
        for (const auto& id : seen_ids) {
            if (!first) id_list << ",";
            id_list << txn.quote(id);
            first = false;
        }

        // Mark dogs not seen as unavailable
        auto updated = txn.exec(
            "UPDATE dogs SET status = 'removed', is_available = false, updated_at = NOW() "
            "WHERE source = 'rescuegroups' AND is_available = true "
            "AND external_id NOT IN (" + id_list.str() + ")");

        result.dogs_removed = updated.affected_rows();

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        if (result.dogs_removed > 0) {
            logInfo("Marked " + std::to_string(result.dogs_removed) + " dogs as removed");
        }

    } catch (const std::exception& e) {
        result.errors++;
        result.error_messages.push_back("Failed to mark removed dogs: " + std::string(e.what()));
    }
}

} // namespace wtl::workers
