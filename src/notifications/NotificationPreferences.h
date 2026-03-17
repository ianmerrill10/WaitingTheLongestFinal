/**
 * @file NotificationPreferences.h
 * @brief User notification preferences for WaitingTheLongest push notification system
 *
 * PURPOSE:
 * Defines user preferences for receiving notifications. Users can control:
 * - Which notification types they receive
 * - Which channels (push, email, SMS) are enabled
 * - Geographic radius for alerts
 * - Quiet hours during which non-critical notifications are suppressed
 * - Minimum match score for match notifications
 *
 * USAGE:
 * UserNotificationPreferences prefs;
 * prefs.receive_critical_alerts = true;
 * prefs.critical_alert_radius_miles = 50;
 * prefs.enable_push = true;
 * prefs.quiet_hours_start = 22; // 10 PM
 * prefs.quiet_hours_end = 7;    // 7 AM
 *
 * DEPENDENCIES:
 * - NotificationType.h
 * - jsoncpp (Json::Value)
 * - pqxx (database row conversion)
 *
 * MODIFICATION GUIDE:
 * - Add new preferences to struct and update all conversion methods
 * - Update defaults in constructor
 * - Ensure database migration matches
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

// Project includes
#include "notifications/Notification.h"
#include "notifications/NotificationType.h"

namespace wtl::notifications {

/**
 * @struct UserNotificationPreferences
 * @brief User preferences for notification delivery
 *
 * Controls what notifications a user receives, through which channels,
 * and when they should be delivered (respecting quiet hours).
 */
struct UserNotificationPreferences {
    // =========================================================================
    // IDENTITY
    // =========================================================================

    std::string id;                          ///< UUID primary key
    std::string user_id;                     ///< User this preferences belong to

    // =========================================================================
    // CRITICAL ALERTS (Dogs < 24 hours)
    // =========================================================================

    bool receive_critical_alerts;            ///< Receive critical (< 24 hour) alerts
    double critical_alert_radius_miles;      ///< Radius for critical alerts (0 = everywhere)
    bool critical_alerts_any_dog;            ///< Alert for any dog, not just favorites/matches

    // =========================================================================
    // URGENCY ALERTS (Dogs < 72 hours)
    // =========================================================================

    bool receive_urgency_alerts;             ///< Receive high urgency alerts
    double urgency_alert_radius_miles;       ///< Radius for urgency alerts

    // =========================================================================
    // MATCH ALERTS
    // =========================================================================

    bool receive_match_alerts;               ///< Receive match notifications
    int min_match_score;                     ///< Minimum match % to notify (0-100)
    bool receive_perfect_match_only;         ///< Only notify on 90%+ matches

    // =========================================================================
    // FOSTER ALERTS
    // =========================================================================

    bool receive_foster_alerts;              ///< Receive foster-related alerts
    bool receive_foster_urgent_alerts;       ///< Receive urgent foster needed alerts
    bool receive_foster_followup;            ///< Receive foster check-in reminders

    // =========================================================================
    // DOG UPDATES
    // =========================================================================

    bool receive_dog_updates;                ///< Receive updates on favorited dogs
    bool receive_success_stories;            ///< Receive adoption success stories

    // =========================================================================
    // CONTENT NOTIFICATIONS
    // =========================================================================

    bool receive_blog_posts;                 ///< Receive new blog post notifications
    bool receive_tips;                       ///< Receive daily tips

    // =========================================================================
    // CHANNEL PREFERENCES
    // =========================================================================

    bool enable_push;                        ///< Enable push notifications
    bool enable_email;                       ///< Enable email notifications
    bool enable_sms;                         ///< Enable SMS notifications
    bool enable_websocket;                   ///< Enable real-time WebSocket alerts

    // =========================================================================
    // QUIET HOURS
    // =========================================================================

    bool quiet_hours_enabled;                ///< Enable quiet hours
    int quiet_hours_start;                   ///< Start hour (0-23), e.g., 22 for 10 PM
    int quiet_hours_end;                     ///< End hour (0-23), e.g., 7 for 7 AM
    std::string timezone;                    ///< User's timezone (e.g., "America/Chicago")

    // =========================================================================
    // FREQUENCY LIMITS
    // =========================================================================

    int max_notifications_per_day;           ///< Max non-critical notifications per day
    int max_emails_per_day;                  ///< Max email notifications per day
    int max_sms_per_week;                    ///< Max SMS notifications per week

    // =========================================================================
    // LOCATION PREFERENCES
    // =========================================================================

    std::optional<double> home_latitude;     ///< User's home latitude
    std::optional<double> home_longitude;    ///< User's home longitude
    std::optional<std::string> home_zip;     ///< User's home ZIP code
    std::vector<std::string> preferred_states; ///< States to receive alerts for

    // =========================================================================
    // TIMESTAMPS
    // =========================================================================

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    // =========================================================================
    // CONSTRUCTOR
    // =========================================================================

    /**
     * @brief Default constructor with sensible defaults
     */
    UserNotificationPreferences();

    // =========================================================================
    // CONVERSION METHODS
    // =========================================================================

    /**
     * @brief Convert preferences to JSON
     *
     * @return Json::Value JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create preferences from JSON
     *
     * @param json The JSON object
     * @return UserNotificationPreferences The constructed preferences
     */
    static UserNotificationPreferences fromJson(const Json::Value& json);

    /**
     * @brief Create preferences from database row
     *
     * @param row The pqxx database row
     * @return UserNotificationPreferences The constructed preferences
     */
    static UserNotificationPreferences fromDbRow(const pqxx::row& row);

    // =========================================================================
    // HELPER METHODS
    // =========================================================================

    /**
     * @brief Check if a notification type is enabled
     *
     * @param type The notification type
     * @return true if user wants to receive this type
     */
    bool isTypeEnabled(NotificationType type) const;

    /**
     * @brief Check if a channel is enabled
     *
     * @param channel The notification channel
     * @return true if channel is enabled
     */
    bool isChannelEnabled(NotificationChannel channel) const;

    /**
     * @brief Check if currently in quiet hours
     *
     * @return true if currently in quiet hours
     */
    bool isQuietHoursNow() const;

    /**
     * @brief Check if notification should be delivered now
     *
     * @param type The notification type
     * @return true if notification should be delivered
     */
    bool shouldDeliverNow(NotificationType type) const;

    /**
     * @brief Check if location is within user's alert radius
     *
     * @param latitude Location latitude
     * @param longitude Location longitude
     * @param type Notification type (determines which radius to use)
     * @return true if within radius
     */
    bool isWithinRadius(double latitude, double longitude, NotificationType type) const;

    /**
     * @brief Get best channel for notification type
     *
     * Returns the most appropriate enabled channel based on type and preferences.
     *
     * @param type The notification type
     * @return NotificationChannel The recommended channel
     */
    NotificationChannel getBestChannel(NotificationType type) const;

    /**
     * @brief Get all enabled channels for a notification type
     *
     * @param type The notification type
     * @return std::vector<NotificationChannel> All enabled channels
     */
    std::vector<NotificationChannel> getEnabledChannels(NotificationType type) const;

    /**
     * @brief Parse timestamp from database
     */
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);

private:
    /**
     * @brief Calculate distance between two points (Haversine formula)
     */
    static double calculateDistance(double lat1, double lon1, double lat2, double lon2);

    /**
     * @brief Get current hour in user's timezone
     */
    int getCurrentHourInTimezone() const;
};

/**
 * @struct DeviceToken
 * @brief Represents a registered device for push notifications
 */
struct DeviceToken {
    std::string id;                          ///< UUID primary key
    std::string user_id;                     ///< User who owns this device
    std::string token;                       ///< FCM/APNs token
    std::string platform;                    ///< "ios", "android", "web"
    std::string device_name;                 ///< Human-readable device name
    bool is_active;                          ///< Whether token is still valid
    std::chrono::system_clock::time_point registered_at;
    std::optional<std::chrono::system_clock::time_point> last_used_at;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static DeviceToken fromJson(const Json::Value& json);

    /**
     * @brief Create from database row
     */
    static DeviceToken fromDbRow(const pqxx::row& row);
};

} // namespace wtl::notifications
