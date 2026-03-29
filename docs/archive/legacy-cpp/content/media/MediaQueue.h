/**
 * @file MediaQueue.h
 * @brief Asynchronous media processing queue
 *
 * PURPOSE:
 * Provides a queue system for async media processing jobs.
 * Heavy operations like video generation are queued and processed
 * in the background to avoid blocking API requests.
 *
 * USAGE:
 * auto& queue = MediaQueue::getInstance();
 * std::string job_id = queue.enqueue(job);
 * auto status = queue.getStatus(job_id);
 *
 * DEPENDENCIES:
 * - ConnectionPool (database persistence)
 * - ErrorCapture, SelfHealing (error handling)
 *
 * @author Agent 15 - WaitingTheLongest Build System
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
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::content::media {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum JobType
 * @brief Types of media processing jobs
 */
enum class JobType {
    VIDEO_GENERATION = 0,
    IMAGE_PROCESSING,
    SHARE_CARD,
    THUMBNAIL,
    PLATFORM_TRANSCODE,
    BATCH_PROCESS,
    CLEANUP
};

/**
 * @enum JobStatus
 * @brief Status of a processing job
 */
enum class JobStatus {
    PENDING = 0,
    PROCESSING,
    COMPLETED,
    FAILED,
    CANCELLED
};

/**
 * @enum JobPriority
 * @brief Priority levels for jobs
 */
enum class JobPriority {
    LOW = 0,
    NORMAL,
    HIGH,
    URGENT
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct MediaJob
 * @brief A media processing job
 */
struct MediaJob {
    std::string id;                    ///< Unique job ID
    JobType type;                      ///< Job type
    JobStatus status = JobStatus::PENDING;  ///< Current status
    JobPriority priority = JobPriority::NORMAL;  ///< Job priority

    std::string dog_id;                ///< Associated dog ID
    Json::Value input_data;            ///< Input data for processing
    Json::Value result_data;           ///< Result data after processing

    std::chrono::system_clock::time_point created_at;    ///< Creation time
    std::chrono::system_clock::time_point started_at;    ///< Processing start time
    std::chrono::system_clock::time_point completed_at;  ///< Completion time

    double progress = 0.0;             ///< Progress (0-100)
    std::string current_stage;         ///< Current processing stage
    std::string error_message;         ///< Error message if failed

    int retry_count = 0;               ///< Number of retries
    int max_retries = 3;               ///< Maximum retries

    std::string callback_url;          ///< Webhook URL for completion notification

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static MediaJob fromJson(const Json::Value& json);

    /**
     * @brief Create from database row
     */
    static MediaJob fromDbRow(const std::vector<std::string>& row);
};

/**
 * @struct QueueStats
 * @brief Statistics about the queue
 */
struct QueueStats {
    int pending_count = 0;
    int processing_count = 0;
    int completed_count = 0;
    int failed_count = 0;
    double avg_processing_time_ms = 0;
    int total_processed = 0;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

// ============================================================================
// MEDIA QUEUE CLASS
// ============================================================================

/**
 * @class MediaQueue
 * @brief Singleton queue for async media processing
 *
 * Manages a persistent queue of media processing jobs.
 * Jobs are stored in the database and processed by MediaWorker.
 */
class MediaQueue {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the MediaQueue singleton
     */
    static MediaQueue& getInstance();

    // Delete copy/move constructors
    MediaQueue(const MediaQueue&) = delete;
    MediaQueue& operator=(const MediaQueue&) = delete;
    MediaQueue(MediaQueue&&) = delete;
    MediaQueue& operator=(MediaQueue&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the queue
     * @param max_queue_size Maximum queue size
     * @return True if successful
     */
    bool initialize(int max_queue_size = 1000);

    /**
     * @brief Initialize from configuration
     * @return True if successful
     */
    bool initializeFromConfig();

    /**
     * @brief Check if queue is initialized
     */
    bool isInitialized() const;

    // =========================================================================
    // ENQUEUE OPERATIONS
    // =========================================================================

    /**
     * @brief Enqueue a media processing job
     * @param job Job to enqueue
     * @return Job ID if successful, empty string if failed
     */
    std::string enqueue(const MediaJob& job);

    /**
     * @brief Enqueue video generation job
     * @param dog_id Dog ID
     * @param photos Photo paths
     * @param template_id Template ID
     * @param priority Job priority
     * @return Job ID
     */
    std::string enqueueVideoGeneration(
        const std::string& dog_id,
        const std::vector<std::string>& photos,
        const std::string& template_id,
        JobPriority priority = JobPriority::NORMAL
    );

    /**
     * @brief Enqueue image processing job
     * @param dog_id Dog ID
     * @param image_path Image path
     * @param operation Operation type
     * @param options Processing options
     * @return Job ID
     */
    std::string enqueueImageProcessing(
        const std::string& dog_id,
        const std::string& image_path,
        const std::string& operation,
        const Json::Value& options = Json::Value()
    );

    /**
     * @brief Enqueue share card generation
     * @param dog_id Dog ID
     * @param platform Target platform
     * @return Job ID
     */
    std::string enqueueShareCard(
        const std::string& dog_id,
        const std::string& platform = "facebook"
    );

    /**
     * @brief Enqueue thumbnail generation
     * @param dog_id Dog ID
     * @param video_path Video path
     * @return Job ID
     */
    std::string enqueueThumbnail(
        const std::string& dog_id,
        const std::string& video_path
    );

    /**
     * @brief Enqueue platform transcode job
     * @param dog_id Dog ID
     * @param video_path Video path
     * @param platforms Target platforms
     * @return Job ID
     */
    std::string enqueuePlatformTranscode(
        const std::string& dog_id,
        const std::string& video_path,
        const std::vector<std::string>& platforms
    );

    // =========================================================================
    // DEQUEUE OPERATIONS
    // =========================================================================

    /**
     * @brief Get next pending job
     * @return Next job if available
     */
    std::optional<MediaJob> dequeue();

    /**
     * @brief Get next pending job of specific type
     * @param type Job type
     * @return Next job if available
     */
    std::optional<MediaJob> dequeue(JobType type);

    /**
     * @brief Peek at next pending job without removing
     * @return Next job if available
     */
    std::optional<MediaJob> peek() const;

    // =========================================================================
    // JOB MANAGEMENT
    // =========================================================================

    /**
     * @brief Get job status
     * @param job_id Job ID
     * @return Job status info
     */
    std::optional<MediaJob> getStatus(const std::string& job_id);

    /**
     * @brief Update job status
     * @param job_id Job ID
     * @param status New status
     * @param result Result data
     * @param error_message Error message if failed
     * @return True if successful
     */
    bool updateStatus(
        const std::string& job_id,
        JobStatus status,
        const Json::Value& result = Json::Value(),
        const std::string& error_message = ""
    );

    /**
     * @brief Update job progress
     * @param job_id Job ID
     * @param progress Progress (0-100)
     * @param stage Current stage name
     * @return True if successful
     */
    bool updateProgress(
        const std::string& job_id,
        double progress,
        const std::string& stage = ""
    );

    /**
     * @brief Cancel a pending job
     * @param job_id Job ID
     * @return True if cancelled
     */
    bool cancel(const std::string& job_id);

    /**
     * @brief Retry a failed job
     * @param job_id Job ID
     * @return True if requeued
     */
    bool retry(const std::string& job_id);

    /**
     * @brief Get all jobs for a dog
     * @param dog_id Dog ID
     * @return List of jobs
     */
    std::vector<MediaJob> getJobsForDog(const std::string& dog_id);

    /**
     * @brief Get pending jobs
     * @param limit Maximum jobs to return
     * @return List of pending jobs
     */
    std::vector<MediaJob> getPendingJobs(int limit = 100);

    /**
     * @brief Get processing jobs
     * @return List of processing jobs
     */
    std::vector<MediaJob> getProcessingJobs();

    /**
     * @brief Get failed jobs
     * @param limit Maximum jobs to return
     * @return List of failed jobs
     */
    std::vector<MediaJob> getFailedJobs(int limit = 100);

    // =========================================================================
    // QUEUE MANAGEMENT
    // =========================================================================

    /**
     * @brief Get queue size
     * @return Number of pending jobs
     */
    int size() const;

    /**
     * @brief Check if queue is empty
     * @return True if no pending jobs
     */
    bool isEmpty() const;

    /**
     * @brief Clear all pending jobs
     * @return Number of jobs cleared
     */
    int clear();

    /**
     * @brief Get queue statistics
     * @return Queue stats
     */
    QueueStats getStats() const;

    /**
     * @brief Cleanup old completed/failed jobs
     * @param days_old Days threshold
     * @return Number of jobs cleaned up
     */
    int cleanup(int days_old = 7);

    /**
     * @brief Reset stuck processing jobs
     * @param timeout_minutes Timeout threshold
     * @return Number of jobs reset
     */
    int resetStuckJobs(int timeout_minutes = 30);

    // =========================================================================
    // NOTIFICATION
    // =========================================================================

    /**
     * @brief Wait for queue activity
     * @param timeout Maximum wait time
     * @return True if activity occurred
     */
    bool waitForActivity(std::chrono::milliseconds timeout);

    /**
     * @brief Notify of queue activity
     */
    void notifyActivity();

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    MediaQueue() = default;
    ~MediaQueue() = default;

    /**
     * @brief Generate unique job ID
     * @return Unique ID
     */
    std::string generateJobId();

    /**
     * @brief Save job to database
     * @param job Job to save
     * @return True if successful
     */
    bool saveJob(const MediaJob& job);

    /**
     * @brief Load job from database
     * @param job_id Job ID
     * @return Job if found
     */
    std::optional<MediaJob> loadJob(const std::string& job_id);

    // Configuration
    int max_queue_size_ = 1000;

    // State
    std::atomic<bool> initialized_{false};

    // Synchronization
    mutable std::mutex mutex_;
    std::condition_variable activity_cv_;

    // Statistics
    std::atomic<uint64_t> total_enqueued_{0};
    std::atomic<uint64_t> total_completed_{0};
    std::atomic<uint64_t> total_failed_{0};
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Convert JobType to string
 */
std::string jobTypeToString(JobType type);

/**
 * @brief Convert string to JobType
 */
JobType stringToJobType(const std::string& str);

/**
 * @brief Convert JobStatus to string
 */
std::string jobStatusToString(JobStatus status);

/**
 * @brief Convert string to JobStatus
 */
JobStatus stringToJobStatus(const std::string& str);

/**
 * @brief Convert JobPriority to string
 */
std::string jobPriorityToString(JobPriority priority);

/**
 * @brief Convert string to JobPriority
 */
JobPriority stringToJobPriority(const std::string& str);

} // namespace wtl::content::media
