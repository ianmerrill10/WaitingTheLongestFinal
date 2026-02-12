/**
 * @file TikTokWorker.h
 * @brief Background worker for TikTok automation tasks
 *
 * PURPOSE:
 * Background worker that runs on a configurable interval to:
 * - Process scheduled posts that are due
 * - Update analytics for published posts
 * - Generate posts for urgent dogs automatically
 * - Retry failed posts
 * - Clean up expired posts
 *
 * USAGE:
 * auto& worker = TikTokWorker::getInstance();
 * worker.start();
 * // ... later ...
 * worker.stop();
 *
 * DEPENDENCIES:
 * - TikTokEngine (post management)
 * - TikTokClient (API communication)
 * - ErrorCapture (error logging)
 * - Config (configuration)
 *
 * MODIFICATION GUIDE:
 * - Add new tasks to the runCycle() method
 * - Adjust intervals in configuration
 * - Add new metrics to WorkerStatistics
 *
 * @author Agent 11 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

// Third-party includes
#include <json/json.h>

namespace wtl::content::tiktok {

// ============================================================================
// WORKER STATISTICS
// ============================================================================

/**
 * @struct WorkerStatistics
 * @brief Statistics for the background worker
 */
struct WorkerStatistics {
    uint64_t total_cycles = 0;              ///< Total work cycles completed
    uint64_t scheduled_posts_processed = 0; ///< Scheduled posts processed
    uint64_t analytics_updates = 0;         ///< Analytics update operations
    uint64_t auto_posts_generated = 0;      ///< Auto-generated posts
    uint64_t failed_retries = 0;            ///< Failed post retries
    uint64_t expired_posts_cleaned = 0;     ///< Expired posts cleaned up
    uint64_t errors = 0;                    ///< Errors encountered

    std::chrono::system_clock::time_point last_run;   ///< Last successful run
    std::chrono::milliseconds last_cycle_duration{0}; ///< Duration of last cycle

    /**
     * @brief Convert statistics to JSON
     * @return Json::Value JSON representation
     */
    Json::Value toJson() const;
};

/**
 * @struct WorkerRunResult
 * @brief Result of a single worker cycle
 */
struct WorkerRunResult {
    bool success = false;
    int posts_processed = 0;
    int analytics_updated = 0;
    int posts_generated = 0;
    int retries_attempted = 0;
    int posts_expired = 0;
    std::string error_message;
    std::chrono::milliseconds duration{0};

    Json::Value toJson() const;
};

// ============================================================================
// TIKTOK WORKER CLASS
// ============================================================================

/**
 * @class TikTokWorker
 * @brief Singleton background worker for TikTok automation
 *
 * Runs periodic tasks related to TikTok posting:
 * - Posts scheduled content when due
 * - Updates analytics periodically
 * - Auto-generates content for urgent dogs
 * - Handles retry logic for failed posts
 */
class TikTokWorker {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the TikTokWorker singleton
     */
    static TikTokWorker& getInstance();

    // Delete copy/move constructors
    TikTokWorker(const TikTokWorker&) = delete;
    TikTokWorker& operator=(const TikTokWorker&) = delete;
    TikTokWorker(TikTokWorker&&) = delete;
    TikTokWorker& operator=(TikTokWorker&&) = delete;

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Start the background worker
     *
     * Starts the worker thread which will process tasks at the
     * configured interval.
     */
    void start();

    /**
     * @brief Stop the background worker
     *
     * Gracefully stops the worker thread, waiting for the current
     * cycle to complete.
     */
    void stop();

    /**
     * @brief Check if worker is running
     * @return true if worker thread is active
     */
    bool isRunning() const;

    /**
     * @brief Trigger an immediate run cycle
     *
     * Wakes up the worker to run immediately instead of waiting
     * for the next scheduled interval.
     */
    void triggerRun();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Initialize from Config singleton
     */
    void initializeFromConfig();

    /**
     * @brief Set the run interval
     *
     * @param interval Time between work cycles
     */
    void setInterval(std::chrono::seconds interval);

    /**
     * @brief Get the current interval
     * @return Current interval between cycles
     */
    std::chrono::seconds getInterval() const;

    /**
     * @brief Enable/disable scheduled post processing
     * @param enabled Whether to process scheduled posts
     */
    void setProcessScheduledEnabled(bool enabled);

    /**
     * @brief Enable/disable analytics updates
     * @param enabled Whether to update analytics
     */
    void setAnalyticsUpdateEnabled(bool enabled);

    /**
     * @brief Enable/disable auto-generation
     * @param enabled Whether to auto-generate posts
     */
    void setAutoGenerateEnabled(bool enabled);

    /**
     * @brief Enable/disable retry processing
     * @param enabled Whether to retry failed posts
     */
    void setRetryProcessingEnabled(bool enabled);

    /**
     * @brief Set analytics update interval
     *
     * @param interval How often to update analytics (multiple of main interval)
     */
    void setAnalyticsInterval(std::chrono::seconds interval);

    /**
     * @brief Set auto-generation interval
     *
     * @param interval How often to auto-generate posts
     */
    void setAutoGenerateInterval(std::chrono::seconds interval);

    // ========================================================================
    // MANUAL OPERATIONS
    // ========================================================================

    /**
     * @brief Run a single work cycle manually
     *
     * @return WorkerRunResult Result of the cycle
     *
     * Can be called manually to test or for on-demand processing.
     */
    WorkerRunResult runCycle();

    /**
     * @brief Process scheduled posts manually
     * @return int Number of posts processed
     */
    int processScheduledPosts();

    /**
     * @brief Update analytics manually
     * @return int Number of posts updated
     */
    int updateAnalytics();

    /**
     * @brief Generate auto posts manually
     * @return int Number of posts generated
     */
    int generateAutoPosts();

    /**
     * @brief Retry failed posts manually
     * @return int Number of retries attempted
     */
    int retryFailedPosts();

    /**
     * @brief Clean up expired posts
     * @return int Number of posts cleaned
     */
    int cleanupExpiredPosts();

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get worker statistics
     * @return WorkerStatistics Current statistics
     */
    WorkerStatistics getStatistics() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

    /**
     * @brief Get worker status as JSON
     * @return Json::Value Status information
     */
    Json::Value getStatus() const;

private:
    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================

    TikTokWorker() = default;
    ~TikTokWorker();

    /**
     * @brief Main worker thread loop
     */
    void workerLoop();

    /**
     * @brief Check if it's time for analytics update
     * @return true if analytics should be updated this cycle
     */
    bool shouldUpdateAnalytics() const;

    /**
     * @brief Check if it's time for auto-generation
     * @return true if auto-generation should run this cycle
     */
    bool shouldAutoGenerate() const;

    // Worker thread
    std::unique_ptr<std::thread> worker_thread_;

    // State
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Synchronization
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    // Configuration
    std::chrono::seconds interval_{300};  // 5 minutes default
    std::chrono::seconds analytics_interval_{1800};  // 30 minutes
    std::chrono::seconds auto_generate_interval_{3600};  // 1 hour

    bool process_scheduled_enabled_ = true;
    bool analytics_update_enabled_ = true;
    bool auto_generate_enabled_ = true;
    bool retry_processing_enabled_ = true;

    int max_retries_ = 3;
    int max_auto_posts_per_cycle_ = 5;
    int max_analytics_updates_per_cycle_ = 50;

    // Timing tracking
    std::chrono::system_clock::time_point last_analytics_update_;
    std::chrono::system_clock::time_point last_auto_generate_;

    // Statistics
    mutable std::mutex stats_mutex_;
    WorkerStatistics stats_;
};

} // namespace wtl::content::tiktok
