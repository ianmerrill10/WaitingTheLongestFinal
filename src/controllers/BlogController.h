/**
 * @file BlogController.h
 * @brief REST API controller for blog operations
 *
 * PURPOSE:
 * Handles all HTTP requests related to blog posts and educational content.
 * Provides endpoints for listing, viewing, creating, updating, and deleting
 * blog posts, as well as content generation triggers.
 *
 * USAGE:
 * This controller is automatically registered with Drogon and handles:
 * - GET  /api/blog/posts - List posts (paginated)
 * - GET  /api/blog/posts/{slug} - Get post by slug
 * - GET  /api/blog/posts/category/{category} - By category
 * - GET  /api/blog/posts/dog/{dog_id} - By featured dog
 * - POST /api/blog/posts - Create post (admin)
 * - PUT  /api/blog/posts/{id} - Update post (admin)
 * - DELETE /api/blog/posts/{id} - Delete post (admin)
 * - POST /api/blog/generate/urgent - Generate urgent roundup
 * - POST /api/blog/generate/feature/{dog_id} - Generate dog feature
 * - GET  /api/blog/educational - Get educational content
 * - GET  /api/blog/educational/{category} - Educational by category
 *
 * DEPENDENCIES:
 * - BlogService - CRUD operations
 * - BlogEngine - Content generation
 * - AuthMiddleware - Authentication/authorization
 * - ApiResponse - Response formatting
 *
 * MODIFICATION GUIDE:
 * - Add new endpoints to METHOD_LIST and implement handlers
 * - Admin endpoints must use REQUIRE_ROLE macro
 * - All errors use WTL_CAPTURE_ERROR macro
 *
 * @author Agent 12 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <string>

// Third-party includes
#include <drogon/HttpController.h>

namespace wtl::controllers {

/**
 * @class BlogController
 * @brief HTTP controller for blog-related API endpoints
 */
class BlogController : public drogon::HttpController<BlogController> {
public:
    METHOD_LIST_BEGIN
    // ========================================================================
    // PUBLIC READ ENDPOINTS
    // ========================================================================

    // List posts with pagination and filters
    ADD_METHOD_TO(BlogController::getPosts, "/api/blog/posts", drogon::Get);

    // Get single post by slug
    ADD_METHOD_TO(BlogController::getPostBySlug, "/api/blog/posts/slug/{slug}", drogon::Get);

    // Get single post by ID
    ADD_METHOD_TO(BlogController::getPostById, "/api/blog/posts/{id}", drogon::Get);

    // Get posts by category
    ADD_METHOD_TO(BlogController::getPostsByCategory, "/api/blog/posts/category/{category}", drogon::Get);

    // Get posts for a dog
    ADD_METHOD_TO(BlogController::getPostsByDog, "/api/blog/posts/dog/{dog_id}", drogon::Get);

    // Get recent posts
    ADD_METHOD_TO(BlogController::getRecentPosts, "/api/blog/recent", drogon::Get);

    // Get popular posts
    ADD_METHOD_TO(BlogController::getPopularPosts, "/api/blog/popular", drogon::Get);

    // Get related posts
    ADD_METHOD_TO(BlogController::getRelatedPosts, "/api/blog/posts/{id}/related", drogon::Get);

    // Track view
    ADD_METHOD_TO(BlogController::trackView, "/api/blog/posts/{id}/view", drogon::Post);

    // Track share
    ADD_METHOD_TO(BlogController::trackShare, "/api/blog/posts/{id}/share", drogon::Post);

    // ========================================================================
    // ADMIN WRITE ENDPOINTS
    // ========================================================================

    // Create post
    ADD_METHOD_TO(BlogController::createPost, "/api/blog/posts", drogon::Post);

    // Update post
    ADD_METHOD_TO(BlogController::updatePost, "/api/blog/posts/{id}", drogon::Put);

    // Delete post
    ADD_METHOD_TO(BlogController::deletePost, "/api/blog/posts/{id}", drogon::Delete);

    // Publish post
    ADD_METHOD_TO(BlogController::publishPost, "/api/blog/posts/{id}/publish", drogon::Post);

    // Unpublish post
    ADD_METHOD_TO(BlogController::unpublishPost, "/api/blog/posts/{id}/unpublish", drogon::Post);

    // Schedule post
    ADD_METHOD_TO(BlogController::schedulePost, "/api/blog/posts/{id}/schedule", drogon::Post);

    // ========================================================================
    // CONTENT GENERATION ENDPOINTS
    // ========================================================================

    // Generate urgent roundup
    ADD_METHOD_TO(BlogController::generateUrgentRoundup, "/api/blog/generate/urgent", drogon::Post);

    // Generate dog feature
    ADD_METHOD_TO(BlogController::generateDogFeature, "/api/blog/generate/feature/{dog_id}", drogon::Post);

    // Generate success story
    ADD_METHOD_TO(BlogController::generateSuccessStory, "/api/blog/generate/success/{adoption_id}", drogon::Post);

    // Generate educational post
    ADD_METHOD_TO(BlogController::generateEducationalPost, "/api/blog/generate/educational", drogon::Post);

    // Schedule pending posts
    ADD_METHOD_TO(BlogController::schedulePendingPosts, "/api/blog/schedule", drogon::Post);

    // Get engine statistics
    ADD_METHOD_TO(BlogController::getEngineStats, "/api/blog/engine/stats", drogon::Get);

    // ========================================================================
    // EDUCATIONAL CONTENT ENDPOINTS
    // ========================================================================

    // Get all educational content
    ADD_METHOD_TO(BlogController::getEducationalContent, "/api/blog/educational", drogon::Get);

    // Get educational by topic
    ADD_METHOD_TO(BlogController::getEducationalByTopic, "/api/blog/educational/topic/{topic}", drogon::Get);

    // Get educational by life stage
    ADD_METHOD_TO(BlogController::getEducationalByLifeStage, "/api/blog/educational/stage/{stage}", drogon::Get);

    // Search educational content
    ADD_METHOD_TO(BlogController::searchEducational, "/api/blog/educational/search", drogon::Get);

    // Vote on educational content
    ADD_METHOD_TO(BlogController::voteEducationalHelpful, "/api/blog/educational/{id}/vote", drogon::Post);

    // Get educational topic list
    ADD_METHOD_TO(BlogController::getEducationalTopics, "/api/blog/educational/topics", drogon::Get);

    METHOD_LIST_END

    // ========================================================================
    // PUBLIC READ HANDLERS
    // ========================================================================

    void getPosts(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void getPostBySlug(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& slug
    );

    void getPostById(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    void getPostsByCategory(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& category
    );

    void getPostsByDog(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id
    );

    void getRecentPosts(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void getPopularPosts(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void getRelatedPosts(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    void trackView(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    void trackShare(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    // ========================================================================
    // ADMIN WRITE HANDLERS
    // ========================================================================

    void createPost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void updatePost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    void deletePost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    void publishPost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    void unpublishPost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    void schedulePost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    // ========================================================================
    // CONTENT GENERATION HANDLERS
    // ========================================================================

    void generateUrgentRoundup(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void generateDogFeature(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id
    );

    void generateSuccessStory(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& adoption_id
    );

    void generateEducationalPost(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void schedulePendingPosts(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void getEngineStats(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    // ========================================================================
    // EDUCATIONAL CONTENT HANDLERS
    // ========================================================================

    void getEducationalContent(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void getEducationalByTopic(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& topic
    );

    void getEducationalByLifeStage(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& stage
    );

    void searchEducational(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void voteEducationalHelpful(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id
    );

    void getEducationalTopics(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

private:
    /**
     * @brief Parse pagination from query parameters
     */
    void parsePaginationParams(
        const drogon::HttpRequestPtr& req,
        int& page,
        int& per_page
    );

    /**
     * @brief Validate post data from JSON
     */
    bool validatePostData(
        const Json::Value& json,
        std::vector<std::pair<std::string, std::string>>& errors
    );
};

} // namespace wtl::controllers
