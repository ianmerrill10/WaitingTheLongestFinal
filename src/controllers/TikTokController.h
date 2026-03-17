/**
 * @file TikTokController.h
 * @brief REST API controller for TikTok automation endpoints
 *
 * PURPOSE:
 * Provides HTTP endpoints for managing TikTok posts including
 * generation, scheduling, posting, and analytics retrieval.
 * All endpoints follow the standard API response format.
 *
 * ENDPOINTS:
 * POST   /api/tiktok/generate      - Generate post for dog
 * POST   /api/tiktok/schedule      - Schedule a post
 * POST   /api/tiktok/post-now      - Post immediately
 * GET    /api/tiktok/posts         - List all posts
 * GET    /api/tiktok/posts/{id}    - Get post details
 * PUT    /api/tiktok/posts/{id}    - Update post
 * DELETE /api/tiktok/posts/{id}    - Delete post
 * GET    /api/tiktok/analytics     - Get analytics summary
 * GET    /api/tiktok/templates     - List available templates
 * GET    /api/tiktok/optimal-time  - Get optimal posting time
 * GET    /api/tiktok/status        - Get worker status
 *
 * USAGE:
 * // Generate a post
 * POST /api/tiktok/generate
 * {"dog_id": "uuid", "template_type": "urgent_countdown"}
 *
 * // Schedule a post
 * POST /api/tiktok/schedule
 * {"post_id": "uuid", "scheduled_at": "2024-01-28T15:00:00Z"}
 *
 * DEPENDENCIES:
 * - TikTokEngine (post management)
 * - TikTokClient (API status)
 * - TikTokWorker (worker status)
 * - AuthMiddleware (authentication)
 * - ApiResponse (response formatting)
 *
 * MODIFICATION GUIDE:
 * - All endpoints should validate input
 * - Use REQUIRE_AUTH macros for protected endpoints
 * - Add rate limiting notes where applicable
 *
 * @author Agent 11 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>

// Third-party includes
#include <drogon/HttpController.h>

// Project includes
#include "content/tiktok/TikTokTemplate.h"

namespace wtl::controllers {

/**
 * @class TikTokController
 * @brief HTTP controller for TikTok automation endpoints
 *
 * Handles all TikTok-related HTTP requests.
 * Uses Drogon's controller pattern with METHOD_LIST macros.
 */
class TikTokController : public drogon::HttpController<TikTokController> {
public:
    // ========================================================================
    // ROUTE REGISTRATION
    // ========================================================================

    METHOD_LIST_BEGIN

    // Post Generation
    ADD_METHOD_TO(TikTokController::generatePost, "/api/tiktok/generate", drogon::Post);
    ADD_METHOD_TO(TikTokController::generateCustomPost, "/api/tiktok/generate/custom", drogon::Post);

    // Post Scheduling
    ADD_METHOD_TO(TikTokController::schedulePost, "/api/tiktok/schedule", drogon::Post);
    ADD_METHOD_TO(TikTokController::getOptimalTime, "/api/tiktok/optimal-time", drogon::Get);
    ADD_METHOD_TO(TikTokController::getScheduledPosts, "/api/tiktok/scheduled", drogon::Get);

    // Posting
    ADD_METHOD_TO(TikTokController::postNow, "/api/tiktok/post-now", drogon::Post);
    ADD_METHOD_TO(TikTokController::retryPost, "/api/tiktok/retry/{id}", drogon::Post);

    // Post CRUD
    ADD_METHOD_TO(TikTokController::getAllPosts, "/api/tiktok/posts", drogon::Get);
    ADD_METHOD_TO(TikTokController::getPostById, "/api/tiktok/posts/{id}", drogon::Get);
    ADD_METHOD_TO(TikTokController::updatePost, "/api/tiktok/posts/{id}", drogon::Put);
    ADD_METHOD_TO(TikTokController::deletePost, "/api/tiktok/posts/{id}", drogon::Delete);
    ADD_METHOD_TO(TikTokController::getPostsByDog, "/api/tiktok/posts/dog/{dog_id}", drogon::Get);

    // Analytics
    ADD_METHOD_TO(TikTokController::getAnalyticsSummary, "/api/tiktok/analytics", drogon::Get);
    ADD_METHOD_TO(TikTokController::getPostAnalytics, "/api/tiktok/analytics/{id}", drogon::Get);
    ADD_METHOD_TO(TikTokController::updatePostAnalytics, "/api/tiktok/analytics/{id}/refresh", drogon::Post);
    ADD_METHOD_TO(TikTokController::getTopPosts, "/api/tiktok/analytics/top", drogon::Get);

    // Templates
    ADD_METHOD_TO(TikTokController::getTemplates, "/api/tiktok/templates", drogon::Get);
    ADD_METHOD_TO(TikTokController::getTemplateById, "/api/tiktok/templates/{id}", drogon::Get);

    // Status and Management
    ADD_METHOD_TO(TikTokController::getStatus, "/api/tiktok/status", drogon::Get);
    ADD_METHOD_TO(TikTokController::getClientStats, "/api/tiktok/client/stats", drogon::Get);
    ADD_METHOD_TO(TikTokController::getWorkerStatus, "/api/tiktok/worker/status", drogon::Get);
    ADD_METHOD_TO(TikTokController::triggerWorkerRun, "/api/tiktok/worker/trigger", drogon::Post);

    METHOD_LIST_END

    // ========================================================================
    // POST GENERATION ENDPOINTS
    // ========================================================================

    /**
     * @brief Generate a TikTok post for a dog
     *
     * Request body:
     * {
     *     "dog_id": "dog-uuid",
     *     "template_type": "urgent_countdown" | "longest_waiting" | etc.
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "post": { ... generated post ... },
     *         "warnings": []
     *     }
     * }
     *
     * @note Rate limiting recommended: 30 requests per minute
     */
    void generatePost(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Generate a custom TikTok post
     *
     * Request body:
     * {
     *     "dog_id": "dog-uuid",
     *     "caption": "Custom caption text",
     *     "hashtags": ["tag1", "tag2"]
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "post": { ... } }
     * }
     */
    void generateCustomPost(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // SCHEDULING ENDPOINTS
    // ========================================================================

    /**
     * @brief Schedule a post for future posting
     *
     * Request body:
     * {
     *     "post_id": "post-uuid",          // Existing post ID
     *     "scheduled_at": "2024-01-28T15:00:00Z"  // Optional, uses optimal time if omitted
     * }
     *
     * OR to generate and schedule in one step:
     * {
     *     "dog_id": "dog-uuid",
     *     "template_type": "urgent_countdown",
     *     "scheduled_at": "2024-01-28T15:00:00Z"
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "post_id": "uuid",
     *         "scheduled_at": "2024-01-28T15:00:00Z"
     *     }
     * }
     */
    void schedulePost(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get optimal posting time
     *
     * Query params:
     * - template_type: Template type (optional)
     * - count: Number of time slots to return (default 1)
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "recommended_time": "2024-01-28T17:00:00Z",
     *         "hour": 17,
     *         "day_of_week": 0,
     *         "confidence": 0.85,
     *         "reason": "High-engagement time for urgent content"
     *     }
     * }
     */
    void getOptimalTime(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get all scheduled posts
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "posts": [ ... ] }
     * }
     */
    void getScheduledPosts(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // POSTING ENDPOINTS
    // ========================================================================

    /**
     * @brief Post immediately to TikTok
     *
     * Request body:
     * {
     *     "post_id": "post-uuid"
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "post_id": "uuid",
     *         "tiktok_post_id": "tiktok-id",
     *         "tiktok_url": "https://tiktok.com/..."
     *     }
     * }
     *
     * @note Requires valid TikTok API authentication
     */
    void postNow(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Retry a failed post
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { ... posting result ... }
     * }
     */
    void retryPost(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& id);

    // ========================================================================
    // POST CRUD ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all posts with pagination
     *
     * Query params:
     * - page: Page number (default 1)
     * - per_page: Items per page (default 20, max 100)
     * - status: Filter by status (draft, scheduled, published, failed)
     * - dog_id: Filter by dog ID
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "posts": [ ... ] },
     *     "meta": { "total": 100, "page": 1, "per_page": 20 }
     * }
     */
    void getAllPosts(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get post by ID
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { ... post details ... }
     * }
     */
    void getPostById(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& id);

    /**
     * @brief Update a post
     *
     * Request body:
     * {
     *     "caption": "Updated caption",
     *     "hashtags": ["new", "tags"],
     *     "scheduled_at": "2024-01-29T10:00:00Z"
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { ... updated post ... }
     * }
     *
     * @note Only draft, scheduled, and failed posts can be updated
     */
    void updatePost(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& id);

    /**
     * @brief Delete a post
     *
     * Query params:
     * - delete_from_tiktok: Also delete from TikTok if published (default false)
     *
     * Response (204): No content
     */
    void deletePost(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& id);

    /**
     * @brief Get all posts for a specific dog
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "posts": [ ... ] }
     * }
     */
    void getPostsByDog(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       const std::string& dog_id);

    // ========================================================================
    // ANALYTICS ENDPOINTS
    // ========================================================================

    /**
     * @brief Get analytics summary across all posts
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "total_posts": 100,
     *         "total_views": 50000,
     *         "total_likes": 5000,
     *         "total_shares": 500,
     *         "total_comments": 300,
     *         "avg_engagement_rate": 2.5
     *     }
     * }
     */
    void getAnalyticsSummary(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get analytics for a specific post
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "views": 1000,
     *         "likes": 100,
     *         "shares": 10,
     *         "comments": 5,
     *         "engagement_rate": 1.15,
     *         "last_updated": "2024-01-28T12:00:00Z"
     *     }
     * }
     */
    void getPostAnalytics(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          const std::string& id);

    /**
     * @brief Refresh analytics for a post from TikTok API
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { ... updated analytics ... }
     * }
     */
    void updatePostAnalytics(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                             const std::string& id);

    /**
     * @brief Get top performing posts
     *
     * Query params:
     * - count: Number of posts (default 10)
     * - metric: Sort metric (views, likes, shares, engagement)
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "posts": [ ... ] }
     * }
     */
    void getTopPosts(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // TEMPLATE ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all available templates
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "templates": [ ... ] }
     * }
     */
    void getTemplates(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get a specific template by ID
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { ... template details ... }
     * }
     */
    void getTemplateById(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                         const std::string& id);

    // ========================================================================
    // STATUS ENDPOINTS
    // ========================================================================

    /**
     * @brief Get overall TikTok integration status
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "engine_initialized": true,
     *         "client_authenticated": true,
     *         "worker_running": true,
     *         "circuit_open": false
     *     }
     * }
     */
    void getStatus(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get TikTok API client statistics
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "total_requests": 1000,
     *         "successful_requests": 980,
     *         "failed_requests": 20,
     *         "avg_latency_ms": 250
     *     }
     * }
     */
    void getClientStats(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get background worker status
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "running": true,
     *         "interval_seconds": 300,
     *         "statistics": { ... }
     *     }
     * }
     */
    void getWorkerStatus(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Trigger an immediate worker run
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "message": "Worker run triggered" }
     * }
     *
     * @note Admin only
     */
    void triggerWorkerRun(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Parse template type from string
     */
    std::optional<wtl::content::tiktok::TikTokTemplateType> parseTemplateType(
        const std::string& type_str);

    /**
     * @brief Parse timestamp from string
     */
    std::optional<std::chrono::system_clock::time_point> parseTimestamp(
        const std::string& timestamp_str);
};

} // namespace wtl::controllers
