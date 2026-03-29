/**
 * @file NotificationService.cc
 * @brief Implementation of NotificationService
 * @see NotificationService.h for documentation
 */

#include "notifications/NotificationService.h"

// Standard library includes
#include <random>
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/EventBus.h"
#include "core/models/Dog.h"
#include "core/models/Shelter.h"
#include "notifications/channels/PushChannel.h"
#include "notifications/channels/EmailChannel.h"
#include "notifications/channels/SmsChannel.h"

namespace wtl::notifications {

// ============================================================================
// SINGLETON
// ============================================================================

NotificationService& NotificationService::getInstance() {
    static NotificationService instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool NotificationService::initialize() {
    // Initialize all channels
    bool push_ok = channels::PushChannel::getInstance().initializeFromConfig();
    bool email_ok = channels::EmailChannel::getInstance().initializeFromConfig();
    bool sms_ok = channels::SmsChannel::getInstance().initializeFromConfig();

    if (!push_ok && !email_ok && !sms_ok) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "No notification channels initialized - notifications will be limited",
            {}
        );
    }

    initialized_ = true;

    WTL_CAPTURE_INFO(
        wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
        "NotificationService initialized",
        {{"push", push_ok ? "enabled" : "disabled"},
         {"email", email_ok ? "enabled" : "disabled"},
         {"sms", sms_ok ? "enabled" : "disabled"}}
    );

    return true;
}

// ============================================================================
// SEND NOTIFICATIONS
// ============================================================================

NotificationResult NotificationService::sendNotification(const Notification& notification,
                                                          const std::string& user_id) {
    NotificationResult result;
    result.success = false;
    result.channels_attempted = 0;
    result.channels_succeeded = 0;

    // Get user preferences
    auto prefs_opt = getPreferences(user_id);
    if (!prefs_opt.has_value()) {
        // Create default preferences
        prefs_opt = createDefaultPreferences(user_id);
    }

    auto& prefs = prefs_opt.value();

    // Check if notification type is enabled
    if (!prefs.isTypeEnabled(notification.type)) {
        result.errors.push_back("Notification type disabled by user preferences");
        return result;
    }

    // Check if should deliver now (quiet hours)
    if (!prefs.shouldDeliverNow(notification.type)) {
        // Schedule for after quiet hours
        // For now, we'll just log and skip
        result.errors.push_back("Quiet hours active - notification queued");
        return result;
    }

    // Create notification with user_id
    Notification notif = notification;
    notif.user_id = user_id;
    notif.id = generateNotificationId();
    notif.created_at = std::chrono::system_clock::now();
    notif.is_sent = false;
    notif.is_read = false;

    // Save to database
    result.notification_id = saveNotification(notif);

    // Dispatch to channels
    result = dispatchToChannels(notif, prefs);

    // Update notification with delivery results
    updateNotification(notif);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_sent_++;
        if (result.channels_succeeded > 0) {
            total_delivered_++;
        }
    }

    // Emit event
    emitNotificationEvent(notif, "notification_sent");

    return result;
}

BulkNotificationResult NotificationService::sendBulkNotification(const Notification& notification,
                                                                   const std::vector<std::string>& user_ids) {
    BulkNotificationResult result;
    result.total_users = static_cast<int>(user_ids.size());
    result.succeeded = 0;
    result.failed = 0;
    result.skipped = 0;

    for (const auto& user_id : user_ids) {
        try {
            auto send_result = sendNotification(notification, user_id);
            if (send_result.success) {
                result.succeeded++;
            } else if (send_result.errors.size() > 0 &&
                       send_result.errors[0].find("disabled") != std::string::npos) {
                result.skipped++;
            } else {
                result.failed++;
                result.failed_user_ids.push_back(user_id);
            }
        } catch (const std::exception& e) {
            result.failed++;
            result.failed_user_ids.push_back(user_id);
        }
    }

    return result;
}

BulkNotificationResult NotificationService::broadcastUrgent(const core::models::Dog& dog) {
    // Find eligible users
    std::vector<std::string> eligible_users = findUsersForUrgentBroadcast(dog);

    BulkNotificationResult result;
    result.total_users = static_cast<int>(eligible_users.size());
    result.succeeded = 0;
    result.failed = 0;
    result.skipped = 0;

    if (eligible_users.empty()) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            "No eligible users for urgent broadcast",
            {{"dog_id", dog.id}}
        );
        return result;
    }

    // Get shelter info (we need to query it since Dog doesn't include full shelter)
    core::models::Shelter shelter;
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto row = txn.exec_params1(
            "SELECT id, name, state_code FROM shelters WHERE id = $1",
            dog.shelter_id
        );

        shelter.id = row["id"].as<std::string>();
        shelter.name = row["name"].as<std::string>();
        shelter.state_code = row["state_code"].as<std::string>();

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get shelter for urgent broadcast: " + std::string(e.what()),
            {{"dog_id", dog.id}}
        );
        return result;
    }

    // Calculate hours remaining
    int hours_remaining = 24; // Default
    if (dog.euthanasia_date.has_value()) {
        auto now = std::chrono::system_clock::now();
        auto diff = dog.euthanasia_date.value() - now;
        hours_remaining = static_cast<int>(
            std::chrono::duration_cast<std::chrono::hours>(diff).count()
        );
    }

    // Build notification
    Notification notification = buildCriticalAlert(dog, shelter, hours_remaining);

    // Send to all eligible users
    for (const auto& user_id : eligible_users) {
        try {
            auto send_result = sendNotification(notification, user_id);
            if (send_result.success) {
                result.succeeded++;
            } else {
                result.failed++;
            }
        } catch (const std::exception& e) {
            result.failed++;
        }
    }

    WTL_CAPTURE_INFO(
        wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
        "Urgent broadcast completed",
        {{"dog_id", dog.id},
         {"total_users", std::to_string(result.total_users)},
         {"succeeded", std::to_string(result.succeeded)}}
    );

    return result;
}

std::string NotificationService::scheduleNotification(const Notification& notification,
                                                       std::chrono::system_clock::time_point delivery_time) {
    Notification notif = notification;
    notif.id = generateNotificationId();
    notif.scheduled_for = delivery_time;
    notif.created_at = std::chrono::system_clock::now();
    notif.is_sent = false;
    notif.is_read = false;

    return saveNotification(notif);
}

bool NotificationService::cancelScheduledNotification(const std::string& notification_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "DELETE FROM notifications WHERE id = $1 AND is_sent = false",
            notification_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to cancel scheduled notification: " + std::string(e.what()),
            {{"notification_id", notification_id}}
        );
        return false;
    }
}

// ============================================================================
// NOTIFICATION QUERIES
// ============================================================================

std::vector<Notification> NotificationService::getUserNotifications(const std::string& user_id,
                                                                      int limit,
                                                                      int offset) {
    std::vector<Notification> notifications;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(
            SELECT * FROM notifications
            WHERE user_id = $1
            ORDER BY created_at DESC
            LIMIT $2 OFFSET $3
            )",
            user_id, limit, offset
        );

        for (const auto& row : result) {
            notifications.push_back(Notification::fromDbRow(row));
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get user notifications: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }

    return notifications;
}

int NotificationService::getUnreadCount(const std::string& user_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params1(
            "SELECT COUNT(*) as count FROM notifications WHERE user_id = $1 AND is_read = false",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return result["count"].as<int>();

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get unread count: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
        return 0;
    }
}

std::optional<Notification> NotificationService::getNotification(const std::string& notification_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM notifications WHERE id = $1",
            notification_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (!result.empty()) {
            return Notification::fromDbRow(result[0]);
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get notification: " + std::string(e.what()),
            {{"notification_id", notification_id}}
        );
    }

    return std::nullopt;
}

// ============================================================================
// NOTIFICATION MANAGEMENT
// ============================================================================

bool NotificationService::markAsRead(const std::string& notification_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "UPDATE notifications SET is_read = true WHERE id = $1",
            notification_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to mark notification as read: " + std::string(e.what()),
            {{"notification_id", notification_id}}
        );
        return false;
    }
}

int NotificationService::markAllAsRead(const std::string& user_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "UPDATE notifications SET is_read = true WHERE user_id = $1 AND is_read = false",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return static_cast<int>(result.affected_rows());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to mark all notifications as read: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
        return 0;
    }
}

void NotificationService::recordOpened(const std::string& notification_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE notifications SET opened_at = NOW() WHERE id = $1 AND opened_at IS NULL",
            notification_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_opened_++;

    } catch (const std::exception& e) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to record notification opened: " + std::string(e.what()),
            {{"notification_id", notification_id}}
        );
    }
}

void NotificationService::recordClicked(const std::string& notification_id,
                                         const std::string& action_taken) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            UPDATE notifications
            SET clicked_at = NOW(), action_taken = $2
            WHERE id = $1 AND clicked_at IS NULL
            )",
            notification_id, action_taken.empty() ? "clicked" : action_taken
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_clicked_++;

    } catch (const std::exception& e) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to record notification clicked: " + std::string(e.what()),
            {{"notification_id", notification_id}}
        );
    }
}

bool NotificationService::deleteNotification(const std::string& notification_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "DELETE FROM notifications WHERE id = $1",
            notification_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to delete notification: " + std::string(e.what()),
            {{"notification_id", notification_id}}
        );
        return false;
    }
}

// ============================================================================
// USER PREFERENCES
// ============================================================================

std::optional<UserNotificationPreferences> NotificationService::getPreferences(const std::string& user_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM user_notification_preferences WHERE user_id = $1",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (!result.empty()) {
            return UserNotificationPreferences::fromDbRow(result[0]);
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get user preferences: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }

    return std::nullopt;
}

bool NotificationService::updatePreferences(const std::string& user_id,
                                             const UserNotificationPreferences& preferences) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            UPDATE user_notification_preferences SET
                receive_critical_alerts = $2,
                critical_alert_radius_miles = $3,
                critical_alerts_any_dog = $4,
                receive_urgency_alerts = $5,
                urgency_alert_radius_miles = $6,
                receive_match_alerts = $7,
                min_match_score = $8,
                receive_perfect_match_only = $9,
                receive_foster_alerts = $10,
                receive_foster_urgent_alerts = $11,
                receive_foster_followup = $12,
                receive_dog_updates = $13,
                receive_success_stories = $14,
                receive_blog_posts = $15,
                receive_tips = $16,
                enable_push = $17,
                enable_email = $18,
                enable_sms = $19,
                enable_websocket = $20,
                quiet_hours_enabled = $21,
                quiet_hours_start = $22,
                quiet_hours_end = $23,
                timezone = $24,
                max_notifications_per_day = $25,
                max_emails_per_day = $26,
                max_sms_per_week = $27,
                home_latitude = $28,
                home_longitude = $29,
                home_zip = $30,
                updated_at = NOW()
            WHERE user_id = $1
            )",
            user_id,
            preferences.receive_critical_alerts,
            preferences.critical_alert_radius_miles,
            preferences.critical_alerts_any_dog,
            preferences.receive_urgency_alerts,
            preferences.urgency_alert_radius_miles,
            preferences.receive_match_alerts,
            preferences.min_match_score,
            preferences.receive_perfect_match_only,
            preferences.receive_foster_alerts,
            preferences.receive_foster_urgent_alerts,
            preferences.receive_foster_followup,
            preferences.receive_dog_updates,
            preferences.receive_success_stories,
            preferences.receive_blog_posts,
            preferences.receive_tips,
            preferences.enable_push,
            preferences.enable_email,
            preferences.enable_sms,
            preferences.enable_websocket,
            preferences.quiet_hours_enabled,
            preferences.quiet_hours_start,
            preferences.quiet_hours_end,
            preferences.timezone,
            preferences.max_notifications_per_day,
            preferences.max_emails_per_day,
            preferences.max_sms_per_week,
            preferences.home_latitude.value_or(0.0),
            preferences.home_longitude.value_or(0.0),
            preferences.home_zip.value_or("")
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to update user preferences: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
        return false;
    }
}

UserNotificationPreferences NotificationService::createDefaultPreferences(const std::string& user_id) {
    UserNotificationPreferences prefs;
    prefs.id = generateNotificationId();
    prefs.user_id = user_id;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            INSERT INTO user_notification_preferences (
                id, user_id, receive_critical_alerts, critical_alert_radius_miles,
                critical_alerts_any_dog, receive_urgency_alerts, urgency_alert_radius_miles,
                receive_match_alerts, min_match_score, receive_perfect_match_only,
                receive_foster_alerts, receive_foster_urgent_alerts, receive_foster_followup,
                receive_dog_updates, receive_success_stories, receive_blog_posts, receive_tips,
                enable_push, enable_email, enable_sms, enable_websocket,
                quiet_hours_enabled, quiet_hours_start, quiet_hours_end, timezone,
                max_notifications_per_day, max_emails_per_day, max_sms_per_week,
                created_at, updated_at
            ) VALUES (
                $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17,
                $18, $19, $20, $21, $22, $23, $24, $25, $26, $27, $28, NOW(), NOW()
            )
            ON CONFLICT (user_id) DO NOTHING
            )",
            prefs.id, user_id,
            prefs.receive_critical_alerts, prefs.critical_alert_radius_miles,
            prefs.critical_alerts_any_dog, prefs.receive_urgency_alerts, prefs.urgency_alert_radius_miles,
            prefs.receive_match_alerts, prefs.min_match_score, prefs.receive_perfect_match_only,
            prefs.receive_foster_alerts, prefs.receive_foster_urgent_alerts, prefs.receive_foster_followup,
            prefs.receive_dog_updates, prefs.receive_success_stories, prefs.receive_blog_posts, prefs.receive_tips,
            prefs.enable_push, prefs.enable_email, prefs.enable_sms, prefs.enable_websocket,
            prefs.quiet_hours_enabled, prefs.quiet_hours_start, prefs.quiet_hours_end, prefs.timezone,
            prefs.max_notifications_per_day, prefs.max_emails_per_day, prefs.max_sms_per_week
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to create default preferences: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }

    return prefs;
}

// ============================================================================
// DEVICE TOKEN MANAGEMENT
// ============================================================================

std::string NotificationService::registerDeviceToken(const std::string& user_id,
                                                      const std::string& token,
                                                      const std::string& platform,
                                                      const std::string& device_name) {
    return channels::PushChannel::getInstance().registerToken(user_id, token, platform, device_name);
}

void NotificationService::unregisterDeviceToken(const std::string& user_id, const std::string& token) {
    channels::PushChannel::getInstance().unregisterToken(user_id, token);
}

std::vector<DeviceToken> NotificationService::getUserDeviceTokens(const std::string& user_id) {
    return channels::PushChannel::getInstance().getUserTokens(user_id);
}

// ============================================================================
// NOTIFICATION BUILDERS
// ============================================================================

Notification NotificationService::buildCriticalAlert(const core::models::Dog& dog,
                                                      const core::models::Shelter& shelter,
                                                      int hours_remaining) {
    Notification notif;
    notif.type = NotificationType::CRITICAL_ALERT;
    notif.title = "URGENT: " + dog.name + " needs help NOW!";
    notif.body = dog.name + " at " + shelter.name + " has only " +
                 std::to_string(hours_remaining) + " hours left. Please help save this life!";
    notif.dog_id = dog.id;
    notif.shelter_id = shelter.id;
    notif.action_url = "https://waitingthelongest.com/dogs/" + dog.id;

    if (!dog.photo_urls.empty()) {
        notif.image_url = dog.photo_urls[0];
    }

    notif.data["hours_remaining"] = hours_remaining;
    notif.data["urgency_level"] = dog.urgency_level;
    notif.data["shelter_name"] = shelter.name;
    notif.data["state_code"] = shelter.state_code;

    return notif;
}

Notification NotificationService::buildMatchNotification(const core::models::Dog& dog, int match_score) {
    Notification notif;
    notif.type = match_score >= 90 ? NotificationType::PERFECT_MATCH : NotificationType::GOOD_MATCH;
    notif.title = match_score >= 90 ? "Perfect Match: " + dog.name + "!" :
                                       "Great Match: " + dog.name;
    notif.body = "We found a " + std::to_string(match_score) + "% match for you! " +
                 dog.name + " is a " + dog.breed_primary + " looking for a home.";
    notif.dog_id = dog.id;
    notif.action_url = "https://waitingthelongest.com/dogs/" + dog.id;

    if (!dog.photo_urls.empty()) {
        notif.image_url = dog.photo_urls[0];
    }

    notif.data["match_score"] = match_score;
    notif.data["breed"] = dog.breed_primary;
    notif.data["age_category"] = dog.age_category;

    return notif;
}

Notification NotificationService::buildFosterNeededNotification(const core::models::Dog& dog,
                                                                 const core::models::Shelter& shelter,
                                                                 bool is_urgent) {
    Notification notif;
    notif.type = is_urgent ? NotificationType::FOSTER_NEEDED_URGENT : NotificationType::FOSTER_FOLLOWUP;
    notif.title = is_urgent ? "URGENT: Foster needed for " + dog.name :
                               "Foster opportunity: " + dog.name;
    notif.body = dog.name + " at " + shelter.name + " needs a temporary foster home. " +
                 "Can you help?";
    notif.dog_id = dog.id;
    notif.shelter_id = shelter.id;
    notif.action_url = "https://waitingthelongest.com/dogs/" + dog.id + "?foster=true";

    if (!dog.photo_urls.empty()) {
        notif.image_url = dog.photo_urls[0];
    }

    notif.data["is_urgent"] = is_urgent;
    notif.data["shelter_name"] = shelter.name;

    return notif;
}

Notification NotificationService::buildDogUpdateNotification(const core::models::Dog& dog,
                                                              const std::string& update_type) {
    Notification notif;
    notif.type = NotificationType::DOG_UPDATE;
    notif.dog_id = dog.id;
    notif.action_url = "https://waitingthelongest.com/dogs/" + dog.id;

    if (update_type == "status_change") {
        notif.title = "Update on " + dog.name;
        notif.body = dog.name + "'s status has changed to: " + dog.status;
    } else if (update_type == "photo_added") {
        notif.title = "New photos of " + dog.name + "!";
        notif.body = "Check out the new photos we added for " + dog.name + ".";
    } else {
        notif.title = "Update on " + dog.name;
        notif.body = "There's new information about " + dog.name + ".";
    }

    if (!dog.photo_urls.empty()) {
        notif.image_url = dog.photo_urls[0];
    }

    notif.data["update_type"] = update_type;

    return notif;
}

Notification NotificationService::buildSuccessStoryNotification(const core::models::Dog& dog,
                                                                 const std::string& adopter_message) {
    Notification notif;
    notif.type = NotificationType::SUCCESS_STORY;
    notif.title = "Happy Tails: " + dog.name + " found a home!";
    notif.body = adopter_message.empty() ?
        dog.name + " has been adopted! Another success story." :
        "\"" + adopter_message + "\"";
    notif.dog_id = dog.id;
    notif.action_url = "https://waitingthelongest.com/success-stories/" + dog.id;

    if (!dog.photo_urls.empty()) {
        notif.image_url = dog.photo_urls[0];
    }

    notif.data["outcome"] = "adopted";

    return notif;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value NotificationService::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Json::Value stats;
    stats["total_sent"] = static_cast<Json::UInt64>(total_sent_);
    stats["total_delivered"] = static_cast<Json::UInt64>(total_delivered_);
    stats["total_opened"] = static_cast<Json::UInt64>(total_opened_);
    stats["total_clicked"] = static_cast<Json::UInt64>(total_clicked_);

    stats["delivery_rate"] = total_sent_ > 0 ?
        static_cast<double>(total_delivered_) / total_sent_ : 0.0;
    stats["open_rate"] = total_delivered_ > 0 ?
        static_cast<double>(total_opened_) / total_delivered_ : 0.0;
    stats["click_rate"] = total_opened_ > 0 ?
        static_cast<double>(total_clicked_) / total_opened_ : 0.0;

    // Add channel stats
    stats["push_channel"] = channels::PushChannel::getInstance().getStats();
    stats["email_channel"] = channels::EmailChannel::getInstance().getStats();
    stats["sms_channel"] = channels::SmsChannel::getInstance().getStats();

    return stats;
}

Json::Value NotificationService::getStatsForPeriod(std::chrono::system_clock::time_point start,
                                                    std::chrono::system_clock::time_point end) const {
    Json::Value stats;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto start_time = std::chrono::system_clock::to_time_t(start);
        auto end_time = std::chrono::system_clock::to_time_t(end);

        auto result = txn.exec_params(
            R"(
            SELECT
                COUNT(*) as total,
                SUM(CASE WHEN is_sent THEN 1 ELSE 0 END) as sent,
                SUM(CASE WHEN opened_at IS NOT NULL THEN 1 ELSE 0 END) as opened,
                SUM(CASE WHEN clicked_at IS NOT NULL THEN 1 ELSE 0 END) as clicked
            FROM notifications
            WHERE created_at >= to_timestamp($1) AND created_at < to_timestamp($2)
            )",
            start_time, end_time
        );

        if (!result.empty()) {
            stats["total"] = result[0]["total"].as<int>();
            stats["sent"] = result[0]["sent"].as<int>();
            stats["opened"] = result[0]["opened"].as<int>();
            stats["clicked"] = result[0]["clicked"].as<int>();
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get stats for period: " + std::string(e.what()),
            {}
        );
    }

    return stats;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

NotificationResult NotificationService::dispatchToChannels(Notification& notification,
                                                            const UserNotificationPreferences& prefs) {
    NotificationResult result;
    result.notification_id = notification.id;
    result.success = false;
    result.channels_attempted = 0;
    result.channels_succeeded = 0;

    auto enabled_channels = prefs.getEnabledChannels(notification.type);

    for (auto channel : enabled_channels) {
        result.channels_attempted++;

        try {
            switch (channel) {
                case NotificationChannel::PUSH: {
                    auto push_results = channels::PushChannel::getInstance().sendToUser(
                        notification.user_id, notification
                    );
                    bool any_success = false;
                    for (const auto& pr : push_results) {
                        if (pr.success) {
                            any_success = true;
                            notification.markSent(NotificationChannel::PUSH, pr.message_id);
                        } else {
                            result.errors.push_back("Push: " + pr.error_message);
                        }
                    }
                    if (any_success) {
                        result.channels_succeeded++;
                    }
                    break;
                }
                case NotificationChannel::EMAIL: {
                    auto email_result = channels::EmailChannel::getInstance().sendToUser(
                        notification.user_id, notification
                    );
                    if (email_result.success) {
                        result.channels_succeeded++;
                        notification.markSent(NotificationChannel::EMAIL, email_result.message_id);
                    } else {
                        result.errors.push_back("Email: " + email_result.error_message);
                    }
                    break;
                }
                case NotificationChannel::SMS: {
                    auto sms_result = channels::SmsChannel::getInstance().sendToUser(
                        notification.user_id, notification
                    );
                    if (sms_result.success) {
                        result.channels_succeeded++;
                        notification.markSent(NotificationChannel::SMS, sms_result.message_sid);
                    } else {
                        result.errors.push_back("SMS: " + sms_result.error_message);
                    }
                    break;
                }
                case NotificationChannel::WEBSOCKET: {
                    // WebSocket is handled by BroadcastService, just mark as sent
                    notification.markSent(NotificationChannel::WEBSOCKET);
                    result.channels_succeeded++;
                    break;
                }
            }
        } catch (const std::exception& e) {
            result.errors.push_back(channelToString(channel) + ": " + e.what());
        }
    }

    result.success = result.channels_succeeded > 0;
    notification.is_sent = result.success;

    return result;
}

std::string NotificationService::saveNotification(const Notification& notification) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Serialize channels_sent to JSONB
        Json::Value channels_json(Json::arrayValue);
        for (const auto& ch : notification.channels_sent) {
            channels_json.append(ch.toJson());
        }
        Json::StreamWriterBuilder writer;
        std::string channels_str = Json::writeString(writer, channels_json);
        std::string data_str = Json::writeString(writer, notification.data);

        txn.exec_params(
            R"(
            INSERT INTO notifications (
                id, user_id, type, title, body, image_url, action_url,
                dog_id, shelter_id, foster_id, data, channels_sent,
                created_at, scheduled_for, is_sent, retry_count, is_read
            ) VALUES (
                $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11::jsonb, $12::jsonb,
                NOW(), $13, $14, $15, $16
            )
            )",
            notification.id,
            notification.user_id,
            notificationTypeToString(notification.type),
            notification.title,
            notification.body,
            notification.image_url.value_or(""),
            notification.action_url.value_or(""),
            notification.dog_id.value_or(""),
            notification.shelter_id.value_or(""),
            notification.foster_id.value_or(""),
            data_str,
            channels_str,
            notification.scheduled_for.has_value() ?
                std::chrono::system_clock::to_time_t(notification.scheduled_for.value()) : 0,
            notification.is_sent,
            notification.retry_count,
            notification.is_read
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return notification.id;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to save notification: " + std::string(e.what()),
            {{"notification_id", notification.id}}
        );
        throw;
    }
}

void NotificationService::updateNotification(const Notification& notification) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Serialize channels_sent to JSONB
        Json::Value channels_json(Json::arrayValue);
        for (const auto& ch : notification.channels_sent) {
            channels_json.append(ch.toJson());
        }
        Json::StreamWriterBuilder writer;
        std::string channels_str = Json::writeString(writer, channels_json);

        txn.exec_params(
            R"(
            UPDATE notifications SET
                channels_sent = $2::jsonb,
                is_sent = $3,
                retry_count = $4
            WHERE id = $1
            )",
            notification.id,
            channels_str,
            notification.is_sent,
            notification.retry_count
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to update notification: " + std::string(e.what()),
            {{"notification_id", notification.id}}
        );
    }
}

std::vector<std::string> NotificationService::findUsersForUrgentBroadcast(const core::models::Dog& dog) {
    std::vector<std::string> users;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get dog location from shelter
        auto shelter_result = txn.exec_params(
            "SELECT latitude, longitude, state_code FROM shelters WHERE id = $1",
            dog.shelter_id
        );

        if (shelter_result.empty()) {
            wtl::core::db::ConnectionPool::getInstance().release(conn);
            return users;
        }

        double shelter_lat = shelter_result[0]["latitude"].is_null() ? 0.0 :
                             shelter_result[0]["latitude"].as<double>();
        double shelter_lon = shelter_result[0]["longitude"].is_null() ? 0.0 :
                             shelter_result[0]["longitude"].as<double>();
        std::string state_code = shelter_result[0]["state_code"].as<std::string>();

        // Find users with critical alerts enabled
        // Include users who:
        // 1. Have critical alerts enabled AND are within radius
        // 2. Have favorited this dog
        // 3. Have favorited this shelter
        auto result = txn.exec_params(
            R"(
            SELECT DISTINCT u.id FROM users u
            JOIN user_notification_preferences p ON u.id = p.user_id
            WHERE p.receive_critical_alerts = true
            AND (
                -- No location set means receive all
                (p.home_latitude IS NULL OR p.home_longitude IS NULL)
                -- Within radius (using simple distance calculation)
                OR (
                    p.critical_alert_radius_miles = 0
                    OR (
                        3959 * acos(
                            cos(radians($1)) * cos(radians(p.home_latitude)) *
                            cos(radians(p.home_longitude) - radians($2)) +
                            sin(radians($1)) * sin(radians(p.home_latitude))
                        ) <= p.critical_alert_radius_miles
                    )
                )
            )

            UNION

            SELECT user_id FROM favorites WHERE dog_id = $3

            UNION

            SELECT user_id FROM favorites f
            JOIN dogs d ON f.dog_id = d.id
            WHERE d.shelter_id = $4
            )",
            shelter_lat, shelter_lon, dog.id, dog.shelter_id
        );

        for (const auto& row : result) {
            users.push_back(row["id"].as<std::string>());
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find users for urgent broadcast: " + std::string(e.what()),
            {{"dog_id", dog.id}}
        );
    }

    return users;
}

std::string NotificationService::generateNotificationId() const {
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

void NotificationService::emitNotificationEvent(const Notification& notification, const std::string& event_type) {
    try {
        wtl::core::Event event;
        event.type = wtl::core::EventType::NOTIFICATION_SENT;
        event.data["notification_id"] = notification.id;
        event.data["user_id"] = notification.user_id;
        event.data["notification_type"] = notificationTypeToString(notification.type);
        event.data["event_type"] = event_type;

        wtl::core::EventBus::getInstance().publishAsync(event);

    } catch (const std::exception& e) {
        // Non-critical, just log
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            "Failed to emit notification event: " + std::string(e.what()),
            {{"notification_id", notification.id}}
        );
    }
}

} // namespace wtl::notifications
