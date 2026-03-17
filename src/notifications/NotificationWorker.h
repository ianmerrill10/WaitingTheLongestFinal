/**
 * @file NotificationWorker.h
 * @brief Background worker for processing notification queue
 *
 * PURPOSE:
 * Handles asynchronous notification processing including:
 * - Processing scheduled notifications when their time arrives
 * - Retrying failed notifications with exponential backoff
 * - Respecting quiet hours by queuing and releasing notifications
 * - Dispatching to appropriate channels based on user preferences
 *
 * USAGE:
 * auto& worker = NotificationWorker::getInstance();
 * worker.start();
 * // ... application runs ...
 * worker.stop();
 *
 * DEPENDENCIES:
 * - NotificationService (notification dispatch)
 * - ConnectionPool (queue database)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - Adjust processing intervals in constants
 * - Add new queue types as needed
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "notifications/Notification.h"

namespace wtl::notifications {

/**
 * @struct QueuedNotification
 * @brief Notification waiting in the queue
 */
struct QueuedNotification {
    std::string id;                          ///< Queue entry ID
    Notification notification;               ///< The notification
    int retry_count;                         ///< Number of retry attempts
    std::chrono::system_clock::time_point scheduled_time; ///< When to process
    std::chrono::system_clock::time_point created_at;     ///< When queued
    std::string status;                      ///< "pending", "processing", "failed", "completed"
    std::optional<std::string> error_message; ///< Last error if any

    /**
     * @brief Check if ready to process
     */
    bool isReadyToProcess() const {
        return std::chrono::system_clock::now() >= scheduled_time;
    }

    /**
     * @brief Calculate next retry time with exponential backoff
     */
    std::chrono::system_clock::time_point calculateNextRetry() const {
        // Exponential backoff: 1min, 2min, 4min, 8min, 16min max
        int delay_minutes = std::min(1 << retry_count, 16);
        return std::chrono::system_clock::now() +
               std::chrono::minutes(delay_minutes);
    }
};

/**
 * @struct WorkerStats
 * @brief Statistics for the notification worker
 */
struct WorkerStats {
    uint64_t total_processed = 0;
    uint64_t total_succeeded = 0;
    uint64_t total_failed = 0;
    uint64_t total_retried = 0;
    uint64_t total_expired = 0;
    uint64_t current_queue_size = 0;
    std::chrono::system_clock::time_point last_run;
    std::chrono::system_clock::time_point started_at;

    Json::Value toJson() const {
        Json::Value json;
        json["total_processed"] = static_cast<Json::UInt64>(total_processed);
        json["total_succeeded"] = static_cast<Json::UInt64>(total_succeeded);
        json["total_failed"] = static_cast<Json::UInt64>(total_failed);
        json["total_retried"] = static_cast<Json::UInt64>(total_retried);
        json["total_expired"] = static_cast<Json::UInt64>(total_expired);
        json["current_queue_size"] = static_cast<Json::UInt64>(current_queue_size);

        auto last_run_time = std::chrono::system_clock::to_time_t(last_run);
        json["last_run"] = static_cast<Json::Int64>(last_run_time);

        auto started_time = std::chrono::system_clock::to_time_t(started_at);
        json["started_at"] = static_cast<Json::Int64>(started_time);

        return json;
    }
};

/**
 * @class NotificationWorker
 * @brief Singleton background worker for notification processing
 *
 * Thread-safe singleton that runs a background thread to process
 * the notification queue. Handles scheduling, retries, and quiet hours.
 */
class NotificationWorker {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to NotificationWorker instance
     */
    static NotificationWorker& getInstance();

    // Prevent copying
    NotificationWorker(const NotificationWorker&) = delete;
    NotificationWorker& operator=(const NotificationWorker&) = delete;

    // =========================================================================
    // LIFECYCLE
    // =========================================================================

    /**
     * @brief Start the worker thread
     *
     * Starts processing the notification queue in a background thread.
     */
    void start();

    /**
     * @brief Stop the worker thread
     *
     * Gracefully stops the worker, completing any in-progress notifications.
     */
    void stop();

    /**
     * @brief Check if worker is running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Trigger immediate processing
     *
     * Wakes up the worker to process immediately instead of waiting
     * for the next interval.
     */
    void triggerProcessing();

    // =========================================================================
    // QUEUE MANAGEMENT
    // =========================================================================

    /**
     * @brief Add notification to queue
     *
     * @param notification The notification to queue
     * @param scheduled_time When to send (defaults to now)
     * @return std::string Queue entry ID
     */
    std::string enqueue(const Notification& notification,
                        std::chrono::system_clock::time_point scheduled_time =
                            std::chrono::system_clock::now());

    /**
     * @brief Remove notification from queue
     *
     * @param queue_id Queue entry ID
     * @return true if removed successfully
     */
    bool dequeue(const std::string& queue_id);

    /**
     * @brief Get queue size
     *
     * @return Number of notifications in queue
     */
    size_t getQueueSize() const;

    /**
     * @brief Get pending notifications for a user
     *
     * @param user_id User UUID
     * @return std::vector<QueuedNotification> Pending notifications
     */
    std::vector<QueuedNotification> getUserPendingNotifications(const std::string& user_id);

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Set processing interval
     *
     * @param seconds Seconds between processing runs
     */
    void setProcessingInterval(int seconds) { processing_interval_seconds_ = seconds; }

    /**
     * @brief Set maximum retries
     *
     * @param max_retries Maximum retry attempts before giving up
     */
    void setMaxRetries(int max_retries) { max_retries_ = max_retries; }

    /**
     * @brief Set batch size
     *
     * @param batch_size Maximum notifications to process per run
     */
    void setBatchSize(int batch_size) { batch_size_ = batch_size; }

    /**
     * @brief Set notification expiry time
     *
     * @param hours Hours after which notifications expire
     */
    void setExpiryHours(int hours) { expiry_hours_ = hours; }

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get worker statistics
     * @return WorkerStats Current statistics
     */
    WorkerStats getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    // =========================================================================
    // QUIET HOURS MANAGEMENT
    // =========================================================================

    /**
     * @brief Release quiet hours queue for a user
     *
     * Moves notifications held for quiet hours back to main queue.
     *
     * @param user_id User UUID
     */
    void releaseQuietHoursQueue(const std::string& user_id);

    /**
     * @brief Check all users for quiet hours end
     *
     * Called periodically to release notifications when quiet hours end.
     */
    void checkQuietHoursRelease();

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    NotificationWorker() = default;
    ~NotificationWorker();

    /**
     * @brief Main worker loop
     */
    void workerLoop();

    /**
     * @brief Process one batch of notifications
     */
    void processBatch();

    /**
     * @brief Fetch ready notifications from database
     */
    std::vector<QueuedNotification> fetchReadyNotifications(int limit);

    /**
     * @brief Process a single notification
     */
    bool processNotification(QueuedNotification& queued);

    /**
     * @brief Mark notification as completed
     */
    void markCompleted(const std::string& queue_id);

    /**
     * @brief Mark notification as failed
     */
    void markFailed(const std::string& queue_id, const std::string& error);

    /**
     * @brief Schedule retry
     */
    void scheduleRetry(QueuedNotification& queued);

    /**
     * @brief Mark notification as expired
     */
    void markExpired(const std::string& queue_id);

    /**
     * @brief Clean up old entries
     */
    void cleanupOldEntries();

    /**
     * @brief Generate queue entry ID
     */
    std::string generateQueueId() const;

    // Worker thread
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    std::mutex mutex_;
    std::condition_variable condition_;

    // Configuration
    int processing_interval_seconds_ = 30;  // Process every 30 seconds
    int max_retries_ = 5;                   // Max 5 retry attempts
    int batch_size_ = 100;                  // Process 100 at a time
    int expiry_hours_ = 24;                 // Expire after 24 hours

    // Statistics
    mutable std::mutex stats_mutex_;
    WorkerStats stats_;
};

} // namespace wtl::notifications
