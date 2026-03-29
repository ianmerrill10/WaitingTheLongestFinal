/**
 * @file MediaWorker.h
 * @brief Background worker for processing media queue jobs
 *
 * PURPOSE:
 * Provides background processing for media jobs from the queue.
 * Handles video generation, image processing, transcoding, and cleanup
 * tasks without blocking API requests.
 *
 * USAGE:
 * auto& worker = MediaWorker::getInstance();
 * worker.start(4);  // Start with 4 worker threads
 * // ... application runs ...
 * worker.stop();    // Graceful shutdown
 *
 * DEPENDENCIES:
 * - MediaQueue (job queue)
 * - VideoGenerator, ImageProcessor (processing)
 * - MediaStorage (storage operations)
 * - ErrorCapture, SelfHealing (error handling)
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/media/MediaQueue.h"

namespace wtl::content::media {

// Forward declarations
class VideoGenerator;
class ImageProcessor;
class MediaStorage;

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct WorkerConfig
 * @brief Configuration for media worker
 */
struct WorkerConfig {
    int num_threads = 4;              ///< Number of worker threads
    int poll_interval_ms = 1000;      ///< Polling interval when queue is empty
    int job_timeout_minutes = 30;     ///< Job timeout
    int max_retries = 3;              ///< Maximum job retries
    bool process_video = true;        ///< Process video generation jobs
    bool process_image = true;        ///< Process image processing jobs
    bool process_transcode = true;    ///< Process transcoding jobs
    bool auto_cleanup = true;         ///< Auto cleanup old jobs
    int cleanup_interval_hours = 24;  ///< Cleanup interval
    int cleanup_days_old = 7;         ///< Delete jobs older than this

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static WorkerConfig fromJson(const Json::Value& json);

    /**
     * @brief Get default configuration
     */
    static WorkerConfig defaults();
};

/**
 * @struct WorkerStats
 * @brief Statistics for media worker
 */
struct WorkerStats {
    int active_threads = 0;           ///< Number of active threads
    int jobs_processed = 0;           ///< Total jobs processed
    int jobs_succeeded = 0;           ///< Successful jobs
    int jobs_failed = 0;              ///< Failed jobs
    int jobs_retried = 0;             ///< Retried jobs
    double avg_processing_time_ms = 0; ///< Average processing time
    std::string last_error;           ///< Last error message
    std::chrono::system_clock::time_point started_at;  ///< Worker start time
    std::chrono::system_clock::time_point last_activity; ///< Last job activity

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @brief Callback for job completion notifications
 */
using JobCompletionCallback = std::function<void(const MediaJob& job, bool success)>;

// ============================================================================
// MEDIA WORKER CLASS
// ============================================================================

/**
 * @class MediaWorker
 * @brief Background worker for processing media queue jobs
 *
 * Manages a pool of worker threads that process jobs from the MediaQueue.
 * Provides graceful startup/shutdown and job progress tracking.
 */
class MediaWorker {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the MediaWorker singleton
     */
    static MediaWorker& getInstance();

    // Delete copy/move constructors
    MediaWorker(const MediaWorker&) = delete;
    MediaWorker& operator=(const MediaWorker&) = delete;
    MediaWorker(MediaWorker&&) = delete;
    MediaWorker& operator=(MediaWorker&&) = delete;

    // =========================================================================
    // LIFECYCLE
    // =========================================================================

    /**
     * @brief Start the worker with specified number of threads
     * @param num_threads Number of worker threads
     * @return True if started successfully
     */
    bool start(int num_threads = 4);

    /**
     * @brief Start with configuration
     * @param config Worker configuration
     * @return True if started successfully
     */
    bool start(const WorkerConfig& config);

    /**
     * @brief Start from application config
     * @return True if started successfully
     */
    bool startFromConfig();

    /**
     * @brief Stop all worker threads gracefully
     * @param wait_for_completion Wait for current jobs to complete
     */
    void stop(bool wait_for_completion = true);

    /**
     * @brief Check if worker is running
     * @return True if running
     */
    bool isRunning() const;

    /**
     * @brief Get number of active threads
     * @return Thread count
     */
    int getActiveThreadCount() const;

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Set job completion callback
     * @param callback Callback function
     */
    void setCompletionCallback(JobCompletionCallback callback);

    /**
     * @brief Set job types to process
     * @param types Vector of job types
     */
    void setProcessableTypes(const std::vector<JobType>& types);

    /**
     * @brief Enable/disable specific job type processing
     * @param type Job type
     * @param enabled Whether to process this type
     */
    void setTypeEnabled(JobType type, bool enabled);

    // =========================================================================
    // MONITORING
    // =========================================================================

    /**
     * @brief Get worker statistics
     * @return Worker stats
     */
    WorkerStats getStats() const;

    /**
     * @brief Get current configuration
     * @return Worker config
     */
    WorkerConfig getConfig() const;

    /**
     * @brief Get queue statistics
     * @return Queue stats from MediaQueue
     */
    QueueStats getQueueStats() const;

    // =========================================================================
    // MANUAL OPERATIONS
    // =========================================================================

    /**
     * @brief Manually process a single job
     * @param job_id Job ID to process
     * @return True if processed successfully
     */
    bool processJob(const std::string& job_id);

    /**
     * @brief Run cleanup of old jobs
     * @return Number of jobs cleaned up
     */
    int runCleanup();

    /**
     * @brief Reset stuck jobs
     * @return Number of jobs reset
     */
    int resetStuckJobs();

    /**
     * @brief Trigger a processing cycle
     */
    void triggerProcessing();

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    MediaWorker() = default;
    ~MediaWorker();

    /**
     * @brief Worker thread function
     * @param thread_id Thread identifier
     */
    void workerThread(int thread_id);

    /**
     * @brief Cleanup thread function
     */
    void cleanupThread();

    /**
     * @brief Process a single job
     * @param job Job to process
     * @return True if successful
     */
    bool processJobInternal(MediaJob& job);

    /**
     * @brief Process video generation job
     * @param job Job to process
     * @return True if successful
     */
    bool processVideoGeneration(MediaJob& job);

    /**
     * @brief Process image processing job
     * @param job Job to process
     * @return True if successful
     */
    bool processImageProcessing(MediaJob& job);

    /**
     * @brief Process share card generation job
     * @param job Job to process
     * @return True if successful
     */
    bool processShareCard(MediaJob& job);

    /**
     * @brief Process thumbnail generation job
     * @param job Job to process
     * @return True if successful
     */
    bool processThumbnail(MediaJob& job);

    /**
     * @brief Process platform transcoding job
     * @param job Job to process
     * @return True if successful
     */
    bool processPlatformTranscode(MediaJob& job);

    /**
     * @brief Process batch processing job
     * @param job Job to process
     * @return True if successful
     */
    bool processBatch(MediaJob& job);

    /**
     * @brief Process cleanup job
     * @param job Job to process
     * @return True if successful
     */
    bool processCleanup(MediaJob& job);

    /**
     * @brief Send webhook notification for job completion
     * @param job Completed job
     * @param success Whether job succeeded
     */
    void sendWebhookNotification(const MediaJob& job, bool success);

    /**
     * @brief Check if job type should be processed
     * @param type Job type
     * @return True if type is enabled
     */
    bool isTypeEnabled(JobType type) const;

    /**
     * @brief Update job progress
     * @param job_id Job ID
     * @param progress Progress percentage
     * @param stage Current stage name
     */
    void updateProgress(const std::string& job_id, double progress, const std::string& stage);

    // Configuration
    WorkerConfig config_;

    // State
    std::atomic<bool> running_{false};
    std::atomic<int> active_threads_{0};

    // Threads
    std::vector<std::thread> worker_threads_;
    std::thread cleanup_thread_;

    // Synchronization
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    // Enabled job types
    std::atomic<bool> video_enabled_{true};
    std::atomic<bool> image_enabled_{true};
    std::atomic<bool> share_card_enabled_{true};
    std::atomic<bool> thumbnail_enabled_{true};
    std::atomic<bool> transcode_enabled_{true};
    std::atomic<bool> batch_enabled_{true};
    std::atomic<bool> cleanup_enabled_{true};

    // Callback
    JobCompletionCallback completion_callback_;

    // Statistics
    std::atomic<uint64_t> jobs_processed_{0};
    std::atomic<uint64_t> jobs_succeeded_{0};
    std::atomic<uint64_t> jobs_failed_{0};
    std::atomic<uint64_t> jobs_retried_{0};
    std::atomic<uint64_t> total_processing_time_ms_{0};
    std::chrono::system_clock::time_point started_at_;
    std::chrono::system_clock::time_point last_activity_;
    std::string last_error_;
};

} // namespace wtl::content::media
