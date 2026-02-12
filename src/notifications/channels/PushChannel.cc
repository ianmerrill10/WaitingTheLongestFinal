/**
 * @file PushChannel.cc
 * @brief Implementation of Firebase Cloud Messaging push notification channel
 * @see PushChannel.h for documentation
 */

#include "notifications/channels/PushChannel.h"

// Standard library includes
#include <sstream>
#include <random>

// Third-party includes (libcurl for HTTP requests)
#include <curl/curl.h>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::notifications::channels {

// ============================================================================
// CURL CALLBACK
// ============================================================================

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// ============================================================================
// PUSH PAYLOAD
// ============================================================================

PushPayload PushPayload::fromNotification(const Notification& notif) {
    PushPayload payload;
    payload.title = notif.title;
    payload.body = notif.body;
    payload.image_url = notif.image_url;
    payload.click_action = notif.action_url;
    payload.icon = getNotificationIcon(notif.type);
    payload.color = getNotificationColor(notif.type);
    payload.ttl_seconds = getNotificationTTL(notif.type);

    // Critical alerts get highest priority
    if (notif.type == NotificationType::CRITICAL_ALERT) {
        payload.priority = "high";
        payload.sound = "critical_alert.wav";
    } else if (requiresImmediateDelivery(notif.type)) {
        payload.priority = "high";
        payload.sound = "urgent.wav";
    } else {
        payload.priority = "normal";
        payload.sound = "default";
    }

    // Copy data payload
    payload.data = notif.data;

    // Add notification metadata to data
    payload.data["notification_id"] = notif.id;
    payload.data["notification_type"] = notificationTypeToString(notif.type);

    if (notif.dog_id.has_value()) {
        payload.data["dog_id"] = notif.dog_id.value();
    }
    if (notif.shelter_id.has_value()) {
        payload.data["shelter_id"] = notif.shelter_id.value();
    }

    // Tag for grouping (prevents notification spam)
    if (notif.dog_id.has_value()) {
        payload.tag = "dog_" + notif.dog_id.value();
    } else {
        payload.tag = notificationTypeToString(notif.type);
    }

    return payload;
}

// ============================================================================
// SINGLETON
// ============================================================================

PushChannel& PushChannel::getInstance() {
    static PushChannel instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool PushChannel::initialize(const std::string& server_key,
                            const std::string& sender_id,
                            const std::string& project_id) {
    server_key_ = server_key;
    sender_id_ = sender_id;
    project_id_ = project_id;

    // FCM v1 API endpoint
    fcm_url_ = "https://fcm.googleapis.com/v1/projects/" + project_id_ + "/messages:send";

    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    initialized_ = true;

    WTL_CAPTURE_INFO(
        wtl::core::debug::ErrorCategory::EXTERNAL_API,
        "PushChannel initialized successfully",
        {{"project_id", project_id_}}
    );

    return true;
}

bool PushChannel::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        std::string server_key = config.getString("notifications.fcm.server_key", "");
        std::string sender_id = config.getString("notifications.fcm.sender_id", "");
        std::string project_id = config.getString("notifications.fcm.project_id", "");

        if (server_key.empty() || project_id.empty()) {
            WTL_CAPTURE_WARNING(
                wtl::core::debug::ErrorCategory::CONFIGURATION,
                "FCM configuration incomplete - push notifications disabled",
                {}
            );
            return false;
        }

        return initialize(server_key, sender_id, project_id);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize PushChannel from config: " + std::string(e.what()),
            {}
        );
        return false;
    }
}

// ============================================================================
// SEND NOTIFICATIONS
// ============================================================================

PushResult PushChannel::sendPush(const std::string& device_token, const Notification& notification) {
    PushPayload payload = PushPayload::fromNotification(notification);
    return sendPushWithPayload(device_token, payload);
}

PushResult PushChannel::sendPushWithPayload(const std::string& device_token, const PushPayload& payload) {
    PushResult result;
    result.success = false;
    result.should_retry = false;
    result.token_invalid = false;

    if (!initialized_) {
        result.error_code = "NOT_INITIALIZED";
        result.error_message = "PushChannel not initialized";
        return result;
    }

    if (device_token.empty()) {
        result.error_code = "INVALID_TOKEN";
        result.error_message = "Device token is empty";
        result.token_invalid = true;
        return result;
    }

    // Build FCM message
    Json::Value message = buildFcmMessage(device_token, payload, false);

    // Send request
    result = sendFcmRequest(message);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_sent_++;
        if (result.success) {
            total_success_++;
        } else {
            total_failed_++;
            if (result.token_invalid) {
                total_invalid_tokens_++;
            }
        }
    }

    // Handle invalid tokens
    if (result.token_invalid) {
        invalidateToken(device_token);
    }

    return result;
}

std::vector<PushResult> PushChannel::sendToMultiple(const std::vector<std::string>& device_tokens,
                                                     const Notification& notification) {
    std::vector<PushResult> results;
    results.reserve(device_tokens.size());

    PushPayload payload = PushPayload::fromNotification(notification);

    for (const auto& token : device_tokens) {
        results.push_back(sendPushWithPayload(token, payload));
    }

    return results;
}

std::vector<PushResult> PushChannel::sendToUser(const std::string& user_id, const Notification& notification) {
    std::vector<DeviceToken> tokens = getUserTokens(user_id);

    std::vector<std::string> token_strings;
    for (const auto& device : tokens) {
        token_strings.push_back(device.token);
    }

    return sendToMultiple(token_strings, notification);
}

PushResult PushChannel::sendToTopic(const std::string& topic, const Notification& notification) {
    PushResult result;
    result.success = false;
    result.should_retry = false;
    result.token_invalid = false;

    if (!initialized_) {
        result.error_code = "NOT_INITIALIZED";
        result.error_message = "PushChannel not initialized";
        return result;
    }

    PushPayload payload = PushPayload::fromNotification(notification);
    Json::Value message = buildFcmMessage("/topics/" + topic, payload, true);

    result = sendFcmRequest(message);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_sent_++;
        if (result.success) {
            total_success_++;
        } else {
            total_failed_++;
        }
    }

    return result;
}

// ============================================================================
// TOKEN MANAGEMENT
// ============================================================================

std::string PushChannel::registerToken(const std::string& user_id,
                                       const std::string& token,
                                       const std::string& platform,
                                       const std::string& device_name) {
    std::string token_id = generateId();

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Upsert - update if token exists, insert if not
        txn.exec_params(
            R"(
            INSERT INTO device_tokens (id, user_id, token, platform, device_name, is_active, registered_at)
            VALUES ($1, $2, $3, $4, $5, true, NOW())
            ON CONFLICT (token) DO UPDATE SET
                user_id = $2,
                platform = $4,
                device_name = $5,
                is_active = true,
                registered_at = NOW()
            )",
            token_id, user_id, token, platform,
            device_name.empty() ? platform + " device" : device_name
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        WTL_CAPTURE_INFO(
            wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            "Device token registered",
            {{"user_id", user_id}, {"platform", platform}}
        );

        return token_id;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to register device token: " + std::string(e.what()),
            {{"user_id", user_id}, {"platform", platform}}
        );
        throw;
    }
}

void PushChannel::unregisterToken(const std::string& user_id, const std::string& token) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "DELETE FROM device_tokens WHERE user_id = $1 AND token = $2",
            user_id, token
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to unregister device token: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }
}

void PushChannel::invalidateToken(const std::string& token) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE device_tokens SET is_active = false WHERE token = $1",
            token
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to invalidate device token: " + std::string(e.what()),
            {}
        );
    }
}

std::vector<DeviceToken> PushChannel::getUserTokens(const std::string& user_id) {
    std::vector<DeviceToken> tokens;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(
            SELECT id, user_id, token, platform, device_name, is_active, registered_at, last_used_at
            FROM device_tokens
            WHERE user_id = $1 AND is_active = true
            ORDER BY last_used_at DESC NULLS LAST
            )",
            user_id
        );

        for (const auto& row : result) {
            tokens.push_back(DeviceToken::fromDbRow(row));
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get user device tokens: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }

    return tokens;
}

void PushChannel::updateTokenLastUsed(const std::string& token) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE device_tokens SET last_used_at = NOW() WHERE token = $1",
            token
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        // Non-critical, just log
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to update token last_used_at: " + std::string(e.what()),
            {}
        );
    }
}

// ============================================================================
// TOPIC MANAGEMENT
// ============================================================================

bool PushChannel::subscribeToTopic(const std::string& token, const std::string& topic) {
    if (!initialized_) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = "https://iid.googleapis.com/iid/v1/" + token + "/rel/topics/" + topic;
    std::string response;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: key=" + server_key_).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Content-Length: 0");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && http_code == 200);
}

bool PushChannel::unsubscribeFromTopic(const std::string& token, const std::string& topic) {
    if (!initialized_) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = "https://iid.googleapis.com/iid/v1:batchRemove";
    std::string response;

    Json::Value body;
    body["to"] = "/topics/" + topic;
    body["registration_tokens"].append(token);

    Json::StreamWriterBuilder writer;
    std::string body_str = Json::writeString(writer, body);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: key=" + server_key_).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && http_code == 200);
}

int PushChannel::subscribeMultipleToTopic(const std::vector<std::string>& tokens, const std::string& topic) {
    int success_count = 0;
    for (const auto& token : tokens) {
        if (subscribeToTopic(token, topic)) {
            success_count++;
        }
    }
    return success_count;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value PushChannel::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Json::Value stats;
    stats["total_sent"] = static_cast<Json::UInt64>(total_sent_);
    stats["total_success"] = static_cast<Json::UInt64>(total_success_);
    stats["total_failed"] = static_cast<Json::UInt64>(total_failed_);
    stats["total_invalid_tokens"] = static_cast<Json::UInt64>(total_invalid_tokens_);
    stats["success_rate"] = total_sent_ > 0 ?
        static_cast<double>(total_success_) / total_sent_ : 0.0;
    stats["initialized"] = initialized_;

    return stats;
}

void PushChannel::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_sent_ = 0;
    total_success_ = 0;
    total_failed_ = 0;
    total_invalid_tokens_ = 0;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

Json::Value PushChannel::buildFcmMessage(const std::string& target, const PushPayload& payload, bool is_topic) {
    Json::Value message;
    Json::Value msg;

    // Target
    if (is_topic) {
        msg["topic"] = target.substr(8); // Remove "/topics/" prefix
    } else {
        msg["token"] = target;
    }

    // Notification block (for display)
    Json::Value notification;
    notification["title"] = payload.title;
    notification["body"] = payload.body;

    if (payload.image_url.has_value()) {
        notification["image"] = payload.image_url.value();
    }

    msg["notification"] = notification;

    // Data block (for app handling)
    if (!payload.data.isNull()) {
        msg["data"] = payload.data;
    }

    // Android specific
    Json::Value android;
    android["priority"] = payload.priority;
    android["ttl"] = std::to_string(payload.ttl_seconds) + "s";

    Json::Value android_notification;
    if (payload.icon.has_value()) {
        android_notification["icon"] = payload.icon.value();
    }
    if (payload.color.has_value()) {
        android_notification["color"] = payload.color.value();
    }
    if (payload.sound.has_value()) {
        android_notification["sound"] = payload.sound.value();
    }
    if (payload.tag.has_value()) {
        android_notification["tag"] = payload.tag.value();
    }
    if (payload.click_action.has_value()) {
        android_notification["click_action"] = payload.click_action.value();
    }
    android["notification"] = android_notification;
    msg["android"] = android;

    // iOS (APNs) specific
    Json::Value apns;
    Json::Value apns_payload;
    Json::Value aps;

    if (payload.sound.has_value()) {
        aps["sound"] = payload.sound.value();
    }
    if (payload.content_available) {
        aps["content-available"] = 1;
    }
    if (payload.mutable_content) {
        aps["mutable-content"] = 1;
    }

    apns_payload["aps"] = aps;
    apns["payload"] = apns_payload;

    Json::Value apns_headers;
    apns_headers["apns-priority"] = payload.priority == "high" ? "10" : "5";
    apns_headers["apns-expiration"] = std::to_string(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + payload.ttl_seconds
    );
    apns["headers"] = apns_headers;

    msg["apns"] = apns;

    // Web push specific
    Json::Value webpush;
    Json::Value webpush_notification;
    if (payload.icon.has_value()) {
        webpush_notification["icon"] = "/icons/" + payload.icon.value() + ".png";
    }
    webpush["notification"] = webpush_notification;
    msg["webpush"] = webpush;

    message["message"] = msg;
    return message;
}

PushResult PushChannel::sendFcmRequest(const Json::Value& message) {
    PushResult result;
    result.success = false;
    result.should_retry = false;
    result.token_invalid = false;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error_code = "CURL_INIT_FAILED";
        result.error_message = "Failed to initialize curl";
        return result;
    }

    // Get access token
    std::string access_token = getAccessToken();
    if (access_token.empty()) {
        // Fall back to legacy API with server key
        access_token = server_key_;
    }

    std::string response;

    Json::StreamWriterBuilder writer;
    std::string body = Json::writeString(writer, message);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + access_token).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, fcm_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        result.error_code = "CURL_ERROR";
        result.error_message = curl_easy_strerror(res);
        result.should_retry = true;
        return result;
    }

    return parseFcmResponse(response, http_code);
}

PushResult PushChannel::parseFcmResponse(const std::string& response, long http_code) {
    PushResult result;
    result.success = false;
    result.should_retry = false;
    result.token_invalid = false;

    Json::Value json;
    Json::Reader reader;
    if (!reader.parse(response, json)) {
        result.error_code = "PARSE_ERROR";
        result.error_message = "Failed to parse FCM response";
        return result;
    }

    if (http_code == 200) {
        result.success = true;
        if (json.isMember("name")) {
            result.message_id = json["name"].asString();
        }
        return result;
    }

    // Handle errors
    if (json.isMember("error")) {
        auto& error = json["error"];
        result.error_code = error.get("status", "UNKNOWN").asString();
        result.error_message = error.get("message", "Unknown error").asString();

        // Check for invalid token
        if (result.error_code == "NOT_FOUND" ||
            result.error_code == "UNREGISTERED" ||
            result.error_message.find("not a valid FCM registration token") != std::string::npos) {
            result.token_invalid = true;
        }

        // Check if should retry
        if (http_code == 429 || // Rate limited
            http_code == 500 || // Server error
            http_code == 503 || // Service unavailable
            result.error_code == "UNAVAILABLE" ||
            result.error_code == "INTERNAL") {
            result.should_retry = true;
        }
    }

    return result;
}

std::string PushChannel::getAccessToken() {
    std::lock_guard<std::mutex> lock(token_mutex_);

    // Check if we have a valid token
    if (!access_token_.empty() && std::chrono::system_clock::now() < token_expiry_) {
        return access_token_;
    }

    // For now, return empty to use legacy API with server key
    // In production, implement OAuth2 service account authentication
    // using Google's OAuth2 library or manual JWT signing

    return "";
}

std::string PushChannel::generateId() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << dis(gen) << dis(gen);

    std::string id = ss.str();
    // Format as UUID-like string
    id.insert(8, "-");
    id.insert(13, "-");
    id.insert(18, "-");
    id.insert(23, "-");

    return id.substr(0, 36);
}

} // namespace wtl::notifications::channels
