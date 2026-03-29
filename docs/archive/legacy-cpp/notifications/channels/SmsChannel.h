/**
 * @file SmsChannel.h
 * @brief Twilio SMS notification channel
 *
 * PURPOSE:
 * Handles sending SMS notifications via Twilio API. Used primarily for
 * critical alerts (dogs < 24 hours) when users have opted in to SMS.
 * Implements rate limiting to control costs and prevent spam.
 *
 * USAGE:
 * auto& sms = SmsChannel::getInstance();
 * sms.sendSms(phone_number, message);
 * sms.sendToUser(user_id, notification);
 *
 * DEPENDENCIES:
 * - libcurl (HTTP requests to Twilio)
 * - Config (Twilio credentials)
 * - ConnectionPool (phone number lookup)
 * - ErrorCapture (error logging)
 *
 * CONFIGURATION:
 * Requires the following in config.json:
 * {
 *   "notifications": {
 *     "twilio": {
 *       "account_sid": "your-twilio-account-sid",
 *       "auth_token": "your-twilio-auth-token",
 *       "from_number": "+15551234567",
 *       "messaging_service_sid": "optional-messaging-service-sid"
 *     }
 *   }
 * }
 *
 * RATE LIMITING:
 * - Default: 3 SMS per user per week
 * - Critical alerts bypass rate limiting
 * - Global rate limit per minute to prevent accidental floods
 *
 * MODIFICATION GUIDE:
 * - Update rate limits in RateLimiter methods
 * - Add MMS support for image attachments if needed
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
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "notifications/Notification.h"

namespace wtl::notifications::channels {

/**
 * @struct SmsResult
 * @brief Result of an SMS send attempt
 */
struct SmsResult {
    bool success;
    std::string message_sid;      ///< Twilio message SID
    std::string error_code;
    std::string error_message;
    bool should_retry;
    bool phone_invalid;           ///< Phone number is invalid/unreachable
    double price;                 ///< Cost of the message

    Json::Value toJson() const {
        Json::Value json;
        json["success"] = success;
        if (!message_sid.empty()) json["message_sid"] = message_sid;
        if (!error_code.empty()) json["error_code"] = error_code;
        if (!error_message.empty()) json["error_message"] = error_message;
        json["should_retry"] = should_retry;
        json["phone_invalid"] = phone_invalid;
        json["price"] = price;
        return json;
    }
};

/**
 * @struct SmsMessage
 * @brief SMS message structure
 */
struct SmsMessage {
    std::string to;               ///< Phone number (E.164 format: +1XXXXXXXXXX)
    std::string body;             ///< Message body (max 1600 chars)
    std::optional<std::string> media_url;  ///< MMS image URL
    std::optional<std::string> callback_url; ///< Status callback URL

    /**
     * @brief Create from notification
     */
    static SmsMessage fromNotification(const Notification& notif);
};

/**
 * @class SmsChannel
 * @brief Singleton service for sending SMS via Twilio
 *
 * Thread-safe singleton that manages Twilio SMS integration.
 * Implements rate limiting and phone number validation.
 */
class SmsChannel {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to SmsChannel instance
     */
    static SmsChannel& getInstance();

    // Prevent copying
    SmsChannel(const SmsChannel&) = delete;
    SmsChannel& operator=(const SmsChannel&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the SMS channel
     *
     * @param account_sid Twilio account SID
     * @param auth_token Twilio auth token
     * @param from_number Twilio phone number (E.164 format)
     * @return true if initialization successful
     */
    bool initialize(const std::string& account_sid,
                   const std::string& auth_token,
                   const std::string& from_number);

    /**
     * @brief Initialize from config.json
     * @return true if initialization successful
     */
    bool initializeFromConfig();

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Set messaging service SID (for A2P messaging)
     */
    void setMessagingServiceSid(const std::string& sid);

    // =========================================================================
    // SEND SMS
    // =========================================================================

    /**
     * @brief Send an SMS message
     *
     * @param phone_number Recipient phone number (E.164 format)
     * @param message Message body (max 1600 characters)
     * @return SmsResult Result of the send attempt
     */
    SmsResult sendSms(const std::string& phone_number, const std::string& message);

    /**
     * @brief Send an SMS from a notification
     *
     * @param phone_number Recipient phone number
     * @param notification The notification
     * @return SmsResult Result of the send attempt
     */
    SmsResult sendNotificationSms(const std::string& phone_number, const Notification& notification);

    /**
     * @brief Send SMS to a user by ID
     *
     * @param user_id User UUID
     * @param notification The notification
     * @return SmsResult Result of the send attempt
     */
    SmsResult sendToUser(const std::string& user_id, const Notification& notification);

    /**
     * @brief Send MMS with image
     *
     * @param phone_number Recipient phone number
     * @param message Message body
     * @param image_url URL of image to attach
     * @return SmsResult Result of the send attempt
     */
    SmsResult sendMms(const std::string& phone_number,
                      const std::string& message,
                      const std::string& image_url);

    /**
     * @brief Send SMS to multiple recipients
     *
     * @param phone_numbers Vector of phone numbers
     * @param message Message body
     * @return std::vector<SmsResult> Results for each recipient
     */
    std::vector<SmsResult> sendBulk(const std::vector<std::string>& phone_numbers,
                                     const std::string& message);

    // =========================================================================
    // RATE LIMITING
    // =========================================================================

    /**
     * @brief Check if user can receive SMS (rate limit)
     *
     * @param user_id User UUID
     * @param type Notification type
     * @return true if SMS can be sent
     */
    bool canSendToUser(const std::string& user_id, NotificationType type);

    /**
     * @brief Get remaining SMS quota for user
     *
     * @param user_id User UUID
     * @return Number of SMS remaining this week
     */
    int getRemainingQuota(const std::string& user_id);

    /**
     * @brief Record SMS sent for rate limiting
     *
     * @param user_id User UUID
     */
    void recordSmsSent(const std::string& user_id);

    /**
     * @brief Set user weekly SMS limit
     *
     * @param limit Maximum SMS per user per week
     */
    void setUserWeeklyLimit(int limit) { user_weekly_limit_ = limit; }

    /**
     * @brief Set global per-minute rate limit
     *
     * @param limit Maximum SMS per minute
     */
    void setGlobalRateLimit(int limit) { global_per_minute_limit_ = limit; }

    // =========================================================================
    // PHONE NUMBER VALIDATION
    // =========================================================================

    /**
     * @brief Validate phone number format
     *
     * @param phone Phone number to validate
     * @return true if phone format is valid E.164
     */
    static bool isValidPhoneNumber(const std::string& phone);

    /**
     * @brief Normalize phone number to E.164 format
     *
     * @param phone Phone number to normalize
     * @param country_code Default country code (e.g., "1" for US)
     * @return std::string Normalized E.164 phone number
     */
    static std::string normalizePhoneNumber(const std::string& phone,
                                            const std::string& country_code = "1");

    /**
     * @brief Check if phone number is in opt-out list
     *
     * @param phone Phone number to check
     * @return true if number has opted out
     */
    bool isPhoneOptedOut(const std::string& phone);

    /**
     * @brief Add phone to opt-out list
     *
     * @param phone Phone number to opt out
     */
    void optOutPhone(const std::string& phone);

    /**
     * @brief Remove phone from opt-out list
     *
     * @param phone Phone number to opt back in
     */
    void optInPhone(const std::string& phone);

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

    /**
     * @brief Get total cost for current period
     */
    double getTotalCost() const;

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    SmsChannel() = default;
    ~SmsChannel() = default;

    /**
     * @brief Send HTTP request to Twilio
     */
    SmsResult sendTwilioRequest(const SmsMessage& message);

    /**
     * @brief Parse Twilio response
     */
    SmsResult parseTwilioResponse(const std::string& response, long http_code);

    /**
     * @brief Get user phone by ID
     */
    std::string getUserPhone(const std::string& user_id);

    /**
     * @brief Build message body for notification
     */
    std::string buildNotificationMessage(const Notification& notification) const;

    /**
     * @brief Check global rate limit
     */
    bool checkGlobalRateLimit();

    /**
     * @brief URL encode string for form data
     */
    static std::string urlEncode(const std::string& str);

    // Configuration
    std::string account_sid_;
    std::string auth_token_;
    std::string from_number_;
    std::string messaging_service_sid_;
    bool initialized_ = false;

    // Rate limiting
    int user_weekly_limit_ = 3;
    int global_per_minute_limit_ = 100;
    std::chrono::system_clock::time_point rate_limit_window_start_;
    int messages_this_minute_ = 0;
    std::mutex rate_limit_mutex_;

    // Statistics
    mutable std::mutex stats_mutex_;
    uint64_t total_sent_ = 0;
    uint64_t total_success_ = 0;
    uint64_t total_failed_ = 0;
    uint64_t total_rate_limited_ = 0;
    double total_cost_ = 0.0;
};

} // namespace wtl::notifications::channels
