/**
 * @file FacebookClient.cc
 * @brief Implementation of Facebook Graph API client
 * @see FacebookClient.h for documentation
 */

#include "content/social/clients/FacebookClient.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <thread>

// Third-party includes
#include <drogon/HttpClient.h>

// Project includes
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::content::social::clients {

using namespace ::wtl::core::debug;
using namespace ::wtl::core::utils;

// ============================================================================
// FACEBOOK POST RESULT
// ============================================================================

Json::Value FacebookPostResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["post_id"] = post_id;
    json["permalink"] = permalink;
    json["error_code"] = error_code;
    json["error_message"] = error_message;
    json["error_subcode"] = error_subcode;
    json["is_rate_limited"] = is_rate_limited;
    json["retry_after_seconds"] = static_cast<int>(retry_after.count());
    return json;
}

// ============================================================================
// FACEBOOK INSIGHTS
// ============================================================================

PlatformAnalytics FacebookInsights::toPlatformAnalytics() const {
    PlatformAnalytics analytics;
    analytics.platform = Platform::FACEBOOK;
    analytics.impressions = post_impressions;
    analytics.reach = post_impressions_unique;
    analytics.profile_visits = 0;  // Not available per-post
    analytics.likes = total_reactions;
    analytics.comments = comments;
    analytics.shares = shares;
    analytics.saves = 0;  // Facebook doesn't have saves
    analytics.link_clicks = post_clicks;
    analytics.video_views = video_views;
    analytics.video_watch_time_seconds = video_avg_time_watched;
    analytics.video_completion_rate = 0.0;  // Would need video length to calculate
    analytics.new_followers = 0;  // Not per-post
    analytics.calculateEngagementRate();
    analytics.fetched_at = fetched_at;
    return analytics;
}

Json::Value FacebookInsights::toJson() const {
    Json::Value json;
    json["post_id"] = post_id;
    json["post_impressions"] = post_impressions;
    json["post_impressions_unique"] = post_impressions_unique;
    json["post_impressions_organic"] = post_impressions_organic;
    json["post_impressions_paid"] = post_impressions_paid;
    json["post_engaged_users"] = post_engaged_users;
    json["post_clicks"] = post_clicks;
    json["post_clicks_unique"] = post_clicks_unique;
    json["reactions_like"] = reactions_like;
    json["reactions_love"] = reactions_love;
    json["reactions_wow"] = reactions_wow;
    json["reactions_haha"] = reactions_haha;
    json["reactions_sorry"] = reactions_sorry;
    json["reactions_anger"] = reactions_anger;
    json["total_reactions"] = total_reactions;
    json["comments"] = comments;
    json["shares"] = shares;
    json["video_views"] = video_views;
    json["video_views_10s"] = video_views_10s;
    json["video_avg_time_watched"] = video_avg_time_watched;
    return json;
}

// ============================================================================
// FACEBOOK POST DATA
// ============================================================================

FacebookPostData FacebookPostData::fromSocialPost(const SocialPost& post) {
    FacebookPostData data;

    // Get Facebook-specific text or use primary
    auto it = post.platform_text.find(Platform::FACEBOOK);
    if (it != post.platform_text.end()) {
        data.message = it->second;
    } else {
        data.message = post.primary_text;
    }

    // Add hashtags
    std::string hashtags = post.getHashtagsForPlatform(Platform::FACEBOOK);
    if (!hashtags.empty()) {
        data.message += "\n\n" + hashtags;
    }

    // Media
    data.image_urls = post.images;
    data.video_url = post.video_url;

    // Scheduling
    data.scheduled_publish_time = post.scheduled_at;
    data.published = post.is_immediate;

    return data;
}

// ============================================================================
// FACEBOOK CLIENT - SINGLETON
// ============================================================================

FacebookClient& FacebookClient::getInstance() {
    static FacebookClient instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void FacebookClient::initialize(const std::string& app_id, const std::string& app_secret) {
    std::lock_guard<std::mutex> lock(mutex_);
    app_id_ = app_id;
    app_secret_ = app_secret;
    initialized_ = true;
}

void FacebookClient::initializeFromConfig() {
    auto& config = Config::getInstance();
    std::string app_id = config.getString("social.facebook.app_id", "");
    std::string app_secret = config.getString("social.facebook.app_secret", "");

    if (app_id.empty() || app_secret.empty()) {
        WTL_CAPTURE_WARNING(
            ErrorCategory::CONFIGURATION,
            "Facebook credentials not configured",
            {}
        );
        return;
    }

    initialize(app_id, app_secret);
}

bool FacebookClient::isInitialized() const {
    return initialized_;
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

std::string FacebookClient::getLoginUrl(
    const std::string& redirect_uri,
    const std::vector<std::string>& scopes,
    const std::string& state) {

    std::ostringstream url;
    url << "https://www.facebook.com/" << API_VERSION << "/dialog/oauth?"
        << "client_id=" << app_id_
        << "&redirect_uri=" << redirect_uri
        << "&state=" << state
        << "&scope=";

    for (size_t i = 0; i < scopes.size(); ++i) {
        if (i > 0) url << ",";
        url << scopes[i];
    }

    return url.str();
}

std::optional<std::pair<std::string, std::chrono::seconds>> FacebookClient::exchangeCodeForToken(
    const std::string& code,
    const std::string& redirect_uri) {

    std::map<std::string, std::string> params;
    params["client_id"] = app_id_;
    params["client_secret"] = app_secret_;
    params["redirect_uri"] = redirect_uri;
    params["code"] = code;

    auto response = graphGet("/oauth/access_token", "", params);
    if (!response) {
        return std::nullopt;
    }

    std::string token = (*response)["access_token"].asString();
    int expires_in = (*response).get("expires_in", 3600).asInt();

    return std::make_pair(token, std::chrono::seconds(expires_in));
}

std::optional<std::pair<std::string, std::chrono::seconds>> FacebookClient::exchangeForLongLivedToken(
    const std::string& short_token) {

    std::map<std::string, std::string> params;
    params["grant_type"] = "fb_exchange_token";
    params["client_id"] = app_id_;
    params["client_secret"] = app_secret_;
    params["fb_exchange_token"] = short_token;

    auto response = graphGet("/oauth/access_token", "", params);
    if (!response) {
        return std::nullopt;
    }

    std::string token = (*response)["access_token"].asString();
    int expires_in = (*response).get("expires_in", 5184000).asInt();  // ~60 days

    return std::make_pair(token, std::chrono::seconds(expires_in));
}

std::optional<std::string> FacebookClient::getPageAccessToken(
    const std::string& page_id,
    const std::string& user_token) {

    auto response = graphGet("/" + page_id, user_token, {{"fields", "access_token"}});
    if (!response) {
        return std::nullopt;
    }

    return (*response)["access_token"].asString();
}

std::optional<Json::Value> FacebookClient::validateToken(const std::string& token) {
    std::map<std::string, std::string> params;
    params["input_token"] = token;
    params["access_token"] = app_id_ + "|" + app_secret_;

    return graphGet("/debug_token", "", params);
}

// ============================================================================
// POSTING
// ============================================================================

FacebookPostResult FacebookClient::post(
    const std::string& page_id,
    const std::string& access_token,
    const FacebookPostData& data) {

    // Check rate limiting first
    if (isRateLimited(page_id)) {
        FacebookPostResult result;
        result.success = false;
        result.is_rate_limited = true;
        result.retry_after = getTimeUntilReset(page_id);
        result.error_message = "Rate limited. Retry after " +
                               std::to_string(result.retry_after.count()) + " seconds";
        return result;
    }

    // Determine what type of post to create
    if (data.video_url.has_value()) {
        return postVideo(page_id, access_token, *data.video_url, data.message);
    } else if (data.image_urls.size() > 1) {
        return postCarousel(page_id, access_token, data.image_urls, data.message);
    } else if (data.image_urls.size() == 1) {
        return postPhoto(page_id, access_token, data.image_urls[0], data.message);
    }

    // Text-only post
    std::map<std::string, std::string> params;
    params["message"] = data.message;

    if (!data.link.empty()) {
        params["link"] = data.link;
    }

    if (data.scheduled_publish_time.has_value()) {
        auto time_t_val = std::chrono::system_clock::to_time_t(*data.scheduled_publish_time);
        params["scheduled_publish_time"] = std::to_string(time_t_val);
        params["published"] = "false";
    }

    auto response = graphPost("/" + page_id + "/feed", access_token, params);

    FacebookPostResult result;
    if (!response) {
        result.success = false;
        result.error_message = "API request failed";
        return result;
    }

    if (response->isMember("error")) {
        return parseError(*response);
    }

    result.success = true;
    result.post_id = (*response)["id"].asString();

    // Get permalink
    auto post_details = getPost(result.post_id, access_token);
    if (post_details && post_details->isMember("permalink_url")) {
        result.permalink = (*post_details)["permalink_url"].asString();
    }

    return result;
}

FacebookPostResult FacebookClient::postPhoto(
    const std::string& page_id,
    const std::string& access_token,
    const std::string& image_url,
    const std::string& caption) {

    std::map<std::string, std::string> params;
    params["url"] = image_url;
    params["caption"] = caption;

    auto response = graphPost("/" + page_id + "/photos", access_token, params);

    FacebookPostResult result;
    if (!response) {
        result.success = false;
        result.error_message = "API request failed";
        return result;
    }

    if (response->isMember("error")) {
        return parseError(*response);
    }

    result.success = true;
    result.post_id = (*response)["post_id"].asString();

    return result;
}

FacebookPostResult FacebookClient::postCarousel(
    const std::string& page_id,
    const std::string& access_token,
    const std::vector<std::string>& image_urls,
    const std::string& caption) {

    // First, upload all photos as unpublished
    std::vector<std::string> media_fbids;

    for (const auto& url : image_urls) {
        auto upload = uploadPhoto(page_id, access_token, url, false);
        if (!upload.success) {
            FacebookPostResult result;
            result.success = false;
            result.error_message = "Failed to upload image: " + upload.error_message;
            return result;
        }
        media_fbids.push_back(upload.media_fbid);
    }

    // Create the post with attached media
    std::map<std::string, std::string> params;
    params["message"] = caption;

    // Build attached_media array
    Json::Value attached_media(Json::arrayValue);
    for (const auto& fbid : media_fbids) {
        Json::Value item;
        item["media_fbid"] = fbid;
        attached_media.append(item);
    }
    Json::StreamWriterBuilder writer;
    params["attached_media"] = Json::writeString(writer, attached_media);

    auto response = graphPost("/" + page_id + "/feed", access_token, params);

    FacebookPostResult result;
    if (!response) {
        result.success = false;
        result.error_message = "API request failed";
        return result;
    }

    if (response->isMember("error")) {
        return parseError(*response);
    }

    result.success = true;
    result.post_id = (*response)["id"].asString();

    return result;
}

FacebookPostResult FacebookClient::postVideo(
    const std::string& page_id,
    const std::string& access_token,
    const std::string& video_url,
    const std::string& description) {

    std::map<std::string, std::string> params;
    params["file_url"] = video_url;
    params["description"] = description;

    auto response = graphPost("/" + page_id + "/videos", access_token, params);

    FacebookPostResult result;
    if (!response) {
        result.success = false;
        result.error_message = "API request failed";
        return result;
    }

    if (response->isMember("error")) {
        return parseError(*response);
    }

    result.success = true;
    result.post_id = (*response)["id"].asString();

    return result;
}

void FacebookClient::postAsync(
    const std::string& page_id,
    const std::string& access_token,
    const FacebookPostData& data,
    std::function<void(const FacebookPostResult&)> callback) {

    // Use std::async for background posting
    std::thread([this, page_id, access_token, data, callback]() {
        auto result = this->post(page_id, access_token, data);
        if (callback) {
            callback(result);
        }
    }).detach();
}

// ============================================================================
// ANALYTICS
// ============================================================================

std::optional<FacebookInsights> FacebookClient::getInsights(
    const std::string& post_id,
    const std::string& access_token) {

    std::map<std::string, std::string> params;
    params["metric"] = "post_impressions,post_impressions_unique,"
                       "post_impressions_organic,post_impressions_paid,"
                       "post_engaged_users,post_clicks,post_clicks_unique";

    auto response = graphGet("/" + post_id + "/insights", access_token, params);
    if (!response) {
        return std::nullopt;
    }

    FacebookInsights insights;
    insights.post_id = post_id;
    insights.fetched_at = std::chrono::system_clock::now();

    // Parse insights data
    if (response->isMember("data")) {
        for (const auto& item : (*response)["data"]) {
            std::string name = item["name"].asString();
            int value = 0;
            if (item.isMember("values") && item["values"].size() > 0) {
                value = item["values"][0]["value"].asInt();
            }

            if (name == "post_impressions") insights.post_impressions = value;
            else if (name == "post_impressions_unique") insights.post_impressions_unique = value;
            else if (name == "post_impressions_organic") insights.post_impressions_organic = value;
            else if (name == "post_impressions_paid") insights.post_impressions_paid = value;
            else if (name == "post_engaged_users") insights.post_engaged_users = value;
            else if (name == "post_clicks") insights.post_clicks = value;
            else if (name == "post_clicks_unique") insights.post_clicks_unique = value;
        }
    }

    // Get engagement counts separately
    auto engagement = getEngagement(post_id, access_token);
    if (engagement) {
        if (engagement->isMember("reactions")) {
            insights.total_reactions = (*engagement)["reactions"]["summary"]["total_count"].asInt();
        }
        if (engagement->isMember("comments")) {
            insights.comments = (*engagement)["comments"]["summary"]["total_count"].asInt();
        }
        if (engagement->isMember("shares")) {
            insights.shares = (*engagement)["shares"]["count"].asInt();
        }
    }

    return insights;
}

std::optional<Json::Value> FacebookClient::getEngagement(
    const std::string& post_id,
    const std::string& access_token) {

    std::map<std::string, std::string> params;
    params["fields"] = "reactions.summary(true),comments.summary(true),shares";

    return graphGet("/" + post_id, access_token, params);
}

std::optional<Json::Value> FacebookClient::getPageInsights(
    const std::string& page_id,
    const std::string& access_token,
    std::chrono::system_clock::time_point since,
    std::chrono::system_clock::time_point until) {

    auto since_t = std::chrono::system_clock::to_time_t(since);
    auto until_t = std::chrono::system_clock::to_time_t(until);

    std::map<std::string, std::string> params;
    params["metric"] = "page_impressions,page_engaged_users,page_fans";
    params["since"] = std::to_string(since_t);
    params["until"] = std::to_string(until_t);

    return graphGet("/" + page_id + "/insights", access_token, params);
}

// ============================================================================
// POST MANAGEMENT
// ============================================================================

bool FacebookClient::deletePost(
    const std::string& post_id,
    const std::string& access_token) {

    return graphDelete("/" + post_id, access_token);
}

bool FacebookClient::updatePost(
    const std::string& post_id,
    const std::string& access_token,
    const std::string& new_message) {

    std::map<std::string, std::string> params;
    params["message"] = new_message;

    auto response = graphPost("/" + post_id, access_token, params);
    return response && response->isMember("success") && (*response)["success"].asBool();
}

std::optional<Json::Value> FacebookClient::getPost(
    const std::string& post_id,
    const std::string& access_token) {

    std::map<std::string, std::string> params;
    params["fields"] = "id,message,created_time,permalink_url,full_picture";

    return graphGet("/" + post_id, access_token, params);
}

// ============================================================================
// RATE LIMITING
// ============================================================================

bool FacebookClient::isRateLimited(const std::string& page_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rate_limits_.find(page_id);
    if (it == rate_limits_.end()) {
        return false;
    }
    return std::chrono::system_clock::now() < it->second.reset_at;
}

std::chrono::seconds FacebookClient::getTimeUntilReset(const std::string& page_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rate_limits_.find(page_id);
    if (it == rate_limits_.end()) {
        return std::chrono::seconds(0);
    }
    auto now = std::chrono::system_clock::now();
    if (now >= it->second.reset_at) {
        return std::chrono::seconds(0);
    }
    return std::chrono::duration_cast<std::chrono::seconds>(it->second.reset_at - now);
}

int FacebookClient::getRemainingCalls() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Return minimum remaining calls across all pages
    int min_remaining = 200;
    for (const auto& [page_id, info] : rate_limits_) {
        if (info.remaining_calls < min_remaining) {
            min_remaining = info.remaining_calls;
        }
    }
    return min_remaining;
}

// ============================================================================
// MEDIA UPLOAD
// ============================================================================

FacebookMediaUpload FacebookClient::uploadPhoto(
    const std::string& page_id,
    const std::string& access_token,
    const std::string& image_url,
    bool published) {

    std::map<std::string, std::string> params;
    params["url"] = image_url;
    params["published"] = published ? "true" : "false";

    auto response = graphPost("/" + page_id + "/photos", access_token, params);

    FacebookMediaUpload result;
    if (!response) {
        result.success = false;
        result.error_message = "API request failed";
        return result;
    }

    if (response->isMember("error")) {
        result.success = false;
        result.error_message = (*response)["error"]["message"].asString();
        return result;
    }

    result.success = true;
    result.media_fbid = (*response)["id"].asString();
    return result;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::optional<Json::Value> FacebookClient::graphGet(
    const std::string& endpoint,
    const std::string& access_token,
    const std::map<std::string, std::string>& params) {

    try {
        // Build URL with query parameters
        std::ostringstream url_stream;
        url_stream << buildUrl(endpoint) << "?";

        if (!access_token.empty()) {
            url_stream << "access_token=" << access_token << "&";
        }

        for (const auto& [key, value] : params) {
            url_stream << key << "=" << value << "&";
        }

        std::string url = url_stream.str();
        url.pop_back();  // Remove trailing '&' or '?'

        // Make HTTP request using Drogon's HttpClient
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);

        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath(url.substr(strlen(API_BASE_URL)));  // Just the path part

        auto [result, resp] = client->sendRequest(req, 30.0);  // 30 second timeout

        if (result != drogon::ReqResult::Ok || !resp) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Facebook API GET request failed",
                {{"endpoint", endpoint}}
            );
            return std::nullopt;
        }

        // Parse JSON response
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(std::string(resp->body()), json)) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Failed to parse Facebook API response",
                {{"endpoint", endpoint}}
            );
            return std::nullopt;
        }

        return json;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Facebook API exception: " + std::string(e.what()),
            {{"endpoint", endpoint}}
        );
        return std::nullopt;
    }
}

std::optional<Json::Value> FacebookClient::graphPost(
    const std::string& endpoint,
    const std::string& access_token,
    const std::map<std::string, std::string>& params) {

    try {
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);

        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setPath(buildUrl(endpoint).substr(strlen(API_BASE_URL)));

        // Add access token to params
        std::map<std::string, std::string> all_params = params;
        all_params["access_token"] = access_token;

        // Build form body
        std::ostringstream body;
        for (const auto& [key, value] : all_params) {
            if (body.tellp() > 0) body << "&";
            body << key << "=" << value;
        }

        req->setBody(body.str());
        req->setContentTypeCode(drogon::CT_APPLICATION_X_FORM);

        auto [result, resp] = client->sendRequest(req, 60.0);  // 60 second timeout for posts

        if (result != drogon::ReqResult::Ok || !resp) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Facebook API POST request failed",
                {{"endpoint", endpoint}}
            );
            return std::nullopt;
        }

        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(std::string(resp->body()), json)) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Failed to parse Facebook API response",
                {{"endpoint", endpoint}}
            );
            return std::nullopt;
        }

        // Check for rate limiting in response headers
        if (resp->getHeader("x-business-use-case-usage").length() > 0) {
            // Parse and track rate limit info
            // Header format: {"PAGE_ID":[{"call_count":X,"total_cputime":Y,...}]}
        }

        return json;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Facebook API exception: " + std::string(e.what()),
            {{"endpoint", endpoint}}
        );
        return std::nullopt;
    }
}

bool FacebookClient::graphDelete(
    const std::string& endpoint,
    const std::string& access_token) {

    try {
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);

        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Delete);

        std::string path = buildUrl(endpoint).substr(strlen(API_BASE_URL));
        path += "?access_token=" + access_token;
        req->setPath(path);

        auto [result, resp] = client->sendRequest(req, 30.0);

        if (result != drogon::ReqResult::Ok || !resp) {
            return false;
        }

        Json::Value json;
        Json::Reader reader;
        if (reader.parse(std::string(resp->body()), json)) {
            return json.get("success", false).asBool();
        }

        return false;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Facebook API DELETE exception: " + std::string(e.what()),
            {{"endpoint", endpoint}}
        );
        return false;
    }
}

void FacebookClient::handleRateLimit(const std::string& page_id, const Json::Value& response) {
    std::lock_guard<std::mutex> lock(mutex_);

    RateLimitInfo info;
    info.remaining_calls = 0;

    // Parse rate limit info from error response
    if (response.isMember("error")) {
        auto& error = response["error"];
        if (error.isMember("error_subcode") && error["error_subcode"].asInt() == 2207034) {
            // Rate limited
            info.reset_at = std::chrono::system_clock::now() + std::chrono::minutes(60);
        }
    }

    rate_limits_[page_id] = info;
}

FacebookPostResult FacebookClient::parseError(const Json::Value& response) {
    FacebookPostResult result;
    result.success = false;

    if (response.isMember("error")) {
        auto& error = response["error"];
        result.error_code = std::to_string(error.get("code", 0).asInt());
        result.error_message = error.get("message", "Unknown error").asString();
        result.error_subcode = error.get("error_subcode", 0).asInt();

        // Check for rate limiting
        if (error.get("code", 0).asInt() == 4 ||
            error.get("error_subcode", 0).asInt() == 2207034) {
            result.is_rate_limited = true;
            result.retry_after = std::chrono::seconds(3600);  // Default 1 hour
        }
    }

    return result;
}

std::string FacebookClient::buildUrl(const std::string& endpoint) const {
    std::string url = API_BASE_URL;
    url += "/";
    url += API_VERSION;
    if (endpoint[0] != '/') {
        url += "/";
    }
    url += endpoint;
    return url;
}

} // namespace wtl::content::social::clients
