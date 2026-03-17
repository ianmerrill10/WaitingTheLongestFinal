/**
 * @file BaseWorker.h
 * @brief Base implementation of the IWorker interface
 *
 * PURPOSE:
 * Provides common functionality for all workers including:
 * - Run loop with configurable interval
 * - Error handling and retry logic
 * - Health monitoring and statistics tracking
 * - Logging integration with ErrorCapture
 *
 * USAGE:
 * class MyWorker : public BaseWorker {
 * public:
 *     MyWorker() : BaseWorker("my_worker", "My Worker", std::chrono::seconds(60)) {}
 * protected:
 *     WorkerResult doExecute() override { ... }
 * };
 *
 * DEPENDENCIES:
 * - IWorker interface
 * - ErrorCapture for error logging
 * - SelfHealing for retry logic
 *
 * MODIFICATION GUIDE:
 * - Override doExecute() for worker-specific logic
 * - Override lifecycle hooks (onStart, onStop) as needed
 * - Configure retry and backoff via constructor or setters
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
#include <string>
#include <thread>

// Project includes
#include "IWorker.h"

namespace wtl::workers {

/**
 * @struct BaseWorkerConfig
 * @brief Configuration for BaseWorker behavior
 */
struct BaseWorkerConfig {
    int max_retries{3};                               ///< Maximum retry attempts on failure
    std::chrono::milliseconds retry_delay{1000};      ///< Initial delay between retries
    double retry_backoff_multiplier{2.0};             ///< Backoff multiplier for retries
    std::chrono::milliseconds max_retry_delay{30000}; ///< Maximum retry delay
    bool log_executions{true};                        ///< Log each execution
    bool capture_errors{true};                        ///< Capture errors to ErrorCapture
    int max_consecutive_failures{10};                 ///< Max failures before auto-stop
    std::chrono::seconds execution_timeout{300};      ///< Maximum execution time
};

/**
 * @class BaseWorker
 * @brief Base class for all worker implementations
 *
 * Provides common functionality including run loop management,
 * error handling, statistics tracking, and logging integration.
 * Subclasses implement doExecute() for their specific logic.
 */
class BaseWorker : public IWorker {
public:
    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Construct a BaseWorker
     *
     * @param name Unique worker name
     * @param description Human-readable description
     * @param interval Execution interval
     * @param priority Worker priority
     */
    BaseWorker(
        const std::string& name,
        const std::string& description,
        std::chrono::seconds interval,
        WorkerPriority priority = WorkerPriority::NORMAL);

    /**
     * @brief Virtual destructor
     */
    virtual ~BaseWorker();

    // Delete copy operations
    BaseWorker(const BaseWorker&) = delete;
    BaseWorker& operator=(const BaseWorker&) = delete;

    // =========================================================================
    // IWORKER INTERFACE
    // =========================================================================

    /**
     * @brief Get the worker name
     * @return Worker name
     */
    std::string getName() const override;

    /**
     * @brief Get the worker description
     * @return Worker description
     */
    std::string getDescription() const override;

    /**
     * @brief Get the execution interval
     * @return Interval between executions
     */
    std::chrono::seconds getInterval() const override;

    /**
     * @brief Get worker priority
     * @return Priority level
     */
    WorkerPriority getPriority() const override;

    /**
     * @brief Execute the worker
     * @return Execution result
     *
     * Wraps doExecute() with error handling, retries, and statistics.
     */
    WorkerResult execute() override;

    /**
     * @brief Handle an execution error
     * @param e The exception
     */
    void onError(const std::exception& e) override;

    /**
     * @brief Check if the worker is healthy
     * @return true if healthy
     */
    bool isHealthy() const override;

    /**
     * @brief Get detailed health information
     * @return JSON with health details
     */
    Json::Value getHealthDetails() const override;

    /**
     * @brief Called when worker starts
     */
    void onStart() override;

    /**
     * @brief Called when worker stops
     */
    void onStop() override;

    // =========================================================================
    // STATE MANAGEMENT
    // =========================================================================

    /**
     * @brief Get current worker status
     * @return Current status
     */
    WorkerStatus getStatus() const;

    /**
     * @brief Get worker statistics
     * @return Statistics structure
     */
    WorkerStats getStats() const;

    /**
     * @brief Get the last execution result
     * @return Last result (may be empty)
     */
    WorkerResult getLastResult() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Set worker configuration
     * @param config Configuration structure
     */
    void setConfig(const BaseWorkerConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    BaseWorkerConfig getConfig() const;

    /**
     * @brief Set execution interval
     * @param interval New interval
     */
    void setInterval(std::chrono::seconds interval);

    /**
     * @brief Set worker enabled state
     * @param enabled Whether the worker should run
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if worker is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

    // =========================================================================
    // RUN LOOP CONTROL
    // =========================================================================

    /**
     * @brief Start the worker's run loop in a background thread
     */
    void startRunLoop();

    /**
     * @brief Stop the worker's run loop
     * @param wait If true, wait for current execution to complete
     */
    void stopRunLoop(bool wait = true);

    /**
     * @brief Check if the run loop is active
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Pause the run loop
     */
    void pause();

    /**
     * @brief Resume the run loop
     */
    void resume();

    /**
     * @brief Check if the worker is paused
     * @return true if paused
     */
    bool isPaused() const;

    /**
     * @brief Request immediate execution
     *
     * Signals the worker to execute immediately instead of
     * waiting for the next scheduled interval.
     */
    void requestImmediateExecution();

protected:
    // =========================================================================
    // PROTECTED METHODS FOR SUBCLASSES
    // =========================================================================

    /**
     * @brief Execute the worker's main task
     * @return Execution result
     *
     * Subclasses MUST implement this method with their specific logic.
     * This method should:
     * - Complete its work as efficiently as possible
     * - Return accurate results for monitoring
     * - Not call execute() (that's the wrapper)
     */
    virtual WorkerResult doExecute() = 0;

    /**
     * @brief Called before execution starts
     *
     * Override to perform pre-execution setup.
     */
    virtual void beforeExecute() {}

    /**
     * @brief Called after execution completes
     * @param result The execution result
     *
     * Override to perform post-execution cleanup or logging.
     */
    virtual void afterExecute(const WorkerResult& result) {
        (void)result;  // Suppress unused parameter warning
    }

    /**
     * @brief Check if execution should be retried
     * @param attempt Current attempt number (0-based)
     * @param e The exception that occurred
     * @return true if should retry
     *
     * Override to customize retry logic.
     */
    virtual bool shouldRetry(int attempt, const std::exception& e);

    /**
     * @brief Log an info message
     * @param message The message
     */
    void logInfo(const std::string& message);

    /**
     * @brief Log a warning message
     * @param message The message
     */
    void logWarning(const std::string& message);

    /**
     * @brief Log an error message
     * @param message The message
     * @param metadata Additional metadata
     */
    void logError(const std::string& message,
                  const std::unordered_map<std::string, std::string>& metadata = {});

    /**
     * @brief Update next run time
     */
    void scheduleNextRun();

    /**
     * @brief Get current time point
     * @return Current time
     */
    std::chrono::system_clock::time_point now() const;

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    /**
     * @brief The main run loop
     */
    void runLoop();

    /**
     * @brief Update statistics after execution
     * @param result The execution result
     */
    void updateStats(const WorkerResult& result);

    /**
     * @brief Calculate retry delay with backoff
     * @param attempt Current attempt number
     * @return Delay duration
     */
    std::chrono::milliseconds calculateRetryDelay(int attempt) const;

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    // Identity
    std::string name_;
    std::string description_;
    std::chrono::seconds interval_;
    WorkerPriority priority_;

    // Configuration
    BaseWorkerConfig config_;

    // State
    std::atomic<WorkerStatus> status_{WorkerStatus::STOPPED};
    std::atomic<bool> enabled_{true};
    std::atomic<bool> paused_{false};
    std::atomic<bool> immediate_execution_requested_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    WorkerStats stats_;
    WorkerResult last_result_;

    // Run loop
    std::unique_ptr<std::thread> run_thread_;
    std::mutex run_mutex_;
    std::condition_variable run_cv_;
    std::atomic<bool> stop_requested_{false};
};

} // namespace wtl::workers
