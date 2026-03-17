/**
 * @file IAggregator.h
 * @brief Interface for external data source aggregators
 *
 * PURPOSE:
 * Defines the contract that all aggregators must implement.
 * Aggregators are responsible for fetching dog and shelter data
 * from external sources like RescueGroups.org or direct feeds (NOTE: There is NO Petfinder API).
 *
 * USAGE:
 * class MyAggregator : public IAggregator {
 *     // Implement all pure virtual methods
 * };
 *
 * DEPENDENCIES:
 * - SyncStats for reporting sync results
 * - jsoncpp for JSON configuration
 *
 * MODIFICATION GUIDE:
 * - Add new methods to interface as needed
 * - All aggregators must implement the full interface
 * - Keep interface minimal but complete
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <memory>
#include <optional>
#include <string>

// Third-party includes
#include <json/json.h>

// Forward declarations
namespace wtl::aggregators {
    struct SyncStats;
}

namespace wtl::aggregators {

/**
 * @enum AggregatorStatus
 * @brief Status of an aggregator
 */
enum class AggregatorStatus {
    UNKNOWN = 0,      ///< Status not determined
    HEALTHY,          ///< Aggregator functioning normally
    DEGRADED,         ///< Partial functionality issues
    UNHEALTHY,        ///< Major functionality issues
    DISABLED          ///< Aggregator is disabled
};

/**
 * @brief Convert AggregatorStatus to string
 * @param status The status to convert
 * @return String representation
 */
inline std::string aggregatorStatusToString(AggregatorStatus status) {
    switch (status) {
        case AggregatorStatus::HEALTHY:   return "healthy";
        case AggregatorStatus::DEGRADED:  return "degraded";
        case AggregatorStatus::UNHEALTHY: return "unhealthy";
        case AggregatorStatus::DISABLED:  return "disabled";
        default:                          return "unknown";
    }
}

/**
 * @brief Convert string to AggregatorStatus
 * @param str The string to convert
 * @return The status enum value
 */
inline AggregatorStatus stringToAggregatorStatus(const std::string& str) {
    if (str == "healthy")   return AggregatorStatus::HEALTHY;
    if (str == "degraded")  return AggregatorStatus::DEGRADED;
    if (str == "unhealthy") return AggregatorStatus::UNHEALTHY;
    if (str == "disabled")  return AggregatorStatus::DISABLED;
    return AggregatorStatus::UNKNOWN;
}

/**
 * @struct AggregatorInfo
 * @brief Information about an aggregator instance
 */
struct AggregatorInfo {
    std::string name;                    ///< Unique aggregator name
    std::string display_name;            ///< Human-readable name
    std::string description;             ///< Description of the data source
    std::string api_url;                 ///< Base API URL
    bool is_enabled;                     ///< Whether aggregator is enabled
    AggregatorStatus status;             ///< Current status

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["name"] = name;
        json["display_name"] = display_name;
        json["description"] = description;
        json["api_url"] = api_url;
        json["is_enabled"] = is_enabled;
        json["status"] = aggregatorStatusToString(status);
        return json;
    }
};

/**
 * @class IAggregator
 * @brief Interface for data source aggregators
 *
 * All aggregators (RescueGroups, ShelterDirect) must implement
 * this interface to integrate with the AggregatorManager.
 */
class IAggregator {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IAggregator() = default;

    // =========================================================================
    // IDENTIFICATION
    // =========================================================================

    /**
     * @brief Get the unique name of this aggregator
     * @return Aggregator name (e.g., "rescuegroups", "shelter_direct")
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Get display name for UI
     * @return Human-readable name
     */
    virtual std::string getDisplayName() const = 0;

    /**
     * @brief Get description of this data source
     * @return Description string
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief Get information about this aggregator
     * @return AggregatorInfo struct
     */
    virtual AggregatorInfo getInfo() const = 0;

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Initialize the aggregator with configuration
     *
     * @param config JSON configuration object
     * @return true if initialization successful
     *
     * Configuration should include API credentials, endpoints, rate limits, etc.
     */
    virtual bool initialize(const Json::Value& config) = 0;

    /**
     * @brief Check if aggregator is properly configured
     * @return true if configured and ready
     */
    virtual bool isConfigured() const = 0;

    /**
     * @brief Enable or disable the aggregator
     * @param enabled New enabled state
     */
    virtual void setEnabled(bool enabled) = 0;

    /**
     * @brief Check if aggregator is enabled
     * @return true if enabled
     */
    virtual bool isEnabled() const = 0;

    // =========================================================================
    // SYNC OPERATIONS
    // =========================================================================

    /**
     * @brief Perform a full sync from this data source
     *
     * @return SyncStats with results of the sync operation
     *
     * Full sync fetches all available data from the source.
     * This may take a long time for large sources.
     */
    virtual SyncStats sync() = 0;

    /**
     * @brief Perform an incremental sync since last update
     *
     * @param since Only fetch records modified after this time
     * @return SyncStats with results of the sync operation
     *
     * Incremental sync only fetches records that changed since the given time.
     * More efficient than full sync for regular updates.
     */
    virtual SyncStats syncSince(std::chrono::system_clock::time_point since) = 0;

    /**
     * @brief Cancel an in-progress sync operation
     *
     * Called to gracefully stop a running sync. The sync() method
     * should check for cancellation and stop as soon as possible.
     */
    virtual void cancelSync() = 0;

    /**
     * @brief Check if a sync is currently in progress
     * @return true if sync is running
     */
    virtual bool isSyncing() const = 0;

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get count of dogs from this source
     * @return Number of dogs
     */
    virtual size_t getDogCount() const = 0;

    /**
     * @brief Get count of shelters from this source
     * @return Number of shelters
     */
    virtual size_t getShelterCount() const = 0;

    /**
     * @brief Get timestamp of last successful sync
     * @return Time of last sync, or epoch if never synced
     */
    virtual std::chrono::system_clock::time_point getLastSyncTime() const = 0;

    /**
     * @brief Get timestamp of next scheduled sync
     * @return Time of next sync, or nullopt if not scheduled
     */
    virtual std::optional<std::chrono::system_clock::time_point> getNextSyncTime() const = 0;

    /**
     * @brief Get stats from last sync operation
     * @return SyncStats from last sync
     */
    virtual SyncStats getLastSyncStats() const = 0;

    // =========================================================================
    // HEALTH
    // =========================================================================

    /**
     * @brief Check if the aggregator is healthy
     *
     * @return true if healthy (can connect, authenticated, within rate limits)
     *
     * Health check should verify:
     * - API connectivity
     * - Authentication status
     * - Rate limit status
     * - Recent error rate
     */
    virtual bool isHealthy() const = 0;

    /**
     * @brief Get current health status
     * @return AggregatorStatus enum value
     */
    virtual AggregatorStatus getStatus() const = 0;

    /**
     * @brief Get detailed health information
     * @return JSON with health details
     */
    virtual Json::Value getHealthDetails() const = 0;

    /**
     * @brief Test connectivity to the external API
     * @return true if API is reachable and responding
     */
    virtual bool testConnection() = 0;
};

/**
 * @typedef AggregatorPtr
 * @brief Shared pointer to an aggregator
 */
using AggregatorPtr = std::shared_ptr<IAggregator>;

} // namespace wtl::aggregators
