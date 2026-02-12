/**
 * @file ShelterDirectAggregator.h
 * @brief Aggregator for direct shelter data feeds (JSON/XML)
 *
 * PURPOSE:
 * Supports shelters that provide direct data feeds rather than using
 * third-party platforms. Parses JSON and XML feeds with configurable
 * field mappings.
 *
 * USAGE:
 * ShelterDirectAggregator aggregator;
 * aggregator.addFeed("shelter1", "https://shelter1.com/api/dogs.json", "json");
 * aggregator.sync();
 *
 * DEPENDENCIES:
 * - BaseAggregator for common functionality
 * - DataMapper for data transformation
 *
 * MODIFICATION GUIDE:
 * - Add new feed formats as needed
 * - Extend field mapping configuration
 * - Add authentication methods for protected feeds
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <optional>
#include <vector>

// Project includes
#include "aggregators/BaseAggregator.h"

// Forward declarations
namespace wtl::core::models {
    struct Dog;
}

namespace wtl::aggregators {

/**
 * @enum FeedFormat
 * @brief Supported feed formats
 */
enum class FeedFormat {
    JSON,      ///< JSON format
    XML,       ///< XML format
    CSV        ///< CSV format (future)
};

/**
 * @brief Convert FeedFormat to string
 */
inline std::string feedFormatToString(FeedFormat format) {
    switch (format) {
        case FeedFormat::JSON: return "json";
        case FeedFormat::XML:  return "xml";
        case FeedFormat::CSV:  return "csv";
        default:               return "unknown";
    }
}

/**
 * @brief Convert string to FeedFormat
 */
inline FeedFormat stringToFeedFormat(const std::string& str) {
    if (str == "json") return FeedFormat::JSON;
    if (str == "xml")  return FeedFormat::XML;
    if (str == "csv")  return FeedFormat::CSV;
    return FeedFormat::JSON;  // Default
}

/**
 * @struct FeedConfig
 * @brief Configuration for a direct feed
 */
struct FeedConfig {
    std::string id;                    ///< Unique feed identifier
    std::string shelter_id;            ///< Associated shelter ID
    std::string shelter_name;          ///< Shelter name for reference
    std::string url;                   ///< Feed URL
    FeedFormat format{FeedFormat::JSON}; ///< Feed format
    bool enabled{true};                ///< Whether feed is enabled
    std::string auth_type;             ///< Auth type (none, basic, bearer)
    std::string auth_value;            ///< Auth credentials
    std::string animals_path;          ///< JSON path to animals array
    std::unordered_map<std::string, std::string> field_mappings;  ///< Field mappings

    /**
     * @brief Create from JSON
     */
    static FeedConfig fromJson(const Json::Value& json) {
        FeedConfig config;
        config.id = json.get("id", "").asString();
        config.shelter_id = json.get("shelter_id", "").asString();
        config.shelter_name = json.get("shelter_name", "").asString();
        config.url = json.get("url", "").asString();
        config.format = stringToFeedFormat(json.get("format", "json").asString());
        config.enabled = json.get("enabled", true).asBool();
        config.auth_type = json.get("auth_type", "none").asString();
        config.auth_value = json.get("auth_value", "").asString();
        config.animals_path = json.get("animals_path", "animals").asString();

        if (json.isMember("field_mappings")) {
            for (const auto& key : json["field_mappings"].getMemberNames()) {
                config.field_mappings[key] = json["field_mappings"][key].asString();
            }
        }

        return config;
    }

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const {
        Json::Value json;
        json["id"] = id;
        json["shelter_id"] = shelter_id;
        json["shelter_name"] = shelter_name;
        json["url"] = url;
        json["format"] = feedFormatToString(format);
        json["enabled"] = enabled;
        json["auth_type"] = auth_type;
        json["animals_path"] = animals_path;

        Json::Value mappings;
        for (const auto& [key, value] : field_mappings) {
            mappings[key] = value;
        }
        json["field_mappings"] = mappings;

        return json;
    }
};

/**
 * @class ShelterDirectAggregator
 * @brief Aggregates data from direct shelter feeds
 *
 * Supports multiple feeds with different formats and field mappings.
 * Each feed is associated with a specific shelter.
 */
class ShelterDirectAggregator : public BaseAggregator {
public:
    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Default constructor
     */
    ShelterDirectAggregator();

    /**
     * @brief Construct with configuration
     * @param config Aggregator configuration
     */
    explicit ShelterDirectAggregator(const AggregatorConfig& config);

    /**
     * @brief Destructor
     */
    ~ShelterDirectAggregator() override = default;

    // =========================================================================
    // FEED MANAGEMENT
    // =========================================================================

    /**
     * @brief Add a feed
     *
     * @param id Feed identifier
     * @param url Feed URL
     * @param format Feed format
     * @param shelter_id Associated shelter ID
     */
    void addFeed(const std::string& id,
                 const std::string& url,
                 const std::string& format,
                 const std::string& shelter_id);

    /**
     * @brief Add a feed with full configuration
     * @param config Feed configuration
     */
    void addFeed(const FeedConfig& config);

    /**
     * @brief Remove a feed
     * @param id Feed identifier
     */
    void removeFeed(const std::string& id);

    /**
     * @brief Get feed configuration
     * @param id Feed identifier
     * @return Feed configuration or nullopt if not found
     */
    std::optional<FeedConfig> getFeed(const std::string& id) const;

    /**
     * @brief Get all feed configurations
     * @return Vector of feed configurations
     */
    std::vector<FeedConfig> getAllFeeds() const;

    /**
     * @brief Enable/disable a feed
     * @param id Feed identifier
     * @param enabled Enabled state
     */
    void setFeedEnabled(const std::string& id, bool enabled);

    /**
     * @brief Load feeds from JSON configuration
     * @param config JSON array of feed configs
     */
    void loadFeeds(const Json::Value& config);

    // =========================================================================
    // SPECIFIC METHODS
    // =========================================================================

    /**
     * @brief Sync a specific feed
     *
     * @param feed_id Feed identifier
     * @return Sync statistics for that feed
     */
    SyncStats syncFeed(const std::string& feed_id);

    /**
     * @brief Validate a feed URL
     *
     * @param url URL to validate
     * @param format Expected format
     * @return true if feed is valid
     */
    bool validateFeed(const std::string& url, FeedFormat format);

protected:
    // =========================================================================
    // BaseAggregator OVERRIDES
    // =========================================================================

    SyncStats doSync() override;
    SyncStats doSyncSince(std::chrono::system_clock::time_point since) override;
    bool doTestConnection() override;

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    /**
     * @brief Fetch feed data
     *
     * @param feed Feed configuration
     * @return Feed data as JSON
     */
    Json::Value fetchFeed(const FeedConfig& feed);

    /**
     * @brief Parse JSON feed
     *
     * @param data Raw JSON string
     * @param feed Feed configuration
     * @return Parsed JSON
     */
    Json::Value parseJsonFeed(const std::string& data, const FeedConfig& feed);

    /**
     * @brief Parse XML feed
     *
     * @param data Raw XML string
     * @param feed Feed configuration
     * @return Parsed as JSON-like structure
     */
    Json::Value parseXmlFeed(const std::string& data, const FeedConfig& feed);

    /**
     * @brief Process animals from feed
     *
     * @param animals Animals data
     * @param feed Feed configuration
     * @param stats Stats to update
     * @return Number processed
     */
    size_t processAnimals(const Json::Value& animals,
                          const FeedConfig& feed,
                          SyncStats& stats);

    /**
     * @brief Map animal data using feed's field mappings
     *
     * @param data Raw animal data
     * @param feed Feed configuration
     * @return Normalized JSON
     */
    Json::Value mapFields(const Json::Value& data, const FeedConfig& feed);

    /**
     * @brief Save dog to database
     *
     * @param dog Dog model
     * @param stats Stats to update
     * @return true if saved
     */
    bool saveDog(const wtl::core::models::Dog& dog, SyncStats& stats);

    /**
     * @brief Get JSON value at path
     *
     * @param json Root JSON
     * @param path Dot-separated path
     * @return Value at path
     */
    Json::Value getAtPath(const Json::Value& json, const std::string& path);

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    std::vector<FeedConfig> feeds_;    ///< Configured feeds
    mutable std::mutex feeds_mutex_;   ///< Thread safety for feeds
};

} // namespace wtl::aggregators
