/**
 * @file IWorker.h
 * @brief Interface for background worker implementations
 *
 * PURPOSE:
 * Defines the contract that all background workers must implement.
 * Workers are scheduled tasks that run at configurable intervals
 * to perform maintenance, synchronization, and processing tasks.
 *
 * USAGE:
 * class MyWorker : public IWorker {
 *     std::string getName() const override { return "my_worker"; }
 *     std::chrono::seconds getInterval() const override { return std::chrono::seconds(60); }
 *     WorkerResult execute() override { ... }
 * };
 *
 * DEPENDENCIES:
 * - Standard library (chrono, string)
 * - jsoncpp for status reporting
 *
 * MODIFICATION GUIDE:
 * - Add new methods to the interface as needed for all workers
 * - Extend WorkerResult with additional fields for metrics
 * - Keep the interface minimal and focused
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <exception>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::workers {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum WorkerStatus
 * @brief Current status of a worker
 */
enum class WorkerStatus {
    STOPPED = 0,    ///< Worker is not running
    STARTING,       ///< Worker is starting up
    RUNNING,        ///< Worker is actively running
    PAUSED,         ///< Worker is paused
    STOPPING,       ///< Worker is shutting down
    ERROR           ///< Worker encountered an error
};

/**
 * @enum WorkerPriority
 * @brief Priority level for worker execution
 */
enum class WorkerPriority {
    LOW = 0,        ///< Low priority, can be delayed
    NORMAL,         ///< Normal priority
    HIGH,           ///< High priority, should run promptly
    CRITICAL        ///< Critical priority, must run immediately
};

// ============================================================================
// RESULT STRUCTURES
// ============================================================================

/**
 * @struct WorkerResult
 * @brief Result of a worker execution cycle
 *
 * Contains information about what the worker accomplished in its
 * most recent execution cycle, including any errors encountered.
 */
struct WorkerResult {
    bool success{true};                              ///< Whether execution succeeded
    std::string message;                              ///< Status/error message
    int items_processed{0};                           ///< Number of items processed
    int items_failed{0};                              ///< Number of items that failed
    std::chrono::milliseconds execution_time{0};      ///< Time taken for execution
    std::vector<std::string> errors;                  ///< List of error messages
    Json::Value metadata;                             ///< Additional execution metadata

    /**
     * @brief Create a success result
     * @param msg Success message
     * @param processed Number of items processed
     * @return WorkerResult indicating success
     */
    static WorkerResult Success(const std::string& msg = "OK", int processed = 0) {
        WorkerResult result;
        result.success = true;
        result.message = msg;
        result.items_processed = processed;
        return result;
    }

    /**
     * @brief Create a failure result
     * @param msg Error message
     * @return WorkerResult indicating failure
     */
    static WorkerResult Failure(const std::string& msg) {
        WorkerResult result;
        result.success = false;
        result.message = msg;
        result.errors.push_back(msg);
        return result;
    }

    /**
     * @brief Convert to JSON for reporting
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["success"] = success;
        json["message"] = message;
        json["items_processed"] = items_processed;
        json["items_failed"] = items_failed;
        json["execution_time_ms"] = static_cast<Json::Int64>(execution_time.count());

        Json::Value error_array(Json::arrayValue);
        for (const auto& error : errors) {
            error_array.append(error);
        }
        json["errors"] = error_array;

        if (!metadata.isNull()) {
            json["metadata"] = metadata;
        }

        return json;
    }
};

/**
 * @struct WorkerStats
 * @brief Statistics for a worker's execution history
 */
struct WorkerStats {
    uint64_t total_executions{0};                     ///< Total number of executions
    uint64_t successful_executions{0};                ///< Number of successful executions
    uint64_t failed_executions{0};                    ///< Number of failed executions
    uint64_t total_items_processed{0};                ///< Total items processed
    uint64_t total_items_failed{0};                   ///< Total items that failed
    std::chrono::milliseconds total_execution_time{0}; ///< Total time spent executing
    std::chrono::milliseconds avg_execution_time{0};   ///< Average execution time
    std::chrono::milliseconds max_execution_time{0};   ///< Maximum execution time
    std::chrono::system_clock::time_point last_run;   ///< Time of last execution
    std::chrono::system_clock::time_point next_run;   ///< Scheduled next execution
    std::string last_error;                           ///< Last error message
    int consecutive_failures{0};                      ///< Number of consecutive failures

    /**
     * @brief Convert to JSON for reporting
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["total_executions"] = static_cast<Json::UInt64>(total_executions);
        json["successful_executions"] = static_cast<Json::UInt64>(successful_executions);
        json["failed_executions"] = static_cast<Json::UInt64>(failed_executions);
        json["total_items_processed"] = static_cast<Json::UInt64>(total_items_processed);
        json["total_items_failed"] = static_cast<Json::UInt64>(total_items_failed);
        json["total_execution_time_ms"] = static_cast<Json::Int64>(total_execution_time.count());
        json["avg_execution_time_ms"] = static_cast<Json::Int64>(avg_execution_time.count());
        json["max_execution_time_ms"] = static_cast<Json::Int64>(max_execution_time.count());

        auto last_run_time = std::chrono::system_clock::to_time_t(last_run);
        auto next_run_time = std::chrono::system_clock::to_time_t(next_run);
        char buffer[32];

        if (last_run.time_since_epoch().count() > 0) {
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&last_run_time));
            json["last_run"] = buffer;
        } else {
            json["last_run"] = Json::nullValue;
        }

        if (next_run.time_since_epoch().count() > 0) {
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&next_run_time));
            json["next_run"] = buffer;
        } else {
            json["next_run"] = Json::nullValue;
        }

        json["last_error"] = last_error;
        json["consecutive_failures"] = consecutive_failures;

        // Calculate success rate
        if (total_executions > 0) {
            double rate = (static_cast<double>(successful_executions) / total_executions) * 100.0;
            json["success_rate_percent"] = rate;
        } else {
            json["success_rate_percent"] = 0.0;
        }

        return json;
    }
};

// ============================================================================
// WORKER INTERFACE
// ============================================================================

/**
 * @class IWorker
 * @brief Abstract interface for all background workers
 *
 * All workers in the system must implement this interface.
 * Workers are managed by the WorkerManager which handles
 * scheduling, lifecycle, and monitoring.
 */
class IWorker {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IWorker() = default;

    // =========================================================================
    // IDENTITY
    // =========================================================================

    /**
     * @brief Get the unique name of this worker
     * @return Worker name (used for identification and logging)
     *
     * The name should be unique across all workers and use snake_case.
     * Example: "api_sync_worker", "urgency_update_worker"
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Get a description of what this worker does
     * @return Human-readable description
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief Get the version of this worker
     * @return Version string (e.g., "1.0.0")
     */
    virtual std::string getVersion() const { return "1.0.0"; }

    // =========================================================================
    // SCHEDULING
    // =========================================================================

    /**
     * @brief Get the execution interval for this worker
     * @return Time between executions
     *
     * The worker will be executed at this interval by the WorkerManager.
     * Return std::chrono::seconds(0) for one-time workers.
     */
    virtual std::chrono::seconds getInterval() const = 0;

    /**
     * @brief Get the priority of this worker
     * @return Worker priority
     */
    virtual WorkerPriority getPriority() const { return WorkerPriority::NORMAL; }

    /**
     * @brief Check if the worker should run now
     * @return true if the worker should execute
     *
     * Override this for conditional execution based on time of day,
     * system load, or other factors. Default always returns true.
     */
    virtual bool shouldRunNow() const { return true; }

    // =========================================================================
    // EXECUTION
    // =========================================================================

    /**
     * @brief Execute the worker's main task
     * @return Result of the execution
     *
     * This is the main entry point for the worker's logic.
     * The WorkerManager calls this method at the configured interval.
     * Implementations should:
     * - Complete within a reasonable time
     * - Handle their own exceptions (or let BaseWorker handle)
     * - Return detailed results for monitoring
     */
    virtual WorkerResult execute() = 0;

    /**
     * @brief Handle an error that occurred during execution
     * @param e The exception that was thrown
     *
     * Called by the WorkerManager when execute() throws an exception.
     * Override to implement custom error handling logic.
     */
    virtual void onError(const std::exception& e) {
        // Default implementation does nothing
        // Subclasses can override to add logging, alerting, etc.
        (void)e;  // Suppress unused parameter warning
    }

    // =========================================================================
    // HEALTH
    // =========================================================================

    /**
     * @brief Check if the worker is healthy
     * @return true if the worker is healthy and can execute
     *
     * Called by health check systems to verify worker status.
     * Override to add custom health checks (e.g., database connectivity).
     */
    virtual bool isHealthy() const { return true; }

    /**
     * @brief Get detailed health information
     * @return JSON object with health details
     */
    virtual Json::Value getHealthDetails() const {
        Json::Value details;
        details["healthy"] = isHealthy();
        details["name"] = getName();
        return details;
    }

    // =========================================================================
    // LIFECYCLE
    // =========================================================================

    /**
     * @brief Called when the worker is starting
     *
     * Override to perform initialization tasks before the first execution.
     * Called once when the worker is registered and started.
     */
    virtual void onStart() {}

    /**
     * @brief Called when the worker is stopping
     *
     * Override to perform cleanup tasks before shutdown.
     * Called once when the worker is stopped or the system shuts down.
     */
    virtual void onStop() {}

    /**
     * @brief Called when the worker is paused
     *
     * Override to handle pause events (e.g., release resources).
     */
    virtual void onPause() {}

    /**
     * @brief Called when the worker is resumed
     *
     * Override to handle resume events (e.g., reacquire resources).
     */
    virtual void onResume() {}
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Convert WorkerStatus to string
 * @param status The status
 * @return String representation
 */
inline std::string workerStatusToString(WorkerStatus status) {
    switch (status) {
        case WorkerStatus::STOPPED:  return "stopped";
        case WorkerStatus::STARTING: return "starting";
        case WorkerStatus::RUNNING:  return "running";
        case WorkerStatus::PAUSED:   return "paused";
        case WorkerStatus::STOPPING: return "stopping";
        case WorkerStatus::ERROR:    return "error";
        default:                     return "unknown";
    }
}

/**
 * @brief Convert string to WorkerStatus
 * @param str The string
 * @return WorkerStatus
 */
inline WorkerStatus stringToWorkerStatus(const std::string& str) {
    if (str == "stopped")  return WorkerStatus::STOPPED;
    if (str == "starting") return WorkerStatus::STARTING;
    if (str == "running")  return WorkerStatus::RUNNING;
    if (str == "paused")   return WorkerStatus::PAUSED;
    if (str == "stopping") return WorkerStatus::STOPPING;
    if (str == "error")    return WorkerStatus::ERROR;
    return WorkerStatus::STOPPED;
}

/**
 * @brief Convert WorkerPriority to string
 * @param priority The priority
 * @return String representation
 */
inline std::string workerPriorityToString(WorkerPriority priority) {
    switch (priority) {
        case WorkerPriority::LOW:      return "low";
        case WorkerPriority::NORMAL:   return "normal";
        case WorkerPriority::HIGH:     return "high";
        case WorkerPriority::CRITICAL: return "critical";
        default:                       return "unknown";
    }
}

/**
 * @brief Convert string to WorkerPriority
 * @param str The string
 * @return WorkerPriority
 */
inline WorkerPriority stringToWorkerPriority(const std::string& str) {
    if (str == "low")      return WorkerPriority::LOW;
    if (str == "normal")   return WorkerPriority::NORMAL;
    if (str == "high")     return WorkerPriority::HIGH;
    if (str == "critical") return WorkerPriority::CRITICAL;
    return WorkerPriority::NORMAL;
}

} // namespace wtl::workers
