/**
 * @file SocialWorker.h
 * @brief Background worker for social media operations
 *
 * PURPOSE:
 * The SocialWorker is a background service that handles asynchronous
 * social media operations including:
 * - Processing scheduled posts when their time comes
 * - Syncing analytics from all platforms
 * - Auto-generating posts for urgent dogs
 * - Cross-posting TikTok content to other platforms
 * - Refreshing expired OAuth tokens
 *
 * USAGE:
 * auto& worker = SocialWorker::getInstance();
 * worker.start();  // Start background processing
 * // ... application runs ...
 * worker.stop();   // Graceful shutdown
 *
 * DEPENDENCIES:
 * - SocialMediaEngine (main posting functionality)
 * - SocialAnalytics (analytics sync)
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - SelfHealing (circuit breaker)
 *
 * MODIFICATION GUIDE:
 * - Add new background tasks by creating task functions and scheduling them
 * - Adjust intervals in the configuration
 * - Add new urgent dog detection criteria as needed
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/social/Platform.h"
#include "content/social/SocialPost.h"

namespace wtl::content::social {

// ============================================================================
// WORKER TASK TYPES
// ============================================================================

/**
 * @enum WorkerTaskType
 * @brief Types of background tasks
 */
enum class WorkerTaskType {
    SCHEDULED_POST,           ///< Process a scheduled post
    ANALYTICS_SYNC,           ///< Sync analytics for posts
    URGENT_DOG_CHECK,         ///< Check for urgent dogs needing posts
    TIKTOK_CROSSPOST,         ///< Cross-post TikTok content
    TOKEN_REFRESH,            ///< Refresh OAuth tokens
    MILESTONE_CHECK,          ///< Check for milestone posts
    CLEANUP                   ///< Clean up old data
};

/**
 * @brief Convert WorkerTaskType to string
 */
std::string workerTaskTypeToString(WorkerTaskType type);

// ============================================================================
// WORKER TASK
// ============================================================================

/**
 * @struct WorkerTask
 * @brief A background task to be executed
 */
struct WorkerTask {
    std::string id;                                    ///< Task ID
    WorkerTaskType type;                               ///< Task type
    std::chrono::system_clock::time_point scheduled_at;///< When to execute
    std::chrono::system_clock::time_point created_at;  ///< When task was created
    int priority;                                      ///< Priority (higher = more urgent)
    Json::Value payload;                               ///< Task-specific data
    int retry_count;                                   ///< Number of retries
    int max_retries;                                   ///< Maximum retries
    std::string last_error;                            ///< Last error message

    /**
     * @brief Comparison for priority queue (higher priority first, then earlier time)
     */
    bool operator<(const WorkerTask& other) const {
        if (priority != other.priority) {
            return priority < other.priority;  // Higher priority first
        }
        return scheduled_at > other.scheduled_at;  // Earlier time first
    }

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static WorkerTask fromJson(const Json::Value& json);
};

/**
 * @struct WorkerTaskResult
 * @brief Result of a task execution
 */
struct WorkerTaskResult {
    std::string task_id;
    bool success;
    std::string error_message;
    std::chrono::milliseconds execution_time;
    Json::Value result_data;
};

// ============================================================================
// WORKER CONFIGURATION
// ============================================================================

/**
 * @struct SocialWorkerConfig
 * @brief Configuration for the social worker
 */
struct SocialWorkerConfig {
    // Scheduling intervals
    std::chrono::seconds scheduled_post_check_interval{60};      ///< Check scheduled posts
    std::chrono::seconds analytics_sync_interval{3600};          ///< Sync analytics (1 hour)
    std::chrono::seconds urgent_dog_check_interval{1800};        ///< Check urgent dogs (30 min)
    std::chrono::seconds tiktok_crosspost_interval{300};         ///< Check TikTok (5 min)
    std::chrono::seconds token_refresh_interval{3600};           ///< Refresh tokens (1 hour)
    std::chrono::seconds milestone_check_interval{86400};        ///< Check milestones (daily)
    std::chrono::seconds cleanup_interval{86400};                ///< Cleanup (daily)

    // Thread pool
    int worker_threads{4};                                       ///< Number of worker threads
    int max_queue_size{1000};                                    ///< Max pending tasks

    // Retry configuration
    int default_max_retries{3};                                  ///< Default retry count
    std::chrono::seconds retry_delay{300};                       ///< Delay between retries

    // Analytics
    int analytics_lookback_hours{24};                            ///< Hours to look back for sync

    // Urgent dog thresholds
    int urgent_dog_min_wait_days{365};                           ///< Min days for urgent post
    int urgent_dog_post_cooldown_days{7};                        ///< Days between urgent posts

    // TikTok cross-posting
    bool auto_crosspost_tiktok{true};                            ///< Auto cross-post TikTok

    /**
     * @brief Load from JSON configuration
     */
    static SocialWorkerConfig fromJson(const Json::Value& json);

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

// ============================================================================
// WORKER STATISTICS
// ============================================================================

/**
 * @struct WorkerStats
 * @brief Statistics for worker operations
 */
struct WorkerStats {
    std::atomic<uint64_t> tasks_processed{0};
    std::atomic<uint64_t> tasks_succeeded{0};
    std::atomic<uint64_t> tasks_failed{0};
    std::atomic<uint64_t> tasks_retried{0};

    std::atomic<uint64_t> posts_published{0};
    std::atomic<uint64_t> analytics_synced{0};
    std::atomic<uint64_t> tokens_refreshed{0};
    std::atomic<uint64_t> urgent_posts_generated{0};
    std::atomic<uint64_t> tiktok_crossposts{0};

    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point last_task_at;

    WorkerStats() = default;

    WorkerStats(const WorkerStats& other)
        : tasks_processed(other.tasks_processed.load())
        , tasks_succeeded(other.tasks_succeeded.load())
        , tasks_failed(other.tasks_failed.load())
        , tasks_retried(other.tasks_retried.load())
        , posts_published(other.posts_published.load())
        , analytics_synced(other.analytics_synced.load())
        , tokens_refreshed(other.tokens_refreshed.load())
        , urgent_posts_generated(other.urgent_posts_generated.load())
        , tiktok_crossposts(other.tiktok_crossposts.load())
        , started_at(other.started_at)
        , last_task_at(other.last_task_at)
    {}

    WorkerStats& operator=(const WorkerStats& other) {
        if (this != &other) {
            tasks_processed.store(other.tasks_processed.load());
            tasks_succeeded.store(other.tasks_succeeded.load());
            tasks_failed.store(other.tasks_failed.load());
            tasks_retried.store(other.tasks_retried.load());
            posts_published.store(other.posts_published.load());
            analytics_synced.store(other.analytics_synced.load());
            tokens_refreshed.store(other.tokens_refreshed.load());
            urgent_posts_generated.store(other.urgent_posts_generated.load());
            tiktok_crossposts.store(other.tiktok_crossposts.load());
            started_at = other.started_at;
            last_task_at = other.last_task_at;
        }
        return *this;
    }

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Reset all counters
     */
    void reset();
};

// ============================================================================
// SOCIAL WORKER
// ============================================================================

/**
 * @class SocialWorker
 * @brief Background worker for social media operations
 */
class SocialWorker {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     */
    static SocialWorker& getInstance();

    // Prevent copying
    SocialWorker(const SocialWorker&) = delete;
    SocialWorker& operator=(const SocialWorker&) = delete;

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Initialize the worker
     * @param config Configuration settings
     */
    void initialize(const SocialWorkerConfig& config = SocialWorkerConfig{});

    /**
     * @brief Initialize from application configuration
     */
    void initializeFromConfig();

    /**
     * @brief Start the background worker
     */
    void start();

    /**
     * @brief Stop the background worker (graceful shutdown)
     * @param wait_for_tasks Wait for pending tasks to complete
     */
    void stop(bool wait_for_tasks = true);

    /**
     * @brief Check if worker is running
     */
    bool isRunning() const;

    /**
     * @brief Check if worker is initialized
     */
    bool isInitialized() const;

    // ========================================================================
    // TASK MANAGEMENT
    // ========================================================================

    /**
     * @brief Queue a task for execution
     * @param task The task to queue
     * @return bool True if queued successfully
     */
    bool queueTask(const WorkerTask& task);

    /**
     * @brief Queue a scheduled post task
     * @param post_id Post ID to publish
     * @param scheduled_time When to publish
     * @return std::string Task ID
     */
    std::string queueScheduledPost(
        const std::string& post_id,
        std::chrono::system_clock::time_point scheduled_time);

    /**
     * @brief Queue an analytics sync task
     * @param post_id Post ID to sync (empty for all recent)
     * @return std::string Task ID
     */
    std::string queueAnalyticsSync(const std::string& post_id = "");

    /**
     * @brief Queue a TikTok cross-post task
     * @param tiktok_url TikTok video URL
     * @param dog_id Associated dog ID
     * @param platforms Target platforms
     * @return std::string Task ID
     */
    std::string queueTikTokCrosspost(
        const std::string& tiktok_url,
        const std::string& dog_id,
        const std::vector<Platform>& platforms = {});

    /**
     * @brief Queue a token refresh task
     * @param connection_id Connection ID to refresh
     * @return std::string Task ID
     */
    std::string queueTokenRefresh(const std::string& connection_id);

    /**
     * @brief Cancel a queued task
     * @param task_id Task ID to cancel
     * @return bool True if cancelled
     */
    bool cancelTask(const std::string& task_id);

    /**
     * @brief Get task status
     * @param task_id Task ID
     * @return std::optional<WorkerTask> Task if found
     */
    std::optional<WorkerTask> getTask(const std::string& task_id) const;

    /**
     * @brief Get all pending tasks
     * @return std::vector<WorkerTask> Pending tasks
     */
    std::vector<WorkerTask> getPendingTasks() const;

    /**
     * @brief Get pending task count
     * @return size_t Number of pending tasks
     */
    size_t getPendingTaskCount() const;

    // ========================================================================
    // MANUAL TRIGGERS
    // ========================================================================

    /**
     * @brief Manually trigger scheduled post check
     * @return int Number of posts processed
     */
    int triggerScheduledPostCheck();

    /**
     * @brief Manually trigger analytics sync
     * @param hours_back Hours to look back
     * @return int Number of posts synced
     */
    int triggerAnalyticsSync(int hours_back = 24);

    /**
     * @brief Manually trigger urgent dog check
     * @return int Number of posts generated
     */
    int triggerUrgentDogCheck();

    /**
     * @brief Manually trigger TikTok cross-post check
     * @return int Number of videos cross-posted
     */
    int triggerTikTokCrosspostCheck();

    /**
     * @brief Manually trigger token refresh
     * @return int Number of tokens refreshed
     */
    int triggerTokenRefresh();

    /**
     * @brief Manually trigger milestone check
     * @return int Number of milestone posts generated
     */
    int triggerMilestoneCheck();

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get worker statistics
     * @return WorkerStats Current statistics
     */
    WorkerStats getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Get current configuration
     * @return SocialWorkerConfig Current config
     */
    SocialWorkerConfig getConfig() const;

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void updateConfig(const SocialWorkerConfig& config);

    // ========================================================================
    // CALLBACKS
    // ========================================================================

    /**
     * @brief Set callback for task completion
     * @param callback Function to call on task completion
     */
    void setTaskCompletionCallback(
        std::function<void(const WorkerTaskResult&)> callback);

    /**
     * @brief Set callback for task failure
     * @param callback Function to call on task failure
     */
    void setTaskFailureCallback(
        std::function<void(const WorkerTask&, const std::string&)> callback);

private:
    SocialWorker() = default;
    ~SocialWorker();

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    /**
     * @brief Main scheduler thread function
     */
    void schedulerLoop();

    /**
     * @brief Worker thread function
     */
    void workerLoop();

    /**
     * @brief Execute a single task
     */
    WorkerTaskResult executeTask(const WorkerTask& task);

    /**
     * @brief Execute scheduled post task
     */
    WorkerTaskResult executeScheduledPost(const WorkerTask& task);

    /**
     * @brief Execute analytics sync task
     */
    WorkerTaskResult executeAnalyticsSync(const WorkerTask& task);

    /**
     * @brief Execute urgent dog check task
     */
    WorkerTaskResult executeUrgentDogCheck(const WorkerTask& task);

    /**
     * @brief Execute TikTok crosspost task
     */
    WorkerTaskResult executeTikTokCrosspost(const WorkerTask& task);

    /**
     * @brief Execute token refresh task
     */
    WorkerTaskResult executeTokenRefresh(const WorkerTask& task);

    /**
     * @brief Execute milestone check task
     */
    WorkerTaskResult executeMilestoneCheck(const WorkerTask& task);

    /**
     * @brief Execute cleanup task
     */
    WorkerTaskResult executeCleanup(const WorkerTask& task);

    /**
     * @brief Schedule recurring tasks
     */
    void scheduleRecurringTasks();

    /**
     * @brief Generate UUID for task
     */
    std::string generateTaskId() const;

    /**
     * @brief Find dogs needing urgent posts
     */
    std::vector<std::string> findUrgentDogs() const;

    /**
     * @brief Find dogs with milestones
     */
    std::vector<std::pair<std::string, std::string>> findMilestoneDogs() const;

    /**
     * @brief Find new TikTok videos to cross-post
     */
    std::vector<std::pair<std::string, std::string>> findNewTikTokVideos() const;

    /**
     * @brief Find connections needing token refresh
     */
    std::vector<std::string> findExpiringConnections() const;

    /**
     * @brief Retry a failed task
     */
    void retryTask(WorkerTask task, const std::string& error);

    // ========================================================================
    // STATE
    // ========================================================================

    bool initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};

    SocialWorkerConfig config_;
    WorkerStats stats_;

    // Task queue (priority queue)
    std::priority_queue<WorkerTask> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Task tracking (for cancellation and lookup)
    std::map<std::string, WorkerTask> task_map_;

    // Threads
    std::thread scheduler_thread_;
    std::vector<std::thread> worker_threads_;

    // Callbacks
    std::function<void(const WorkerTaskResult&)> completion_callback_;
    std::function<void(const WorkerTask&, const std::string&)> failure_callback_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::content::social
