/**
 * @file JobProcessor.h
 * @brief Job processor for handling queued jobs
 *
 * PURPOSE:
 * Processes jobs from the JobQueue by routing them to appropriate
 * handlers and managing their execution lifecycle.
 *
 * USAGE:
 * auto& processor = JobProcessor::getInstance();
 * processor.registerHandler("send_email", emailHandler);
 * processor.start(4);  // Start with 4 worker threads
 *
 * DEPENDENCIES:
 * - JobQueue for job storage
 * - ErrorCapture for error logging
 * - SelfHealing for retry logic
 *
 * MODIFICATION GUIDE:
 * - Add new job types by registering handlers
 * - Customize error handling per job type
 * - Add metrics collection for monitoring
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
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "JobQueue.h"

namespace wtl::workers::jobs {

using namespace ::wtl::workers::scheduler;

/**
 * @brief Job handler function type
 */
using JobHandlerFunc = std::function<JobExecutionResult(const ScheduledJob&)>;

/**
 * @struct JobProcessorConfig
 * @brief Configuration for JobProcessor
 */
struct JobProcessorConfig {
    int num_workers{4};                               ///< Number of worker threads
    std::chrono::milliseconds poll_interval{100};     ///< Queue poll interval
    std::chrono::seconds job_timeout{300};            ///< Max execution time per job
    bool process_on_startup{true};                    ///< Start processing immediately
    int max_jobs_per_worker{1000};                    ///< Max jobs per worker before restart
};

/**
 * @struct JobProcessorStats
 * @brief Statistics for the job processor
 */
struct JobProcessorStats {
    uint64_t jobs_processed{0};                       ///< Total jobs processed
    uint64_t jobs_succeeded{0};                       ///< Jobs that succeeded
    uint64_t jobs_failed{0};                          ///< Jobs that failed
    uint64_t jobs_timed_out{0};                       ///< Jobs that timed out
    std::chrono::milliseconds total_processing_time{0}; ///< Total time spent processing
    std::chrono::milliseconds avg_processing_time{0};   ///< Average processing time
    int active_workers{0};                            ///< Currently active workers

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["jobs_processed"] = static_cast<Json::UInt64>(jobs_processed);
        json["jobs_succeeded"] = static_cast<Json::UInt64>(jobs_succeeded);
        json["jobs_failed"] = static_cast<Json::UInt64>(jobs_failed);
        json["jobs_timed_out"] = static_cast<Json::UInt64>(jobs_timed_out);
        json["total_processing_time_ms"] = static_cast<Json::Int64>(total_processing_time.count());
        json["avg_processing_time_ms"] = static_cast<Json::Int64>(avg_processing_time.count());
        json["active_workers"] = active_workers;
        return json;
    }
};

/**
 * @class JobProcessor
 * @brief Singleton job processor for handling queued jobs
 *
 * Manages a pool of worker threads that process jobs from the
 * JobQueue by routing them to registered handlers.
 */
class JobProcessor {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the JobProcessor singleton
     */
    static JobProcessor& getInstance();

    // Delete copy/move operations
    JobProcessor(const JobProcessor&) = delete;
    JobProcessor& operator=(const JobProcessor&) = delete;
    JobProcessor(JobProcessor&&) = delete;
    JobProcessor& operator=(JobProcessor&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the job processor
     * @param config Configuration settings
     */
    void initialize(const JobProcessorConfig& config = {});

    /**
     * @brief Initialize from Config singleton
     */
    void initializeFromConfig();

    /**
     * @brief Check if processor is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    /**
     * @brief Start processing jobs
     * @param num_workers Number of worker threads (0 = use config value)
     */
    void start(int num_workers = 0);

    /**
     * @brief Stop processing jobs
     * @param wait_for_current Wait for current jobs to complete
     */
    void stop(bool wait_for_current = true);

    /**
     * @brief Check if processor is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Shutdown the processor
     */
    void shutdown();

    // =========================================================================
    // HANDLER REGISTRATION
    // =========================================================================

    /**
     * @brief Register a job handler
     *
     * @param job_type Job type to handle
     * @param handler Handler function
     */
    void registerHandler(const std::string& job_type, JobHandlerFunc handler);

    /**
     * @brief Unregister a job handler
     *
     * @param job_type Job type
     */
    void unregisterHandler(const std::string& job_type);

    /**
     * @brief Check if a handler is registered
     *
     * @param job_type Job type
     * @return true if handler exists
     */
    bool hasHandler(const std::string& job_type) const;

    /**
     * @brief Get registered handler types
     * @return Vector of job types
     */
    std::vector<std::string> getHandlerTypes() const;

    // =========================================================================
    // JOB PROCESSING
    // =========================================================================

    /**
     * @brief Process a single job immediately
     *
     * @param job The job to process
     * @return Execution result
     */
    JobExecutionResult processJob(const ScheduledJob& job);

    /**
     * @brief Process a job by ID from the queue
     *
     * @param job_id Job ID
     * @return Execution result
     */
    JobExecutionResult processJobById(const std::string& job_id);

    // =========================================================================
    // WORKER MANAGEMENT
    // =========================================================================

    /**
     * @brief Get number of active workers
     * @return Worker count
     */
    int getActiveWorkerCount() const;

    /**
     * @brief Scale workers up or down
     *
     * @param num_workers New worker count
     */
    void scaleWorkers(int num_workers);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get processor statistics
     * @return Statistics
     */
    JobProcessorStats getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    // =========================================================================
    // HEALTH
    // =========================================================================

    /**
     * @brief Check if processor is healthy
     * @return true if healthy
     */
    bool isHealthy() const;

    /**
     * @brief Get health details
     * @return JSON with health information
     */
    Json::Value getHealthDetails() const;

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    JobProcessor() = default;
    ~JobProcessor();

    /**
     * @brief Worker thread function
     * @param worker_id ID for this worker
     */
    void workerLoop(int worker_id);

    /**
     * @brief Execute a job with timeout
     *
     * @param job The job to execute
     * @return Execution result
     */
    JobExecutionResult executeWithTimeout(const ScheduledJob& job);

    /**
     * @brief Update statistics after job completion
     *
     * @param result Execution result
     */
    void updateStats(const JobExecutionResult& result);

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

    /**
     * @brief Generate worker ID
     * @param index Worker index
     * @return Worker ID string
     */
    std::string generateWorkerId(int index) const;

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    // Configuration
    JobProcessorConfig config_;

    // Handlers
    mutable std::mutex handlers_mutex_;
    std::unordered_map<std::string, JobHandlerFunc> handlers_;

    // Worker threads
    std::vector<std::thread> workers_;
    mutable std::mutex workers_mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    JobProcessorStats stats_;

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<int> active_workers_{0};
};

} // namespace wtl::workers::jobs
