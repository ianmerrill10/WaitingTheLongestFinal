/**
 * @file Scheduler.h
 * @brief Job scheduler for timed and recurring tasks
 *
 * PURPOSE:
 * Manages scheduled jobs - one-time and recurring tasks that execute
 * at specified times. Uses cron expressions for recurring schedules.
 *
 * USAGE:
 * auto& scheduler = Scheduler::getInstance();
 * scheduler.schedule(job, std::chrono::system_clock::now() + std::chrono::hours(1));
 * scheduler.scheduleRecurring(job, "0 0/6 * * *");  // Every 6 hours
 * scheduler.start();
 *
 * DEPENDENCIES:
 * - ScheduledJob for job representation
 * - CronParser for cron expression handling
 * - ConnectionPool for database persistence
 * - ErrorCapture for error logging
 *
 * MODIFICATION GUIDE:
 * - Add new job types in the handler registry
 * - Extend processing logic for complex job types
 * - Customize persistence strategy as needed
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
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "ScheduledJob.h"

namespace wtl::workers::scheduler {

// Forward declaration
class CronParser;

/**
 * @brief Job handler function type
 */
using JobHandler = std::function<JobExecutionResult(const ScheduledJob&)>;

/**
 * @struct SchedulerConfig
 * @brief Configuration for the Scheduler
 */
struct SchedulerConfig {
    std::chrono::seconds poll_interval{1};            ///< How often to check for due jobs
    int max_concurrent_jobs{10};                      ///< Max jobs running at once
    bool persist_to_database{true};                   ///< Whether to persist jobs
    bool load_on_startup{true};                       ///< Load jobs from DB on startup
    std::chrono::seconds job_timeout{300};            ///< Max execution time per job
    int max_retries{3};                               ///< Default max retries
    std::chrono::seconds retry_delay{60};             ///< Delay between retries
};

/**
 * @struct SchedulerStats
 * @brief Statistics for the scheduler
 */
struct SchedulerStats {
    uint64_t total_jobs_scheduled{0};                 ///< Total jobs ever scheduled
    uint64_t total_jobs_executed{0};                  ///< Total jobs executed
    uint64_t successful_jobs{0};                      ///< Jobs that succeeded
    uint64_t failed_jobs{0};                          ///< Jobs that failed
    uint64_t cancelled_jobs{0};                       ///< Jobs that were cancelled
    uint64_t expired_jobs{0};                         ///< Jobs that expired
    uint64_t retried_jobs{0};                         ///< Jobs that were retried
    int current_pending{0};                           ///< Currently pending jobs
    int current_running{0};                           ///< Currently running jobs
    std::chrono::system_clock::time_point last_run;   ///< Last job execution time

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["total_jobs_scheduled"] = static_cast<Json::UInt64>(total_jobs_scheduled);
        json["total_jobs_executed"] = static_cast<Json::UInt64>(total_jobs_executed);
        json["successful_jobs"] = static_cast<Json::UInt64>(successful_jobs);
        json["failed_jobs"] = static_cast<Json::UInt64>(failed_jobs);
        json["cancelled_jobs"] = static_cast<Json::UInt64>(cancelled_jobs);
        json["expired_jobs"] = static_cast<Json::UInt64>(expired_jobs);
        json["retried_jobs"] = static_cast<Json::UInt64>(retried_jobs);
        json["current_pending"] = current_pending;
        json["current_running"] = current_running;

        if (last_run.time_since_epoch().count() > 0) {
            auto t = std::chrono::system_clock::to_time_t(last_run);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
            json["last_run"] = buf;
        }

        return json;
    }
};

/**
 * @class Scheduler
 * @brief Singleton scheduler for managing timed jobs
 *
 * Handles scheduling, persistence, and execution of background jobs.
 * Supports one-time and recurring (cron-based) schedules.
 */
class Scheduler {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the Scheduler singleton
     */
    static Scheduler& getInstance();

    // Delete copy/move operations
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler(Scheduler&&) = delete;
    Scheduler& operator=(Scheduler&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the scheduler
     * @param config Configuration settings
     */
    void initialize(const SchedulerConfig& config = {});

    /**
     * @brief Initialize from Config singleton
     */
    void initializeFromConfig();

    /**
     * @brief Check if scheduler is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    /**
     * @brief Start the scheduler processing loop
     */
    void start();

    /**
     * @brief Stop the scheduler
     * @param wait_for_running Wait for running jobs to complete
     */
    void stop(bool wait_for_running = true);

    /**
     * @brief Check if scheduler is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Shutdown the scheduler
     */
    void shutdown();

    // =========================================================================
    // JOB SCHEDULING
    // =========================================================================

    /**
     * @brief Schedule a job for a specific time
     *
     * @param job The job to schedule
     * @param time When to execute
     * @return Job ID
     */
    std::string schedule(ScheduledJob job,
                         std::chrono::system_clock::time_point time);

    /**
     * @brief Schedule a job with a delay
     *
     * @param job The job to schedule
     * @param delay How long to wait before execution
     * @return Job ID
     */
    std::string scheduleDelayed(ScheduledJob job,
                                std::chrono::seconds delay);

    /**
     * @brief Schedule a recurring job with cron expression
     *
     * @param job The job to schedule
     * @param cron_expression Cron expression for schedule
     * @return Job ID, or empty string if invalid cron
     */
    std::string scheduleRecurring(ScheduledJob job,
                                   const std::string& cron_expression);

    /**
     * @brief Schedule a recurring job with fixed interval
     *
     * @param job The job to schedule
     * @param interval Time between executions
     * @return Job ID
     */
    std::string scheduleInterval(ScheduledJob job,
                                  std::chrono::seconds interval);

    /**
     * @brief Cancel a scheduled job
     *
     * @param job_id Job ID to cancel
     * @return true if cancelled
     */
    bool cancel(const std::string& job_id);

    /**
     * @brief Cancel all jobs of a specific type
     *
     * @param job_type Job type to cancel
     * @return Number of jobs cancelled
     */
    int cancelByType(const std::string& job_type);

    /**
     * @brief Cancel all jobs for an entity
     *
     * @param entity_id Entity ID
     * @return Number of jobs cancelled
     */
    int cancelByEntity(const std::string& entity_id);

    /**
     * @brief Reschedule a job
     *
     * @param job_id Job ID
     * @param new_time New execution time
     * @return true if rescheduled
     */
    bool reschedule(const std::string& job_id,
                    std::chrono::system_clock::time_point new_time);

    // =========================================================================
    // JOB QUERIES
    // =========================================================================

    /**
     * @brief Get a job by ID
     *
     * @param job_id Job ID
     * @return Job if found
     */
    std::optional<ScheduledJob> getJob(const std::string& job_id) const;

    /**
     * @brief Get all scheduled jobs
     *
     * @param include_completed Include completed jobs
     * @return Vector of jobs
     */
    std::vector<ScheduledJob> getScheduledJobs(bool include_completed = false) const;

    /**
     * @brief Get pending jobs
     * @return Vector of pending jobs
     */
    std::vector<ScheduledJob> getPendingJobs() const;

    /**
     * @brief Get jobs by type
     *
     * @param job_type Job type
     * @return Vector of matching jobs
     */
    std::vector<ScheduledJob> getJobsByType(const std::string& job_type) const;

    /**
     * @brief Get jobs for an entity
     *
     * @param entity_id Entity ID
     * @return Vector of matching jobs
     */
    std::vector<ScheduledJob> getJobsByEntity(const std::string& entity_id) const;

    /**
     * @brief Get due jobs (ready for execution)
     * @return Vector of due jobs
     */
    std::vector<ScheduledJob> getDueJobs() const;

    /**
     * @brief Get job count by status
     *
     * @param status Job status
     * @return Count
     */
    int getJobCount(JobStatus status) const;

    // =========================================================================
    // JOB HANDLERS
    // =========================================================================

    /**
     * @brief Register a handler for a job type
     *
     * @param job_type Job type
     * @param handler Handler function
     */
    void registerHandler(const std::string& job_type, JobHandler handler);

    /**
     * @brief Unregister a handler
     *
     * @param job_type Job type
     */
    void unregisterHandler(const std::string& job_type);

    /**
     * @brief Check if a handler is registered
     *
     * @param job_type Job type
     * @return true if registered
     */
    bool hasHandler(const std::string& job_type) const;

    // =========================================================================
    // PROCESSING
    // =========================================================================

    /**
     * @brief Process scheduled jobs (main loop)
     *
     * Called automatically when scheduler is running.
     * Can be called manually for testing.
     */
    void processScheduledJobs();

    /**
     * @brief Execute a specific job immediately
     *
     * @param job_id Job ID
     * @return Execution result
     */
    JobExecutionResult executeJobNow(const std::string& job_id);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get scheduler statistics
     * @return Statistics
     */
    SchedulerStats getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    // =========================================================================
    // PERSISTENCE
    // =========================================================================

    /**
     * @brief Save all jobs to database
     */
    void persistJobs();

    /**
     * @brief Load jobs from database
     */
    void loadJobs();

    /**
     * @brief Clear all jobs
     * @param include_database Also clear from database
     */
    void clearAllJobs(bool include_database = false);

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    Scheduler() = default;
    ~Scheduler();

    /**
     * @brief The main processing loop
     */
    void runLoop();

    /**
     * @brief Execute a job
     *
     * @param job The job to execute
     * @return Execution result
     */
    JobExecutionResult executeJob(ScheduledJob& job);

    /**
     * @brief Handle job completion
     *
     * @param job The completed job
     * @param result Execution result
     */
    void handleJobComplete(ScheduledJob& job, const JobExecutionResult& result);

    /**
     * @brief Handle job failure
     *
     * @param job The failed job
     * @param result Execution result
     */
    void handleJobFailure(ScheduledJob& job, const JobExecutionResult& result);

    /**
     * @brief Generate a unique job ID
     * @return UUID string
     */
    std::string generateJobId() const;

    /**
     * @brief Save a single job to database
     *
     * @param job The job to save
     */
    void saveJob(const ScheduledJob& job);

    /**
     * @brief Delete a job from database
     *
     * @param job_id Job ID
     */
    void deleteJob(const std::string& job_id);

    /**
     * @brief Update job in database
     *
     * @param job The job to update
     */
    void updateJob(const ScheduledJob& job);

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
    SchedulerConfig config_;

    // Jobs storage (in-memory)
    mutable std::mutex jobs_mutex_;
    std::unordered_map<std::string, ScheduledJob> jobs_;

    // Handlers
    mutable std::mutex handlers_mutex_;
    std::unordered_map<std::string, JobHandler> handlers_;

    // Processing thread
    std::unique_ptr<std::thread> process_thread_;
    std::mutex process_mutex_;
    std::condition_variable process_cv_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    SchedulerStats stats_;

    // State
    std::atomic<bool> initialized_{false};
};

} // namespace wtl::workers::scheduler
