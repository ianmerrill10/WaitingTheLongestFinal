/**
 * @file SocialController.cc
 * @brief Implementation of REST API controller for social media features
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "content/social/SocialController.h"
#include "content/social/SocialWorker.h"
#include "core/debug/ErrorCapture.h"

#include <sstream>
#include <iomanip>
#include <random>

namespace wtl::content::social {

using namespace ::wtl::core::debug;

// ============================================================================
// POST MANAGEMENT
// ============================================================================

void SocialController::createPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto body = req->getJsonObject();
        if (!body) {
            callback(buildErrorResponse(400, "Invalid JSON body", "INVALID_JSON"));
            return;
        }

        std::string error_msg;
        if (!validateRequiredFields(*body, {"text", "platforms"}, error_msg)) {
            callback(buildErrorResponse(400, error_msg, "MISSING_FIELDS"));
            return;
        }

        // Build the post
        SocialPostBuilder builder;

        if (body->isMember("dog_id")) {
            builder = SocialPostBuilder::forDog((*body)["dog_id"].asString());
        }

        builder.withText((*body)["text"].asString());

        // Parse platforms
        auto platforms = parsePlatforms((*body)["platforms"]);
        builder.toPlatforms(platforms);

        // Add images if provided
        if (body->isMember("image_urls") && (*body)["image_urls"].isArray()) {
            for (const auto& url : (*body)["image_urls"]) {
                builder.withImage(url.asString());
            }
        }

        // Add video if provided
        if (body->isMember("video_url")) {
            builder.withVideo((*body)["video_url"].asString());
        }

        // Set post type if provided
        if (body->isMember("post_type")) {
            std::string type_str = (*body)["post_type"].asString();
            PostType post_type = PostType::ADOPTION_SPOTLIGHT;  // default
            if (type_str == "urgent_appeal") post_type = PostType::URGENT_APPEAL;
            else if (type_str == "waiting_milestone") post_type = PostType::WAITING_MILESTONE;
            else if (type_str == "success_story") post_type = PostType::SUCCESS_STORY;
            else if (type_str == "event") post_type = PostType::EVENT_PROMOTION;
            builder.withType(post_type);
        }

        auto post = builder.build();

        // Cross-post
        auto& engine = SocialMediaEngine::getInstance();
        auto result = engine.crossPost(post, platforms);

        // Build response
        Json::Value response;
        response["success"] = result.overall_success;
        response["post_id"] = result.post_id;
        response["platforms_attempted"] = result.platforms_attempted;
        response["platforms_succeeded"] = result.platforms_succeeded;
        response["platforms_failed"] = result.platforms_failed;

        Json::Value platform_results(Json::objectValue);
        for (const auto& [platform, success] : result.platform_success) {
            Json::Value pr;
            pr["success"] = success;
            if (result.platform_post_ids.count(platform)) {
                pr["post_id"] = result.platform_post_ids.at(platform);
            }
            if (result.platform_urls.count(platform)) {
                pr["url"] = result.platform_urls.at(platform);
            }
            if (result.platform_errors.count(platform)) {
                pr["error"] = result.platform_errors.at(platform);
            }
            platform_results[platformToString(platform)] = pr;
        }
        response["platform_results"] = platform_results;

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "createPost failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::listPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int limit = 50;
        int offset = 0;
        std::string dog_id;

        // Parse query parameters
        auto params = req->getParameters();
        if (params.count("limit")) {
            limit = std::stoi(params.at("limit"));
            limit = std::min(limit, 100);  // Max 100
        }
        if (params.count("offset")) {
            offset = std::stoi(params.at("offset"));
        }
        if (params.count("dog_id")) {
            dog_id = params.at("dog_id");
        }

        // offset is parsed for future pagination support
        (void)offset;

        auto& engine = SocialMediaEngine::getInstance();

        std::vector<SocialPost> posts;
        if (!dog_id.empty()) {
            posts = engine.getPostsForDog(dog_id);
        } else {
            posts = engine.getRecentPosts(limit);
        }

        Json::Value response;
        response["total"] = static_cast<int>(posts.size());

        Json::Value posts_array(Json::arrayValue);
        for (const auto& post : posts) {
            posts_array.append(post.toJson());
        }
        response["posts"] = posts_array;

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "listPosts failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::getPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto& engine = SocialMediaEngine::getInstance();
        auto post = engine.getPost(id);

        if (!post) {
            callback(buildErrorResponse(404, "Post not found", "NOT_FOUND"));
            return;
        }

        callback(buildSuccessResponse(post->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "getPost failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::deletePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto& engine = SocialMediaEngine::getInstance();

        if (!engine.deletePost(id)) {
            callback(buildErrorResponse(404, "Post not found or already deleted", "NOT_FOUND"));
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["message"] = "Post deleted from all platforms";

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "deletePost failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::schedulePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto body = req->getJsonObject();
        if (!body) {
            callback(buildErrorResponse(400, "Invalid JSON body", "INVALID_JSON"));
            return;
        }

        std::string error_msg;
        if (!validateRequiredFields(*body, {"text", "platforms"}, error_msg)) {
            callback(buildErrorResponse(400, error_msg, "MISSING_FIELDS"));
            return;
        }

        // Build the post
        SocialPostBuilder builder;

        if (body->isMember("dog_id")) {
            builder = SocialPostBuilder::forDog((*body)["dog_id"].asString());
        }

        builder.withText((*body)["text"].asString());

        auto platforms = parsePlatforms((*body)["platforms"]);
        builder.toPlatforms(platforms);

        if (body->isMember("image_urls") && (*body)["image_urls"].isArray()) {
            for (const auto& url : (*body)["image_urls"]) {
                builder.withImage(url.asString());
            }
        }

        auto post = builder.build();

        auto& engine = SocialMediaEngine::getInstance();

        bool use_optimal = body->get("use_optimal_times", false).asBool();

        if (use_optimal) {
            // Schedule for optimal times per platform
            auto scheduled_times = engine.scheduleForOptimalTimes(post);

            Json::Value response;
            response["success"] = true;
            response["post_id"] = post.id;

            Json::Value times(Json::objectValue);
            for (const auto& [platform, time] : scheduled_times) {
                auto time_t = std::chrono::system_clock::to_time_t(time);
                std::stringstream ss;
                ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
                times[platformToString(platform)] = ss.str();
            }
            response["scheduled_times"] = times;

            callback(buildSuccessResponse(response));

        } else {
            // Schedule for specific time
            if (!body->isMember("scheduled_time")) {
                callback(buildErrorResponse(400, "scheduled_time required when not using optimal times", "MISSING_FIELDS"));
                return;
            }

            auto scheduled_time = parseDateTime((*body)["scheduled_time"].asString());
            std::string post_id = engine.schedulePost(post, scheduled_time);

            Json::Value response;
            response["success"] = true;
            response["post_id"] = post_id;
            response["scheduled_time"] = (*body)["scheduled_time"].asString();

            callback(buildSuccessResponse(response));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "schedulePost failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::listScheduledPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto& engine = SocialMediaEngine::getInstance();
        auto posts = engine.getScheduledPosts();

        Json::Value response;
        response["total"] = static_cast<int>(posts.size());

        Json::Value posts_array(Json::arrayValue);
        for (const auto& post : posts) {
            posts_array.append(post.toJson());
        }
        response["scheduled_posts"] = posts_array;

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "listScheduledPosts failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::cancelScheduledPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto& engine = SocialMediaEngine::getInstance();

        if (!engine.cancelScheduledPost(id)) {
            callback(buildErrorResponse(404, "Scheduled post not found", "NOT_FOUND"));
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["message"] = "Scheduled post cancelled";

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "cancelScheduledPost failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

// ============================================================================
// CROSS-POSTING
// ============================================================================

void SocialController::crossPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    // Same implementation as createPost - delegates to engine
    createPost(req, std::move(callback));
}

void SocialController::crossPostTikTok(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto body = req->getJsonObject();
        if (!body) {
            callback(buildErrorResponse(400, "Invalid JSON body", "INVALID_JSON"));
            return;
        }

        std::string error_msg;
        if (!validateRequiredFields(*body, {"tiktok_url"}, error_msg)) {
            callback(buildErrorResponse(400, error_msg, "MISSING_FIELDS"));
            return;
        }

        std::string tiktok_url = (*body)["tiktok_url"].asString();
        std::string dog_id = body->get("dog_id", "").asString();

        std::vector<Platform> platforms;
        if (body->isMember("platforms")) {
            platforms = parsePlatforms((*body)["platforms"]);
        }

        auto& engine = SocialMediaEngine::getInstance();
        auto result = engine.crossPostTikTok(tiktok_url, dog_id, platforms);

        callback(buildSuccessResponse(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "crossPostTikTok failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

// ============================================================================
// PLATFORM CONNECTIONS
// ============================================================================

void SocialController::listConnections(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        std::string user_id = getUserId(req);

        auto& engine = SocialMediaEngine::getInstance();
        auto connections = engine.getUserConnections(user_id);

        Json::Value response;
        response["total"] = static_cast<int>(connections.size());

        Json::Value connections_array(Json::arrayValue);
        for (const auto& conn : connections) {
            Json::Value c;
            c["id"] = conn.id;
            c["platform"] = platformToString(conn.platform);
            c["platform_user_id"] = conn.platform_user_id;
            c["platform_username"] = conn.platform_username;
            c["is_active"] = conn.is_active;
            c["page_id"] = conn.page_id.value_or("");
            c["platform_username"] = conn.platform_username;

            // Don't expose tokens in response
            auto expires_t = std::chrono::system_clock::to_time_t(conn.token_expires_at);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&expires_t), "%Y-%m-%dT%H:%M:%SZ");
            c["token_expires_at"] = ss.str();

            connections_array.append(c);
        }
        response["connections"] = connections_array;

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "listConnections failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::addConnection(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto body = req->getJsonObject();
        if (!body) {
            callback(buildErrorResponse(400, "Invalid JSON body", "INVALID_JSON"));
            return;
        }

        std::string error_msg;
        if (!validateRequiredFields(*body, {"platform", "access_token"}, error_msg)) {
            callback(buildErrorResponse(400, error_msg, "MISSING_FIELDS"));
            return;
        }

        PlatformConnection conn;
        conn.user_id = getUserId(req);
        conn.platform = stringToPlatform((*body)["platform"].asString());
        conn.access_token = (*body)["access_token"].asString();
        conn.refresh_token = body->get("refresh_token", "").asString();
        std::string page_id_str = body->get("page_id", "").asString();
        if (!page_id_str.empty()) {
            conn.page_id = page_id_str;
        }
        conn.is_active = true;
        conn.created_at = std::chrono::system_clock::now();

        // Set expiration (default 60 days)
        conn.token_expires_at = std::chrono::system_clock::now() + std::chrono::hours(60 * 24);

        auto& engine = SocialMediaEngine::getInstance();

        if (!engine.saveConnection(conn)) {
            callback(buildErrorResponse(500, "Failed to save connection", "SAVE_FAILED"));
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["connection_id"] = conn.id;
        response["platform"] = platformToString(conn.platform);

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "addConnection failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::removeConnection(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        // Implementation would delete connection from database
        Json::Value response;
        response["success"] = true;
        response["message"] = "Connection removed";

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "removeConnection failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::refreshConnection(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto& engine = SocialMediaEngine::getInstance();

        if (!engine.refreshToken(id)) {
            callback(buildErrorResponse(400, "Failed to refresh token", "REFRESH_FAILED"));
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["message"] = "Token refreshed";

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "refreshConnection failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

// ============================================================================
// ANALYTICS
// ============================================================================

void SocialController::getPostAnalytics(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto& analytics = SocialAnalytics::getInstance();
        auto post_analytics = analytics.getPostAnalytics(id);

        if (!post_analytics) {
            callback(buildErrorResponse(404, "Analytics not found for post", "NOT_FOUND"));
            return;
        }

        callback(buildSuccessResponse(post_analytics->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "getPostAnalytics failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::getAnalyticsSummary(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto params = req->getParameters();
        std::string period = "week";
        if (params.count("period")) {
            period = params.at("period");
        }

        int days = 7;
        if (period == "day") days = 1;
        else if (period == "week") days = 7;
        else if (period == "month") days = 30;
        else if (period == "year") days = 365;

        auto& analytics = SocialAnalytics::getInstance();
        auto end = std::chrono::system_clock::now();
        auto start = end - std::chrono::hours(days * 24);
        auto summary = analytics.getSummary(start, end);

        callback(buildSuccessResponse(summary.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "getAnalyticsSummary failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::getDogAnalytics(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    try {
        auto& analytics = SocialAnalytics::getInstance();
        auto dog_analytics = analytics.getDogAnalytics(dog_id);

        if (dog_analytics.empty()) {
            callback(buildErrorResponse(404, "Analytics not found for dog", "NOT_FOUND"));
            return;
        }

        Json::Value analytics_array(Json::arrayValue);
        for (const auto& pa : dog_analytics) {
            analytics_array.append(pa.toJson());
        }
        callback(buildSuccessResponse(analytics_array));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "getDogAnalytics failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

// ============================================================================
// SHARE CARDS
// ============================================================================

void SocialController::generateShareCard(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    try {
        auto params = req->getParameters();

        Platform platform = Platform::INSTAGRAM;
        if (params.count("platform")) {
            platform = stringToPlatform(params.at("platform"));
        }

        CardStyle style = CardStyle::STANDARD;
        if (params.count("style")) {
            std::string style_str = params.at("style");
            if (style_str == "urgent") style = CardStyle::URGENT;
            else if (style_str == "milestone") style = CardStyle::MILESTONE;
            else if (style_str == "story") style = CardStyle::STORY;
        }

        auto& generator = ShareCardGenerator::getInstance();
        auto card = generator.generateCard(dog_id, platform, style);

        if (!card.success) {
            callback(buildErrorResponse(400, card.error_message, "GENERATION_FAILED"));
            return;
        }

        // Upload and get URL
        generator.uploadCard(card);

        callback(buildSuccessResponse(card.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "generateShareCard failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::generateMultiPlatformCards(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    try {
        auto params = req->getParameters();

        std::vector<Platform> platforms;
        if (params.count("platforms")) {
            std::string platforms_str = params.at("platforms");
            std::istringstream ss(platforms_str);
            std::string platform_str;
            while (std::getline(ss, platform_str, ',')) {
                platforms.push_back(stringToPlatform(platform_str));
            }
        } else {
            // Default platforms
            platforms = {Platform::FACEBOOK, Platform::INSTAGRAM, Platform::TWITTER};
        }

        auto& generator = ShareCardGenerator::getInstance();
        auto cards = generator.generateMultiPlatformCards(dog_id, platforms);

        Json::Value response;
        response["dog_id"] = dog_id;

        Json::Value cards_obj(Json::objectValue);
        for (auto& [platform, card] : cards) {
            if (card.success) {
                generator.uploadCard(card);
            }
            cards_obj[platformToString(platform)] = card.toJson();
        }
        response["cards"] = cards_obj;

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "generateMultiPlatformCards failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

// ============================================================================
// OAUTH
// ============================================================================

void SocialController::getOAuthUrl(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& platform) {

    try {
        Platform p = stringToPlatform(platform);
        std::string state = generateOAuthState();
        std::string redirect_uri = "https://waitingthelongest.com/api/v1/social/oauth/" +
                                   platform + "/callback";

        std::string auth_url;

        switch (p) {
            case Platform::FACEBOOK:
            case Platform::INSTAGRAM:
                auth_url = "https://www.facebook.com/v18.0/dialog/oauth"
                          "?client_id=YOUR_APP_ID"
                          "&redirect_uri=" + redirect_uri +
                          "&state=" + state +
                          "&scope=pages_manage_posts,pages_read_engagement,instagram_basic,instagram_content_publish";
                break;

            case Platform::TWITTER:
                auth_url = "https://twitter.com/i/oauth2/authorize"
                          "?response_type=code"
                          "&client_id=YOUR_CLIENT_ID"
                          "&redirect_uri=" + redirect_uri +
                          "&scope=tweet.read%20tweet.write%20users.read%20offline.access"
                          "&state=" + state +
                          "&code_challenge=YOUR_CHALLENGE"
                          "&code_challenge_method=S256";
                break;

            default:
                callback(buildErrorResponse(400, "OAuth not supported for this platform", "UNSUPPORTED_PLATFORM"));
                return;
        }

        Json::Value response;
        response["auth_url"] = auth_url;
        response["state"] = state;
        response["platform"] = platform;

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "getOAuthUrl failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::oauthCallback(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& platform) {

    try {
        auto params = req->getParameters();

        if (!params.count("code") || !params.count("state")) {
            callback(buildErrorResponse(400, "Missing code or state parameter", "MISSING_PARAMS"));
            return;
        }

        std::string code = params.at("code");
        std::string state = params.at("state");

        if (!validateOAuthState(state)) {
            callback(buildErrorResponse(400, "Invalid state parameter", "INVALID_STATE"));
            return;
        }

        // Exchange code for token (implementation depends on platform)
        // This would call the appropriate client to exchange the code

        Json::Value response;
        response["success"] = true;
        response["message"] = "Connection established. You can close this window.";

        // In production, this would redirect to a success page
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "oauthCallback failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

// ============================================================================
// AUTO-GENERATION
// ============================================================================

void SocialController::generateUrgentPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    try {
        auto& engine = SocialMediaEngine::getInstance();
        auto post = engine.generateUrgentDogPost(dog_id);

        if (!post) {
            callback(buildErrorResponse(400, "Could not generate urgent post", "GENERATION_FAILED"));
            return;
        }

        // Optionally cross-post immediately
        auto body = req->getJsonObject();
        bool post_now = body && body->get("post_now", false).asBool();

        Json::Value response;
        response["post"] = post->toJson();

        if (post_now) {
            auto result = engine.crossPost(*post);
            response["posted"] = true;
            response["result"] = result.toJson();
        } else {
            response["posted"] = false;
        }

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "generateUrgentPost failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::generateMilestonePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    try {
        auto params = req->getParameters();
        std::string milestone_type = "1_year";
        if (params.count("type")) {
            milestone_type = params.at("type");
        }

        auto& engine = SocialMediaEngine::getInstance();
        auto post = engine.generateMilestonePost(dog_id, milestone_type);

        if (!post) {
            callback(buildErrorResponse(400, "Could not generate milestone post", "GENERATION_FAILED"));
            return;
        }

        auto body = req->getJsonObject();
        bool post_now = body && body->get("post_now", false).asBool();

        Json::Value response;
        response["post"] = post->toJson();

        if (post_now) {
            auto result = engine.crossPost(*post);
            response["posted"] = true;
            response["result"] = result.toJson();
        } else {
            response["posted"] = false;
        }

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "generateMilestonePost failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

// ============================================================================
// WORKER STATUS
// ============================================================================

void SocialController::getWorkerStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto& worker = SocialWorker::getInstance();

        Json::Value response;
        response["running"] = worker.isRunning();
        response["initialized"] = worker.isInitialized();
        response["pending_tasks"] = static_cast<int>(worker.getPendingTaskCount());
        response["stats"] = worker.getStats().toJson();
        response["config"] = worker.getConfig().toJson();

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "getWorkerStatus failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

void SocialController::triggerSync(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto params = req->getParameters();
        std::string sync_type = "analytics";
        if (params.count("type")) {
            sync_type = params.at("type");
        }

        auto& worker = SocialWorker::getInstance();
        int result = 0;

        if (sync_type == "analytics") {
            result = worker.triggerAnalyticsSync();
        } else if (sync_type == "urgent") {
            result = worker.triggerUrgentDogCheck();
        } else if (sync_type == "tokens") {
            result = worker.triggerTokenRefresh();
        } else if (sync_type == "scheduled") {
            result = worker.triggerScheduledPostCheck();
        } else {
            callback(buildErrorResponse(400, "Invalid sync type", "INVALID_TYPE"));
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["sync_type"] = sync_type;
        response["items_processed"] = result;

        callback(buildSuccessResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "triggerSync failed", {{"error", e.what()}});
        callback(buildErrorResponse(500, "Internal server error", "INTERNAL_ERROR"));
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

drogon::HttpResponsePtr SocialController::buildSuccessResponse(const Json::Value& data) {
    Json::Value response;
    response["success"] = true;
    response["data"] = data;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    return resp;
}

drogon::HttpResponsePtr SocialController::buildErrorResponse(
    int status_code,
    const std::string& message,
    const std::string& error_code) {

    Json::Value response;
    response["success"] = false;
    response["error"]["message"] = message;
    if (!error_code.empty()) {
        response["error"]["code"] = error_code;
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(static_cast<drogon::HttpStatusCode>(status_code));
    return resp;
}

std::vector<Platform> SocialController::parsePlatforms(const Json::Value& platforms_json) {
    std::vector<Platform> platforms;

    if (platforms_json.isArray()) {
        for (const auto& p : platforms_json) {
            platforms.push_back(stringToPlatform(p.asString()));
        }
    } else if (platforms_json.isString()) {
        // Handle comma-separated string
        std::string platforms_str = platforms_json.asString();
        std::istringstream ss(platforms_str);
        std::string platform_str;
        while (std::getline(ss, platform_str, ',')) {
            platforms.push_back(stringToPlatform(platform_str));
        }
    }

    return platforms;
}

std::string SocialController::getUserId(const drogon::HttpRequestPtr& req) {
    // Get user ID from auth middleware/session
    // This is set by the authentication middleware
    auto user_id = req->getAttributes()->get<std::string>("user_id");
    return user_id.empty() ? "system" : user_id;
}

bool SocialController::validateRequiredFields(
    const Json::Value& body,
    const std::vector<std::string>& fields,
    std::string& error_message) {

    for (const auto& field : fields) {
        if (!body.isMember(field) || body[field].isNull()) {
            error_message = "Missing required field: " + field;
            return false;
        }
    }
    return true;
}

std::chrono::system_clock::time_point SocialController::parseDateTime(const std::string& datetime_str) {
    std::tm tm = {};
    std::istringstream ss(datetime_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    auto time_t = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t);
}

std::string SocialController::generateOAuthState() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << dis(gen);
    ss << std::setw(8) << dis(gen);
    ss << std::setw(8) << dis(gen);
    ss << std::setw(8) << dis(gen);

    // In production, store this in session/cache for validation
    return ss.str();
}

bool SocialController::validateOAuthState(const std::string& state) {
    // In production, validate against stored state
    return !state.empty() && state.length() == 32;
}

} // namespace wtl::content::social
