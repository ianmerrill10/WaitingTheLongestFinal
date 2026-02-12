/**
 * @file SmsChannel.cc
 * @brief Implementation of Twilio SMS notification channel
 * @see SmsChannel.h for documentation
 */

#include "notifications/channels/SmsChannel.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <regex>
#include <sstream>

// Third-party includes
#include <curl/curl.h>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::notifications::channels {

// ============================================================================
// CONSTANTS
// ============================================================================

// Maximum SMS body length (Twilio supports 1600 for concatenated SMS)
constexpr size_t MAX_SMS_LENGTH = 1600;

// Twilio API base URL
constexpr const char* TWILIO_API_BASE = "https://api.twilio.com/2010-04-01/Accounts/";

// ============================================================================
// CURL CALLBACK
// ============================================================================

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// ============================================================================
// SMS MESSAGE
// ============================================================================

SmsMessage SmsMessage::fromNotification(const Notification& notif) {
    SmsMessage msg;
    // Body will be set later based on notification content
    // Include image URL for MMS if available
    if (notif.image_url.has_value()) {
        msg.media_url = notif.image_url;
    }
    return msg;
}

// ============================================================================
// SINGLETON
// ============================================================================

SmsChannel& SmsChannel::getInstance() {
    static SmsChannel instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool SmsChannel::initialize(const std::string& account_sid,
                           const std::string& auth_token,
                           const std::string& from_number) {
    account_sid_ = account_sid;
    auth_token_ = auth_token;
    from_number_ = from_number;

    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Initialize rate limit window
    rate_limit_window_start_ = std::chrono::system_clock::now();
    messages_this_minute_ = 0;

    initialized_ = true;

    WTL_CAPTURE_INFO(
        wtl::core::debug::ErrorCategory::EXTERNAL_API,
        "SmsChannel initialized successfully",
        {{"from_number", from_number_}}
    );

    return true;
}

bool SmsChannel::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        std::string account_sid = config.getString("notifications.twilio.account_sid", "");
        std::string auth_token = config.getString("notifications.twilio.auth_token", "");
        std::string from_number = config.getString("notifications.twilio.from_number", "");

        if (account_sid.empty() || auth_token.empty() || from_number.empty()) {
            WTL_CAPTURE_WARNING(
                wtl::core::debug::ErrorCategory::CONFIGURATION,
                "Twilio configuration incomplete - SMS notifications disabled",
                {}
            );
            return false;
        }

        bool result = initialize(account_sid, auth_token, from_number);

        // Load optional messaging service SID
        std::string messaging_sid = config.getString("notifications.twilio.messaging_service_sid", "");
        if (!messaging_sid.empty()) {
            setMessagingServiceSid(messaging_sid);
        }

        // Load rate limits
        user_weekly_limit_ = config.getInt("notifications.twilio.user_weekly_limit", 3);
        global_per_minute_limit_ = config.getInt("notifications.twilio.global_rate_limit", 100);

        return result;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize SmsChannel from config: " + std::string(e.what()),
            {}
        );
        return false;
    }
}

void SmsChannel::setMessagingServiceSid(const std::string& sid) {
    messaging_service_sid_ = sid;
}

// ============================================================================
// SEND SMS
// ============================================================================

SmsResult SmsChannel::sendSms(const std::string& phone_number, const std::string& message) {
    SmsResult result;
    result.success = false;
    result.should_retry = false;
    result.phone_invalid = false;
    result.price = 0.0;

    if (!initialized_) {
        result.error_code = "NOT_INITIALIZED";
        result.error_message = "SmsChannel not initialized";
        return result;
    }

    // Validate phone number
    std::string normalized_phone = normalizePhoneNumber(phone_number);
    if (!isValidPhoneNumber(normalized_phone)) {
        result.error_code = "INVALID_PHONE";
        result.error_message = "Invalid phone number format";
        result.phone_invalid = true;
        return result;
    }

    // Check opt-out
    if (isPhoneOptedOut(normalized_phone)) {
        result.error_code = "OPTED_OUT";
        result.error_message = "Phone number has opted out of SMS";
        return result;
    }

    // Check global rate limit
    if (!checkGlobalRateLimit()) {
        result.error_code = "RATE_LIMITED";
        result.error_message = "Global SMS rate limit exceeded";
        result.should_retry = true;

        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_rate_limited_++;
        return result;
    }

    // Truncate message if too long
    std::string truncated_message = message;
    if (truncated_message.length() > MAX_SMS_LENGTH) {
        truncated_message = truncated_message.substr(0, MAX_SMS_LENGTH - 3) + "...";
    }

    SmsMessage sms_msg;
    sms_msg.to = normalized_phone;
    sms_msg.body = truncated_message;

    result = sendTwilioRequest(sms_msg);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_sent_++;
        if (result.success) {
            total_success_++;
            total_cost_ += result.price;
        } else {
            total_failed_++;
        }
    }

    return result;
}

SmsResult SmsChannel::sendNotificationSms(const std::string& phone_number, const Notification& notification) {
    std::string message = buildNotificationMessage(notification);
    return sendSms(phone_number, message);
}

SmsResult SmsChannel::sendToUser(const std::string& user_id, const Notification& notification) {
    // Check rate limit for user
    if (!canSendToUser(user_id, notification.type)) {
        SmsResult result;
        result.success = false;
        result.error_code = "USER_RATE_LIMITED";
        result.error_message = "User has exceeded weekly SMS limit";

        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_rate_limited_++;
        return result;
    }

    std::string phone = getUserPhone(user_id);

    if (phone.empty()) {
        SmsResult result;
        result.success = false;
        result.error_code = "NO_PHONE";
        result.error_message = "User has no verified phone number";
        return result;
    }

    SmsResult result = sendNotificationSms(phone, notification);

    // Record for rate limiting if successful
    if (result.success) {
        recordSmsSent(user_id);
    }

    return result;
}

SmsResult SmsChannel::sendMms(const std::string& phone_number,
                              const std::string& message,
                              const std::string& image_url) {
    SmsResult result;
    result.success = false;
    result.should_retry = false;
    result.phone_invalid = false;
    result.price = 0.0;

    if (!initialized_) {
        result.error_code = "NOT_INITIALIZED";
        result.error_message = "SmsChannel not initialized";
        return result;
    }

    std::string normalized_phone = normalizePhoneNumber(phone_number);
    if (!isValidPhoneNumber(normalized_phone)) {
        result.error_code = "INVALID_PHONE";
        result.error_message = "Invalid phone number format";
        result.phone_invalid = true;
        return result;
    }

    if (!checkGlobalRateLimit()) {
        result.error_code = "RATE_LIMITED";
        result.error_message = "Global SMS rate limit exceeded";
        result.should_retry = true;
        return result;
    }

    SmsMessage sms_msg;
    sms_msg.to = normalized_phone;
    sms_msg.body = message;
    sms_msg.media_url = image_url;

    result = sendTwilioRequest(sms_msg);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_sent_++;
        if (result.success) {
            total_success_++;
            total_cost_ += result.price;
        } else {
            total_failed_++;
        }
    }

    return result;
}

std::vector<SmsResult> SmsChannel::sendBulk(const std::vector<std::string>& phone_numbers,
                                            const std::string& message) {
    std::vector<SmsResult> results;
    results.reserve(phone_numbers.size());

    for (const auto& phone : phone_numbers) {
        results.push_back(sendSms(phone, message));
    }

    return results;
}

// ============================================================================
// RATE LIMITING
// ============================================================================

bool SmsChannel::canSendToUser(const std::string& user_id, NotificationType type) {
    // Critical alerts bypass rate limiting - a dog's life is at stake
    if (type == NotificationType::CRITICAL_ALERT) {
        return true;
    }

    int remaining = getRemainingQuota(user_id);
    return remaining > 0;
}

int SmsChannel::getRemainingQuota(const std::string& user_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Count SMS sent in the last 7 days
        auto result = txn.exec_params(
            R"(
            SELECT COUNT(*) as count
            FROM sms_log
            WHERE user_id = $1
            AND sent_at > NOW() - INTERVAL '7 days'
            )",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        int sent = result[0]["count"].as<int>();
        return std::max(0, user_weekly_limit_ - sent);

    } catch (const std::exception& e) {
        // If we can't check, allow the SMS (fail open for critical features)
        return user_weekly_limit_;
    }
}

void SmsChannel::recordSmsSent(const std::string& user_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            INSERT INTO sms_log (user_id, sent_at)
            VALUES ($1, NOW())
            )",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to record SMS sent: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }
}

// ============================================================================
// PHONE NUMBER VALIDATION
// ============================================================================

bool SmsChannel::isValidPhoneNumber(const std::string& phone) {
    // E.164 format: +[country code][number], total 8-15 digits
    static const std::regex phone_regex(R"(\+[1-9]\d{7,14})");
    return std::regex_match(phone, phone_regex);
}

std::string SmsChannel::normalizePhoneNumber(const std::string& phone,
                                             const std::string& country_code) {
    // Remove all non-digit characters except leading +
    std::string normalized;
    bool has_plus = !phone.empty() && phone[0] == '+';

    for (char c : phone) {
        if (std::isdigit(c)) {
            normalized += c;
        }
    }

    // If no country code and US number (10 digits), add +1
    if (!has_plus && normalized.length() == 10) {
        normalized = "+" + country_code + normalized;
    } else if (!has_plus && normalized.length() == 11 && normalized[0] == '1') {
        // Already has country code, just add +
        normalized = "+" + normalized;
    } else if (has_plus) {
        normalized = "+" + normalized;
    } else {
        // Add default country code
        normalized = "+" + country_code + normalized;
    }

    return normalized;
}

bool SmsChannel::isPhoneOptedOut(const std::string& phone) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT 1 FROM sms_opt_outs WHERE phone = $1",
            phone
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return !result.empty();

    } catch (const std::exception& e) {
        // If we can't check, assume not opted out
        return false;
    }
}

void SmsChannel::optOutPhone(const std::string& phone) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(
            INSERT INTO sms_opt_outs (phone, opted_out_at)
            VALUES ($1, NOW())
            ON CONFLICT (phone) DO NOTHING
            )",
            phone
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        WTL_CAPTURE_INFO(
            wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            "Phone number opted out of SMS",
            {{"phone", phone.substr(0, 6) + "****"}} // Partial for privacy
        );

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to opt out phone: " + std::string(e.what()),
            {}
        );
    }
}

void SmsChannel::optInPhone(const std::string& phone) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "DELETE FROM sms_opt_outs WHERE phone = $1",
            phone
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to opt in phone: " + std::string(e.what()),
            {}
        );
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value SmsChannel::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Json::Value stats;
    stats["total_sent"] = static_cast<Json::UInt64>(total_sent_);
    stats["total_success"] = static_cast<Json::UInt64>(total_success_);
    stats["total_failed"] = static_cast<Json::UInt64>(total_failed_);
    stats["total_rate_limited"] = static_cast<Json::UInt64>(total_rate_limited_);
    stats["total_cost"] = total_cost_;
    stats["success_rate"] = total_sent_ > 0 ?
        static_cast<double>(total_success_) / total_sent_ : 0.0;
    stats["initialized"] = initialized_;
    stats["user_weekly_limit"] = user_weekly_limit_;
    stats["global_rate_limit"] = global_per_minute_limit_;

    return stats;
}

void SmsChannel::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_sent_ = 0;
    total_success_ = 0;
    total_failed_ = 0;
    total_rate_limited_ = 0;
    total_cost_ = 0.0;
}

double SmsChannel::getTotalCost() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return total_cost_;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

SmsResult SmsChannel::sendTwilioRequest(const SmsMessage& message) {
    SmsResult result;
    result.success = false;
    result.should_retry = false;
    result.phone_invalid = false;
    result.price = 0.0;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error_code = "CURL_INIT_FAILED";
        result.error_message = "Failed to initialize curl";
        return result;
    }

    std::string response;

    // Build URL
    std::string url = TWILIO_API_BASE + account_sid_ + "/Messages.json";

    // Build form data
    std::string post_data;
    post_data += "To=" + urlEncode(message.to);
    post_data += "&Body=" + urlEncode(message.body);

    if (!messaging_service_sid_.empty()) {
        post_data += "&MessagingServiceSid=" + urlEncode(messaging_service_sid_);
    } else {
        post_data += "&From=" + urlEncode(from_number_);
    }

    if (message.media_url.has_value()) {
        post_data += "&MediaUrl=" + urlEncode(message.media_url.value());
    }

    if (message.callback_url.has_value()) {
        post_data += "&StatusCallback=" + urlEncode(message.callback_url.value());
    }

    // Set up authentication
    std::string auth = account_sid_ + ":" + auth_token_;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        result.error_code = "CURL_ERROR";
        result.error_message = curl_easy_strerror(res);
        result.should_retry = true;
        return result;
    }

    return parseTwilioResponse(response, http_code);
}

SmsResult SmsChannel::parseTwilioResponse(const std::string& response, long http_code) {
    SmsResult result;
    result.success = false;
    result.should_retry = false;
    result.phone_invalid = false;
    result.price = 0.0;

    Json::Value json;
    Json::Reader reader;
    if (!reader.parse(response, json)) {
        result.error_code = "PARSE_ERROR";
        result.error_message = "Failed to parse Twilio response";
        return result;
    }

    if (http_code == 201 || http_code == 200) {
        result.success = true;
        result.message_sid = json.get("sid", "").asString();

        // Parse price if available
        if (json.isMember("price") && !json["price"].isNull()) {
            std::string price_str = json["price"].asString();
            if (!price_str.empty()) {
                result.price = std::abs(std::stod(price_str));
            }
        }

        return result;
    }

    // Handle errors
    result.error_code = std::to_string(json.get("code", 0).asInt());
    result.error_message = json.get("message", "Unknown error").asString();

    // Check for specific error codes
    int code = json.get("code", 0).asInt();

    // Invalid phone number errors
    if (code == 21211 || code == 21214 || code == 21217 || code == 21612) {
        result.phone_invalid = true;
    }

    // Rate limiting
    if (http_code == 429 || code == 14107) {
        result.should_retry = true;
    }

    // Server errors
    if (http_code >= 500) {
        result.should_retry = true;
    }

    return result;
}

std::string SmsChannel::getUserPhone(const std::string& user_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT phone FROM users WHERE id = $1 AND phone_verified = true",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (!result.empty() && !result[0]["phone"].is_null()) {
            return result[0]["phone"].as<std::string>();
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get user phone: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }

    return "";
}

std::string SmsChannel::buildNotificationMessage(const Notification& notification) const {
    std::ostringstream msg;

    // Keep SMS concise
    msg << getNotificationTypeTitle(notification.type) << "\n\n";
    msg << notification.body;

    // Add action URL if available
    if (notification.action_url.has_value()) {
        msg << "\n\n" << notification.action_url.value();
    }

    // Add standard footer
    msg << "\n\n- WaitingTheLongest.com";

    // Reply STOP to opt out (required by carriers)
    msg << "\nReply STOP to unsubscribe";

    return msg.str();
}

bool SmsChannel::checkGlobalRateLimit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - rate_limit_window_start_
    );

    // Reset window every minute
    if (elapsed.count() >= 60) {
        rate_limit_window_start_ = now;
        messages_this_minute_ = 0;
    }

    if (messages_this_minute_ >= global_per_minute_limit_) {
        return false;
    }

    messages_this_minute_++;
    return true;
}

std::string SmsChannel::urlEncode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

} // namespace wtl::notifications::channels
