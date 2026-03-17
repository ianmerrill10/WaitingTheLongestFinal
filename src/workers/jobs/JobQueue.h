/**
 * @file JobQueue.h
 * @brief Job queue with Redis backend for distributed job processing
 *
 * PURPOSE:
 * Provides a reliable job queue for background task processing.
 * Uses Redis for queue storage to support distributed workers.
 * Falls back to in-memory queue if Redis is unavailable.
 *
 * USAGE:
 * auto& queue = JobQueue::getInstance();
 * queue.enqueue(job);
 * auto next_job = queue.dequeue();
 * queue.markComplete(job_id);
 *
 * DEPENDENCIES:
 * - Redis (optional, falls back to in-memory)
 * - ScheduledJob for job representation
 * - ErrorCapture for error logging
 *
 * MODIFICATION GUIDE:
 * - Add priority queue support
 * - Add dead letter queue for failed jobs
 * - Add queue metrics and monitoring
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "workers/scheduler/ScheduledJob.h"

namespace wtl::workers::jobs {

using namespace ::wtl::workers::scheduler;

/**
 * @struct QueuedJob
 * @brief A job in the queue with additional queue metadata
 */
struct QueuedJob {
    ScheduledJob job;                                 ///< The job
    std::chrono::system_clock::time_point queued_at;  ///< When job was enqueued
    int attempts{0};                                  ///< Number of processing attempts
    std::optional<std::chrono::system_clock::time_point> processing_since; ///< When processing started
    std::string worker_id;                            ///< ID of worker processing this job

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json = job.toJson();

        auto queued_time = std::chrono::system_clock::to_time_t(queued_at);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&queued_time));
        json["queued_at"] = buf;

        json["attempts"] = attempts;
        json["worker_id"] = worker_id;

        if (processing_since) {
            auto proc_time = std::chrono::system_clock::to_time_t(*processing_since);
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&proc_time));
            json["processing_since"] = buf;
        }

        return json;
    }
};

/**
 * @struct JobQueueConfig
 * @brief Configuration for JobQueue
 */
struct JobQueueConfig {
    std::string redis_host{"localhost"};              ///< Redis host
    int redis_port{6379};                             ///< Redis port
    std::string redis_password;                       ///< Redis password (optional)
    int redis_db{0};                                  ///< Redis database number
    std::string queue_name{"wtl:job_queue"};          ///< Queue name in Redis
    std::chrono::seconds visibility_timeout{300};     ///< Time before job becomes visible again
    int max_retries{3};                               ///< Max retries before dead letter
    bool use_redis{false};                            ///< Whether to use Redis (false = in-memory)
    size_t max_queue_size{10000};                     ///< Max jobs in queue (in-memory only)
};

/**
 * @struct JobQueueStats
 * @brief Statistics for the job queue
 */
struct JobQueueStats {
    uint64_t total_enqueued{0};                       ///< Total jobs enqueued
    uint64_t total_dequeued{0};                       ///< Total jobs dequeued
    uint64_t total_completed{0};                      ///< Total jobs completed
    uint64_t total_failed{0};                         ///< Total jobs failed
    uint64_t total_retried{0};                        ///< Total jobs retried
    uint64_t total_dead_lettered{0};                  ///< Total jobs sent to DLQ
    int current_pending{0};                           ///< Currently pending jobs
    int current_processing{0};                        ///< Currently processing jobs
    int current_dead_letter{0};                       ///< Currently in DLQ

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["total_enqueued"] = static_cast<Json::UInt64>(total_enqueued);
        json["total_dequeued"] = static_cast<Json::UInt64>(total_dequeued);
        json["total_completed"] = static_cast<Json::UInt64>(total_completed);
        json["total_failed"] = static_cast<Json::UInt64>(total_failed);
        json["total_retried"] = static_cast<Json::UInt64>(total_retried);
        json["total_dead_lettered"] = static_cast<Json::UInt64>(total_dead_lettered);
        json["current_pending"] = current_pending;
        json["current_processing"] = current_processing;
        json["current_dead_letter"] = current_dead_letter;
        return json;
    }
};

/**
 * @class JobQueue
 * @brief Singleton job queue for distributed job processing
 *
 * Provides reliable job queue with at-least-once delivery semantics.
 * Uses Redis when available, falls back to in-memory queue.
 */
class JobQueue {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the JobQueue singleton
     */
    static JobQueue& getInstance();

    // Delete copy/move operations
    JobQueue(const JobQueue&) = delete;
    JobQueue& operator=(const JobQueue&) = delete;
    JobQueue(JobQueue&&) = delete;
    JobQueue& operator=(JobQueue&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the job queue
     * @param config Configuration settings
     */
    void initialize(const JobQueueConfig& config = {});

    /**
     * @brief Initialize from Config singleton
     */
    void initializeFromConfig();

    /**
     * @brief Check if queue is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    /**
     * @brief Shutdown the queue
     */
    void shutdown();

    // =========================================================================
    // QUEUE OPERATIONS
    // =========================================================================

    /**
     * @brief Enqueue a job
     *
     * @param job The job to enqueue
     * @return Job ID
     */
    std::string enqueue(const ScheduledJob& job);

    /**
     * @brief Enqueue a job with priority
     *
     * @param job The job to enqueue
     * @param priority Priority (higher = more urgent)
     * @return Job ID
     */
    std::string enqueueWithPriority(const ScheduledJob& job, int priority);

    /**
     * @brief Dequeue the next job
     *
     * @param worker_id ID of the worker dequeuing
     * @return Next job, or nullopt if queue is empty
     */
    std::optional<QueuedJob> dequeue(const std::string& worker_id = "");

    /**
     * @brief Dequeue jobs of a specific type
     *
     * @param job_type Job type to dequeue
     * @param worker_id ID of the worker
     * @return Matching job, or nullopt if none
     */
    std::optional<QueuedJob> dequeueByType(const std::string& job_type,
                                            const std::string& worker_id = "");

    /**
     * @brief Peek at the next job without dequeuing
     *
     * @return Next job, or nullopt if queue is empty
     */
    std::optional<QueuedJob> peek() const;

    /**
     * @brief Mark a job as complete
     *
     * @param job_id Job ID
     * @return true if marked successfully
     */
    bool markComplete(const std::string& job_id);

    /**
     * @brief Mark a job as failed
     *
     * @param job_id Job ID
     * @param error Error message
     * @param retry Whether to retry
     * @return true if marked successfully
     */
    bool markFailed(const std::string& job_id, const std::string& error, bool retry = true);

    /**
     * @brief Retry a failed job
     *
     * @param job_id Job ID
     * @return true if requeued successfully
     */
    bool retry(const std::string& job_id);

    /**
     * @brief Extend the visibility timeout for a job
     *
     * @param job_id Job ID
     * @param extension Additional time
     * @return true if extended successfully
     */
    bool extendTimeout(const std::string& job_id, std::chrono::seconds extension);

    /**
     * @brief Release a job back to the queue (abandon processing)
     *
     * @param job_id Job ID
     * @return true if released successfully
     */
    bool release(const std::string& job_id);

    // =========================================================================
    // QUEUE QUERIES
    // =========================================================================

    /**
     * @brief Get queue length
     * @return Number of pending jobs
     */
    size_t size() const;

    /**
     * @brief Check if queue is empty
     * @return true if empty
     */
    bool isEmpty() const;

    /**
     * @brief Get all pending jobs
     * @return Vector of pending jobs
     */
    std::vector<QueuedJob> getPendingJobs() const;

    /**
     * @brief Get all processing jobs
     * @return Vector of processing jobs
     */
    std::vector<QueuedJob> getProcessingJobs() const;

    /**
     * @brief Get dead letter queue jobs
     * @return Vector of dead letter jobs
     */
    std::vector<QueuedJob> getDeadLetterJobs() const;

    /**
     * @brief Get a specific job
     *
     * @param job_id Job ID
     * @return Job if found
     */
    std::optional<QueuedJob> getJob(const std::string& job_id) const;

    // =========================================================================
    // QUEUE MANAGEMENT
    // =========================================================================

    /**
     * @brief Clear the queue
     * @param include_processing Also clear processing jobs
     */
    void clear(bool include_processing = false);

    /**
     * @brief Clear dead letter queue
     * @return Number of jobs cleared
     */
    int clearDeadLetter();

    /**
     * @brief Requeue dead letter jobs
     * @return Number of jobs requeued
     */
    int requeueDeadLetter();

    /**
     * @brief Recover stale processing jobs
     *
     * Jobs that have been processing longer than the visibility timeout
     * are returned to the queue.
     *
     * @return Number of jobs recovered
     */
    int recoverStaleJobs();

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get queue statistics
     * @return Statistics
     */
    JobQueueStats getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    // =========================================================================
    // WAITING
    // =========================================================================

    /**
     * @brief Wait for a job to be available
     *
     * @param timeout Maximum time to wait
     * @return true if a job is available
     */
    bool waitForJob(std::chrono::milliseconds timeout);

    /**
     * @brief Notify that a job was enqueued
     */
    void notifyJobEnqueued();

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    JobQueue() = default;
    ~JobQueue() = default;

    /**
     * @brief Generate a unique job ID
     * @return UUID string
     */
    std::string generateJobId() const;

    /**
     * @brief Move job to dead letter queue
     *
     * @param job The job to move
     */
    void moveToDeadLetter(const QueuedJob& job);

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
    JobQueueConfig config_;

    // In-memory queue storage
    mutable std::mutex queue_mutex_;
    std::deque<QueuedJob> pending_queue_;                             ///< Pending jobs
    std::unordered_map<std::string, QueuedJob> processing_jobs_;      ///< Jobs being processed
    std::deque<QueuedJob> dead_letter_queue_;                         ///< Failed jobs

    // Index for faster lookups
    std::unordered_set<std::string> job_ids_;                         ///< All known job IDs

    // Waiting
    std::condition_variable job_cv_;

    // Statistics
    mutable std::mutex stats_mutex_;
    JobQueueStats stats_;

    // State
    std::atomic<bool> initialized_{false};
};

} // namespace wtl::workers::jobs
