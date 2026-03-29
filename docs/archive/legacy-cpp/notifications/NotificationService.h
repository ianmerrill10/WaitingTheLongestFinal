/**
 * @file NotificationService.h
 * @brief Main notification service coordinating all notification channels
 *
 * PURPOSE:
 * Central service for sending notifications to users. Coordinates between
 * push, email, SMS, and WebSocket channels based on user preferences.
 * Handles notification lifecycle: creation, scheduling, delivery, tracking.
 *
 * USAGE:
 * auto& service = NotificationService::getInstance();
 * service.sendNotification(notification, user_id);
 * service.broadcastUrgent(dog);
 * auto notifs = service.getUserNotifications(user_id, 20, 0);
 *
 * DEPENDENCIES:
 * - PushChannel, EmailChannel, SmsChannel (delivery)
 * - NotificationPreferences (user settings)
 * - ConnectionPool (database)
 * - EventBus (event publishing)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - To add new channels, extend dispatchToChannels()
 * - For new notification types, update buildNotificationContent()
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "notifications/Notification.h"
#include "notifications/NotificationPreferences.h"

// Forward declarations
namespace wtl::core::models {
    struct Dog;
    struct Shelter;
    struct User;
}

namespace wtl::notifications {

/**
 * @struct NotificationResult
 * @brief Result of a notification send operation
 */
struct NotificationResult {
    bool success;
    std::string notification_id;
    int channels_attempted;
    int channels_succeeded;
    std::vector<std::string> errors;

    Json::Value toJson() const {
        Json::Value json;
        json["success"] = success;
        json["notification_id"] = notification_id;
        json["channels_attempted"] = channels_attempted;
        json["channels_succeeded"] = channels_succeeded;

        Json::Value errors_json(Json::arrayValue);
        for (const auto& error : errors) {
            errors_json.append(error);
        }
        json["errors"] = errors_json;

        return json;
    }
};

/**
 * @struct BulkNotificationResult
 * @brief Result of bulk notification operation
 */
struct BulkNotificationResult {
    int total_users;
    int succeeded;
    int failed;
    int skipped;           ///< Users who have notifications disabled
    std::vector<std::string> failed_user_ids;

    Json::Value toJson() const {
        Json::Value json;
        json["total_users"] = total_users;
        json["succeeded"] = succeeded;
        json["failed"] = failed;
        json["skipped"] = skipped;
        return json;
    }
};

/**
 * @class NotificationService
 * @brief Singleton service for managing all notifications
 *
 * Thread-safe singleton that coordinates notification delivery across
 * all channels while respecting user preferences and rate limits.
 */
class NotificationService {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to NotificationService instance
     */
    static NotificationService& getInstance();

    // Prevent copying
    NotificationService(const NotificationService&) = delete;
    NotificationService& operator=(const NotificationService&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the notification service
     *
     * Initializes all notification channels from config.
     *
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized_; }

    // =========================================================================
    // SEND NOTIFICATIONS
    // =========================================================================

    /**
     * @brief Send a notification to a user
     *
     * Delivers notification via appropriate channels based on user preferences.
     *
     * @param notification The notification to send
     * @param user_id Target user UUID
     * @return NotificationResult Result of the operation
     */
    NotificationResult sendNotification(const Notification& notification, const std::string& user_id);

    /**
     * @brief Send notification to multiple users
     *
     * @param notification The notification to send
     * @param user_ids Vector of user UUIDs
     * @return BulkNotificationResult Results for all users
     */
    BulkNotificationResult sendBulkNotification(const Notification& notification,
                                                 const std::vector<std::string>& user_ids);

    /**
     * @brief Broadcast urgent alert for a dog
     *
     * Sends critical/urgent notification to all users who:
     * - Have critical alerts enabled
     * - Are within the dog's geographic area
     * - Have favorited the dog or shelter
     *
     * @param dog The dog needing urgent help
     * @return BulkNotificationResult Results
     */
    BulkNotificationResult broadcastUrgent(const core::models::Dog& dog);

    /**
     * @brief Schedule a notification for later delivery
     *
     * @param notification The notification to schedule
     * @param delivery_time When to send
     * @return std::string Notification ID
     */
    std::string scheduleNotification(const Notification& notification,
                                      std::chrono::system_clock::time_point delivery_time);

    /**
     * @brief Cancel a scheduled notification
     *
     * @param notification_id Notification UUID
     * @return true if cancelled successfully
     */
    bool cancelScheduledNotification(const std::string& notification_id);

    // =========================================================================
    // NOTIFICATION QUERIES
    // =========================================================================

    /**
     * @brief Get notifications for a user
     *
     * @param user_id User UUID
     * @param limit Maximum number to return
     * @param offset Offset for pagination
     * @return std::vector<Notification> User's notifications
     */
    std::vector<Notification> getUserNotifications(const std::string& user_id,
                                                    int limit = 20,
                                                    int offset = 0);

    /**
     * @brief Get unread notification count for a user
     *
     * @param user_id User UUID
     * @return int Number of unread notifications
     */
    int getUnreadCount(const std::string& user_id);

    /**
     * @brief Get a specific notification
     *
     * @param notification_id Notification UUID
     * @return std::optional<Notification> The notification if found
     */
    std::optional<Notification> getNotification(const std::string& notification_id);

    // =========================================================================
    // NOTIFICATION MANAGEMENT
    // =========================================================================

    /**
     * @brief Mark a notification as read
     *
     * @param notification_id Notification UUID
     * @return true if marked successfully
     */
    bool markAsRead(const std::string& notification_id);

    /**
     * @brief Mark all notifications as read for a user
     *
     * @param user_id User UUID
     * @return int Number of notifications marked as read
     */
    int markAllAsRead(const std::string& user_id);

    /**
     * @brief Record notification opened
     *
     * @param notification_id Notification UUID
     */
    void recordOpened(const std::string& notification_id);

    /**
     * @brief Record notification clicked
     *
     * @param notification_id Notification UUID
     * @param action_taken Optional action identifier
     */
    void recordClicked(const std::string& notification_id,
                       const std::string& action_taken = "");

    /**
     * @brief Delete a notification
     *
     * @param notification_id Notification UUID
     * @return true if deleted successfully
     */
    bool deleteNotification(const std::string& notification_id);

    // =========================================================================
    // USER PREFERENCES
    // =========================================================================

    /**
     * @brief Get user notification preferences
     *
     * @param user_id User UUID
     * @return std::optional<UserNotificationPreferences> Preferences if found
     */
    std::optional<UserNotificationPreferences> getPreferences(const std::string& user_id);

    /**
     * @brief Update user notification preferences
     *
     * @param user_id User UUID
     * @param preferences New preferences
     * @return true if updated successfully
     */
    bool updatePreferences(const std::string& user_id, const UserNotificationPreferences& preferences);

    /**
     * @brief Create default preferences for a new user
     *
     * @param user_id User UUID
     * @return UserNotificationPreferences Created preferences
     */
    UserNotificationPreferences createDefaultPreferences(const std::string& user_id);

    // =========================================================================
    // DEVICE TOKEN MANAGEMENT
    // =========================================================================

    /**
     * @brief Register a device token
     *
     * @param user_id User UUID
     * @param token FCM/APNs token
     * @param platform "ios", "android", or "web"
     * @param device_name Human-readable name
     * @return std::string Token registration ID
     */
    std::string registerDeviceToken(const std::string& user_id,
                                    const std::string& token,
                                    const std::string& platform,
                                    const std::string& device_name = "");

    /**
     * @brief Unregister a device token
     *
     * @param user_id User UUID
     * @param token Token to unregister
     */
    void unregisterDeviceToken(const std::string& user_id, const std::string& token);

    /**
     * @brief Get all device tokens for a user
     *
     * @param user_id User UUID
     * @return std::vector<DeviceToken> User's registered devices
     */
    std::vector<DeviceToken> getUserDeviceTokens(const std::string& user_id);

    // =========================================================================
    // NOTIFICATION BUILDERS
    // =========================================================================

    /**
     * @brief Build notification for critical dog alert
     *
     * @param dog The dog in critical condition
     * @param shelter The shelter
     * @param hours_remaining Hours until euthanasia
     * @return Notification The built notification
     */
    Notification buildCriticalAlert(const core::models::Dog& dog,
                                    const core::models::Shelter& shelter,
                                    int hours_remaining);

    /**
     * @brief Build notification for match found
     *
     * @param dog The matched dog
     * @param match_score Match percentage (0-100)
     * @return Notification The built notification
     */
    Notification buildMatchNotification(const core::models::Dog& dog, int match_score);

    /**
     * @brief Build notification for foster needed
     *
     * @param dog The dog needing foster
     * @param shelter The shelter
     * @param is_urgent Whether this is an urgent request
     * @return Notification The built notification
     */
    Notification buildFosterNeededNotification(const core::models::Dog& dog,
                                               const core::models::Shelter& shelter,
                                               bool is_urgent);

    /**
     * @brief Build notification for dog update
     *
     * @param dog The updated dog
     * @param update_type Type of update (status_change, photo_added, etc.)
     * @return Notification The built notification
     */
    Notification buildDogUpdateNotification(const core::models::Dog& dog,
                                            const std::string& update_type);

    /**
     * @brief Build success story notification
     *
     * @param dog The adopted dog
     * @param adopter_message Optional message from adopter
     * @return Notification The built notification
     */
    Notification buildSuccessStoryNotification(const core::models::Dog& dog,
                                               const std::string& adopter_message = "");

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get notification statistics
     * @return Json::Value Statistics JSON
     */
    Json::Value getStats() const;

    /**
     * @brief Get statistics for a specific time period
     *
     * @param start Start of period
     * @param end End of period
     * @return Json::Value Statistics JSON
     */
    Json::Value getStatsForPeriod(std::chrono::system_clock::time_point start,
                                   std::chrono::system_clock::time_point end) const;

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    NotificationService() = default;
    ~NotificationService() = default;

    /**
     * @brief Dispatch notification to appropriate channels
     */
    NotificationResult dispatchToChannels(Notification& notification,
                                           const UserNotificationPreferences& prefs);

    /**
     * @brief Save notification to database
     */
    std::string saveNotification(const Notification& notification);

    /**
     * @brief Update notification in database
     */
    void updateNotification(const Notification& notification);

    /**
     * @brief Find users eligible for urgent broadcast
     */
    std::vector<std::string> findUsersForUrgentBroadcast(const core::models::Dog& dog);

    /**
     * @brief Generate unique notification ID
     */
    std::string generateNotificationId() const;

    /**
     * @brief Emit notification event
     */
    void emitNotificationEvent(const Notification& notification, const std::string& event_type);

    // State
    bool initialized_ = false;

    // Statistics
    mutable std::mutex stats_mutex_;
    uint64_t total_sent_ = 0;
    uint64_t total_delivered_ = 0;
    uint64_t total_opened_ = 0;
    uint64_t total_clicked_ = 0;
};

} // namespace wtl::notifications
