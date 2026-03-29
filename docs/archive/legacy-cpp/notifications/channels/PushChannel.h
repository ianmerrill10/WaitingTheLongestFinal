/**
 * @file PushChannel.h
 * @brief Firebase Cloud Messaging (FCM) push notification channel
 *
 * PURPOSE:
 * Handles sending push notifications to mobile devices and web browsers
 * via Firebase Cloud Messaging. Supports both iOS (via APNs) and Android
 * devices, as well as web push notifications.
 *
 * USAGE:
 * auto& push = PushChannel::getInstance();
 * push.sendPush(device_token, notification);
 * push.sendToUser(user_id, notification);
 *
 * DEPENDENCIES:
 * - libcurl (HTTP requests to FCM)
 * - Config (FCM credentials)
 * - ConnectionPool (token storage)
 * - ErrorCapture (error logging)
 *
 * CONFIGURATION:
 * Requires the following in config.json:
 * {
 *   "notifications": {
 *     "fcm": {
 *       "server_key": "your-fcm-server-key",
 *       "sender_id": "your-sender-id",
 *       "project_id": "your-firebase-project-id"
 *     }
 *   }
 * }
 *
 * MODIFICATION GUIDE:
 * - To support additional platforms, extend sendPush()
 * - For batch sending optimization, use sendToMultiple()
 * - Token management is handled by registerToken/unregisterToken
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
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

namespace wtl::notifications::channels {

// Import types from parent namespace
using wtl::notifications::DeviceToken;

/**
 * @struct PushResult
 * @brief Result of a push notification send attempt
 */
struct PushResult {
    bool success;
    std::string message_id;       ///< FCM message ID on success
    std::string error_code;       ///< Error code on failure
    std::string error_message;    ///< Human-readable error message
    bool should_retry;            ///< Whether to retry (e.g., rate limit)
    bool token_invalid;           ///< Token is no longer valid

    Json::Value toJson() const {
        Json::Value json;
        json["success"] = success;
        if (!message_id.empty()) json["message_id"] = message_id;
        if (!error_code.empty()) json["error_code"] = error_code;
        if (!error_message.empty()) json["error_message"] = error_message;
        json["should_retry"] = should_retry;
        json["token_invalid"] = token_invalid;
        return json;
    }
};

/**
 * @struct PushPayload
 * @brief FCM message payload
 */
struct PushPayload {
    std::string title;
    std::string body;
    std::optional<std::string> image_url;
    std::optional<std::string> click_action;  ///< URL to open on click
    std::optional<std::string> icon;
    std::optional<std::string> color;
    std::optional<std::string> sound;
    std::optional<std::string> tag;           ///< Notification tag for grouping
    Json::Value data;                         ///< Custom data payload
    int ttl_seconds;                          ///< Time to live
    std::string priority;                     ///< "high" or "normal"
    bool content_available;                   ///< iOS background notification
    bool mutable_content;                     ///< iOS notification extension

    PushPayload()
        : ttl_seconds(86400)
        , priority("high")
        , content_available(false)
        , mutable_content(false) {}

    /**
     * @brief Create payload from Notification
     */
    static PushPayload fromNotification(const Notification& notif);
};

/**
 * @class PushChannel
 * @brief Singleton service for sending push notifications via FCM
 *
 * Thread-safe singleton that manages Firebase Cloud Messaging integration.
 * Handles token registration, message sending, and error handling.
 */
class PushChannel {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to PushChannel instance
     */
    static PushChannel& getInstance();

    // Prevent copying
    PushChannel(const PushChannel&) = delete;
    PushChannel& operator=(const PushChannel&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the push channel
     *
     * @param server_key FCM server key
     * @param sender_id FCM sender ID
     * @param project_id Firebase project ID
     * @return true if initialization successful
     */
    bool initialize(const std::string& server_key,
                   const std::string& sender_id,
                   const std::string& project_id);

    /**
     * @brief Initialize from config.json
     * @return true if initialization successful
     */
    bool initializeFromConfig();

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized_; }

    // =========================================================================
    // SEND NOTIFICATIONS
    // =========================================================================

    /**
     * @brief Send push notification to a device
     *
     * @param device_token FCM device token
     * @param notification The notification to send
     * @return PushResult Result of the send attempt
     */
    PushResult sendPush(const std::string& device_token, const Notification& notification);

    /**
     * @brief Send push notification with custom payload
     *
     * @param device_token FCM device token
     * @param payload Custom push payload
     * @return PushResult Result of the send attempt
     */
    PushResult sendPushWithPayload(const std::string& device_token, const PushPayload& payload);

    /**
     * @brief Send push notification to multiple devices
     *
     * @param device_tokens Vector of FCM device tokens
     * @param notification The notification to send
     * @return std::vector<PushResult> Results for each token
     */
    std::vector<PushResult> sendToMultiple(const std::vector<std::string>& device_tokens,
                                            const Notification& notification);

    /**
     * @brief Send push notification to all devices for a user
     *
     * @param user_id User UUID
     * @param notification The notification to send
     * @return std::vector<PushResult> Results for each registered device
     */
    std::vector<PushResult> sendToUser(const std::string& user_id, const Notification& notification);

    /**
     * @brief Send push notification to a topic
     *
     * @param topic FCM topic name (e.g., "urgent_dogs_TX")
     * @param notification The notification to send
     * @return PushResult Result of the send attempt
     */
    PushResult sendToTopic(const std::string& topic, const Notification& notification);

    // =========================================================================
    // TOKEN MANAGEMENT
    // =========================================================================

    /**
     * @brief Register a device token for a user
     *
     * @param user_id User UUID
     * @param token FCM device token
     * @param platform "ios", "android", or "web"
     * @param device_name Human-readable device name
     * @return std::string Token registration ID
     */
    std::string registerToken(const std::string& user_id,
                              const std::string& token,
                              const std::string& platform,
                              const std::string& device_name = "");

    /**
     * @brief Unregister a device token
     *
     * @param user_id User UUID
     * @param token FCM device token
     */
    void unregisterToken(const std::string& user_id, const std::string& token);

    /**
     * @brief Mark a token as invalid (stale, expired, etc.)
     *
     * @param token FCM device token
     */
    void invalidateToken(const std::string& token);

    /**
     * @brief Get all active tokens for a user
     *
     * @param user_id User UUID
     * @return std::vector<DeviceToken> Active device tokens
     */
    std::vector<DeviceToken> getUserTokens(const std::string& user_id);

    /**
     * @brief Update token's last used timestamp
     *
     * @param token FCM device token
     */
    void updateTokenLastUsed(const std::string& token);

    // =========================================================================
    // TOPIC MANAGEMENT
    // =========================================================================

    /**
     * @brief Subscribe a device to a topic
     *
     * @param token FCM device token
     * @param topic Topic name
     * @return true if subscription successful
     */
    bool subscribeToTopic(const std::string& token, const std::string& topic);

    /**
     * @brief Unsubscribe a device from a topic
     *
     * @param token FCM device token
     * @param topic Topic name
     * @return true if unsubscription successful
     */
    bool unsubscribeFromTopic(const std::string& token, const std::string& topic);

    /**
     * @brief Subscribe multiple devices to a topic
     *
     * @param tokens Vector of FCM device tokens
     * @param topic Topic name
     * @return Number of successful subscriptions
     */
    int subscribeMultipleToTopic(const std::vector<std::string>& tokens, const std::string& topic);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get channel statistics
     * @return Json::Value Statistics JSON
     */
    Json::Value getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    PushChannel() = default;
    ~PushChannel() = default;

    /**
     * @brief Build FCM message JSON
     */
    Json::Value buildFcmMessage(const std::string& target, const PushPayload& payload, bool is_topic = false);

    /**
     * @brief Send HTTP request to FCM
     */
    PushResult sendFcmRequest(const Json::Value& message);

    /**
     * @brief Parse FCM response
     */
    PushResult parseFcmResponse(const std::string& response, long http_code);

    /**
     * @brief Get OAuth2 access token for FCM v1 API
     */
    std::string getAccessToken();

    /**
     * @brief Generate unique ID
     */
    std::string generateId() const;

    // Configuration
    std::string server_key_;
    std::string sender_id_;
    std::string project_id_;
    std::string fcm_url_;
    bool initialized_ = false;

    // OAuth2 token (for FCM v1 API)
    std::string access_token_;
    std::chrono::system_clock::time_point token_expiry_;
    std::mutex token_mutex_;

    // Statistics
    mutable std::mutex stats_mutex_;
    uint64_t total_sent_ = 0;
    uint64_t total_success_ = 0;
    uint64_t total_failed_ = 0;
    uint64_t total_invalid_tokens_ = 0;
};

} // namespace wtl::notifications::channels
