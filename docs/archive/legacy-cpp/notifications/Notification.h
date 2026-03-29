/**
 * @file Notification.h
 * @brief Notification data model for WaitingTheLongest push notification system
 *
 * PURPOSE:
 * Defines the Notification structure that represents a notification sent to a user.
 * Tracks all aspects of the notification lifecycle including creation, delivery
 * across multiple channels, and user engagement (opens, clicks, actions).
 *
 * USAGE:
 * Notification notif;
 * notif.type = NotificationType::CRITICAL_ALERT;
 * notif.title = "URGENT: Dog Needs Help";
 * notif.body = "Max has only 12 hours left...";
 * auto json = notif.toJson();
 *
 * DEPENDENCIES:
 * - NotificationType.h
 * - jsoncpp (Json::Value)
 * - pqxx (database row conversion)
 * - chrono (timestamps)
 *
 * MODIFICATION GUIDE:
 * - Add new fields to struct and update all conversion methods
 * - Update both toJson() and fromJson() when adding fields
 * - Ensure database columns match in fromDbRow()
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
#include "notifications/NotificationType.h"

namespace wtl::notifications {

/**
 * @enum NotificationChannel
 * @brief Channels through which notifications can be sent
 */
enum class NotificationChannel {
    PUSH,   ///< Push notification (FCM, APNs)
    EMAIL,  ///< Email (SendGrid)
    SMS,    ///< SMS text message (Twilio)
    WEBSOCKET ///< Real-time WebSocket
};

/**
 * @brief Convert NotificationChannel to string
 */
inline std::string channelToString(NotificationChannel channel) {
    switch (channel) {
        case NotificationChannel::PUSH:
            return "push";
        case NotificationChannel::EMAIL:
            return "email";
        case NotificationChannel::SMS:
            return "sms";
        case NotificationChannel::WEBSOCKET:
            return "websocket";
        default:
            return "unknown";
    }
}

/**
 * @brief Parse string to NotificationChannel
 */
inline NotificationChannel stringToChannel(const std::string& str) {
    if (str == "push") return NotificationChannel::PUSH;
    if (str == "email") return NotificationChannel::EMAIL;
    if (str == "sms") return NotificationChannel::SMS;
    if (str == "websocket") return NotificationChannel::WEBSOCKET;
    return NotificationChannel::PUSH;
}

/**
 * @struct ChannelDeliveryStatus
 * @brief Track delivery status for each channel
 */
struct ChannelDeliveryStatus {
    NotificationChannel channel;
    bool sent;
    bool delivered;
    std::optional<std::chrono::system_clock::time_point> sent_at;
    std::optional<std::chrono::system_clock::time_point> delivered_at;
    std::optional<std::string> error_message;
    std::optional<std::string> external_id; ///< ID from provider (FCM message ID, etc.)

    Json::Value toJson() const {
        Json::Value json;
        json["channel"] = channelToString(channel);
        json["sent"] = sent;
        json["delivered"] = delivered;

        if (sent_at.has_value()) {
            auto time_t_val = std::chrono::system_clock::to_time_t(sent_at.value());
            json["sent_at"] = static_cast<Json::Int64>(time_t_val);
        }

        if (delivered_at.has_value()) {
            auto time_t_val = std::chrono::system_clock::to_time_t(delivered_at.value());
            json["delivered_at"] = static_cast<Json::Int64>(time_t_val);
        }

        if (error_message.has_value()) {
            json["error_message"] = error_message.value();
        }

        if (external_id.has_value()) {
            json["external_id"] = external_id.value();
        }

        return json;
    }
};

/**
 * @struct Notification
 * @brief Represents a notification in the system
 *
 * Contains all information about a notification including its content,
 * target user, delivery status across channels, and engagement metrics.
 */
struct Notification {
    // =========================================================================
    // IDENTITY
    // =========================================================================

    std::string id;                          ///< UUID primary key
    std::string user_id;                     ///< Target user UUID
    NotificationType type;                   ///< Notification type

    // =========================================================================
    // CONTENT
    // =========================================================================

    std::string title;                       ///< Notification title
    std::string body;                        ///< Notification body text
    std::optional<std::string> image_url;    ///< Optional image URL
    std::optional<std::string> action_url;   ///< URL to open on click

    // =========================================================================
    // RELATED ENTITIES
    // =========================================================================

    std::optional<std::string> dog_id;       ///< Related dog (if applicable)
    std::optional<std::string> shelter_id;   ///< Related shelter (if applicable)
    std::optional<std::string> foster_id;    ///< Related foster (if applicable)

    // =========================================================================
    // DATA PAYLOAD
    // =========================================================================

    Json::Value data;                        ///< Additional JSON data payload

    // =========================================================================
    // DELIVERY
    // =========================================================================

    std::vector<ChannelDeliveryStatus> channels_sent; ///< Delivery status per channel
    std::chrono::system_clock::time_point created_at; ///< When notification was created
    std::optional<std::chrono::system_clock::time_point> scheduled_for; ///< Scheduled delivery time
    bool is_sent;                            ///< Whether notification has been sent
    int retry_count;                         ///< Number of delivery attempts

    // =========================================================================
    // ENGAGEMENT TRACKING
    // =========================================================================

    std::optional<std::chrono::system_clock::time_point> opened_at;   ///< When user opened
    std::optional<std::chrono::system_clock::time_point> clicked_at;  ///< When user clicked
    std::optional<std::string> action_taken;  ///< Action user took (e.g., "viewed_dog", "applied_foster")
    bool is_read;                             ///< Whether notification is marked as read

    // =========================================================================
    // CONVERSION METHODS
    // =========================================================================

    /**
     * @brief Convert Notification to JSON for API responses
     *
     * @return Json::Value The JSON representation
     *
     * @example
     * Notification notif;
     * Json::Value json = notif.toJson();
     */
    Json::Value toJson() const;

    /**
     * @brief Create a Notification from JSON data
     *
     * @param json The JSON object containing notification data
     * @return Notification The constructed Notification object
     */
    static Notification fromJson(const Json::Value& json);

    /**
     * @brief Create a Notification from a database row
     *
     * @param row The pqxx database row
     * @return Notification The constructed Notification object
     */
    static Notification fromDbRow(const pqxx::row& row);

    // =========================================================================
    // HELPER METHODS
    // =========================================================================

    /**
     * @brief Check if notification was delivered successfully
     */
    bool wasDelivered() const;

    /**
     * @brief Get first successful delivery channel
     */
    std::optional<NotificationChannel> getDeliveredChannel() const;

    /**
     * @brief Check if notification is past scheduled time
     */
    bool isReadyToSend() const;

    /**
     * @brief Get priority based on type
     */
    int getPriority() const;

    /**
     * @brief Mark as sent to a specific channel
     */
    void markSent(NotificationChannel channel, const std::string& external_id = "");

    /**
     * @brief Mark as delivered on a specific channel
     */
    void markDelivered(NotificationChannel channel);

    /**
     * @brief Mark as failed on a specific channel
     */
    void markFailed(NotificationChannel channel, const std::string& error);

private:
    // =========================================================================
    // HELPER METHODS
    // =========================================================================

    /**
     * @brief Parse a timestamp from database
     */
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);

    /**
     * @brief Format a timestamp for JSON
     */
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);

    /**
     * @brief Parse PostgreSQL JSONB to ChannelDeliveryStatus vector
     */
    static std::vector<ChannelDeliveryStatus> parseChannelsJson(const std::string& json_str);
};

} // namespace wtl::notifications
