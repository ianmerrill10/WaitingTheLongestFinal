/**
 * @file ShelterDirectAggregator.cc
 * @brief Implementation of direct shelter feed aggregator
 * @see ShelterDirectAggregator.h for documentation
 */

#include "aggregators/ShelterDirectAggregator.h"

#include <regex>
#include <sstream>

#include "aggregators/DataMapper.h"
#include "aggregators/DuplicateDetector.h"
#include "clients/HttpClient.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/models/Dog.h"

namespace wtl::aggregators {

// ============================================================================
// CONSTRUCTION
// ============================================================================

ShelterDirectAggregator::ShelterDirectAggregator()
    : BaseAggregator("shelter_direct") {

    config_.display_name = "Shelter Direct Feeds";
    config_.description = "Direct data feeds from partner shelters";
}

ShelterDirectAggregator::ShelterDirectAggregator(const AggregatorConfig& config)
    : BaseAggregator(config) {
}

// ============================================================================
// FEED MANAGEMENT
// ============================================================================

void ShelterDirectAggregator::addFeed(const std::string& id,
                                       const std::string& url,
                                       const std::string& format,
                                       const std::string& shelter_id) {
    FeedConfig config;
    config.id = id;
    config.url = url;
    config.format = stringToFeedFormat(format);
    config.shelter_id = shelter_id;
    config.enabled = true;
    config.animals_path = "animals";

    addFeed(config);
}

void ShelterDirectAggregator::addFeed(const FeedConfig& config) {
    std::lock_guard<std::mutex> lock(feeds_mutex_);

    // Remove existing feed with same ID
    feeds_.erase(
        std::remove_if(feeds_.begin(), feeds_.end(),
            [&config](const FeedConfig& f) { return f.id == config.id; }),
        feeds_.end()
    );

    feeds_.push_back(config);
}

void ShelterDirectAggregator::removeFeed(const std::string& id) {
    std::lock_guard<std::mutex> lock(feeds_mutex_);
    feeds_.erase(
        std::remove_if(feeds_.begin(), feeds_.end(),
            [&id](const FeedConfig& f) { return f.id == id; }),
        feeds_.end()
    );
}

std::optional<FeedConfig> ShelterDirectAggregator::getFeed(const std::string& id) const {
    std::lock_guard<std::mutex> lock(feeds_mutex_);
    for (const auto& feed : feeds_) {
        if (feed.id == id) {
            return feed;
        }
    }
    return std::nullopt;
}

std::vector<FeedConfig> ShelterDirectAggregator::getAllFeeds() const {
    std::lock_guard<std::mutex> lock(feeds_mutex_);
    return feeds_;
}

void ShelterDirectAggregator::setFeedEnabled(const std::string& id, bool enabled) {
    std::lock_guard<std::mutex> lock(feeds_mutex_);
    for (auto& feed : feeds_) {
        if (feed.id == id) {
            feed.enabled = enabled;
            break;
        }
    }
}

void ShelterDirectAggregator::loadFeeds(const Json::Value& config) {
    if (!config.isArray()) {
        return;
    }

    std::lock_guard<std::mutex> lock(feeds_mutex_);
    feeds_.clear();

    for (const auto& item : config) {
        feeds_.push_back(FeedConfig::fromJson(item));
    }
}

// ============================================================================
// SPECIFIC METHODS
// ============================================================================

SyncStats ShelterDirectAggregator::syncFeed(const std::string& feed_id) {
    SyncStats stats;
    stats.source_name = config_.name + ":" + feed_id;
    stats.markStarted();

    auto feed_opt = getFeed(feed_id);
    if (!feed_opt) {
        stats.addError("FEED_NOT_FOUND", "Feed not found: " + feed_id);
        stats.markComplete();
        return stats;
    }

    const auto& feed = *feed_opt;

    if (!feed.enabled) {
        stats.addWarning("FEED_DISABLED", "Feed is disabled: " + feed_id);
        stats.markComplete();
        return stats;
    }

    try {
        reportProgress("Fetching feed: " + feed.shelter_name, 10);

        auto data = fetchFeed(feed);
        if (data.isNull()) {
            stats.addError("FETCH_FAILED", "Failed to fetch feed: " + feed_id);
            stats.markComplete();
            return stats;
        }

        // Get animals array from data
        Json::Value animals = getAtPath(data, feed.animals_path);
        if (animals.isNull() || !animals.isArray()) {
            stats.addError("INVALID_FORMAT", "Animals not found at path: " + feed.animals_path);
            stats.markComplete();
            return stats;
        }

        reportProgress("Processing animals", 50);
        processAnimals(animals, feed, stats);

        reportProgress("Feed sync complete", 100);

    } catch (const std::exception& e) {
        stats.addError("SYNC_ERROR", std::string("Feed sync failed: ") + e.what());
    }

    stats.markComplete();
    return stats;
}

bool ShelterDirectAggregator::validateFeed(const std::string& url, FeedFormat format) {
    try {
        clients::HttpClient client(url);
        auto response = client.get("");

        if (!response.isOk()) {
            return false;
        }

        if (format == FeedFormat::JSON) {
            auto json = response.json();
            return !json.isNull();
        } else if (format == FeedFormat::XML) {
            // Basic XML validation - check for opening tag
            return response.body.find("<?xml") != std::string::npos ||
                   response.body.find("<") != std::string::npos;
        }

        return false;

    } catch (...) {
        return false;
    }
}

// ============================================================================
// BaseAggregator OVERRIDES
// ============================================================================

SyncStats ShelterDirectAggregator::doSync() {
    SyncStats stats;
    stats.source_name = config_.name;
    stats.is_incremental = false;
    stats.markStarted();

    std::vector<FeedConfig> feeds_copy;
    {
        std::lock_guard<std::mutex> lock(feeds_mutex_);
        feeds_copy = feeds_;
    }

    if (feeds_copy.empty()) {
        stats.addWarning("NO_FEEDS", "No feeds configured");
        stats.markComplete();
        return stats;
    }

    int feed_idx = 0;
    size_t total_animals = 0;

    for (const auto& feed : feeds_copy) {
        if (shouldCancel()) break;

        if (!feed.enabled) {
            continue;
        }

        int progress = static_cast<int>((feed_idx * 100) / feeds_copy.size());
        reportProgress("Syncing: " + feed.shelter_name, progress);

        try {
            auto data = fetchFeed(feed);
            if (!data.isNull()) {
                Json::Value animals = getAtPath(data, feed.animals_path);
                if (!animals.isNull() && animals.isArray()) {
                    size_t processed = processAnimals(animals, feed, stats);
                    total_animals += processed;
                }
            }
        } catch (const std::exception& e) {
            stats.addError("FEED_ERROR",
                std::string("Feed ") + feed.id + " failed: " + e.what(),
                feed.id);
        }

        feed_idx++;

        if (total_animals >= static_cast<size_t>(config_.max_dogs_per_sync)) {
            stats.addWarning("MAX_DOGS_REACHED", "Reached maximum dogs limit");
            break;
        }
    }

    setDogCount(total_animals);
    reportProgress("All feeds synced", 100);

    return stats;
}

SyncStats ShelterDirectAggregator::doSyncSince(std::chrono::system_clock::time_point since) {
    // Direct feeds typically don't support incremental sync
    // Do a full sync instead
    return doSync();
}

bool ShelterDirectAggregator::doTestConnection() {
    std::vector<FeedConfig> feeds_copy;
    {
        std::lock_guard<std::mutex> lock(feeds_mutex_);
        feeds_copy = feeds_;
    }

    if (feeds_copy.empty()) {
        return true;  // No feeds to test
    }

    // Test first enabled feed
    for (const auto& feed : feeds_copy) {
        if (feed.enabled) {
            return validateFeed(feed.url, feed.format);
        }
    }

    return true;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

Json::Value ShelterDirectAggregator::fetchFeed(const FeedConfig& feed) {
    try {
        clients::HttpClient client(feed.url);
        client.setTimeout(config_.request_timeout_seconds);

        clients::HttpClient::Headers headers;

        // Add authentication if configured
        if (feed.auth_type == "basic") {
            headers["Authorization"] = "Basic " + feed.auth_value;
        } else if (feed.auth_type == "bearer") {
            headers["Authorization"] = "Bearer " + feed.auth_value;
        } else if (feed.auth_type == "api_key") {
            headers["x-api-key"] = feed.auth_value;
        }

        auto response = client.get("", {}, headers);

        last_sync_stats_.api_requests_made++;

        if (!response.isOk()) {
            last_sync_stats_.api_requests_failed++;
            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::EXTERNAL_API,
                "Feed fetch failed: HTTP " + std::to_string(response.status_code),
                {{"feed_id", feed.id}, {"url", feed.url}}
            );
            return Json::Value();
        }

        last_sync_stats_.bytes_received += response.bytes_received;

        // Parse based on format
        if (feed.format == FeedFormat::JSON) {
            return parseJsonFeed(response.body, feed);
        } else if (feed.format == FeedFormat::XML) {
            return parseXmlFeed(response.body, feed);
        }

        return Json::Value();

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "Feed fetch exception: " + std::string(e.what()),
            {{"feed_id", feed.id}}
        );
        return Json::Value();
    }
}

Json::Value ShelterDirectAggregator::parseJsonFeed(const std::string& data,
                                                    const FeedConfig& feed) {
    Json::Value result;
    Json::Reader reader;

    if (!reader.parse(data, result)) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "Failed to parse JSON feed",
            {{"feed_id", feed.id}}
        );
        return Json::Value();
    }

    return result;
}

Json::Value ShelterDirectAggregator::parseXmlFeed(const std::string& data,
                                                   const FeedConfig& feed) {
    // Simple XML to JSON conversion
    // In a production system, use a proper XML parser (e.g., tinyxml2, pugixml)
    Json::Value result;
    result["_format"] = "xml";
    result["_raw"] = data;

    // Extract animal elements using regex (basic implementation)
    std::regex animal_regex("<animal>(.*?)</animal>", std::regex::icase);
    std::sregex_iterator iter(data.begin(), data.end(), animal_regex);
    std::sregex_iterator end;

    Json::Value animals(Json::arrayValue);

    while (iter != end) {
        std::string animal_xml = (*iter)[1].str();
        Json::Value animal;

        // Extract common fields
        std::regex name_regex("<name>(.*?)</name>", std::regex::icase);
        std::smatch name_match;
        if (std::regex_search(animal_xml, name_match, name_regex)) {
            animal["name"] = name_match[1].str();
        }

        std::regex id_regex("<id>(.*?)</id>", std::regex::icase);
        std::smatch id_match;
        if (std::regex_search(animal_xml, id_match, id_regex)) {
            animal["external_id"] = id_match[1].str();
        }

        std::regex breed_regex("<breed>(.*?)</breed>", std::regex::icase);
        std::smatch breed_match;
        if (std::regex_search(animal_xml, breed_match, breed_regex)) {
            animal["breed_primary"] = breed_match[1].str();
        }

        std::regex age_regex("<age>(.*?)</age>", std::regex::icase);
        std::smatch age_match;
        if (std::regex_search(animal_xml, age_match, age_regex)) {
            animal["age_category"] = age_match[1].str();
        }

        std::regex gender_regex("<gender>(.*?)</gender>", std::regex::icase);
        std::smatch gender_match;
        if (std::regex_search(animal_xml, gender_match, gender_regex)) {
            animal["gender"] = gender_match[1].str();
        }

        std::regex desc_regex("<description>(.*?)</description>", std::regex::icase);
        std::smatch desc_match;
        if (std::regex_search(animal_xml, desc_match, desc_regex)) {
            animal["description"] = desc_match[1].str();
        }

        animals.append(animal);
        ++iter;
    }

    result["animals"] = animals;
    return result;
}

size_t ShelterDirectAggregator::processAnimals(const Json::Value& animals,
                                                const FeedConfig& feed,
                                                SyncStats& stats) {
    if (!animals.isArray()) {
        return 0;
    }

    size_t processed = 0;

    for (const auto& animal_data : animals) {
        if (shouldCancel()) break;

        stats.dogs_fetched++;

        // Apply field mappings
        Json::Value mapped_data = mapFields(animal_data, feed);

        // Set shelter_id and source
        mapped_data["shelter_id"] = feed.shelter_id;
        mapped_data["source"] = "shelter_direct";

        // Map to Dog model
        auto mapping_result = DataMapper::getInstance().mapDirectFeedDog(mapped_data);

        if (!mapping_result.success) {
            stats.addWarning("MAPPING_FAILED", mapping_result.error, "", "dog");
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

        if (saveDog(dog, stats)) {
            processed++;

            if (dog.external_id.has_value()) {
                DuplicateDetector::getInstance().registerMapping(
                    dog.id, *dog.external_id, dog.source);
            }
        }
    }

    return processed;
}

Json::Value ShelterDirectAggregator::mapFields(const Json::Value& data,
                                                const FeedConfig& feed) {
    if (feed.field_mappings.empty()) {
        return data;  // No mapping needed
    }

    Json::Value result;

    // Copy all original fields
    for (const auto& key : data.getMemberNames()) {
        result[key] = data[key];
    }

    // Apply mappings (source field -> target field)
    for (const auto& [target_field, source_path] : feed.field_mappings) {
        Json::Value value = getAtPath(data, source_path);
        if (!value.isNull()) {
            result[target_field] = value;
        }
    }

    return result;
}

bool ShelterDirectAggregator::saveDog(const wtl::core::models::Dog& dog, SyncStats& stats) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto existing = txn.exec_params(
            "SELECT id FROM dogs WHERE external_id = $1 AND source = $2",
            dog.external_id.value_or(""), dog.source
        );

        if (existing.empty()) {
            txn.exec_params(
                "INSERT INTO dogs (name, shelter_id, breed_primary, breed_secondary, "
                "size, age_category, age_months, gender, color_primary, status, "
                "is_available, intake_date, description, external_id, source, "
                "urgency_level, is_on_euthanasia_list) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17)",
                dog.name,
                dog.shelter_id,
                dog.breed_primary,
                dog.breed_secondary.value_or(""),
                dog.size,
                dog.age_category,
                dog.age_months,
                dog.gender,
                dog.color_primary,
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
            txn.exec_params(
                "UPDATE dogs SET name = $1, breed_primary = $2, status = $3, "
                "is_available = $4, description = $5, updated_at = NOW() "
                "WHERE external_id = $6 AND source = $7",
                dog.name,
                dog.breed_primary,
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
                      dog.external_id.value_or(""), "dog");
        return false;
    }
}

Json::Value ShelterDirectAggregator::getAtPath(const Json::Value& json,
                                                const std::string& path) {
    if (path.empty()) {
        return json;
    }

    std::istringstream iss(path);
    std::string part;
    const Json::Value* current = &json;

    while (std::getline(iss, part, '.')) {
        if (current->isArray()) {
            // Handle array index
            try {
                int index = std::stoi(part);
                if (index >= 0 && index < static_cast<int>(current->size())) {
                    current = &(*current)[index];
                } else {
                    return Json::Value();
                }
            } catch (...) {
                return Json::Value();
            }
        } else if (current->isObject()) {
            if (!current->isMember(part)) {
                return Json::Value();
            }
            current = &(*current)[part];
        } else {
            return Json::Value();
        }
    }

    return *current;
}

} // namespace wtl::aggregators
