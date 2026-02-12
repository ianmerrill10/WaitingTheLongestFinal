/**
 * @file TikTokClient.cc
 * @brief Implementation of TikTok API HTTP client
 * @see TikTokClient.h for documentation
 */

#include "TikTokClient.h"

// Standard library includes
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

// Project includes
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::content::tiktok {

// ============================================================================
// TIKTOKAUTHTOKEN IMPLEMENTATION
// ============================================================================

bool TikTokAuthToken::isExpired() const {
    auto now = std::chrono::system_clock::now();
    return now >= expires_at;
}

bool TikTokAuthToken::needsRefresh() const {
    auto now = std::chrono::system_clock::now();
    // Refresh if less than 5 minutes until expiration
    auto refresh_threshold = expires_at - std::chrono::minutes(5);
    return now >= refresh_threshold;
}

Json::Value TikTokAuthToken::toJson() const {
    Json::Value json;
    json["access_token"] = access_token;
    json["refresh_token"] = refresh_token;
    json["open_id"] = open_id;
    json["scope"] = scope;

    auto time = std::chrono::system_clock::to_time_t(expires_at);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    json["expires_at"] = oss.str();

    return json;
}

TikTokAuthToken TikTokAuthToken::fromJson(const Json::Value& json) {
    TikTokAuthToken token;
    token.access_token = json.get("access_token", "").asString();
    token.refresh_token = json.get("refresh_token", "").asString();
    token.open_id = json.get("open_id", "").asString();
    token.scope = json.get("scope", "").asString();

    if (json.isMember("expires_at") && !json["expires_at"].isNull()) {
        std::string ts = json["expires_at"].asString();
        std::tm tm = {};
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        token.expires_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    } else if (json.isMember("expires_in")) {
        int expires_in = json["expires_in"].asInt();
        token.expires_at = std::chrono::system_clock::now() + std::chrono::seconds(expires_in);
    } else {
        // Default to 1 hour
        token.expires_at = std::chrono::system_clock::now() + std::chrono::hours(1);
    }

    return token;
}

// ============================================================================
// TIKTOKRATELIMITINFO IMPLEMENTATION
// ============================================================================

std::chrono::milliseconds TikTokRateLimitInfo::getWaitTime() const {
    if (!is_limited) {
        return std::chrono::milliseconds(0);
    }

    auto now = std::chrono::system_clock::now();
    if (now >= reset_at) {
        return std::chrono::milliseconds(0);
    }

    auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(reset_at - now);
    return wait;
}

// ============================================================================
// TIKTOKPOSTCONTENT IMPLEMENTATION
// ============================================================================

std::vector<std::string> TikTokPostContent::validate() const {
    std::vector<std::string> errors;

    // Caption limits (TikTok allows up to 2200 characters)
    if (caption.length() > 2200) {
        errors.push_back("Caption exceeds 2200 character limit");
    }

    // Must have either video or photos
    if (is_video && (!video_url || video_url->empty())) {
        errors.push_back("Video URL is required for video posts");
    }

    if (!is_video && photo_urls.empty()) {
        errors.push_back("Photo URLs are required for photo posts");
    }

    // Photo limit (max 35 photos in slideshow)
    if (!is_video && photo_urls.size() > 35) {
        errors.push_back("Maximum 35 photos allowed in slideshow");
    }

    // Hashtag count (recommendation, not hard limit)
    if (hashtags.size() > 30) {
        errors.push_back("Too many hashtags (recommend max 30)");
    }

    return errors;
}

// ============================================================================
// TIKTOKAPIRESULT IMPLEMENTATION
// ============================================================================

Json::Value TikTokApiResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["error"] = TikTokClient::errorToString(error);
    json["error_message"] = error_message;

    if (post_id) json["post_id"] = *post_id;
    else json["post_id"] = Json::nullValue;

    if (post_url) json["post_url"] = *post_url;
    else json["post_url"] = Json::nullValue;

    json["data"] = data;
    json["http_status"] = http_status;
    json["retry_count"] = retry_count;
    json["latency_ms"] = static_cast<Json::Int64>(latency.count());

    return json;
}

// ============================================================================
// TIKTOKCLIENT SINGLETON
// ============================================================================

TikTokClient& TikTokClient::getInstance() {
    static TikTokClient instance;
    return instance;
}

// ============================================================================
// TIKTOKCLIENT INITIALIZATION
// ============================================================================

bool TikTokClient::initialize(const std::string& client_key,
                              const std::string& client_secret,
                              const std::string& redirect_uri) {
    if (initialized_) {
        return true;
    }

    client_key_ = client_key;
    client_secret_ = client_secret;
    redirect_uri_ = redirect_uri;

    if (client_key_.empty() || client_secret_.empty()) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "TikTok client initialized without API credentials - API calls will fail",
            {});
    }

    initialized_ = true;
    LOG_INFO << "TikTokClient initialized";

    return true;
}

bool TikTokClient::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        std::string client_key = config.getString("tiktok.client_key", "");
        std::string client_secret = config.getString("tiktok.client_secret", "");
        std::string redirect_uri = config.getString("tiktok.redirect_uri", "");

        // Optional: custom API base URL for testing/sandboxing
        std::string api_url = config.getString("tiktok.api_base_url", "");
        if (!api_url.empty()) {
            api_base_url_ = api_url;
        }

        // Request timeout
        int timeout = config.getInt("tiktok.request_timeout_seconds", 30);
        request_timeout_ = std::chrono::seconds(timeout);

        // Retry configuration
        max_retries_ = config.getInt("tiktok.max_retries", 3);
        retry_base_delay_ = std::chrono::milliseconds(
            config.getInt("tiktok.retry_base_delay_ms", 1000));

        return initialize(client_key, client_secret, redirect_uri);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize TikTokClient from config: " + std::string(e.what()),
            {});
        return false;
    }
}

bool TikTokClient::isInitialized() const {
    return initialized_;
}

void TikTokClient::shutdown() {
    shutting_down_ = true;
    initialized_ = false;
    LOG_INFO << "TikTokClient shutdown";
}

// ============================================================================
// TIKTOKCLIENT AUTHENTICATION
// ============================================================================

std::string TikTokClient::getAuthorizationUrl(const std::string& state) const {
    std::ostringstream url;
    url << "https://www.tiktok.com/v2/auth/authorize/";
    url << "?client_key=" << client_key_;
    url << "&scope=user.info.basic,video.list,video.upload,video.publish";
    url << "&response_type=code";
    url << "&redirect_uri=" << drogon::utils::urlEncode(redirect_uri_);
    url << "&state=" << state;

    return url.str();
}

TikTokApiResult TikTokClient::authenticate(const std::string& code) {
    TikTokApiResult result;
    auto start = std::chrono::steady_clock::now();

    try {
        Json::Value body;
        body["client_key"] = client_key_;
        body["client_secret"] = client_secret_;
        body["code"] = code;
        body["grant_type"] = "authorization_code";
        body["redirect_uri"] = redirect_uri_;

        // Make token request
        auto http_client = drogon::HttpClient::newHttpClient(
            "https://open.tiktokapis.com");

        auto req = drogon::HttpRequest::newHttpJsonRequest(body);
        req->setPath("/v2/oauth/token/");
        req->setMethod(drogon::Post);
        req->addHeader("Content-Type", "application/x-www-form-urlencoded");

        std::promise<drogon::HttpResponsePtr> prom;
        auto future = prom.get_future();

        http_client->sendRequest(req,
            [&prom](drogon::ReqResult r, const drogon::HttpResponsePtr& resp) {
                if (r == drogon::ReqResult::Ok) {
                    prom.set_value(resp);
                } else {
                    prom.set_value(nullptr);
                }
            },
            request_timeout_.count());

        auto response = future.get();

        auto end = std::chrono::steady_clock::now();
        result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (!response) {
            result.error = TikTokApiError::NETWORK_ERROR;
            result.error_message = "Failed to connect to TikTok API";
            return result;
        }

        result.http_status = static_cast<int>(response->getStatusCode());

        if (response->getStatusCode() == drogon::k200OK) {
            auto json_body = response->getJsonObject();
            if (json_body) {
                if (json_body->isMember("data")) {
                    TikTokAuthToken token = TikTokAuthToken::fromJson((*json_body)["data"]);
                    setAuthToken(token);
                    result.success = true;
                    result.data = *json_body;
                } else if (json_body->isMember("access_token")) {
                    TikTokAuthToken token = TikTokAuthToken::fromJson(*json_body);
                    setAuthToken(token);
                    result.success = true;
                    result.data = *json_body;
                } else {
                    result.error = TikTokApiError::API_ERROR;
                    result.error_message = "Invalid token response format";
                }
            }
        } else {
            result.error = parseApiError(response);
            auto json_body = response->getJsonObject();
            if (json_body && json_body->isMember("error")) {
                result.error_message = (*json_body)["error"]["message"].asString();
            } else {
                result.error_message = "Authentication failed with status " +
                    std::to_string(result.http_status);
            }
        }

    } catch (const std::exception& e) {
        result.error = TikTokApiError::UNKNOWN;
        result.error_message = e.what();
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::EXTERNAL_API,
            (std::unordered_map<std::string, std::string>{{"operation", "authenticate"}}));
    }

    recordMetrics(result.success, result.latency);
    return result;
}

TikTokApiResult TikTokClient::refreshToken() {
    std::lock_guard<std::mutex> lock(auth_mutex_);

    TikTokApiResult result;

    if (!auth_token_) {
        result.error = TikTokApiError::AUTH_FAILED;
        result.error_message = "No token to refresh";
        return result;
    }

    auto start = std::chrono::steady_clock::now();

    try {
        Json::Value body;
        body["client_key"] = client_key_;
        body["client_secret"] = client_secret_;
        body["grant_type"] = "refresh_token";
        body["refresh_token"] = auth_token_->refresh_token;

        auto http_client = drogon::HttpClient::newHttpClient(
            "https://open.tiktokapis.com");

        auto req = drogon::HttpRequest::newHttpJsonRequest(body);
        req->setPath("/v2/oauth/token/");
        req->setMethod(drogon::Post);

        std::promise<drogon::HttpResponsePtr> prom;
        auto future = prom.get_future();

        http_client->sendRequest(req,
            [&prom](drogon::ReqResult r, const drogon::HttpResponsePtr& resp) {
                if (r == drogon::ReqResult::Ok) {
                    prom.set_value(resp);
                } else {
                    prom.set_value(nullptr);
                }
            },
            request_timeout_.count());

        auto response = future.get();

        auto end = std::chrono::steady_clock::now();
        result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (!response) {
            result.error = TikTokApiError::NETWORK_ERROR;
            result.error_message = "Failed to connect to TikTok API";
            return result;
        }

        result.http_status = static_cast<int>(response->getStatusCode());

        if (response->getStatusCode() == drogon::k200OK) {
            auto json_body = response->getJsonObject();
            if (json_body) {
                TikTokAuthToken token;
                if (json_body->isMember("data")) {
                    token = TikTokAuthToken::fromJson((*json_body)["data"]);
                } else {
                    token = TikTokAuthToken::fromJson(*json_body);
                }
                auth_token_ = token;
                result.success = true;
                result.data = *json_body;
            }
        } else {
            result.error = parseApiError(response);
            result.error_message = "Token refresh failed";
        }

    } catch (const std::exception& e) {
        result.error = TikTokApiError::UNKNOWN;
        result.error_message = e.what();
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::EXTERNAL_API,
            (std::unordered_map<std::string, std::string>{{"operation", "refreshToken"}}));
    }

    recordMetrics(result.success, result.latency);
    return result;
}

void TikTokClient::setAuthToken(const TikTokAuthToken& token) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    auth_token_ = token;
}

std::optional<TikTokAuthToken> TikTokClient::getAuthToken() const {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    return auth_token_;
}

bool TikTokClient::isAuthenticated() const {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    return auth_token_.has_value() && !auth_token_->isExpired();
}

TikTokApiResult TikTokClient::revokeToken() {
    std::lock_guard<std::mutex> lock(auth_mutex_);

    TikTokApiResult result;

    if (!auth_token_) {
        result.success = true;  // Already no token
        return result;
    }

    auto start = std::chrono::steady_clock::now();

    try {
        Json::Value body;
        body["client_key"] = client_key_;
        body["client_secret"] = client_secret_;
        body["token"] = auth_token_->access_token;

        auto http_client = drogon::HttpClient::newHttpClient(
            "https://open.tiktokapis.com");

        auto req = drogon::HttpRequest::newHttpJsonRequest(body);
        req->setPath("/v2/oauth/revoke/");
        req->setMethod(drogon::Post);

        std::promise<drogon::HttpResponsePtr> prom;
        auto future = prom.get_future();

        http_client->sendRequest(req,
            [&prom](drogon::ReqResult r, const drogon::HttpResponsePtr& resp) {
                if (r == drogon::ReqResult::Ok) {
                    prom.set_value(resp);
                } else {
                    prom.set_value(nullptr);
                }
            },
            request_timeout_.count());

        auto response = future.get();

        auto end = std::chrono::steady_clock::now();
        result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Clear local token regardless of API response
        auth_token_ = std::nullopt;

        if (response && response->getStatusCode() == drogon::k200OK) {
            result.success = true;
        } else {
            result.success = true;  // Token cleared locally
            result.error_message = "Token revocation may have failed on TikTok side";
        }

    } catch (const std::exception& e) {
        auth_token_ = std::nullopt;  // Clear anyway
        result.success = true;
        result.error_message = e.what();
    }

    return result;
}

// ============================================================================
// TIKTOKCLIENT POSTING
// ============================================================================

TikTokApiResult TikTokClient::post(const TikTokPostContent& content) {
    TikTokApiResult result;

    // Validate content first
    auto validation_errors = content.validate();
    if (!validation_errors.empty()) {
        result.error = TikTokApiError::INVALID_CONTENT;
        result.error_message = "Validation failed: " + validation_errors[0];
        return result;
    }

    // Ensure we have a valid token
    if (!ensureValidToken()) {
        result.error = TikTokApiError::AUTH_EXPIRED;
        result.error_message = "Authentication required";
        return result;
    }

    // Check circuit breaker
    if (circuit_open_) {
        auto now = std::chrono::system_clock::now();
        if (now - circuit_opened_at_ < CIRCUIT_RESET_TIMEOUT) {
            result.error = TikTokApiError::CIRCUIT_OPEN;
            result.error_message = "Service temporarily unavailable (circuit open)";
            return result;
        }
        // Try to close circuit
        circuit_open_ = false;
        consecutive_failures_ = 0;
    }

    // Wait for rate limit if needed
    waitForRateLimit();

    auto start = std::chrono::steady_clock::now();

    try {
        // Build post request
        Json::Value body;
        body["post_info"]["title"] = content.caption;
        body["post_info"]["privacy_level"] = "SELF_ONLY";  // Change to PUBLIC_TO_EVERYONE for production

        // Add hashtags to caption
        std::string full_caption = content.caption;
        for (const auto& tag : content.hashtags) {
            if (!tag.empty()) {
                full_caption += " " + (tag[0] == '#' ? tag : "#" + tag);
            }
        }
        body["post_info"]["title"] = full_caption;

        // Add disclosure if present
        if (content.disclosure) {
            body["post_info"]["disclosure_type"] = "BRAND_ORGANIC";
        }

        // Comment/interaction settings
        body["post_info"]["disable_comment"] = !content.allow_comments;
        body["post_info"]["disable_duet"] = !content.allow_duet;
        body["post_info"]["disable_stitch"] = !content.allow_stitch;

        // Source info
        if (content.is_video && content.video_url) {
            body["source_info"]["source"] = "PULL_FROM_URL";
            body["source_info"]["video_url"] = *content.video_url;
        } else if (!content.photo_urls.empty()) {
            body["source_info"]["source"] = "PULL_FROM_URL";
            Json::Value photos(Json::arrayValue);
            for (const auto& url : content.photo_urls) {
                photos.append(url);
            }
            body["source_info"]["photo_images"] = photos;
        }

        // Make the API call
        result = makeRequest(drogon::Post, "/v2/post/publish/video/init/", body);

        if (result.success && result.data.isMember("data")) {
            if (result.data["data"].isMember("publish_id")) {
                result.post_id = result.data["data"]["publish_id"].asString();
            }
        }

    } catch (const std::exception& e) {
        result.error = TikTokApiError::UNKNOWN;
        result.error_message = e.what();
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::EXTERNAL_API,
            (std::unordered_map<std::string, std::string>{{"operation", "post"}}));
    }

    auto end = std::chrono::steady_clock::now();
    result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    recordMetrics(result.success, result.latency);
    return result;
}

TikTokApiResult TikTokClient::uploadVideo(const std::string& video_url) {
    TikTokApiResult result;

    if (!ensureValidToken()) {
        result.error = TikTokApiError::AUTH_EXPIRED;
        result.error_message = "Authentication required";
        return result;
    }

    Json::Value body;
    body["source_info"]["source"] = "PULL_FROM_URL";
    body["source_info"]["video_url"] = video_url;

    return makeRequest(drogon::Post, "/v2/post/publish/inbox/video/init/", body);
}

TikTokApiResult TikTokClient::uploadPhotos(const std::vector<std::string>& photo_urls) {
    TikTokApiResult result;

    if (photo_urls.empty()) {
        result.error = TikTokApiError::INVALID_CONTENT;
        result.error_message = "No photos provided";
        return result;
    }

    if (!ensureValidToken()) {
        result.error = TikTokApiError::AUTH_EXPIRED;
        result.error_message = "Authentication required";
        return result;
    }

    Json::Value body;
    body["source_info"]["source"] = "PULL_FROM_URL";

    Json::Value photos(Json::arrayValue);
    for (const auto& url : photo_urls) {
        photos.append(url);
    }
    body["source_info"]["photo_images"] = photos;

    return makeRequest(drogon::Post, "/v2/post/publish/content/init/", body);
}

TikTokApiResult TikTokClient::deletePost(const std::string& post_id) {
    TikTokApiResult result;

    if (post_id.empty()) {
        result.error = TikTokApiError::INVALID_CONTENT;
        result.error_message = "Post ID required";
        return result;
    }

    if (!ensureValidToken()) {
        result.error = TikTokApiError::AUTH_EXPIRED;
        result.error_message = "Authentication required";
        return result;
    }

    // Note: TikTok API doesn't have a direct delete endpoint
    // This would need to be implemented when/if TikTok adds this feature
    result.error = TikTokApiError::PERMISSION_DENIED;
    result.error_message = "Post deletion not supported via API";

    return result;
}

// ============================================================================
// TIKTOKCLIENT ANALYTICS
// ============================================================================

TikTokApiResult TikTokClient::getPostAnalytics(const std::string& post_id) {
    TikTokApiResult result;

    if (post_id.empty()) {
        result.error = TikTokApiError::INVALID_CONTENT;
        result.error_message = "Post ID required";
        return result;
    }

    if (!ensureValidToken()) {
        result.error = TikTokApiError::AUTH_EXPIRED;
        result.error_message = "Authentication required";
        return result;
    }

    std::string endpoint = "/v2/video/query/?fields=id,create_time,cover_image_url,"
                          "share_url,video_description,duration,title,embed_link,"
                          "like_count,comment_count,share_count,view_count";

    Json::Value body;
    Json::Value filters;
    Json::Value video_ids(Json::arrayValue);
    video_ids.append(post_id);
    filters["video_ids"] = video_ids;
    body["filters"] = filters;

    return makeRequest(drogon::Post, endpoint, body);
}

TikTokApiResult TikTokClient::getMultiPostAnalytics(const std::vector<std::string>& post_ids) {
    TikTokApiResult result;

    if (post_ids.empty()) {
        result.error = TikTokApiError::INVALID_CONTENT;
        result.error_message = "Post IDs required";
        return result;
    }

    if (!ensureValidToken()) {
        result.error = TikTokApiError::AUTH_EXPIRED;
        result.error_message = "Authentication required";
        return result;
    }

    std::string endpoint = "/v2/video/query/?fields=id,create_time,like_count,"
                          "comment_count,share_count,view_count";

    Json::Value body;
    Json::Value filters;
    Json::Value video_ids(Json::arrayValue);
    for (const auto& id : post_ids) {
        video_ids.append(id);
    }
    filters["video_ids"] = video_ids;
    body["filters"] = filters;

    return makeRequest(drogon::Post, endpoint, body);
}

TikTokApiResult TikTokClient::getAccountAnalytics() {
    TikTokApiResult result;

    if (!ensureValidToken()) {
        result.error = TikTokApiError::AUTH_EXPIRED;
        result.error_message = "Authentication required";
        return result;
    }

    // Get user info which includes follower count
    std::string endpoint = "/v2/user/info/?fields=open_id,union_id,avatar_url,"
                          "display_name,bio_description,profile_deep_link,"
                          "is_verified,follower_count,following_count,"
                          "likes_count,video_count";

    return makeRequest(drogon::Get, endpoint);
}

// ============================================================================
// TIKTOKCLIENT UTILITY
// ============================================================================

TikTokRateLimitInfo TikTokClient::getRateLimitInfo() const {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    return rate_limit_;
}

bool TikTokClient::isCircuitOpen() const {
    if (!circuit_open_) return false;

    auto now = std::chrono::system_clock::now();
    if (now - circuit_opened_at_ >= CIRCUIT_RESET_TIMEOUT) {
        return false;  // Ready to retry
    }
    return true;
}

Json::Value TikTokClient::getStats() const {
    Json::Value stats;

    stats["total_requests"] = static_cast<Json::UInt64>(total_requests_.load());
    stats["successful_requests"] = static_cast<Json::UInt64>(successful_requests_.load());
    stats["failed_requests"] = static_cast<Json::UInt64>(failed_requests_.load());
    stats["rate_limited_requests"] = static_cast<Json::UInt64>(rate_limited_requests_.load());

    uint64_t total = total_requests_.load();
    uint64_t latency = total_latency_ms_.load();
    stats["avg_latency_ms"] = total > 0 ?
        static_cast<Json::UInt64>(latency / total) : 0;

    stats["circuit_open"] = circuit_open_.load();
    stats["consecutive_failures"] = consecutive_failures_.load();
    stats["is_authenticated"] = isAuthenticated();

    {
        std::lock_guard<std::mutex> lock(rate_limit_mutex_);
        stats["rate_limit"]["limit"] = rate_limit_.limit;
        stats["rate_limit"]["remaining"] = rate_limit_.remaining;
        stats["rate_limit"]["is_limited"] = rate_limit_.is_limited;
    }

    return stats;
}

bool TikTokClient::testConnectivity() {
    if (!initialized_) return false;

    try {
        auto http_client = drogon::HttpClient::newHttpClient(api_base_url_);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setPath("/");
        req->setMethod(drogon::Get);

        std::promise<bool> prom;
        auto future = prom.get_future();

        http_client->sendRequest(req,
            [&prom](drogon::ReqResult r, const drogon::HttpResponsePtr& resp) {
                prom.set_value(r == drogon::ReqResult::Ok);
            }, 10);

        return future.get();

    } catch (...) {
        return false;
    }
}

std::string TikTokClient::errorToString(TikTokApiError error) {
    switch (error) {
        case TikTokApiError::NONE:              return "none";
        case TikTokApiError::NETWORK_ERROR:     return "network_error";
        case TikTokApiError::AUTH_FAILED:       return "auth_failed";
        case TikTokApiError::AUTH_EXPIRED:      return "auth_expired";
        case TikTokApiError::RATE_LIMITED:      return "rate_limited";
        case TikTokApiError::QUOTA_EXCEEDED:    return "quota_exceeded";
        case TikTokApiError::INVALID_CONTENT:   return "invalid_content";
        case TikTokApiError::CONTENT_TOO_LONG:  return "content_too_long";
        case TikTokApiError::MEDIA_ERROR:       return "media_error";
        case TikTokApiError::POST_NOT_FOUND:    return "post_not_found";
        case TikTokApiError::PERMISSION_DENIED: return "permission_denied";
        case TikTokApiError::API_ERROR:         return "api_error";
        case TikTokApiError::CIRCUIT_OPEN:      return "circuit_open";
        case TikTokApiError::TIMEOUT:           return "timeout";
        case TikTokApiError::UNKNOWN:           return "unknown";
        default:                                return "unknown";
    }
}

// ============================================================================
// TIKTOKCLIENT PRIVATE METHODS
// ============================================================================

TikTokApiResult TikTokClient::makeRequest(
    drogon::HttpMethod method,
    const std::string& endpoint,
    const Json::Value& body,
    bool with_retry) {

    TikTokApiResult result;
    auto start = std::chrono::steady_clock::now();

    int attempts = with_retry ? max_retries_ : 1;

    for (int attempt = 0; attempt < attempts; ++attempt) {
        result.retry_count = attempt;

        try {
            auto http_client = drogon::HttpClient::newHttpClient(api_base_url_);

            drogon::HttpRequestPtr req;
            if (body.isNull() || body.empty()) {
                req = drogon::HttpRequest::newHttpRequest();
            } else {
                req = drogon::HttpRequest::newHttpJsonRequest(body);
            }

            req->setPath(endpoint);
            req->setMethod(method);

            // Add authorization header
            {
                std::lock_guard<std::mutex> lock(auth_mutex_);
                if (auth_token_) {
                    req->addHeader("Authorization", "Bearer " + auth_token_->access_token);
                }
            }

            std::promise<drogon::HttpResponsePtr> prom;
            auto future = prom.get_future();

            http_client->sendRequest(req,
                [&prom](drogon::ReqResult r, const drogon::HttpResponsePtr& resp) {
                    if (r == drogon::ReqResult::Ok) {
                        prom.set_value(resp);
                    } else {
                        prom.set_value(nullptr);
                    }
                },
                request_timeout_.count());

            auto response = future.get();

            if (!response) {
                result.error = TikTokApiError::NETWORK_ERROR;
                result.error_message = "Request failed";
                consecutive_failures_++;

                if (consecutive_failures_ >= CIRCUIT_FAILURE_THRESHOLD) {
                    circuit_open_ = true;
                    circuit_opened_at_ = std::chrono::system_clock::now();
                }

                // Retry with backoff
                if (attempt < attempts - 1) {
                    auto delay = retry_base_delay_ * (1 << attempt);
                    std::this_thread::sleep_for(delay);
                    continue;
                }
                break;
            }

            result.http_status = static_cast<int>(response->getStatusCode());
            parseRateLimitHeaders(response);

            auto json_body = response->getJsonObject();
            if (json_body) {
                result.data = *json_body;
            }

            if (response->getStatusCode() == drogon::k200OK ||
                response->getStatusCode() == drogon::k201Created) {
                result.success = true;
                consecutive_failures_ = 0;
                break;
            }

            // Handle specific error codes
            result.error = parseApiError(response);

            if (result.error == TikTokApiError::RATE_LIMITED) {
                rate_limited_requests_++;
                waitForRateLimit();
                continue;  // Retry after rate limit
            }

            if (result.error == TikTokApiError::AUTH_EXPIRED) {
                // Try to refresh token and retry
                auto refresh_result = refreshToken();
                if (refresh_result.success) {
                    continue;  // Retry with new token
                }
            }

            // For other errors, don't retry
            if (json_body && json_body->isMember("error")) {
                result.error_message = (*json_body)["error"]["message"].asString();
            }

            break;

        } catch (const std::exception& e) {
            result.error = TikTokApiError::UNKNOWN;
            result.error_message = e.what();
            WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::EXTERNAL_API,
                (std::unordered_map<std::string, std::string>{{"endpoint", endpoint}, {"attempt", std::to_string(attempt)}}));

            if (attempt < attempts - 1) {
                auto delay = retry_base_delay_ * (1 << attempt);
                std::this_thread::sleep_for(delay);
            }
        }
    }

    auto end = std::chrono::steady_clock::now();
    result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    recordMetrics(result.success, result.latency);
    return result;
}

void TikTokClient::parseRateLimitHeaders(const drogon::HttpResponsePtr& response) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto limit_str = response->getHeader("x-ratelimit-limit");
    auto remaining_str = response->getHeader("x-ratelimit-remaining");
    auto reset_str = response->getHeader("x-ratelimit-reset");

    if (!limit_str.empty()) {
        rate_limit_.limit = std::stoi(limit_str);
    }

    if (!remaining_str.empty()) {
        rate_limit_.remaining = std::stoi(remaining_str);
        rate_limit_.is_limited = (rate_limit_.remaining <= 0);
    }

    if (!reset_str.empty()) {
        auto reset_time = std::stol(reset_str);
        rate_limit_.reset_at = std::chrono::system_clock::from_time_t(reset_time);
    }
}

TikTokApiError TikTokClient::parseApiError(const drogon::HttpResponsePtr& response) {
    auto status = response->getStatusCode();

    switch (status) {
        case drogon::k401Unauthorized:
            return TikTokApiError::AUTH_EXPIRED;
        case drogon::k403Forbidden:
            return TikTokApiError::PERMISSION_DENIED;
        case drogon::k404NotFound:
            return TikTokApiError::POST_NOT_FOUND;
        case drogon::k429TooManyRequests:
            return TikTokApiError::RATE_LIMITED;
        default:
            break;
    }

    auto json_body = response->getJsonObject();
    if (json_body && json_body->isMember("error")) {
        std::string error_code = (*json_body)["error"]["code"].asString();

        if (error_code == "access_token_invalid" ||
            error_code == "access_token_expired") {
            return TikTokApiError::AUTH_EXPIRED;
        }
        if (error_code == "rate_limit_exceeded") {
            return TikTokApiError::RATE_LIMITED;
        }
        if (error_code == "spam_risk_detected") {
            return TikTokApiError::INVALID_CONTENT;
        }
    }

    if (static_cast<int>(status) >= 500) {
        return TikTokApiError::API_ERROR;
    }
    if (static_cast<int>(status) >= 400) {
        return TikTokApiError::INVALID_CONTENT;
    }

    return TikTokApiError::UNKNOWN;
}

void TikTokClient::waitForRateLimit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto wait_time = rate_limit_.getWaitTime();
    if (wait_time.count() > 0) {
        LOG_DEBUG << "Rate limited, waiting " << wait_time.count() << "ms";
        std::this_thread::sleep_for(wait_time);
        rate_limit_.is_limited = false;
    }
}

void TikTokClient::recordMetrics(bool success, std::chrono::milliseconds latency) {
    total_requests_++;

    if (success) {
        successful_requests_++;
    } else {
        failed_requests_++;
    }

    total_latency_ms_ += latency.count();
}

bool TikTokClient::ensureValidToken() {
    std::lock_guard<std::mutex> lock(auth_mutex_);

    if (!auth_token_) {
        return false;
    }

    if (auth_token_->isExpired()) {
        // Token is fully expired, need re-authentication
        return false;
    }

    if (auth_token_->needsRefresh()) {
        // Release lock before refresh (refresh will acquire it)
        // Note: This creates a brief window - in production, consider more robust handling
        // For now, we'll just proceed with the current token
        LOG_INFO << "Token needs refresh but continuing with current token";
    }

    return true;
}

} // namespace wtl::content::tiktok
