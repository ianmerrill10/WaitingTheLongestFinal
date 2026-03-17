/**
 * @file AggregatorManager.h
 * @brief Central manager for all data source aggregators
 *
 * PURPOSE:
 * Manages the lifecycle and coordination of all aggregators. Provides
 * methods to register, sync, and monitor aggregators from a single point.
 *
 * USAGE:
 * auto& manager = AggregatorManager::getInstance();
 * manager.registerAggregator("rescuegroups", std::make_unique<RescueGroupsAggregator>());
 * manager.syncAll();
 *
 * DEPENDENCIES:
 * - IAggregator interface
 * - SyncStats for tracking
 * - EventBus for sync events
 *
 * MODIFICATION GUIDE:
 * - Add new aggregator types to the factory method
 * - Extend sync scheduling if needed
 * - Add metrics collection points
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "aggregators/IAggregator.h"
#include "aggregators/SyncStats.h"

namespace wtl::aggregators {

/**
 * @struct AggregatorSyncResult
 * @brief Result of a sync operation for one aggregator
 */
struct AggregatorSyncResult {
    std::string name;                  ///< Aggregator name
    bool success{false};               ///< Whether sync succeeded
    SyncStats stats;                   ///< Detailed statistics
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;

    Json::Value toJson() const {
        Json::Value json;
        json["name"] = name;
        json["success"] = success;
        json["stats"] = stats.toJson();

        auto start_t = std::chrono::system_clock::to_time_t(started_at);
        auto end_t = std::chrono::system_clock::to_time_t(completed_at);
        char buffer[30];

        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&start_t));
        json["started_at"] = std::string(buffer);

        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&end_t));
        json["completed_at"] = std::string(buffer);

        return json;
    }
};

/**
 * @struct SyncAllResult
 * @brief Result of syncing all aggregators
 */
struct SyncAllResult {
    bool overall_success{true};        ///< All syncs succeeded
    size_t total_aggregators{0};       ///< Number of aggregators synced
    size_t successful{0};              ///< Number of successful syncs
    size_t failed{0};                  ///< Number of failed syncs
    std::vector<AggregatorSyncResult> results;  ///< Individual results

    // Totals
    size_t total_dogs_added{0};
    size_t total_dogs_updated{0};
    size_t total_shelters_added{0};
    size_t total_shelters_updated{0};

    std::chrono::milliseconds total_duration{0};

    Json::Value toJson() const {
        Json::Value json;
        json["overall_success"] = overall_success;
        json["total_aggregators"] = static_cast<Json::UInt64>(total_aggregators);
        json["successful"] = static_cast<Json::UInt64>(successful);
        json["failed"] = static_cast<Json::UInt64>(failed);
        json["total_dogs_added"] = static_cast<Json::UInt64>(total_dogs_added);
        json["total_dogs_updated"] = static_cast<Json::UInt64>(total_dogs_updated);
        json["total_shelters_added"] = static_cast<Json::UInt64>(total_shelters_added);
        json["total_shelters_updated"] = static_cast<Json::UInt64>(total_shelters_updated);
        json["total_duration_ms"] = static_cast<Json::Int64>(total_duration.count());

        Json::Value results_array(Json::arrayValue);
        for (const auto& r : results) {
            results_array.append(r.toJson());
        }
        json["results"] = results_array;

        return json;
    }
};

/**
 * @class AggregatorManager
 * @brief Singleton manager for all aggregators
 *
 * Coordinates sync operations across multiple data sources,
 * manages aggregator lifecycle, and provides status monitoring.
 */
class AggregatorManager {
public:
    // Type aliases
    using ProgressCallback = std::function<void(const std::string& aggregator, int percent)>;
    using CompletionCallback = std::function<void(const AggregatorSyncResult& result)>;

    // =========================================================================
    // SINGLETON
    // =========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to manager
     */
    static AggregatorManager& getInstance();

    // Delete copy/move
    AggregatorManager(const AggregatorManager&) = delete;
    AggregatorManager& operator=(const AggregatorManager&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize manager from configuration
     * @param config JSON configuration
     * @return true if initialization successful
     */
    bool initialize(const Json::Value& config);

    /**
     * @brief Initialize manager from config file
     * @param config_path Path to configuration file
     * @return true if initialization successful
     */
    bool initializeFromFile(const std::string& config_path);

    /**
     * @brief Shutdown manager and stop all syncs
     */
    void shutdown();

    // =========================================================================
    // AGGREGATOR MANAGEMENT
    // =========================================================================

    /**
     * @brief Register an aggregator
     *
     * @param name Unique aggregator name
     * @param aggregator Aggregator instance
     */
    void registerAggregator(const std::string& name, AggregatorPtr aggregator);

    /**
     * @brief Unregister an aggregator
     * @param name Aggregator name
     */
    void unregisterAggregator(const std::string& name);

    /**
     * @brief Get an aggregator by name
     * @param name Aggregator name
     * @return Pointer to aggregator, or nullptr if not found
     */
    IAggregator* getAggregator(const std::string& name);

    /**
     * @brief Check if aggregator exists
     * @param name Aggregator name
     * @return true if exists
     */
    bool hasAggregator(const std::string& name) const;

    /**
     * @brief Get list of registered aggregator names
     * @return Vector of names
     */
    std::vector<std::string> getAggregatorNames() const;

    /**
     * @brief Get information about all aggregators
     * @return Vector of AggregatorInfo
     */
    std::vector<AggregatorInfo> getAllAggregatorInfo() const;

    // =========================================================================
    // SYNC OPERATIONS
    // =========================================================================

    /**
     * @brief Sync all enabled aggregators
     * @return SyncAllResult with combined statistics
     */
    SyncAllResult syncAll();

    /**
     * @brief Sync all aggregators asynchronously
     * @param callback Called when complete
     */
    void syncAllAsync(std::function<void(SyncAllResult)> callback);

    /**
     * @brief Sync a specific aggregator
     *
     * @param name Aggregator name
     * @return SyncStats for the sync operation
     */
    SyncStats syncOne(const std::string& name);

    /**
     * @brief Sync a specific aggregator asynchronously
     *
     * @param name Aggregator name
     * @param callback Called when complete
     */
    void syncOneAsync(const std::string& name,
                      std::function<void(SyncStats)> callback);

    /**
     * @brief Perform incremental sync for all aggregators
     * @return SyncAllResult
     */
    SyncAllResult syncAllIncremental();

    /**
     * @brief Cancel all running syncs
     */
    void cancelAllSyncs();

    /**
     * @brief Cancel sync for specific aggregator
     * @param name Aggregator name
     */
    void cancelSync(const std::string& name);

    /**
     * @brief Check if any sync is currently running
     * @return true if syncing
     */
    bool isSyncing() const;

    /**
     * @brief Check if specific aggregator is syncing
     * @param name Aggregator name
     * @return true if syncing
     */
    bool isSyncing(const std::string& name) const;

    // =========================================================================
    // STATUS AND STATISTICS
    // =========================================================================

    /**
     * @brief Get status of all aggregators
     * @return JSON with status information
     */
    Json::Value getStatus() const;

    /**
     * @brief Get status of specific aggregator
     * @param name Aggregator name
     * @return JSON status
     */
    Json::Value getStatus(const std::string& name) const;

    /**
     * @brief Get last sync time for an aggregator
     * @param name Aggregator name
     * @return Last sync time or epoch if never synced
     */
    std::chrono::system_clock::time_point getLastSync(const std::string& name) const;

    /**
     * @brief Get last sync stats for an aggregator
     * @param name Aggregator name
     * @return Last sync stats
     */
    SyncStats getLastSyncStats(const std::string& name) const;

    /**
     * @brief Get combined statistics
     * @return JSON with combined stats
     */
    Json::Value getCombinedStats() const;

    /**
     * @brief Get recent sync errors
     * @param limit Maximum errors to return
     * @return JSON array of errors
     */
    Json::Value getRecentErrors(size_t limit = 100) const;

    // =========================================================================
    // SCHEDULING
    // =========================================================================

    /**
     * @brief Start automatic sync scheduling
     *
     * @param interval_minutes Interval between syncs
     *
     * Syncs will run automatically at the specified interval.
     */
    void startAutoSync(int interval_minutes);

    /**
     * @brief Stop automatic sync scheduling
     */
    void stopAutoSync();

    /**
     * @brief Check if auto-sync is enabled
     * @return true if enabled
     */
    bool isAutoSyncEnabled() const;

    /**
     * @brief Get next scheduled sync time
     * @return Time of next sync
     */
    std::chrono::system_clock::time_point getNextScheduledSync() const;

    // =========================================================================
    // CALLBACKS
    // =========================================================================

    /**
     * @brief Set progress callback
     * @param callback Called with progress updates
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Set completion callback
     * @param callback Called when each aggregator completes
     */
    void setCompletionCallback(CompletionCallback callback);

private:
    AggregatorManager() = default;
    ~AggregatorManager();

    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    /**
     * @brief Create aggregator from configuration
     * @param config Aggregator config
     * @return Created aggregator
     */
    AggregatorPtr createAggregator(const Json::Value& config);

    /**
     * @brief Auto-sync thread function
     */
    void autoSyncLoop();

    /**
     * @brief Report progress for an aggregator
     */
    void reportProgress(const std::string& name, int percent);

    /**
     * @brief Report completion for an aggregator
     */
    void reportCompletion(const AggregatorSyncResult& result);

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    std::unordered_map<std::string, AggregatorPtr> aggregators_;
    mutable std::mutex aggregators_mutex_;

    // Sync history
    std::vector<AggregatorSyncResult> sync_history_;
    mutable std::mutex history_mutex_;
    static const size_t MAX_HISTORY_SIZE = 1000;

    // Auto-sync
    std::atomic<bool> auto_sync_enabled_{false};
    int auto_sync_interval_minutes_{60};
    std::thread auto_sync_thread_;
    std::atomic<bool> shutdown_requested_{false};
    std::condition_variable auto_sync_cv_;
    std::mutex auto_sync_mutex_;
    std::chrono::system_clock::time_point next_scheduled_sync_;

    // Callbacks
    ProgressCallback progress_callback_;
    CompletionCallback completion_callback_;
    std::mutex callback_mutex_;

    // State
    std::atomic<bool> is_initialized_{false};
};

} // namespace wtl::aggregators
