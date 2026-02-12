/**
 * @file TwitterClient.cc
 * @brief Implementation of Twitter/X API v2 client
 * @see TwitterClient.h for documentation
 */

#include "content/social/clients/TwitterClient.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

// Third-party includes
#include <drogon/HttpClient.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

// Project includes
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::content::social::clients {

using namespace ::wtl::core::debug;
using namespace ::wtl::core::utils;

// ============================================================================
// TWITTER POST RESULT
// ============================================================================

Json::Value TwitterPostResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["tweet_id"] = tweet_id;
    json["tweet_url"] = tweet_url;
    json["error_type"] = error_type;
    json["error_detail"] = error_detail;
    json["is_rate_limited"] = is_rate_limited;
    json["retry_after_seconds"] = static_cast<int>(retry_after.count());
    return json;
}

// ============================================================================
// TWITTER METRICS
// ============================================================================

PlatformAnalytics TwitterMetrics::toPlatformAnalytics() const {
    PlatformAnalytics analytics;
    analytics.platform = Platform::TWITTER;
    analytics.impressions = impression_count;
    analytics.reach = impression_count;  // Twitter doesn't separate reach
    analytics.profile_visits = user_profile_clicks;
    analytics.likes = like_count;
    analytics.comments = reply_count;
    analytics.shares = retweet_count + quote_count;
    analytics.saves = bookmark_count;
    analytics.link_clicks = url_link_clicks;
    analytics.video_views = video_views;
    analytics.video_watch_time_seconds = 0;
    analytics.video_completion_rate = 0.0;
    analytics.new_followers = 0;
    analytics.calculateEngagementRate();
    analytics.fetched_at = fetched_at;
    return analytics;
}

Json::Value TwitterMetrics::toJson() const {
    Json::Value json;
    json["tweet_id"] = tweet_id;
    json["retweet_count"] = retweet_count;
    json["reply_count"] = reply_count;
    json["like_count"] = like_count;
    json["quote_count"] = quote_count;
    json["bookmark_count"] = bookmark_count;
    json["impression_count"] = impression_count;
    json["url_link_clicks"] = url_link_clicks;
    json["user_profile_clicks"] = user_profile_clicks;
    json["video_views"] = video_views;
    json["engagement_rate"] = engagement_rate;
    return json;
}

// ============================================================================
// TWITTER POST DATA
// ============================================================================

TwitterPostData TwitterPostData::fromSocialPost(const SocialPost& post) {
    TwitterPostData data;

    // Get Twitter-specific text or use primary
    auto it = post.platform_text.find(Platform::TWITTER);
    if (it != post.platform_text.end()) {
        data.text = it->second;
    } else {
        // Twitter has a 280 char limit, so truncate if needed
        data.text = post.primary_text;
        if (data.text.length() > 250) {  // Leave room for hashtags
            data.text = data.text.substr(0, 247) + "...";
        }
    }

    // Add hashtags (Twitter limit is ~10)
    std::string hashtags = post.getHashtagsForPlatform(Platform::TWITTER);
    if (!hashtags.empty() && data.text.length() + hashtags.length() + 1 <= 280) {
        data.text += "\n" + hashtags;
    }

    // Media
    data.image_urls = post.images;
    data.video_url = post.video_url;

    return data;
}

// ============================================================================
// TWITTER CLIENT - SINGLETON
// ============================================================================

TwitterClient& TwitterClient::getInstance() {
    static TwitterClient instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void TwitterClient::initialize(
    const std::string& api_key,
    const std::string& api_secret,
    const std::string& bearer_token) {

    std::lock_guard<std::mutex> lock(mutex_);
    api_key_ = api_key;
    api_secret_ = api_secret;
    bearer_token_ = bearer_token;
    initialized_ = true;
}

void TwitterClient::initializeFromConfig() {
    auto& config = Config::getInstance();
    std::string api_key = config.getString("social.twitter.api_key", "");
    std::string api_secret = config.getString("social.twitter.api_secret", "");
    std::string bearer = config.getString("social.twitter.bearer_token", "");

    if (api_key.empty() || api_secret.empty()) {
        WTL_CAPTURE_WARNING(
            ErrorCategory::CONFIGURATION,
            "Twitter API credentials not configured",
            {}
        );
        return;
    }

    initialize(api_key, api_secret, bearer);
}

bool TwitterClient::isInitialized() const {
    return initialized_;
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

std::string TwitterClient::getAuthorizationUrl(
    const std::string& redirect_uri,
    const std::vector<std::string>& scopes,
    const std::string& state,
    const std::string& code_challenge) {

    std::ostringstream url;
    url << "https://twitter.com/i/oauth2/authorize?"
        << "response_type=code"
        << "&client_id=" << api_key_
        << "&redirect_uri=" << redirect_uri
        << "&scope=";

    for (size_t i = 0; i < scopes.size(); ++i) {
        if (i > 0) url << "%20";
        url << scopes[i];
    }

    url << "&state=" << state
        << "&code_challenge=" << code_challenge
        << "&code_challenge_method=S256";

    return url.str();
}

std::optional<std::tuple<std::string, std::string, std::chrono::seconds>>
TwitterClient::exchangeCodeForToken(
    const std::string& code,
    const std::string& redirect_uri,
    const std::string& code_verifier) {

    try {
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setPath("/2/oauth2/token");

        std::ostringstream body;
        body << "code=" << code
             << "&grant_type=authorization_code"
             << "&client_id=" << api_key_
             << "&redirect_uri=" << redirect_uri
             << "&code_verifier=" << code_verifier;

        req->setBody(body.str());
        req->setContentTypeCode(drogon::CT_APPLICATION_X_FORM);

        // Use Basic auth with client credentials
        std::string credentials = api_key_ + ":" + api_secret_;
        // Base64 encode credentials
        std::string encoded;
        // Simple base64 encoding
        static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        size_t i = 0;
        while (i < credentials.length()) {
            uint32_t chunk = 0;
            int valid_bits = 0;
            for (int j = 0; j < 3 && i < credentials.length(); ++j, ++i) {
                chunk = (chunk << 8) | static_cast<uint8_t>(credentials[i]);
                valid_bits += 8;
            }
            chunk <<= (24 - valid_bits);
            for (int j = 0; j < (valid_bits / 6) + 1; ++j) {
                encoded += table[(chunk >> (18 - 6 * j)) & 0x3F];
            }
        }
        while (encoded.length() % 4) encoded += '=';

        req->addHeader("Authorization", "Basic " + encoded);

        auto [result, resp] = client->sendRequest(req, 30.0);

        if (result != drogon::ReqResult::Ok || !resp) {
            return std::nullopt;
        }

        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(std::string(resp->body()), json)) {
            return std::nullopt;
        }

        if (json.isMember("error")) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Twitter token exchange failed: " + json["error_description"].asString(),
                {}
            );
            return std::nullopt;
        }

        std::string access_token = json["access_token"].asString();
        std::string refresh_token = json.get("refresh_token", "").asString();
        int expires_in = json.get("expires_in", 7200).asInt();

        return std::make_tuple(access_token, refresh_token, std::chrono::seconds(expires_in));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Twitter token exchange exception: " + std::string(e.what()),
            {}
        );
        return std::nullopt;
    }
}

std::optional<std::tuple<std::string, std::string, std::chrono::seconds>>
TwitterClient::refreshAccessToken(const std::string& refresh_token) {

    try {
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setPath("/2/oauth2/token");

        std::ostringstream body;
        body << "grant_type=refresh_token"
             << "&refresh_token=" << refresh_token
             << "&client_id=" << api_key_;

        req->setBody(body.str());
        req->setContentTypeCode(drogon::CT_APPLICATION_X_FORM);

        auto [result, resp] = client->sendRequest(req, 30.0);

        if (result != drogon::ReqResult::Ok || !resp) {
            return std::nullopt;
        }

        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(std::string(resp->body()), json)) {
            return std::nullopt;
        }

        if (json.isMember("error")) {
            return std::nullopt;
        }

        std::string access_token = json["access_token"].asString();
        std::string new_refresh_token = json.get("refresh_token", refresh_token).asString();
        int expires_in = json.get("expires_in", 7200).asInt();

        return std::make_tuple(access_token, new_refresh_token, std::chrono::seconds(expires_in));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Twitter token refresh exception: " + std::string(e.what()),
            {}
        );
        return std::nullopt;
    }
}

bool TwitterClient::revokeToken(const std::string& token) {
    try {
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setPath("/2/oauth2/revoke");

        std::ostringstream body;
        body << "token=" << token
             << "&client_id=" << api_key_;

        req->setBody(body.str());
        req->setContentTypeCode(drogon::CT_APPLICATION_X_FORM);

        auto [result, resp] = client->sendRequest(req, 30.0);

        return result == drogon::ReqResult::Ok && resp &&
               resp->getStatusCode() == drogon::k200OK;

    } catch (const std::exception& e) {
        return false;
    }
}

// ============================================================================
// POSTING - TWEETS
// ============================================================================

TwitterPostResult TwitterClient::postTweet(
    const std::string& access_token,
    const std::string& text) {

    // Check rate limiting
    if (isRateLimited("tweets")) {
        TwitterPostResult result;
        result.success = false;
        result.is_rate_limited = true;
        result.retry_after = getTimeUntilReset("tweets");
        result.error_detail = "Rate limited. Too many tweets.";
        return result;
    }

    Json::Value body;
    body["text"] = text;

    auto response = apiPost("/2/tweets", access_token, body);

    TwitterPostResult result;
    if (!response) {
        result.success = false;
        result.error_detail = "API request failed";
        return result;
    }

    if (response->isMember("errors") || response->isMember("detail")) {
        return parseError(*response);
    }

    result.success = true;
    result.tweet_id = (*response)["data"]["id"].asString();

    // Construct tweet URL
    auto user = getAuthenticatedUser(access_token);
    if (user && (*user).isMember("data")) {
        std::string username = (*user)["data"]["username"].asString();
        result.tweet_url = "https://twitter.com/" + username + "/status/" + result.tweet_id;
    }

    return result;
}

TwitterPostResult TwitterClient::postTweetWithMedia(
    const std::string& access_token,
    const std::string& text,
    const std::vector<std::string>& media_ids) {

    if (isRateLimited("tweets")) {
        TwitterPostResult result;
        result.success = false;
        result.is_rate_limited = true;
        result.retry_after = getTimeUntilReset("tweets");
        result.error_detail = "Rate limited.";
        return result;
    }

    Json::Value body;
    body["text"] = text;

    if (!media_ids.empty()) {
        Json::Value media;
        Json::Value ids(Json::arrayValue);
        for (const auto& id : media_ids) {
            ids.append(id);
        }
        media["media_ids"] = ids;
        body["media"] = media;
    }

    auto response = apiPost("/2/tweets", access_token, body);

    TwitterPostResult result;
    if (!response) {
        result.success = false;
        result.error_detail = "API request failed";
        return result;
    }

    if (response->isMember("errors") || response->isMember("detail")) {
        return parseError(*response);
    }

    result.success = true;
    result.tweet_id = (*response)["data"]["id"].asString();

    return result;
}

TwitterPostResult TwitterClient::post(
    const std::string& access_token,
    const TwitterPostData& data) {

    // Upload media first if needed
    std::vector<std::string> media_ids = data.media_ids;

    // Upload images
    for (const auto& url : data.image_urls) {
        auto upload = uploadImage(access_token, url);
        if (upload.success) {
            media_ids.push_back(upload.media_id_string);
        }
    }

    // Upload video
    if (data.video_url) {
        auto upload = uploadVideo(access_token, *data.video_url);
        if (upload.success) {
            // Wait for processing
            if (waitForVideoProcessing(access_token, upload.media_id_string, 300)) {
                media_ids.push_back(upload.media_id_string);
            }
        }
    }

    // Build tweet body
    Json::Value body;
    body["text"] = data.text;

    if (!media_ids.empty()) {
        Json::Value media;
        Json::Value ids(Json::arrayValue);
        for (const auto& id : media_ids) {
            ids.append(id);
        }
        media["media_ids"] = ids;
        body["media"] = media;
    }

    if (data.reply_to_tweet_id) {
        Json::Value reply;
        reply["in_reply_to_tweet_id"] = *data.reply_to_tweet_id;
        body["reply"] = reply;
    }

    if (data.quote_tweet_id) {
        body["quote_tweet_id"] = *data.quote_tweet_id;
    }

    if (!data.poll_options.empty() && data.poll_options.size() >= 2) {
        Json::Value poll;
        Json::Value options(Json::arrayValue);
        for (const auto& opt : data.poll_options) {
            options.append(opt);
        }
        poll["options"] = options;
        poll["duration_minutes"] = data.poll_duration_minutes;
        body["poll"] = poll;
    }

    if (data.reply_settings) {
        body["reply_settings"] = *data.reply_settings;
    }

    auto response = apiPost("/2/tweets", access_token, body);

    TwitterPostResult result;
    if (!response) {
        result.success = false;
        result.error_detail = "API request failed";
        return result;
    }

    if (response->isMember("errors") || response->isMember("detail")) {
        return parseError(*response);
    }

    result.success = true;
    result.tweet_id = (*response)["data"]["id"].asString();

    return result;
}

// ============================================================================
// POSTING - THREADS
// ============================================================================

std::vector<TwitterPostResult> TwitterClient::postThread(
    const std::string& access_token,
    const std::vector<std::string>& tweets) {

    std::vector<TwitterPostResult> results;
    std::string previous_tweet_id;

    for (const auto& text : tweets) {
        TwitterPostData data;
        data.text = text;

        if (!previous_tweet_id.empty()) {
            data.reply_to_tweet_id = previous_tweet_id;
        }

        auto result = post(access_token, data);
        results.push_back(result);

        if (result.success) {
            previous_tweet_id = result.tweet_id;
        } else {
            // Stop thread on failure
            break;
        }

        // Small delay between tweets to avoid rate limits
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return results;
}

std::vector<TwitterPostResult> TwitterClient::postThread(
    const std::string& access_token,
    const std::vector<TwitterPostData>& tweets_data) {

    std::vector<TwitterPostResult> results;
    std::string previous_tweet_id;

    for (auto data : tweets_data) {
        if (!previous_tweet_id.empty()) {
            data.reply_to_tweet_id = previous_tweet_id;
        }

        auto result = post(access_token, data);
        results.push_back(result);

        if (result.success) {
            previous_tweet_id = result.tweet_id;
        } else {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return results;
}

// ============================================================================
// MEDIA UPLOAD
// ============================================================================

TwitterMediaUpload TwitterClient::uploadImage(
    const std::string& access_token,
    const std::string& image_url,
    const std::string& alt_text) {

    // Download image from URL
    // Then upload to Twitter's media endpoint
    // This is a simplified implementation - production would download and upload the actual bytes

    TwitterMediaUpload result;

    std::map<std::string, std::string> params;
    params["media_category"] = "tweet_image";

    // In production, this would:
    // 1. Download the image from image_url
    // 2. Base64 encode it
    // 3. POST to https://upload.twitter.com/1.1/media/upload.json

    // For now, we'll assume the media is already uploaded
    result.success = false;
    result.error_message = "Direct URL upload not supported - use pre-uploaded media_id";

    return result;
}

TwitterMediaUpload TwitterClient::uploadVideo(
    const std::string& access_token,
    const std::string& video_url) {

    TwitterMediaUpload result;

    // Twitter video upload uses chunked upload:
    // 1. INIT - get media_id
    // 2. APPEND - upload chunks
    // 3. FINALIZE - complete upload
    // 4. STATUS - check processing

    result.success = false;
    result.error_message = "Direct URL upload not supported - use pre-uploaded media_id";

    return result;
}

TwitterMediaUpload TwitterClient::checkUploadStatus(
    const std::string& access_token,
    const std::string& media_id) {

    TwitterMediaUpload result;
    result.media_id_string = media_id;

    std::map<std::string, std::string> params;
    params["command"] = "STATUS";
    params["media_id"] = media_id;

    auto response = mediaUpload("/1.1/media/upload.json", access_token, params);
    if (!response) {
        result.success = false;
        result.error_message = "Failed to check status";
        return result;
    }

    if (response->isMember("processing_info")) {
        std::string state = (*response)["processing_info"]["state"].asString();
        if (state == "succeeded") {
            result.success = true;
            result.processing_status = 3;
        } else if (state == "failed") {
            result.success = false;
            result.processing_status = 2;
            result.error_message = (*response)["processing_info"]["error"]["message"].asString();
        } else {
            result.processing_status = 1;  // Still processing
        }
    } else {
        result.success = true;
        result.processing_status = 3;
    }

    return result;
}

bool TwitterClient::waitForVideoProcessing(
    const std::string& access_token,
    const std::string& media_id,
    int timeout_seconds) {

    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start);

        if (elapsed.count() >= timeout_seconds) {
            return false;
        }

        auto status = checkUploadStatus(access_token, media_id);
        if (status.processing_status == 3) {  // Succeeded
            return true;
        }
        if (status.processing_status == 2) {  // Failed
            return false;
        }

        // Wait before next check
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

// ============================================================================
// ANALYTICS
// ============================================================================

std::optional<TwitterMetrics> TwitterClient::getMetrics(
    const std::string& tweet_id,
    const std::string& access_token) {

    std::map<std::string, std::string> params;
    params["tweet.fields"] = "public_metrics,non_public_metrics,organic_metrics";

    auto response = apiGet("/2/tweets/" + tweet_id, access_token, params);
    if (!response || !response->isMember("data")) {
        return std::nullopt;
    }

    TwitterMetrics metrics;
    metrics.tweet_id = tweet_id;
    metrics.fetched_at = std::chrono::system_clock::now();

    auto& data = (*response)["data"];

    // Public metrics
    if (data.isMember("public_metrics")) {
        auto& pm = data["public_metrics"];
        metrics.retweet_count = pm.get("retweet_count", 0).asInt();
        metrics.reply_count = pm.get("reply_count", 0).asInt();
        metrics.like_count = pm.get("like_count", 0).asInt();
        metrics.quote_count = pm.get("quote_count", 0).asInt();
        metrics.bookmark_count = pm.get("bookmark_count", 0).asInt();
    }

    // Non-public metrics (only available if user owns the tweet)
    if (data.isMember("non_public_metrics")) {
        auto& npm = data["non_public_metrics"];
        metrics.impression_count = npm.get("impression_count", 0).asInt();
        metrics.url_link_clicks = npm.get("url_link_clicks", 0).asInt();
        metrics.user_profile_clicks = npm.get("user_profile_clicks", 0).asInt();
    }

    // Calculate engagement rate
    if (metrics.impression_count > 0) {
        int engagements = metrics.like_count + metrics.retweet_count +
                          metrics.reply_count + metrics.quote_count;
        metrics.engagement_rate = static_cast<double>(engagements) /
                                  metrics.impression_count * 100.0;
    }

    return metrics;
}

std::map<std::string, TwitterMetrics> TwitterClient::getBatchMetrics(
    const std::vector<std::string>& tweet_ids,
    const std::string& access_token) {

    std::map<std::string, TwitterMetrics> results;

    // Twitter allows up to 100 IDs per request
    std::string ids_param;
    for (size_t i = 0; i < tweet_ids.size() && i < 100; ++i) {
        if (i > 0) ids_param += ",";
        ids_param += tweet_ids[i];
    }

    std::map<std::string, std::string> params;
    params["ids"] = ids_param;
    params["tweet.fields"] = "public_metrics,non_public_metrics";

    auto response = apiGet("/2/tweets", access_token, params);
    if (!response || !response->isMember("data")) {
        return results;
    }

    for (const auto& tweet : (*response)["data"]) {
        TwitterMetrics metrics;
        metrics.tweet_id = tweet["id"].asString();
        metrics.fetched_at = std::chrono::system_clock::now();

        if (tweet.isMember("public_metrics")) {
            auto& pm = tweet["public_metrics"];
            metrics.retweet_count = pm.get("retweet_count", 0).asInt();
            metrics.reply_count = pm.get("reply_count", 0).asInt();
            metrics.like_count = pm.get("like_count", 0).asInt();
            metrics.quote_count = pm.get("quote_count", 0).asInt();
        }

        results[metrics.tweet_id] = metrics;
    }

    return results;
}

// ============================================================================
// TWEET MANAGEMENT
// ============================================================================

bool TwitterClient::deleteTweet(
    const std::string& tweet_id,
    const std::string& access_token) {

    return apiDelete("/2/tweets/" + tweet_id, access_token);
}

std::optional<Json::Value> TwitterClient::getTweet(
    const std::string& tweet_id,
    const std::string& access_token) {

    std::map<std::string, std::string> params;
    params["tweet.fields"] = "created_at,text,author_id,public_metrics";
    params["expansions"] = "author_id";
    params["user.fields"] = "username,name";

    return apiGet("/2/tweets/" + tweet_id, access_token, params);
}

std::optional<Json::Value> TwitterClient::getAuthenticatedUser(
    const std::string& access_token) {

    return apiGet("/2/users/me", access_token,
                  {{"user.fields", "id,name,username,profile_image_url"}});
}

// ============================================================================
// RATE LIMITING
// ============================================================================

bool TwitterClient::isRateLimited(const std::string& endpoint) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rate_limits_.find(endpoint);
    if (it == rate_limits_.end()) {
        return false;
    }
    return it->second.remaining <= 0 &&
           std::chrono::system_clock::now() < it->second.reset_at;
}

std::chrono::seconds TwitterClient::getTimeUntilReset(const std::string& endpoint) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rate_limits_.find(endpoint);
    if (it == rate_limits_.end()) {
        return std::chrono::seconds(0);
    }
    auto now = std::chrono::system_clock::now();
    if (now >= it->second.reset_at) {
        return std::chrono::seconds(0);
    }
    return std::chrono::duration_cast<std::chrono::seconds>(it->second.reset_at - now);
}

int TwitterClient::getRemainingCalls(const std::string& endpoint) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rate_limits_.find(endpoint);
    if (it == rate_limits_.end()) {
        return 15;  // Default
    }
    return it->second.remaining;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::optional<Json::Value> TwitterClient::apiGet(
    const std::string& endpoint,
    const std::string& access_token,
    const std::map<std::string, std::string>& params,
    bool use_bearer) {

    try {
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);

        std::string path = endpoint;
        if (!params.empty()) {
            path += "?";
            for (const auto& [key, value] : params) {
                if (path.back() != '?') path += "&";
                path += key + "=" + value;
            }
        }
        req->setPath(path);

        // Set authorization header
        if (use_bearer && !bearer_token_.empty()) {
            req->addHeader("Authorization", "Bearer " + bearer_token_);
        } else {
            req->addHeader("Authorization", "Bearer " + access_token);
        }

        auto [result, resp] = client->sendRequest(req, 30.0);

        if (result != drogon::ReqResult::Ok || !resp) {
            return std::nullopt;
        }

        // Update rate limits from headers
        // x-rate-limit-remaining, x-rate-limit-reset

        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(std::string(resp->body()), json)) {
            return std::nullopt;
        }

        return json;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Twitter API GET exception: " + std::string(e.what()),
            {{"endpoint", endpoint}}
        );
        return std::nullopt;
    }
}

std::optional<Json::Value> TwitterClient::apiPost(
    const std::string& endpoint,
    const std::string& access_token,
    const Json::Value& body) {

    try {
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setPath(endpoint);

        Json::StreamWriterBuilder writer;
        req->setBody(Json::writeString(writer, body));
        req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        req->addHeader("Authorization", "Bearer " + access_token);

        auto [result, resp] = client->sendRequest(req, 30.0);

        if (result != drogon::ReqResult::Ok || !resp) {
            return std::nullopt;
        }

        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(std::string(resp->body()), json)) {
            return std::nullopt;
        }

        return json;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Twitter API POST exception: " + std::string(e.what()),
            {{"endpoint", endpoint}}
        );
        return std::nullopt;
    }
}

bool TwitterClient::apiDelete(
    const std::string& endpoint,
    const std::string& access_token) {

    try {
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Delete);
        req->setPath(endpoint);
        req->addHeader("Authorization", "Bearer " + access_token);

        auto [result, resp] = client->sendRequest(req, 30.0);

        if (result != drogon::ReqResult::Ok || !resp) {
            return false;
        }

        Json::Value json;
        Json::Reader reader;
        if (reader.parse(std::string(resp->body()), json)) {
            return json.get("data", Json::Value())["deleted"].asBool();
        }

        return resp->getStatusCode() == drogon::k200OK;

    } catch (const std::exception& e) {
        return false;
    }
}

std::optional<Json::Value> TwitterClient::mediaUpload(
    const std::string& endpoint,
    const std::string& access_token,
    const std::map<std::string, std::string>& params) {

    // Media upload uses OAuth 1.0a - would need full implementation
    // For now, return nullopt as media upload via URL isn't directly supported

    return std::nullopt;
}

void TwitterClient::updateRateLimits(
    const std::string& endpoint,
    const std::map<std::string, std::string>& headers) {

    std::lock_guard<std::mutex> lock(mutex_);

    RateLimitInfo info;

    auto remaining_it = headers.find("x-rate-limit-remaining");
    if (remaining_it != headers.end()) {
        info.remaining = std::stoi(remaining_it->second);
    }

    auto limit_it = headers.find("x-rate-limit-limit");
    if (limit_it != headers.end()) {
        info.limit = std::stoi(limit_it->second);
    }

    auto reset_it = headers.find("x-rate-limit-reset");
    if (reset_it != headers.end()) {
        auto reset_time = std::stol(reset_it->second);
        info.reset_at = std::chrono::system_clock::from_time_t(reset_time);
    }

    rate_limits_[endpoint] = info;
}

TwitterPostResult TwitterClient::parseError(const Json::Value& response) {
    TwitterPostResult result;
    result.success = false;

    if (response.isMember("errors") && response["errors"].isArray() &&
        response["errors"].size() > 0) {
        auto& error = response["errors"][0];
        result.error_type = error.get("title", "Unknown").asString();
        result.error_detail = error.get("detail", "Unknown error").asString();

        // Check for rate limiting
        if (result.error_type == "Too Many Requests") {
            result.is_rate_limited = true;
            result.retry_after = std::chrono::seconds(900);  // 15 min default
        }
    } else if (response.isMember("detail")) {
        result.error_detail = response["detail"].asString();
        result.error_type = response.get("title", "Error").asString();
    }

    return result;
}

std::string TwitterClient::generateCodeChallenge(const std::string& verifier) {
    // SHA256 hash of verifier, then base64url encode
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(verifier.c_str()),
           verifier.length(), hash);

    // Base64url encode
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string encoded;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i += 3) {
        uint32_t chunk = (hash[i] << 16);
        if (i + 1 < SHA256_DIGEST_LENGTH) chunk |= (hash[i + 1] << 8);
        if (i + 2 < SHA256_DIGEST_LENGTH) chunk |= hash[i + 2];

        encoded += table[(chunk >> 18) & 0x3F];
        encoded += table[(chunk >> 12) & 0x3F];
        if (i + 1 < SHA256_DIGEST_LENGTH) encoded += table[(chunk >> 6) & 0x3F];
        if (i + 2 < SHA256_DIGEST_LENGTH) encoded += table[chunk & 0x3F];
    }

    return encoded;
}

std::string TwitterClient::generateRandomString(int length) {
    static const char chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);

    std::string result;
    result.reserve(length);
    for (int i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }
    return result;
}

} // namespace wtl::content::social::clients
