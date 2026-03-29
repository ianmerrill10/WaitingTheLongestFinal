/**
 * @file SocialController.h
 * @brief REST API controller for social media features
 *
 * PURPOSE:
 * The SocialController exposes REST API endpoints for social media operations:
 * - Cross-posting content to multiple platforms
 * - Managing platform connections (OAuth)
 * - Viewing and managing posts
 * - Accessing analytics data
 * - Generating share cards
 * - Scheduling posts
 *
 * ENDPOINTS:
 * POST   /api/v1/social/posts             - Create and post to social media
 * GET    /api/v1/social/posts             - List recent posts
 * GET    /api/v1/social/posts/{id}        - Get post details
 * DELETE /api/v1/social/posts/{id}        - Delete a post
 * POST   /api/v1/social/posts/schedule    - Schedule a post
 * GET    /api/v1/social/posts/scheduled   - List scheduled posts
 *
 * POST   /api/v1/social/crosspost         - Cross-post to multiple platforms
 * POST   /api/v1/social/tiktok/crosspost  - Cross-post TikTok video
 *
 * GET    /api/v1/social/connections       - List platform connections
 * POST   /api/v1/social/connections       - Add new connection
 * DELETE /api/v1/social/connections/{id}  - Remove connection
 * POST   /api/v1/social/connections/{id}/refresh - Refresh token
 *
 * GET    /api/v1/social/analytics/{id}    - Get post analytics
 * GET    /api/v1/social/analytics/summary - Get analytics summary
 *
 * GET    /api/v1/social/share-card/{dog_id} - Generate share card
 * GET    /api/v1/social/share-card/{dog_id}/multi - Generate for multiple platforms
 *
 * GET    /api/v1/social/oauth/{platform}/url    - Get OAuth URL
 * GET    /api/v1/social/oauth/{platform}/callback - OAuth callback
 *
 * DEPENDENCIES:
 * - SocialMediaEngine (main posting)
 * - SocialAnalytics (analytics)
 * - ShareCardGenerator (share cards)
 * - AuthMiddleware (authentication)
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>
#include <vector>

// Third-party includes
#include <drogon/HttpController.h>
#include <json/json.h>

// Project includes
#include "content/social/Platform.h"
#include "content/social/SocialPost.h"
#include "content/social/SocialMediaEngine.h"
#include "content/social/SocialAnalytics.h"
#include "content/social/ShareCardGenerator.h"

namespace wtl::content::social {

/**
 * @class SocialController
 * @brief REST API controller for social media features
 */
class SocialController : public drogon::HttpController<SocialController> {
public:
    // ========================================================================
    // ROUTE REGISTRATION
    // ========================================================================

    METHOD_LIST_BEGIN

    // Posts
    ADD_METHOD_TO(SocialController::createPost, "/api/v1/social/posts", drogon::Post);
    ADD_METHOD_TO(SocialController::listPosts, "/api/v1/social/posts", drogon::Get);
    ADD_METHOD_TO(SocialController::getPost, "/api/v1/social/posts/{id}", drogon::Get);
    ADD_METHOD_TO(SocialController::deletePost, "/api/v1/social/posts/{id}", drogon::Delete);
    ADD_METHOD_TO(SocialController::schedulePost, "/api/v1/social/posts/schedule", drogon::Post);
    ADD_METHOD_TO(SocialController::listScheduledPosts, "/api/v1/social/posts/scheduled", drogon::Get);
    ADD_METHOD_TO(SocialController::cancelScheduledPost, "/api/v1/social/posts/scheduled/{id}", drogon::Delete);

    // Cross-posting
    ADD_METHOD_TO(SocialController::crossPost, "/api/v1/social/crosspost", drogon::Post);
    ADD_METHOD_TO(SocialController::crossPostTikTok, "/api/v1/social/tiktok/crosspost", drogon::Post);

    // Connections
    ADD_METHOD_TO(SocialController::listConnections, "/api/v1/social/connections", drogon::Get);
    ADD_METHOD_TO(SocialController::addConnection, "/api/v1/social/connections", drogon::Post);
    ADD_METHOD_TO(SocialController::removeConnection, "/api/v1/social/connections/{id}", drogon::Delete);
    ADD_METHOD_TO(SocialController::refreshConnection, "/api/v1/social/connections/{id}/refresh", drogon::Post);

    // Analytics
    ADD_METHOD_TO(SocialController::getPostAnalytics, "/api/v1/social/analytics/{id}", drogon::Get);
    ADD_METHOD_TO(SocialController::getAnalyticsSummary, "/api/v1/social/analytics/summary", drogon::Get);
    ADD_METHOD_TO(SocialController::getDogAnalytics, "/api/v1/social/analytics/dog/{dog_id}", drogon::Get);

    // Share Cards
    ADD_METHOD_TO(SocialController::generateShareCard, "/api/v1/social/share-card/{dog_id}", drogon::Get);
    ADD_METHOD_TO(SocialController::generateMultiPlatformCards, "/api/v1/social/share-card/{dog_id}/multi", drogon::Get);

    // OAuth
    ADD_METHOD_TO(SocialController::getOAuthUrl, "/api/v1/social/oauth/{platform}/url", drogon::Get);
    ADD_METHOD_TO(SocialController::oauthCallback, "/api/v1/social/oauth/{platform}/callback", drogon::Get);

    // Auto-generation
    ADD_METHOD_TO(SocialController::generateUrgentPost, "/api/v1/social/generate/urgent/{dog_id}", drogon::Post);
    ADD_METHOD_TO(SocialController::generateMilestonePost, "/api/v1/social/generate/milestone/{dog_id}", drogon::Post);

    // Worker status
    ADD_METHOD_TO(SocialController::getWorkerStatus, "/api/v1/social/worker/status", drogon::Get);
    ADD_METHOD_TO(SocialController::triggerSync, "/api/v1/social/worker/sync", drogon::Post);

    METHOD_LIST_END

    // ========================================================================
    // POST MANAGEMENT
    // ========================================================================

    /**
     * @brief Create and publish a new post
     * POST /api/v1/social/posts
     *
     * Request body:
     * {
     *   "dog_id": "uuid",
     *   "text": "Check out this amazing dog!",
     *   "image_urls": ["url1", "url2"],
     *   "video_url": "optional video url",
     *   "platforms": ["facebook", "instagram", "twitter"],
     *   "post_type": "adoption_spotlight"
     * }
     */
    void createPost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief List recent posts
     * GET /api/v1/social/posts?limit=50&offset=0&dog_id=optional
     */
    void listPosts(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get post details
     * GET /api/v1/social/posts/{id}
     */
    void getPost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Delete a post from all platforms
     * DELETE /api/v1/social/posts/{id}
     */
    void deletePost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Schedule a post for later
     * POST /api/v1/social/posts/schedule
     *
     * Request body:
     * {
     *   "dog_id": "uuid",
     *   "text": "Post content",
     *   "platforms": ["facebook", "instagram"],
     *   "scheduled_time": "2024-01-30T14:00:00Z",
     *   "use_optimal_times": false
     * }
     */
    void schedulePost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief List scheduled posts
     * GET /api/v1/social/posts/scheduled
     */
    void listScheduledPosts(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Cancel a scheduled post
     * DELETE /api/v1/social/posts/scheduled/{id}
     */
    void cancelScheduledPost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    // ========================================================================
    // CROSS-POSTING
    // ========================================================================

    /**
     * @brief Cross-post content to multiple platforms
     * POST /api/v1/social/crosspost
     *
     * Request body:
     * {
     *   "dog_id": "uuid",
     *   "text": "Meet Max!",
     *   "image_urls": ["url1"],
     *   "platforms": ["facebook", "instagram", "twitter"]
     * }
     */
    void crossPost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Cross-post a TikTok video to other platforms
     * POST /api/v1/social/tiktok/crosspost
     *
     * Request body:
     * {
     *   "tiktok_url": "https://www.tiktok.com/@user/video/123",
     *   "dog_id": "uuid",
     *   "platforms": ["facebook", "instagram", "twitter"]
     * }
     */
    void crossPostTikTok(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // PLATFORM CONNECTIONS
    // ========================================================================

    /**
     * @brief List platform connections
     * GET /api/v1/social/connections
     */
    void listConnections(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Add a new platform connection
     * POST /api/v1/social/connections
     *
     * Request body:
     * {
     *   "platform": "facebook",
     *   "access_token": "token",
     *   "refresh_token": "refresh",
     *   "page_id": "optional for facebook"
     * }
     */
    void addConnection(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Remove a platform connection
     * DELETE /api/v1/social/connections/{id}
     */
    void removeConnection(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Refresh connection token
     * POST /api/v1/social/connections/{id}/refresh
     */
    void refreshConnection(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    // ========================================================================
    // ANALYTICS
    // ========================================================================

    /**
     * @brief Get analytics for a specific post
     * GET /api/v1/social/analytics/{id}
     */
    void getPostAnalytics(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Get analytics summary
     * GET /api/v1/social/analytics/summary?period=week
     */
    void getAnalyticsSummary(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get analytics for a specific dog
     * GET /api/v1/social/analytics/dog/{dog_id}
     */
    void getDogAnalytics(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id);

    // ========================================================================
    // SHARE CARDS
    // ========================================================================

    /**
     * @brief Generate a share card for a dog
     * GET /api/v1/social/share-card/{dog_id}?platform=instagram&style=standard
     */
    void generateShareCard(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id);

    /**
     * @brief Generate share cards for multiple platforms
     * GET /api/v1/social/share-card/{dog_id}/multi?platforms=facebook,instagram,twitter
     */
    void generateMultiPlatformCards(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id);

    // ========================================================================
    // OAUTH
    // ========================================================================

    /**
     * @brief Get OAuth authorization URL for a platform
     * GET /api/v1/social/oauth/{platform}/url
     */
    void getOAuthUrl(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& platform);

    /**
     * @brief OAuth callback handler
     * GET /api/v1/social/oauth/{platform}/callback?code=xxx&state=xxx
     */
    void oauthCallback(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& platform);

    // ========================================================================
    // AUTO-GENERATION
    // ========================================================================

    /**
     * @brief Generate and post urgent appeal for a dog
     * POST /api/v1/social/generate/urgent/{dog_id}
     */
    void generateUrgentPost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id);

    /**
     * @brief Generate and post milestone announcement
     * POST /api/v1/social/generate/milestone/{dog_id}?type=1_year
     */
    void generateMilestonePost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id);

    // ========================================================================
    // WORKER STATUS
    // ========================================================================

    /**
     * @brief Get background worker status
     * GET /api/v1/social/worker/status
     */
    void getWorkerStatus(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Trigger manual analytics sync
     * POST /api/v1/social/worker/sync?type=analytics
     */
    void triggerSync(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Build a success response
     */
    drogon::HttpResponsePtr buildSuccessResponse(const Json::Value& data);

    /**
     * @brief Build an error response
     */
    drogon::HttpResponsePtr buildErrorResponse(
        int status_code,
        const std::string& message,
        const std::string& error_code = "");

    /**
     * @brief Parse platforms from request
     */
    std::vector<Platform> parsePlatforms(const Json::Value& platforms_json);

    /**
     * @brief Get user ID from request (via auth middleware)
     */
    std::string getUserId(const drogon::HttpRequestPtr& req);

    /**
     * @brief Validate required fields in request body
     */
    bool validateRequiredFields(
        const Json::Value& body,
        const std::vector<std::string>& fields,
        std::string& error_message);

    /**
     * @brief Parse ISO 8601 datetime string
     */
    std::chrono::system_clock::time_point parseDateTime(const std::string& datetime_str);

    /**
     * @brief Generate OAuth state token
     */
    std::string generateOAuthState();

    /**
     * @brief Validate OAuth state token
     */
    bool validateOAuthState(const std::string& state);
};

} // namespace wtl::content::social
