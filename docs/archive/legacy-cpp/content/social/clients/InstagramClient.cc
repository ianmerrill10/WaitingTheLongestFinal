/**
 * @file InstagramClient.cc
 * @brief Implementation of Instagram Graph API client
 * @see InstagramClient.h for documentation
 */

#include "content/social/clients/InstagramClient.h"

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
// INSTAGRAM POST RESULT
// ============================================================================

Json::Value InstagramPostResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["media_id"] = media_id;
    json["permalink"] = permalink;
    json["error_code"] = error_code;
    json["error_message"] = error_message;
    json["is_rate_limited"] = is_rate_limited;
    json["retry_after_seconds"] = static_cast<int>(retry_after.count());
    return json;
}

// ============================================================================
// INSTAGRAM INSIGHTS
// ============================================================================

PlatformAnalytics InstagramInsights::toPlatformAnalytics() const {
    PlatformAnalytics analytics;
    analytics.platform = Platform::INSTAGRAM;
    analytics.impressions = impressions;
    analytics.reach = reach;
    analytics.profile_visits = 0;  // Not per-post
    analytics.likes = likes;
    analytics.comments = comments;
    analytics.shares = shares;
    analytics.saves = saves;
    analytics.link_clicks = 0;  // Instagram doesn't have clickable links in posts
    analytics.video_views = video_views;
    analytics.video_watch_time_seconds = 0;
    analytics.video_completion_rate = 0.0;
    analytics.new_followers = 0;
    analytics.calculateEngagementRate();
    analytics.fetched_at = fetched_at;
    return analytics;
}

Json::Value InstagramInsights::toJson() const {
    Json::Value json;
    json["media_id"] = media_id;
    json["impressions"] = impressions;
    json["reach"] = reach;
    json["likes"] = likes;
    json["comments"] = comments;
    json["saves"] = saves;
    json["shares"] = shares;
    json["video_views"] = video_views;
    json["plays"] = plays;
    json["exits"] = exits;
    json["replies"] = replies;
    json["taps_forward"] = taps_forward;
    json["taps_back"] = taps_back;
    json["engagement_rate"] = engagement_rate;
    return json;
}

// ============================================================================
// INSTAGRAM POST DATA
// ============================================================================

InstagramPostData InstagramPostData::fromSocialPost(const SocialPost& post) {
    InstagramPostData data;

    // Get Instagram-specific caption or use primary
    auto it = post.platform_text.find(Platform::INSTAGRAM);
    if (it != post.platform_text.end()) {
        data.caption = it->second;
    } else {
        data.caption = post.primary_text;
    }

    // Add hashtags
    std::string hashtags = post.getHashtagsForPlatform(Platform::INSTAGRAM);
    if (!hashtags.empty()) {
        data.caption += "\n.\n.\n.\n" + hashtags;  // Instagram style hashtag block
    }

    // Determine media type
    if (post.video_url.has_value()) {
        // Check if it's a Reel (vertical video)
        data.media_type = "REELS";
        data.video_url = post.video_url;
    } else if (post.images.size() > 1) {
        data.media_type = "CAROUSEL_ALBUM";
        data.carousel_image_urls = post.images;
    } else if (post.images.size() == 1) {
        data.media_type = "IMAGE";
        data.image_url = post.images[0];
    }

    return data;
}

// ============================================================================
// INSTAGRAM CLIENT - SINGLETON
// ============================================================================

InstagramClient& InstagramClient::getInstance() {
    static InstagramClient instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void InstagramClient::initialize(const std::string& app_id, const std::string& app_secret) {
    std::lock_guard<std::mutex> lock(mutex_);
    app_id_ = app_id;
    app_secret_ = app_secret;
    initialized_ = true;
}

void InstagramClient::initializeFromConfig() {
    auto& config = Config::getInstance();
    std::string app_id = config.getString("social.instagram.app_id", "");
    std::string app_secret = config.getString("social.instagram.app_secret", "");

    // Instagram uses Facebook credentials
    if (app_id.empty()) {
        app_id = config.getString("social.facebook.app_id", "");
    }
    if (app_secret.empty()) {
        app_secret = config.getString("social.facebook.app_secret", "");
    }

    if (app_id.empty() || app_secret.empty()) {
        WTL_CAPTURE_WARNING(
            ErrorCategory::CONFIGURATION,
            "Instagram/Facebook credentials not configured",
            {}
        );
        return;
    }

    initialize(app_id, app_secret);
}

bool InstagramClient::isInitialized() const {
    return initialized_;
}

// ============================================================================
// ACCOUNT MANAGEMENT
// ============================================================================

std::optional<std::string> InstagramClient::getInstagramAccountId(
    const std::string& page_id,
    const std::string& access_token) {

    auto response = graphGet("/" + page_id, access_token,
                             {{"fields", "instagram_business_account"}});
    if (!response) {
        return std::nullopt;
    }

    if (response->isMember("instagram_business_account")) {
        return (*response)["instagram_business_account"]["id"].asString();
    }

    return std::nullopt;
}

std::optional<Json::Value> InstagramClient::getAccountInfo(
    const std::string& ig_user_id,
    const std::string& access_token) {

    return graphGet("/" + ig_user_id, access_token,
                    {{"fields", "id,username,name,biography,followers_count,"
                               "follows_count,media_count,profile_picture_url"}});
}

// ============================================================================
// POSTING - PHOTOS
// ============================================================================

InstagramPostResult InstagramClient::postPhoto(
    const std::string& ig_user_id,
    const std::string& access_token,
    const std::string& image_url,
    const std::string& caption) {

    // Check rate limiting
    if (isRateLimited(ig_user_id)) {
        InstagramPostResult result;
        result.success = false;
        result.is_rate_limited = true;
        result.retry_after = getTimeUntilReset(ig_user_id);
        result.error_message = "Rate limited. Posts per day exceeded.";
        return result;
    }

    // Step 1: Create container
    auto container = createContainer(ig_user_id, access_token, "IMAGE",
                                      image_url, caption);
    if (!container) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Failed to create media container";
        return result;
    }

    // Step 2: Wait for container to be ready (images are usually instant)
    if (!waitForContainer(container->container_id, access_token, 30)) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Container preparation timeout: " + container->error_message;
        return result;
    }

    // Step 3: Publish
    auto media_id = publishContainer(ig_user_id, access_token, container->container_id);
    if (!media_id) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Failed to publish container";
        return result;
    }

    // Get permalink
    InstagramPostResult result;
    result.success = true;
    result.media_id = *media_id;

    auto media_info = graphGet("/" + *media_id, access_token, {{"fields", "permalink"}});
    if (media_info && media_info->isMember("permalink")) {
        result.permalink = (*media_info)["permalink"].asString();
    }

    return result;
}

// ============================================================================
// POSTING - CAROUSEL
// ============================================================================

InstagramPostResult InstagramClient::postCarousel(
    const std::string& ig_user_id,
    const std::string& access_token,
    const std::vector<std::string>& media_urls,
    const std::string& caption) {

    if (media_urls.size() < 2 || media_urls.size() > 10) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Carousel must have 2-10 items";
        return result;
    }

    // Check rate limiting
    if (isRateLimited(ig_user_id)) {
        InstagramPostResult result;
        result.success = false;
        result.is_rate_limited = true;
        result.retry_after = getTimeUntilReset(ig_user_id);
        result.error_message = "Rate limited. Posts per day exceeded.";
        return result;
    }

    // Step 1: Create child containers for each item
    auto child_ids = createCarouselChildren(ig_user_id, access_token, media_urls);
    if (child_ids.empty()) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Failed to create carousel children";
        return result;
    }

    // Step 2: Create carousel container
    std::map<std::string, std::string> params;
    Json::Value children_array(Json::arrayValue);
    for (const auto& id : child_ids) {
        children_array.append(id);
    }
    Json::StreamWriterBuilder writer;
    params["children"] = Json::writeString(writer, children_array);

    auto container = createContainer(ig_user_id, access_token, "CAROUSEL_ALBUM",
                                      std::nullopt, caption, params);
    if (!container) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Failed to create carousel container";
        return result;
    }

    // Step 3: Wait for container
    if (!waitForContainer(container->container_id, access_token, 60)) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Carousel preparation timeout";
        return result;
    }

    // Step 4: Publish
    auto media_id = publishContainer(ig_user_id, access_token, container->container_id);
    if (!media_id) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Failed to publish carousel";
        return result;
    }

    InstagramPostResult result;
    result.success = true;
    result.media_id = *media_id;

    auto media_info = graphGet("/" + *media_id, access_token, {{"fields", "permalink"}});
    if (media_info && media_info->isMember("permalink")) {
        result.permalink = (*media_info)["permalink"].asString();
    }

    return result;
}

// ============================================================================
// POSTING - REELS
// ============================================================================

InstagramPostResult InstagramClient::postReel(
    const std::string& ig_user_id,
    const std::string& access_token,
    const std::string& video_url,
    const std::string& caption,
    const std::optional<std::string>& cover_url) {

    // Check rate limiting
    if (isRateLimited(ig_user_id)) {
        InstagramPostResult result;
        result.success = false;
        result.is_rate_limited = true;
        result.retry_after = getTimeUntilReset(ig_user_id);
        result.error_message = "Rate limited. Posts per day exceeded.";
        return result;
    }

    // Create container with Reels-specific params
    std::map<std::string, std::string> params;
    params["media_type"] = "REELS";
    params["video_url"] = video_url;
    params["caption"] = caption;
    if (cover_url) {
        params["cover_url"] = *cover_url;
    }
    params["share_to_feed"] = "true";  // Also share to feed

    auto container = createContainer(ig_user_id, access_token, "REELS",
                                      video_url, caption, params);
    if (!container) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Failed to create Reel container";
        return result;
    }

    // Wait for video processing (can take longer)
    if (!waitForContainer(container->container_id, access_token, 300)) {  // 5 min timeout
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Reel processing timeout: " + container->error_message;
        return result;
    }

    // Publish
    auto media_id = publishContainer(ig_user_id, access_token, container->container_id);
    if (!media_id) {
        InstagramPostResult result;
        result.success = false;
        result.error_message = "Failed to publish Reel";
        return result;
    }

    InstagramPostResult result;
    result.success = true;
    result.media_id = *media_id;

    auto media_info = graphGet("/" + *media_id, access_token, {{"fields", "permalink"}});
    if (media_info && media_info->isMember("permalink")) {
        result.permalink = (*media_info)["permalink"].asString();
    }

    return result;
}

// ============================================================================
// POSTING - VIDEO
// ============================================================================

InstagramPostResult InstagramClient::postVideo(
    const std::string& ig_user_id,
    const std::string& access_token,
    const std::string& video_url,
    const std::string& caption) {

    // Regular video posts now go to Reels
    return postReel(ig_user_id, access_token, video_url, caption);
}

// ============================================================================
// POSTING - GENERAL
// ============================================================================

InstagramPostResult InstagramClient::post(
    const std::string& ig_user_id,
    const std::string& access_token,
    const InstagramPostData& data) {

    if (data.media_type == "REELS" && data.video_url) {
        return postReel(ig_user_id, access_token, *data.video_url,
                        data.caption, data.cover_url);
    } else if (data.media_type == "VIDEO" && data.video_url) {
        return postVideo(ig_user_id, access_token, *data.video_url, data.caption);
    } else if (data.media_type == "CAROUSEL_ALBUM") {
        return postCarousel(ig_user_id, access_token,
                           data.carousel_image_urls, data.caption);
    } else if (data.media_type == "IMAGE" && data.image_url) {
        return postPhoto(ig_user_id, access_token, *data.image_url, data.caption);
    }

    InstagramPostResult result;
    result.success = false;
    result.error_message = "Invalid media type or missing media URL";
    return result;
}

void InstagramClient::postAsync(
    const std::string& ig_user_id,
    const std::string& access_token,
    const InstagramPostData& data,
    std::function<void(const InstagramPostResult&)> callback) {

    std::thread([this, ig_user_id, access_token, data, callback]() {
        auto result = this->post(ig_user_id, access_token, data);
        if (callback) {
            callback(result);
        }
    }).detach();
}

// ============================================================================
// CONTAINER MANAGEMENT
// ============================================================================

std::optional<InstagramContainer> InstagramClient::createContainer(
    const std::string& ig_user_id,
    const std::string& access_token,
    const std::string& media_type,
    const std::optional<std::string>& media_url,
    const std::string& caption,
    const std::map<std::string, std::string>& additional_params) {

    std::map<std::string, std::string> params = additional_params;
    params["media_type"] = media_type;
    params["caption"] = caption;

    if (media_url) {
        if (media_type == "IMAGE") {
            params["image_url"] = *media_url;
        } else if (media_type == "VIDEO" || media_type == "REELS") {
            params["video_url"] = *media_url;
        }
    }

    auto response = graphPost("/" + ig_user_id + "/media", access_token, params);
    if (!response) {
        return std::nullopt;
    }

    if (response->isMember("error")) {
        InstagramContainer container;
        container.status = "ERROR";
        container.error_message = (*response)["error"]["message"].asString();
        return container;
    }

    InstagramContainer container;
    container.container_id = (*response)["id"].asString();
    container.status = "IN_PROGRESS";
    return container;
}

std::optional<InstagramContainer> InstagramClient::getContainerStatus(
    const std::string& container_id,
    const std::string& access_token) {

    auto response = graphGet("/" + container_id, access_token,
                             {{"fields", "status_code,status"}});
    if (!response) {
        return std::nullopt;
    }

    InstagramContainer container;
    container.container_id = container_id;
    container.status = response->get("status_code", "IN_PROGRESS").asString();

    if (response->isMember("status")) {
        container.error_message = (*response)["status"].asString();
    }

    return container;
}

std::optional<std::string> InstagramClient::publishContainer(
    const std::string& ig_user_id,
    const std::string& access_token,
    const std::string& container_id) {

    std::map<std::string, std::string> params;
    params["creation_id"] = container_id;

    auto response = graphPost("/" + ig_user_id + "/media_publish", access_token, params);
    if (!response) {
        return std::nullopt;
    }

    if (response->isMember("error")) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Instagram publish failed: " + (*response)["error"]["message"].asString(),
            {{"container_id", container_id}}
        );
        return std::nullopt;
    }

    return (*response)["id"].asString();
}

bool InstagramClient::waitForContainer(
    const std::string& container_id,
    const std::string& access_token,
    int timeout_seconds) {

    auto start = std::chrono::steady_clock::now();
    int poll_interval_ms = 1000;  // Start with 1 second

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start);

        if (elapsed.count() >= timeout_seconds) {
            return false;
        }

        auto status = getContainerStatus(container_id, access_token);
        if (!status) {
            return false;
        }

        if (status->status == "FINISHED") {
            return true;
        }

        if (status->status == "ERROR") {
            return false;
        }

        // Wait before next poll (with exponential backoff up to 5 seconds)
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
        poll_interval_ms = std::min(poll_interval_ms * 2, 5000);
    }
}

// ============================================================================
// ANALYTICS
// ============================================================================

std::optional<InstagramInsights> InstagramClient::getInsights(
    const std::string& media_id,
    const std::string& access_token) {

    // Get basic engagement
    auto media_info = graphGet("/" + media_id, access_token,
                               {{"fields", "like_count,comments_count,media_type"}});
    if (!media_info) {
        return std::nullopt;
    }

    InstagramInsights insights;
    insights.media_id = media_id;
    insights.fetched_at = std::chrono::system_clock::now();

    insights.likes = media_info->get("like_count", 0).asInt();
    insights.comments = media_info->get("comments_count", 0).asInt();

    std::string media_type = media_info->get("media_type", "IMAGE").asString();

    // Get detailed insights
    std::string metrics = "impressions,reach,saved";
    if (media_type == "VIDEO" || media_type == "REELS") {
        metrics += ",plays,total_interactions";
    }

    auto insights_response = graphGet("/" + media_id + "/insights", access_token,
                                       {{"metric", metrics}});

    if (insights_response && insights_response->isMember("data")) {
        for (const auto& item : (*insights_response)["data"]) {
            std::string name = item["name"].asString();
            int value = 0;
            if (item.isMember("values") && item["values"].size() > 0) {
                value = item["values"][0]["value"].asInt();
            }

            if (name == "impressions") insights.impressions = value;
            else if (name == "reach") insights.reach = value;
            else if (name == "saved") insights.saves = value;
            else if (name == "plays") {
                insights.plays = value;
                insights.video_views = value;
            }
        }
    }

    // Calculate engagement rate
    if (insights.reach > 0) {
        int total_engagement = insights.likes + insights.comments + insights.saves + insights.shares;
        insights.engagement_rate = static_cast<double>(total_engagement) / insights.reach * 100.0;
    }

    return insights;
}

std::optional<Json::Value> InstagramClient::getAccountInsights(
    const std::string& ig_user_id,
    const std::string& access_token,
    std::chrono::system_clock::time_point since,
    std::chrono::system_clock::time_point until) {

    auto since_t = std::chrono::system_clock::to_time_t(since);
    auto until_t = std::chrono::system_clock::to_time_t(until);

    std::map<std::string, std::string> params;
    params["metric"] = "impressions,reach,follower_count,profile_views";
    params["period"] = "day";
    params["since"] = std::to_string(since_t);
    params["until"] = std::to_string(until_t);

    return graphGet("/" + ig_user_id + "/insights", access_token, params);
}

std::optional<Json::Value> InstagramClient::getMediaList(
    const std::string& ig_user_id,
    const std::string& access_token,
    int limit) {

    std::map<std::string, std::string> params;
    params["fields"] = "id,caption,media_type,media_url,permalink,timestamp,"
                       "like_count,comments_count";
    params["limit"] = std::to_string(limit);

    return graphGet("/" + ig_user_id + "/media", access_token, params);
}

// ============================================================================
// RATE LIMITING
// ============================================================================

bool InstagramClient::isRateLimited(const std::string& ig_user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rate_limits_.find(ig_user_id);
    if (it == rate_limits_.end()) {
        return false;
    }
    return it->second.remaining_posts <= 0 &&
           std::chrono::system_clock::now() < it->second.reset_at;
}

std::chrono::seconds InstagramClient::getTimeUntilReset(const std::string& ig_user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rate_limits_.find(ig_user_id);
    if (it == rate_limits_.end()) {
        return std::chrono::seconds(0);
    }
    auto now = std::chrono::system_clock::now();
    if (now >= it->second.reset_at) {
        return std::chrono::seconds(0);
    }
    return std::chrono::duration_cast<std::chrono::seconds>(it->second.reset_at - now);
}

std::optional<Json::Value> InstagramClient::getContentPublishingLimit(
    const std::string& ig_user_id,
    const std::string& access_token) {

    return graphGet("/" + ig_user_id + "/content_publishing_limit", access_token,
                    {{"fields", "config,quota_usage"}});
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::optional<Json::Value> InstagramClient::graphGet(
    const std::string& endpoint,
    const std::string& access_token,
    const std::map<std::string, std::string>& params) {

    try {
        std::ostringstream url_stream;
        url_stream << buildUrl(endpoint) << "?";

        if (!access_token.empty()) {
            url_stream << "access_token=" << access_token << "&";
        }

        for (const auto& [key, value] : params) {
            url_stream << key << "=" << value << "&";
        }

        std::string url = url_stream.str();
        url.pop_back();

        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath(url.substr(strlen(API_BASE_URL)));

        auto [result, resp] = client->sendRequest(req, 30.0);

        if (result != drogon::ReqResult::Ok || !resp) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Instagram API GET request failed",
                {{"endpoint", endpoint}}
            );
            return std::nullopt;
        }

        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(std::string(resp->body()), json)) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Failed to parse Instagram API response",
                {{"endpoint", endpoint}}
            );
            return std::nullopt;
        }

        return json;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Instagram API exception: " + std::string(e.what()),
            {{"endpoint", endpoint}}
        );
        return std::nullopt;
    }
}

std::optional<Json::Value> InstagramClient::graphPost(
    const std::string& endpoint,
    const std::string& access_token,
    const std::map<std::string, std::string>& params) {

    try {
        auto client = drogon::HttpClient::newHttpClient(API_BASE_URL);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setPath(buildUrl(endpoint).substr(strlen(API_BASE_URL)));

        std::map<std::string, std::string> all_params = params;
        all_params["access_token"] = access_token;

        std::ostringstream body;
        for (const auto& [key, value] : all_params) {
            if (body.tellp() > 0) body << "&";
            body << key << "=" << value;
        }

        req->setBody(body.str());
        req->setContentTypeCode(drogon::CT_APPLICATION_X_FORM);

        auto [result, resp] = client->sendRequest(req, 120.0);  // Longer timeout for uploads

        if (result != drogon::ReqResult::Ok || !resp) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Instagram API POST request failed",
                {{"endpoint", endpoint}}
            );
            return std::nullopt;
        }

        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(std::string(resp->body()), json)) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Failed to parse Instagram API response",
                {{"endpoint", endpoint}}
            );
            return std::nullopt;
        }

        return json;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Instagram API exception: " + std::string(e.what()),
            {{"endpoint", endpoint}}
        );
        return std::nullopt;
    }
}

InstagramPostResult InstagramClient::parseError(const Json::Value& response) {
    InstagramPostResult result;
    result.success = false;

    if (response.isMember("error")) {
        auto& error = response["error"];
        result.error_code = std::to_string(error.get("code", 0).asInt());
        result.error_message = error.get("message", "Unknown error").asString();

        // Check for rate limiting
        if (error.get("code", 0).asInt() == 4) {
            result.is_rate_limited = true;
            result.retry_after = std::chrono::seconds(86400);  // 24 hours
        }
    }

    return result;
}

std::string InstagramClient::buildUrl(const std::string& endpoint) const {
    std::string url = API_BASE_URL;
    url += "/";
    url += API_VERSION;
    if (endpoint[0] != '/') {
        url += "/";
    }
    url += endpoint;
    return url;
}

std::vector<std::string> InstagramClient::createCarouselChildren(
    const std::string& ig_user_id,
    const std::string& access_token,
    const std::vector<std::string>& media_urls) {

    std::vector<std::string> child_ids;

    for (const auto& url : media_urls) {
        // Determine if it's an image or video based on URL
        bool is_video = url.find(".mp4") != std::string::npos ||
                        url.find(".mov") != std::string::npos;

        std::map<std::string, std::string> params;
        params["is_carousel_item"] = "true";

        if (is_video) {
            params["media_type"] = "VIDEO";
            params["video_url"] = url;
        } else {
            params["image_url"] = url;
        }

        auto response = graphPost("/" + ig_user_id + "/media", access_token, params);
        if (!response || response->isMember("error")) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::EXTERNAL_API,
                "Failed to create carousel child",
                {{"url", url}}
            );
            continue;  // Skip failed items
        }

        child_ids.push_back((*response)["id"].asString());
    }

    return child_ids;
}

} // namespace wtl::content::social::clients
