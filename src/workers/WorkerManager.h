/**
 * @file WorkerManager.h
 * @brief Central manager for all background workers
 *
 * PURPOSE:
 * Manages the lifecycle of all background workers in the system.
 * Provides registration, starting, stopping, and monitoring of workers.
 * Uses a thread pool for efficient worker execution.
 *
 * USAGE:
 * auto& manager = WorkerManager::getInstance();
 * manager.registerWorker("api_sync", std::make_unique<ApiSyncWorker>());
 * manager.startAll();
 * // ... application runs ...
 * manager.stopAll();
 *
 * DEPENDENCIES:
 * - IWorker interface
 * - BaseWorker (optional, for concrete workers)
 * - ErrorCapture for logging
 * - Config for settings
 *
 * MODIFICATION GUIDE:
 * - Add new worker management methods as needed
 * - Extend statistics tracking
 * - Customize thread pool size via configuration
 *
 * @author Agent 18 - WaitingTheLongest Build System
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
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "IWorker.h"

namespace wtl::workers {

// Forward declarations
class BaseWorker;

/**
 * @struct WorkerInfo
 * @brief Information about a registered worker
 */
struct WorkerInfo {
    std::string name;                               ///< Worker name
    std::string description;                        ///< Worker description
    WorkerStatus status{WorkerStatus::STOPPED};     ///< Current status
    WorkerPriority priority{WorkerPriority::NORMAL}; ///< Priority level
    std::chrono::seconds interval;                  ///< Execution interval
    WorkerStats stats;                              ///< Execution statistics
    bool enabled{true};                             ///< Whether worker is enabled
    std::chrono::system_clock::time_point registered_at; ///< Registration time

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["name"] = name;
        json["description"] = description;
        json["status"] = workerStatusToString(status);
        json["priority"] = workerPriorityToString(priority);
        json["interval_seconds"] = static_cast<Json::Int64>(interval.count());
        json["enabled"] = enabled;
        json["stats"] = stats.toJson();

        auto reg_time = std::chrono::system_clock::to_time_t(registered_at);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&reg_time));
        json["registered_at"] = buffer;

        return json;
    }
};

/**
 * @struct WorkerManagerConfig
 * @brief Configuration for WorkerManager
 */
struct WorkerManagerConfig {
    int thread_pool_size{4};                        ///< Number of threads in pool
    std::chrono::seconds health_check_interval{60}; ///< Health check interval
    bool auto_restart_on_failure{true};             ///< Auto-restart failed workers
    int max_restart_attempts{3};                    ///< Max restart attempts
    std::chrono::seconds restart_delay{10};         ///< Delay between restarts
};

/**
 * @class WorkerManager
 * @brief Singleton manager for background workers
 *
 * Manages all background workers in the application.
 * Provides a thread pool for worker execution and
 * handles lifecycle management.
 */
class WorkerManager {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the WorkerManager singleton
     */
    static WorkerManager& getInstance();

    // Delete copy/move operations
    WorkerManager(const WorkerManager&) = delete;
    WorkerManager& operator=(const WorkerManager&) = delete;
    WorkerManager(WorkerManager&&) = delete;
    WorkerManager& operator=(WorkerManager&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the worker manager
     * @param config Configuration settings
     */
    void initialize(const WorkerManagerConfig& config = {});

    /**
     * @brief Initialize from Config singleton
     */
    void initializeFromConfig();

    /**
     * @brief Check if manager is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    /**
     * @brief Shutdown the worker manager
     */
    void shutdown();

    // =========================================================================
    // WORKER REGISTRATION
    // =========================================================================

    /**
     * @brief Register a worker
     *
     * @param name Unique name for the worker
     * @param worker The worker instance
     * @param start_immediately Whether to start the worker immediately
     * @return true if registration successful
     *
     * @example
     * manager.registerWorker("api_sync", std::make_unique<ApiSyncWorker>(), true);
     */
    bool registerWorker(const std::string& name,
                        std::unique_ptr<IWorker> worker,
                        bool start_immediately = false);

    /**
     * @brief Register a worker with custom interval
     *
     * @param name Unique name for the worker
     * @param worker The worker instance
     * @param interval Custom execution interval
     * @param start_immediately Whether to start the worker immediately
     * @return true if registration successful
     */
    bool registerWorker(const std::string& name,
                        std::unique_ptr<IWorker> worker,
                        std::chrono::seconds interval,
                        bool start_immediately = false);

    /**
     * @brief Unregister a worker
     *
     * @param name Worker name
     * @return true if unregistration successful
     */
    bool unregisterWorker(const std::string& name);

    /**
     * @brief Check if a worker is registered
     *
     * @param name Worker name
     * @return true if worker is registered
     */
    bool hasWorker(const std::string& name) const;

    /**
     * @brief Get list of registered worker names
     * @return Vector of worker names
     */
    std::vector<std::string> getWorkerNames() const;

    // =========================================================================
    // WORKER LIFECYCLE
    // =========================================================================

    /**
     * @brief Start all registered workers
     */
    void startAll();

    /**
     * @brief Stop all workers gracefully
     * @param timeout Maximum time to wait for workers to stop
     */
    void stopAll(std::chrono::seconds timeout = std::chrono::seconds(30));

    /**
     * @brief Start a specific worker
     * @param name Worker name
     * @return true if started successfully
     */
    bool startWorker(const std::string& name);

    /**
     * @brief Stop a specific worker
     * @param name Worker name
     * @param wait Wait for worker to finish current execution
     * @return true if stopped successfully
     */
    bool stopWorker(const std::string& name, bool wait = true);

    /**
     * @brief Pause a worker
     * @param name Worker name
     * @return true if paused successfully
     */
    bool pauseWorker(const std::string& name);

    /**
     * @brief Resume a paused worker
     * @param name Worker name
     * @return true if resumed successfully
     */
    bool resumeWorker(const std::string& name);

    /**
     * @brief Restart a worker
     * @param name Worker name
     * @return true if restarted successfully
     */
    bool restartWorker(const std::string& name);

    /**
     * @brief Request immediate execution of a worker
     * @param name Worker name
     * @return true if request accepted
     */
    bool runWorkerNow(const std::string& name);

    // =========================================================================
    // WORKER STATUS
    // =========================================================================

    /**
     * @brief Get status of a specific worker
     * @param name Worker name
     * @return Worker status, or STOPPED if not found
     */
    WorkerStatus getWorkerStatus(const std::string& name) const;

    /**
     * @brief Get detailed info for a worker
     * @param name Worker name
     * @return WorkerInfo (empty if not found)
     */
    WorkerInfo getWorkerInfo(const std::string& name) const;

    /**
     * @brief Get statistics for a worker
     * @param name Worker name
     * @return WorkerStats
     */
    WorkerStats getWorkerStats(const std::string& name) const;

    /**
     * @brief Get all workers' status
     * @return JSON object with all worker statuses
     */
    Json::Value getAllStatus() const;

    /**
     * @brief Get all workers' info
     * @return Vector of WorkerInfo
     */
    std::vector<WorkerInfo> getAllInfo() const;

    // =========================================================================
    // WORKER CONFIGURATION
    // =========================================================================

    /**
     * @brief Enable a worker
     * @param name Worker name
     */
    void enableWorker(const std::string& name);

    /**
     * @brief Disable a worker
     * @param name Worker name
     */
    void disableWorker(const std::string& name);

    /**
     * @brief Set worker interval
     * @param name Worker name
     * @param interval New interval
     */
    void setWorkerInterval(const std::string& name, std::chrono::seconds interval);

    // =========================================================================
    // HEALTH MONITORING
    // =========================================================================

    /**
     * @brief Check if all workers are healthy
     * @return true if all workers are healthy
     */
    bool isHealthy() const;

    /**
     * @brief Get health status of all workers
     * @return JSON object with health details
     */
    Json::Value getHealthStatus() const;

    /**
     * @brief Get list of unhealthy workers
     * @return Vector of worker names
     */
    std::vector<std::string> getUnhealthyWorkers() const;

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get manager statistics
     * @return JSON object with statistics
     */
    Json::Value getManagerStats() const;

    /**
     * @brief Reset statistics for all workers
     */
    void resetAllStats();

    /**
     * @brief Reset statistics for a specific worker
     * @param name Worker name
     */
    void resetWorkerStats(const std::string& name);

private:
    // =========================================================================
    // PRIVATE TYPES
    // =========================================================================

    struct RegisteredWorker {
        std::unique_ptr<IWorker> worker;
        std::unique_ptr<std::thread> thread;
        std::atomic<bool> running{false};
        WorkerInfo info;
    };

    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    WorkerManager() = default;
    ~WorkerManager();

    /**
     * @brief Get a registered worker
     * @param name Worker name
     * @return Pointer to registered worker, or nullptr
     */
    RegisteredWorker* getRegisteredWorker(const std::string& name);

    /**
     * @brief Get a registered worker (const version)
     * @param name Worker name
     * @return Pointer to registered worker, or nullptr
     */
    const RegisteredWorker* getRegisteredWorker(const std::string& name) const;

    /**
     * @brief Thread pool worker function
     */
    void threadPoolWorker();

    /**
     * @brief Enqueue work for thread pool
     * @param task The task to execute
     */
    void enqueueWork(std::function<void()> task);

    /**
     * @brief Log a message
     * @param message The message
     */
    void log(const std::string& message);

    /**
     * @brief Log an error
     * @param message The message
     */
    void logError(const std::string& message);

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    // Configuration
    WorkerManagerConfig config_;

    // Registered workers
    mutable std::shared_mutex workers_mutex_;
    std::unordered_map<std::string, std::unique_ptr<RegisteredWorker>> workers_;

    // Thread pool
    std::vector<std::thread> thread_pool_;
    std::queue<std::function<void()>> work_queue_;
    std::mutex work_mutex_;
    std::condition_variable work_cv_;
    std::atomic<bool> shutdown_requested_{false};

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> all_started_{false};

    // Statistics
    std::atomic<uint64_t> total_workers_registered_{0};
    std::atomic<uint64_t> total_workers_started_{0};
    std::atomic<uint64_t> total_workers_stopped_{0};
    std::atomic<uint64_t> total_worker_failures_{0};
    std::chrono::system_clock::time_point manager_start_time_;
};

} // namespace wtl::workers
