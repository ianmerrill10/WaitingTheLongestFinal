/**
 * @file BlogController.cc
 * @brief Implementation of BlogController
 * @see BlogController.h for documentation
 */

#include "controllers/BlogController.h"

// Standard library includes
#include <iomanip>
#include <map>
#include <sstream>

// Project includes
#include "content/blog/BlogService.h"
#include "content/blog/BlogEngine.h"
#include "content/blog/EducationalContent.h"
#include "core/auth/AuthMiddleware.h"
#include "core/debug/ErrorCapture.h"
#include "core/utils/ApiResponse.h"

namespace wtl::controllers {

using namespace ::wtl::content::blog;
using namespace ::wtl::core::utils;

// ============================================================================
// PUBLIC READ HANDLERS
// ============================================================================

void BlogController::getPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    try {
        // Parse query parameters
        auto params = req->getParameters();
        std::map<std::string, std::string> ordered_params(params.begin(), params.end());
        BlogSearchFilters filters = BlogSearchFilters::fromQueryParams(ordered_params);

        BlogSearchOptions options;
        parsePaginationParams(req, options.page, options.per_page);

        auto sort_it = params.find("sort_by");
        if (sort_it != params.end()) {
            options.sort_by = sort_it->second;
        }
        auto order_it = params.find("sort_order");
        if (order_it != params.end()) {
            options.sort_order = order_it->second;
        }
        options.validate();

        // Search
        auto& service = BlogService::getInstance();
        auto result = service.search(filters, options);

        callback(ApiResponse::success(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to get blog posts: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to retrieve posts"));
    }
}

void BlogController::getPostBySlug(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& slug
) {
    try {
        auto& service = BlogService::getInstance();
        auto post = service.findBySlug(slug);

        if (!post) {
            callback(ApiResponse::notFound("Post not found"));
            return;
        }

        // Only show published posts to public
        if (post->status != PostStatus::PUBLISHED) {
            callback(ApiResponse::notFound("Post not found"));
            return;
        }

        callback(ApiResponse::success(post->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to get post by slug: " + std::string(e.what()),
            {{"slug", slug}}
        );
        callback(ApiResponse::serverError("Failed to retrieve post"));
    }
}

void BlogController::getPostById(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    try {
        auto& service = BlogService::getInstance();
        auto post = service.findById(id);

        if (!post) {
            callback(ApiResponse::notFound("Post not found"));
            return;
        }

        callback(ApiResponse::success(post->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to get post by ID: " + std::string(e.what()),
            {{"id", id}}
        );
        callback(ApiResponse::serverError("Failed to retrieve post"));
    }
}

void BlogController::getPostsByCategory(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& category
) {
    try {
        BlogCategory cat;
        try {
            cat = blogCategoryFromString(category);
        } catch (...) {
            callback(ApiResponse::badRequest("INVALID_CATEGORY", "Invalid blog category"));
            return;
        }

        int limit = 20;
        int offset = 0;

        auto params = req->getParameters();
        auto limit_it = params.find("limit");
        if (limit_it != params.end()) {
            limit = std::min(100, std::max(1, std::stoi(limit_it->second)));
        }
        auto offset_it = params.find("offset");
        if (offset_it != params.end()) {
            offset = std::max(0, std::stoi(offset_it->second));
        }

        auto& service = BlogService::getInstance();
        auto posts = service.findByCategory(cat, limit, offset);

        Json::Value result(Json::arrayValue);
        for (const auto& post : posts) {
            result.append(post.toListJson());
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to get posts by category: " + std::string(e.what()),
            {{"category", category}}
        );
        callback(ApiResponse::serverError("Failed to retrieve posts"));
    }
}

void BlogController::getPostsByDog(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id
) {
    try {
        auto& service = BlogService::getInstance();
        auto posts = service.findByDogId(dog_id);

        Json::Value result(Json::arrayValue);
        for (const auto& post : posts) {
            result.append(post.toListJson());
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to get posts by dog: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
        callback(ApiResponse::serverError("Failed to retrieve posts"));
    }
}

void BlogController::getRecentPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    try {
        int limit = 10;
        auto params = req->getParameters();
        auto limit_it = params.find("limit");
        if (limit_it != params.end()) {
            limit = std::min(50, std::max(1, std::stoi(limit_it->second)));
        }

        auto& service = BlogService::getInstance();
        auto posts = service.getRecentPosts(limit);

        Json::Value result(Json::arrayValue);
        for (const auto& post : posts) {
            result.append(post.toListJson());
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to retrieve recent posts"));
    }
}

void BlogController::getPopularPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    try {
        int limit = 10;
        int days_back = 30;

        auto params = req->getParameters();
        auto limit_it = params.find("limit");
        if (limit_it != params.end()) {
            limit = std::min(50, std::max(1, std::stoi(limit_it->second)));
        }
        auto days_it = params.find("days");
        if (days_it != params.end()) {
            days_back = std::min(365, std::max(1, std::stoi(days_it->second)));
        }

        auto& service = BlogService::getInstance();
        auto posts = service.getPopularPosts(limit, days_back);

        Json::Value result(Json::arrayValue);
        for (const auto& post : posts) {
            result.append(post.toListJson());
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to retrieve popular posts"));
    }
}

void BlogController::getRelatedPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    try {
        int limit = 5;
        auto params = req->getParameters();
        auto limit_it = params.find("limit");
        if (limit_it != params.end()) {
            limit = std::min(20, std::max(1, std::stoi(limit_it->second)));
        }

        auto& service = BlogService::getInstance();
        auto posts = service.getRelatedPosts(id, limit);

        Json::Value result(Json::arrayValue);
        for (const auto& post : posts) {
            result.append(post.toListJson());
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to retrieve related posts"));
    }
}

void BlogController::trackView(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    auto& service = BlogService::getInstance();
    service.incrementViewCount(id);

    callback(ApiResponse::noContent());
}

void BlogController::trackShare(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    std::string platform;
    auto json = req->getJsonObject();
    if (json && json->isMember("platform")) {
        platform = (*json)["platform"].asString();
    }

    auto& service = BlogService::getInstance();
    service.incrementShareCount(id, platform);

    callback(ApiResponse::noContent());
}

// ============================================================================
// ADMIN WRITE HANDLERS
// ============================================================================

void BlogController::createPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("INVALID_JSON", "Invalid JSON body"));
            return;
        }

        std::vector<std::pair<std::string, std::string>> errors;
        if (!validatePostData(*json, errors)) {
            callback(ApiResponse::validationError(errors));
            return;
        }

        BlogPost post = BlogPost::fromJson(*json);

        auto& service = BlogService::getInstance();
        auto saved = service.save(post);

        callback(ApiResponse::created(saved.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to create post: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to create post"));
    }
}

void BlogController::updatePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("INVALID_JSON", "Invalid JSON body"));
            return;
        }

        auto& service = BlogService::getInstance();
        auto existing = service.findById(id);
        if (!existing) {
            callback(ApiResponse::notFound("Post not found"));
            return;
        }

        // Update fields from JSON
        BlogPost post = *existing;
        if (json->isMember("title")) post.title = (*json)["title"].asString();
        if (json->isMember("slug")) post.slug = (*json)["slug"].asString();
        if (json->isMember("content")) post.content = (*json)["content"].asString();
        if (json->isMember("excerpt")) post.excerpt = (*json)["excerpt"].asString();
        if (json->isMember("category")) {
            try {
                post.category = blogCategoryFromString((*json)["category"].asString());
            } catch (const std::exception& e) {
                LOG_WARN << "BlogController: " << e.what();
            } catch (...) {
                LOG_WARN << "BlogController: unknown error";
            }
        }
        if (json->isMember("meta_description")) {
            post.meta_description = (*json)["meta_description"].asString();
        }

        auto updated = service.update(post);

        callback(ApiResponse::success(updated.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to update post: " + std::string(e.what()),
            {{"id", id}}
        );
        callback(ApiResponse::serverError("Failed to update post"));
    }
}

void BlogController::deletePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto& service = BlogService::getInstance();
        bool deleted = service.deleteById(id);

        if (!deleted) {
            callback(ApiResponse::notFound("Post not found"));
            return;
        }

        callback(ApiResponse::noContent());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::HTTP_REQUEST,
            "Failed to delete post: " + std::string(e.what()),
            {{"id", id}}
        );
        callback(ApiResponse::serverError("Failed to delete post"));
    }
}

void BlogController::publishPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto& service = BlogService::getInstance();
        bool success = service.publish(id);

        if (!success) {
            callback(ApiResponse::serverError("Failed to publish post"));
            return;
        }

        auto post = service.findById(id);
        callback(ApiResponse::success(post->toJson()));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to publish post"));
    }
}

void BlogController::unpublishPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto& service = BlogService::getInstance();
        bool success = service.unpublish(id);

        if (!success) {
            callback(ApiResponse::serverError("Failed to unpublish post"));
            return;
        }

        auto post = service.findById(id);
        callback(ApiResponse::success(post->toJson()));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to unpublish post"));
    }
}

void BlogController::schedulePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("scheduled_at")) {
            callback(ApiResponse::badRequest("MISSING_DATE", "scheduled_at is required"));
            return;
        }

        std::string date_str = (*json)["scheduled_at"].asString();
        std::tm tm = {};
        std::istringstream ss(date_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

        if (ss.fail()) {
            callback(ApiResponse::badRequest("INVALID_DATE", "Invalid date format"));
            return;
        }

        auto scheduled_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        auto& service = BlogService::getInstance();
        bool success = service.schedule(id, scheduled_at);

        if (!success) {
            callback(ApiResponse::serverError("Failed to schedule post"));
            return;
        }

        auto post = service.findById(id);
        callback(ApiResponse::success(post->toJson()));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to schedule post"));
    }
}

// ============================================================================
// CONTENT GENERATION HANDLERS
// ============================================================================

void BlogController::generateUrgentRoundup(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_ADMIN(req, callback);

    try {
        bool auto_publish = false;
        auto json = req->getJsonObject();
        if (json && json->isMember("auto_publish")) {
            auto_publish = (*json)["auto_publish"].asBool();
        }

        auto& engine = BlogEngine::getInstance();
        auto result = engine.generateUrgentRoundup(auto_publish);

        if (!result.success) {
            callback(ApiResponse::badRequest("GENERATION_FAILED", result.error));
            return;
        }

        callback(ApiResponse::created(result.post.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            "Failed to generate urgent roundup: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to generate urgent roundup"));
    }
}

void BlogController::generateDogFeature(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id
) {
    REQUIRE_ADMIN(req, callback);

    try {
        bool auto_publish = false;
        auto json = req->getJsonObject();
        if (json && json->isMember("auto_publish")) {
            auto_publish = (*json)["auto_publish"].asBool();
        }

        auto& engine = BlogEngine::getInstance();
        auto result = engine.generateDogFeatureById(dog_id, auto_publish);

        if (!result.success) {
            callback(ApiResponse::badRequest("GENERATION_FAILED", result.error));
            return;
        }

        callback(ApiResponse::created(result.post.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            "Failed to generate dog feature: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
        callback(ApiResponse::serverError("Failed to generate dog feature"));
    }
}

void BlogController::generateSuccessStory(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& adoption_id
) {
    REQUIRE_ADMIN(req, callback);

    try {
        std::string adopter_story;
        std::vector<std::string> after_photos;

        auto json = req->getJsonObject();
        if (json) {
            if (json->isMember("adopter_story")) {
                adopter_story = (*json)["adopter_story"].asString();
            }
            if (json->isMember("after_photos") && (*json)["after_photos"].isArray()) {
                for (const auto& photo : (*json)["after_photos"]) {
                    after_photos.push_back(photo.asString());
                }
            }
        }

        auto& engine = BlogEngine::getInstance();
        auto result = engine.generateSuccessStoryById(adoption_id, adopter_story, after_photos);

        if (!result.success) {
            callback(ApiResponse::badRequest("GENERATION_FAILED", result.error));
            return;
        }

        callback(ApiResponse::created(result.post.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            "Failed to generate success story: " + std::string(e.what()),
            {{"adoption_id", adoption_id}}
        );
        callback(ApiResponse::serverError("Failed to generate success story"));
    }
}

void BlogController::generateEducationalPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("topic")) {
            callback(ApiResponse::badRequest("MISSING_TOPIC", "topic is required"));
            return;
        }

        std::string topic = (*json)["topic"].asString();
        Json::Value custom_data;
        if (json->isMember("custom_data")) {
            custom_data = (*json)["custom_data"];
        }

        auto& engine = BlogEngine::getInstance();
        auto result = engine.generateEducationalPost(topic, custom_data);

        if (!result.success) {
            callback(ApiResponse::badRequest("GENERATION_FAILED", result.error));
            return;
        }

        callback(ApiResponse::created(result.post.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONTENT,
            "Failed to generate educational post: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to generate educational post"));
    }
}

void BlogController::schedulePendingPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto& engine = BlogEngine::getInstance();
        int scheduled = engine.schedulePosts();

        Json::Value result;
        result["scheduled_count"] = scheduled;

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to schedule posts"));
    }
}

void BlogController::getEngineStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    REQUIRE_ADMIN(req, callback);

    try {
        auto& engine = BlogEngine::getInstance();
        auto stats = engine.getStats();

        Json::Value result;
        result["total_posts_generated"] = stats.total_posts_generated;
        result["posts_published_today"] = stats.posts_published_today;
        result["posts_scheduled"] = stats.posts_scheduled;
        result["urgent_roundups_this_week"] = stats.urgent_roundups_this_week;
        result["dog_features_this_week"] = stats.dog_features_this_week;
        result["success_stories_this_week"] = stats.success_stories_this_week;

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to get engine stats"));
    }
}

// ============================================================================
// EDUCATIONAL CONTENT HANDLERS
// ============================================================================

void BlogController::getEducationalContent(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    try {
        int limit = 20;
        auto params = req->getParameters();
        auto limit_it = params.find("limit");
        if (limit_it != params.end()) {
            limit = std::min(100, std::max(1, std::stoi(limit_it->second)));
        }

        auto content = EducationalContentRepository::getFeatured(limit);

        Json::Value result(Json::arrayValue);
        for (const auto& item : content) {
            result.append(item.toListJson());
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to get educational content"));
    }
}

void BlogController::getEducationalByTopic(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& topic
) {
    try {
        EducationalTopic topic_enum;
        try {
            topic_enum = educationalTopicFromString(topic);
        } catch (...) {
            callback(ApiResponse::badRequest("INVALID_TOPIC", "Invalid topic"));
            return;
        }

        auto content = EducationalContentRepository::getByTopic(topic_enum);

        Json::Value result(Json::arrayValue);
        for (const auto& item : content) {
            result.append(item.toListJson());
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to get educational content by topic"));
    }
}

void BlogController::getEducationalByLifeStage(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& stage
) {
    try {
        LifeStage stage_enum = lifeStageFromString(stage);

        auto content = EducationalContentRepository::getByLifeStage(stage_enum);

        Json::Value result(Json::arrayValue);
        for (const auto& item : content) {
            result.append(item.toListJson());
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to get educational content by life stage"));
    }
}

void BlogController::searchEducational(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    try {
        auto params = req->getParameters();
        auto query_it = params.find("query");
        if (query_it == params.end() || query_it->second.empty()) {
            callback(ApiResponse::badRequest("MISSING_QUERY", "query parameter is required"));
            return;
        }

        int limit = 20;
        auto limit_it = params.find("limit");
        if (limit_it != params.end()) {
            limit = std::min(100, std::max(1, std::stoi(limit_it->second)));
        }

        auto content = EducationalContentRepository::searchByKeyword(query_it->second, limit);

        Json::Value result(Json::arrayValue);
        for (const auto& item : content) {
            result.append(item.toListJson());
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to search educational content"));
    }
}

void BlogController::voteEducationalHelpful(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    try {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("helpful")) {
            callback(ApiResponse::badRequest("MISSING_VOTE", "helpful (boolean) is required"));
            return;
        }

        bool helpful = (*json)["helpful"].asBool();

        auto& service = BlogService::getInstance();
        service.voteEducationalHelpful(id, helpful);

        callback(ApiResponse::noContent());

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to record vote"));
    }
}

void BlogController::getEducationalTopics(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    try {
        auto topics = getAllEducationalTopics();

        Json::Value result(Json::arrayValue);
        for (const auto& topic : topics) {
            Json::Value item;
            item["id"] = educationalTopicToString(topic);
            item["display_name"] = educationalTopicDisplayName(topic);
            item["icon"] = educationalTopicIcon(topic);
            result.append(item);
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        callback(ApiResponse::serverError("Failed to get topics"));
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void BlogController::parsePaginationParams(
    const drogon::HttpRequestPtr& req,
    int& page,
    int& per_page
) {
    page = 1;
    per_page = 20;

    auto params = req->getParameters();

    auto page_it = params.find("page");
    if (page_it != params.end()) {
        try {
            page = std::max(1, std::stoi(page_it->second));
        } catch (const std::exception& e) {
            LOG_WARN << "BlogController: " << e.what();
        } catch (...) {
            LOG_WARN << "BlogController: unknown error";
        }
    }

    auto per_page_it = params.find("per_page");
    if (per_page_it != params.end()) {
        try {
            per_page = std::min(100, std::max(1, std::stoi(per_page_it->second)));
        } catch (const std::exception& e) {
            LOG_WARN << "BlogController: " << e.what();
        } catch (...) {
            LOG_WARN << "BlogController: unknown error";
        }
    }
}

bool BlogController::validatePostData(
    const Json::Value& json,
    std::vector<std::pair<std::string, std::string>>& errors
) {
    bool valid = true;

    if (!json.isMember("title") || json["title"].asString().empty()) {
        errors.push_back({"title", "Title is required"});
        valid = false;
    }

    if (!json.isMember("content") || json["content"].asString().empty()) {
        errors.push_back({"content", "Content is required"});
        valid = false;
    }

    if (json.isMember("category")) {
        try {
            blogCategoryFromString(json["category"].asString());
        } catch (...) {
            errors.push_back({"category", "Invalid category"});
            valid = false;
        }
    }

    return valid;
}

} // namespace wtl::controllers
