/**
 * @file ApiSyncWorker.h
 * @brief Worker for synchronizing data from external APIs
 *
 * PURPOSE:
 * Synchronizes dog, shelter, and status data from external sources
 * like RescueGroups API. Runs on a configurable interval to keep
 * the database updated with the latest information.
 *
 * USAGE:
 * auto worker = std::make_unique<ApiSyncWorker>();
 * WorkerManager::getInstance().registerWorker("api_sync", std::move(worker));
 *
 * DEPENDENCIES:
 * - BaseWorker for worker infrastructure
 * - DogService, ShelterService for data persistence
 * - HTTP client for API calls
 * - ErrorCapture for logging
 *
 * MODIFICATION GUIDE:
 * - Add new API sources in doExecute()
 * - Configure sync intervals per data type
 * - Add data transformation logic for new sources
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "BaseWorker.h"

namespace wtl::workers {

/**
 * @struct ApiSyncConfig
 * @brief Configuration for API sync worker
 */
struct ApiSyncConfig {
    std::string rescuegroups_api_key;                 ///< RescueGroups API key
    std::string rescuegroups_api_url{"https://api.rescuegroups.org/http/v2.json"};
    std::chrono::seconds sync_interval{3600};         ///< Default 1 hour
    int batch_size{100};                              ///< Number of records per API call
    int max_pages{100};                               ///< Max pages to fetch per sync
    bool sync_dogs{true};                             ///< Sync dog data
    bool sync_shelters{true};                         ///< Sync shelter data
    std::vector<std::string> target_states;           ///< States to sync (empty = all)
    bool dry_run{false};                              ///< Log changes without applying
};

/**
 * @struct SyncResult
 * @brief Result of a sync operation
 */
struct SyncResult {
    int dogs_created{0};                              ///< New dogs created
    int dogs_updated{0};                              ///< Existing dogs updated
    int dogs_removed{0};                              ///< Dogs marked as removed
    int shelters_created{0};                          ///< New shelters created
    int shelters_updated{0};                          ///< Existing shelters updated
    int errors{0};                                    ///< Errors encountered
    std::vector<std::string> error_messages;          ///< Error details
    std::chrono::milliseconds duration{0};            ///< Total sync duration

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["dogs_created"] = dogs_created;
        json["dogs_updated"] = dogs_updated;
        json["dogs_removed"] = dogs_removed;
        json["shelters_created"] = shelters_created;
        json["shelters_updated"] = shelters_updated;
        json["errors"] = errors;
        json["duration_ms"] = static_cast<Json::Int64>(duration.count());

        Json::Value errors_arr(Json::arrayValue);
        for (const auto& msg : error_messages) {
            errors_arr.append(msg);
        }
        json["error_messages"] = errors_arr;

        return json;
    }
};

/**
 * @class ApiSyncWorker
 * @brief Worker for synchronizing external API data
 *
 * Periodically fetches data from external APIs like RescueGroups
 * and updates the local database with new and changed records.
 */
class ApiSyncWorker : public BaseWorker {
public:
    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Construct with default settings
     */
    ApiSyncWorker();

    /**
     * @brief Construct with custom configuration
     * @param config Sync configuration
     */
    explicit ApiSyncWorker(const ApiSyncConfig& config);

    /**
     * @brief Destructor
     */
    ~ApiSyncWorker() override = default;

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void setApiSyncConfig(const ApiSyncConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    ApiSyncConfig getApiSyncConfig() const;

    /**
     * @brief Load configuration from Config singleton
     */
    void loadConfigFromSettings();

    // =========================================================================
    // SYNC CONTROL
    // =========================================================================

    /**
     * @brief Force immediate sync
     * @return Sync result
     */
    SyncResult syncNow();

    /**
     * @brief Sync dogs only
     * @return Sync result
     */
    SyncResult syncDogs();

    /**
     * @brief Sync shelters only
     * @return Sync result
     */
    SyncResult syncShelters();

    /**
     * @brief Get last sync result
     * @return Last result
     */
    SyncResult getLastSyncResult() const;

    /**
     * @brief Get last sync time
     * @return Time of last sync
     */
    std::chrono::system_clock::time_point getLastSyncTime() const;

protected:
    // =========================================================================
    // WORKER IMPLEMENTATION
    // =========================================================================

    /**
     * @brief Execute sync operation
     * @return Worker result
     */
    WorkerResult doExecute() override;

    /**
     * @brief Pre-execution hook
     */
    void beforeExecute() override;

    /**
     * @brief Post-execution hook
     * @param result Execution result
     */
    void afterExecute(const WorkerResult& result) override;

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    /**
     * @brief Sync dogs from RescueGroups API
     * @param result Result to update
     */
    void syncDogsFromRescueGroups(SyncResult& result);

    /**
     * @brief Sync shelters from RescueGroups API
     * @param result Result to update
     */
    void syncSheltersFromRescueGroups(SyncResult& result);

    /**
     * @brief Make API request to RescueGroups
     * @param endpoint Endpoint to call
     * @param params Request parameters
     * @return Response JSON
     */
    Json::Value makeRescueGroupsRequest(const std::string& endpoint,
                                         const Json::Value& params);

    /**
     * @brief Transform RescueGroups dog data to our format
     * @param api_dog API response dog data
     * @return Transformed dog data
     */
    Json::Value transformDogData(const Json::Value& api_dog);

    /**
     * @brief Transform RescueGroups shelter data to our format
     * @param api_shelter API response shelter data
     * @return Transformed shelter data
     */
    Json::Value transformShelterData(const Json::Value& api_shelter);

    /**
     * @brief Update or create dog in database
     * @param dog_data Dog data to save
     * @param result Result to update
     */
    void upsertDog(const Json::Value& dog_data, SyncResult& result);

    /**
     * @brief Update or create shelter in database
     * @param shelter_data Shelter data to save
     * @param result Result to update
     */
    void upsertShelter(const Json::Value& shelter_data, SyncResult& result);

    /**
     * @brief Mark dogs not seen in sync as removed
     * @param seen_ids IDs of dogs seen in this sync
     * @param result Result to update
     */
    void markRemovedDogs(const std::unordered_set<std::string>& seen_ids,
                          SyncResult& result);

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    ApiSyncConfig sync_config_;
    SyncResult last_result_;
    std::chrono::system_clock::time_point last_sync_time_;
    mutable std::mutex result_mutex_;
};

} // namespace wtl::workers
