/**
 * @file NotificationWorker.cc
 * @brief Implementation of NotificationWorker background processing
 * @see NotificationWorker.h for documentation
 */

#include "notifications/NotificationWorker.h"

// Standard library includes
#include <iomanip>
#include <random>
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "notifications/NotificationService.h"

namespace wtl::notifications {

// ============================================================================
// SINGLETON
// ============================================================================

NotificationWorker& NotificationWorker::getInstance() {
    static NotificationWorker instance;
    return instance;
}

NotificationWorker::~NotificationWorker() {
    stop();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void NotificationWorker::start() {
    if (running_) {
        return;
    }

    should_stop_ = false;
    running_ = true;
    stats_.started_at = std::chrono::system_clock::now();

    worker_thread_ = std::thread(&NotificationWorker::workerLoop, this);

    WTL_CAPTURE_INFO(
        wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
        "NotificationWorker started",
        {{"interval_seconds", std::to_string(processing_interval_seconds_)},
         {"batch_size", std::to_string(batch_size_)}}
    );
}

void NotificationWorker::stop() {
    if (!running_) {
        return;
    }

    should_stop_ = true;
    condition_.notify_all();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    running_ = false;

    WTL_CAPTURE_INFO(
        wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
        "NotificationWorker stopped",
        {{"total_processed", std::to_string(stats_.total_processed)}}
    );
}

void NotificationWorker::triggerProcessing() {
    condition_.notify_one();
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

std::string NotificationWorker::enqueue(const Notification& notification,
                                         std::chrono::system_clock::time_point scheduled_time) {
    std::string queue_id = generateQueueId();

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Serialize notification to JSON
        Json::StreamWriterBuilder writer;
        std::string notification_json = Json::writeString(writer, notification.toJson());

        auto scheduled_time_t = std::chrono::system_clock::to_time_t(scheduled_time);

        txn.exec_params(
            R"(
            INSERT INTO notification_queue (
                id, notification_id, user_id, notification_data,
                scheduled_time, created_at, status, retry_count
            ) VALUES (
                $1, $2, $3, $4::jsonb,
                to_timestamp($5), NOW(), 'pending', 0
            )
            )",
            queue_id,
            notification.id,
            notification.user_id,
            notification_json,
            scheduled_time_t
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Update queue size
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.current_queue_size++;
        }

        return queue_id;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to enqueue notification: " + std::string(e.what()),
            {{"notification_id", notification.id}}
        );
        throw;
    }
}

bool NotificationWorker::dequeue(const std::string& queue_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "DELETE FROM notification_queue WHERE id = $1",
            queue_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (result.affected_rows() > 0) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (stats_.current_queue_size > 0) {
                stats_.current_queue_size--;
            }
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to dequeue notification: " + std::string(e.what()),
            {{"queue_id", queue_id}}
        );
        return false;
    }
}

size_t NotificationWorker::getQueueSize() const {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params1(
            "SELECT COUNT(*) as count FROM notification_queue WHERE status = 'pending'"
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return result["count"].as<size_t>();

    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_.current_queue_size;
    }
}

std::vector<QueuedNotification> NotificationWorker::getUserPendingNotifications(const std::string& user_id) {
    std::vector<QueuedNotification> results;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(
            SELECT id, notification_id, notification_data, scheduled_time,
                   created_at, status, retry_count, error_message
            FROM notification_queue
            WHERE user_id = $1 AND status IN ('pending', 'processing')
            ORDER BY scheduled_time ASC
            )",
            user_id
        );

        for (const auto& row : result) {
            QueuedNotification queued;
            queued.id = row["id"].as<std::string>();
            queued.retry_count = row["retry_count"].as<int>();
            queued.status = row["status"].as<std::string>();

            // Parse notification from JSON
            std::string notification_json = row["notification_data"].as<std::string>();
            Json::Value json;
            Json::Reader reader;
            if (reader.parse(notification_json, json)) {
                queued.notification = Notification::fromJson(json);
            }

            // Parse timestamps
            std::string scheduled_str = row["scheduled_time"].as<std::string>();
            std::string created_str = row["created_at"].as<std::string>();

            // Simple timestamp parsing
            std::tm tm = {};
            std::istringstream ss(scheduled_str);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            queued.scheduled_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            ss.clear();
            ss.str(created_str);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            queued.created_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            if (!row["error_message"].is_null()) {
                queued.error_message = row["error_message"].as<std::string>();
            }

            results.push_back(queued);
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get user pending notifications: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }

    return results;
}

// ============================================================================
// STATISTICS
// ============================================================================

WorkerStats NotificationWorker::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    WorkerStats copy = stats_;
    copy.current_queue_size = getQueueSize();
    return copy;
}

void NotificationWorker::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_processed = 0;
    stats_.total_succeeded = 0;
    stats_.total_failed = 0;
    stats_.total_retried = 0;
    stats_.total_expired = 0;
}

// ============================================================================
// QUIET HOURS
// ============================================================================

void NotificationWorker::releaseQuietHoursQueue(const std::string& user_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Update notifications held for quiet hours to pending
        txn.exec_params(
            R"(
            UPDATE notification_queue
            SET status = 'pending', scheduled_time = NOW()
            WHERE user_id = $1 AND status = 'quiet_hours_hold'
            )",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to release quiet hours queue: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }
}

void NotificationWorker::checkQuietHoursRelease() {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find users whose quiet hours have ended
        // This is simplified - in production, use proper timezone handling
        auto result = txn.exec(
            R"(
            SELECT DISTINCT q.user_id
            FROM notification_queue q
            JOIN user_notification_preferences p ON q.user_id = p.user_id
            WHERE q.status = 'quiet_hours_hold'
            AND (
                p.quiet_hours_enabled = false
                OR (
                    EXTRACT(HOUR FROM NOW() AT TIME ZONE p.timezone) >= p.quiet_hours_end
                    AND EXTRACT(HOUR FROM NOW() AT TIME ZONE p.timezone) < p.quiet_hours_start
                )
            )
            )"
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Release queue for each user
        for (const auto& row : result) {
            releaseQuietHoursQueue(row["user_id"].as<std::string>());
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to check quiet hours release: " + std::string(e.what()),
            {}
        );
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void NotificationWorker::workerLoop() {
    while (!should_stop_) {
        try {
            processBatch();

            // Check quiet hours periodically
            checkQuietHoursRelease();

            // Clean up old entries periodically
            cleanupOldEntries();

        } catch (const std::exception& e) {
            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
                "Error in notification worker loop: " + std::string(e.what()),
                {}
            );
        }

        // Wait for next interval or wake signal
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait_for(
            lock,
            std::chrono::seconds(processing_interval_seconds_),
            [this] { return should_stop_.load(); }
        );
    }
}

void NotificationWorker::processBatch() {
    auto ready_notifications = fetchReadyNotifications(batch_size_);

    if (ready_notifications.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.last_run = std::chrono::system_clock::now();
    }

    for (auto& queued : ready_notifications) {
        if (should_stop_) {
            break;
        }

        // Check if expired
        auto age = std::chrono::system_clock::now() - queued.created_at;
        if (std::chrono::duration_cast<std::chrono::hours>(age).count() > expiry_hours_) {
            markExpired(queued.id);
            continue;
        }

        bool success = processNotification(queued);

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_processed++;
            if (success) {
                stats_.total_succeeded++;
            }
        }
    }
}

std::vector<QueuedNotification> NotificationWorker::fetchReadyNotifications(int limit) {
    std::vector<QueuedNotification> results;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Select and lock notifications that are ready to process
        auto result = txn.exec_params(
            R"(
            UPDATE notification_queue
            SET status = 'processing'
            WHERE id IN (
                SELECT id FROM notification_queue
                WHERE status = 'pending'
                AND scheduled_time <= NOW()
                ORDER BY
                    CASE
                        WHEN notification_data->>'type' = 'critical_alert' THEN 1
                        WHEN notification_data->>'type' = 'high_urgency' THEN 2
                        ELSE 3
                    END,
                    scheduled_time ASC
                LIMIT $1
                FOR UPDATE SKIP LOCKED
            )
            RETURNING id, notification_id, notification_data, scheduled_time,
                      created_at, status, retry_count, error_message
            )",
            limit
        );

        for (const auto& row : result) {
            QueuedNotification queued;
            queued.id = row["id"].as<std::string>();
            queued.retry_count = row["retry_count"].as<int>();
            queued.status = "processing";

            // Parse notification from JSON
            std::string notification_json = row["notification_data"].as<std::string>();
            Json::Value json;
            Json::Reader reader;
            if (reader.parse(notification_json, json)) {
                queued.notification = Notification::fromJson(json);
            }

            // Parse timestamps
            std::string scheduled_str = row["scheduled_time"].as<std::string>();
            std::string created_str = row["created_at"].as<std::string>();

            std::tm tm = {};
            std::istringstream ss(scheduled_str);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            queued.scheduled_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            ss.clear();
            ss.str(created_str);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            queued.created_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            if (!row["error_message"].is_null()) {
                queued.error_message = row["error_message"].as<std::string>();
            }

            results.push_back(queued);
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to fetch ready notifications: " + std::string(e.what()),
            {}
        );
    }

    return results;
}

bool NotificationWorker::processNotification(QueuedNotification& queued) {
    try {
        // Send notification via NotificationService
        auto result = NotificationService::getInstance().sendNotification(
            queued.notification,
            queued.notification.user_id
        );

        if (result.success) {
            markCompleted(queued.id);
            return true;
        }

        // Check if should retry
        if (queued.retry_count < max_retries_) {
            scheduleRetry(queued);
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_retried++;
            }
        } else {
            // Max retries exceeded
            std::string error = result.errors.empty() ? "Max retries exceeded" :
                                result.errors[0];
            markFailed(queued.id, error);
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_failed++;
            }
        }

        return false;

    } catch (const std::exception& e) {
        markFailed(queued.id, e.what());
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_failed++;
        }
        return false;
    }
}

void NotificationWorker::markCompleted(const std::string& queue_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            UPDATE notification_queue
            SET status = 'completed', processed_at = NOW()
            WHERE id = $1
            )",
            queue_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to mark notification completed: " + std::string(e.what()),
            {{"queue_id", queue_id}}
        );
    }
}

void NotificationWorker::markFailed(const std::string& queue_id, const std::string& error) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            UPDATE notification_queue
            SET status = 'failed', error_message = $2, processed_at = NOW()
            WHERE id = $1
            )",
            queue_id, error
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to mark notification failed: " + std::string(e.what()),
            {{"queue_id", queue_id}}
        );
    }
}

void NotificationWorker::scheduleRetry(QueuedNotification& queued) {
    try {
        auto next_retry = queued.calculateNextRetry();
        auto next_retry_t = std::chrono::system_clock::to_time_t(next_retry);

        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            UPDATE notification_queue
            SET status = 'pending',
                retry_count = retry_count + 1,
                scheduled_time = to_timestamp($2)
            WHERE id = $1
            )",
            queued.id, next_retry_t
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to schedule retry: " + std::string(e.what()),
            {{"queue_id", queued.id}}
        );
    }
}

void NotificationWorker::markExpired(const std::string& queue_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            UPDATE notification_queue
            SET status = 'expired', processed_at = NOW()
            WHERE id = $1
            )",
            queue_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_expired++;
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to mark notification expired: " + std::string(e.what()),
            {{"queue_id", queue_id}}
        );
    }
}

void NotificationWorker::cleanupOldEntries() {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Delete completed/failed/expired entries older than 7 days
        txn.exec(
            R"(
            DELETE FROM notification_queue
            WHERE status IN ('completed', 'failed', 'expired')
            AND processed_at < NOW() - INTERVAL '7 days'
            )"
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        // Non-critical, just log
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to cleanup old queue entries: " + std::string(e.what()),
            {}
        );
    }
}

std::string NotificationWorker::generateQueueId() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << dis(gen) << dis(gen);

    std::string id = ss.str();
    id.insert(8, "-");
    id.insert(13, "-");
    id.insert(18, "-");
    id.insert(23, "-");

    return id.substr(0, 36);
}

} // namespace wtl::notifications
