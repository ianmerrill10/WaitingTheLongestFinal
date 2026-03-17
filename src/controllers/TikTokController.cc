/**
 * @file TikTokController.cc
 * @brief Implementation of TikTok REST API controller
 * @see TikTokController.h for documentation
 */

#include "TikTokController.h"

// Standard library includes
#include <ctime>
#include <iomanip>
#include <sstream>

// Project includes
#include "content/tiktok/TikTokEngine.h"
#include "content/tiktok/TikTokClient.h"
#include "content/tiktok/TikTokWorker.h"
#include "content/tiktok/TikTokTemplate.h"
#include "core/auth/AuthMiddleware.h"
#include "core/debug/ErrorCapture.h"
#include "core/models/Dog.h"
#include "core/services/DogService.h"

// Use appropriate namespaces
using namespace ::wtl::content::tiktok;
using namespace ::wtl::core::debug;

namespace wtl::controllers {

// ============================================================================
// HELPER METHODS
// ============================================================================

std::optional<TikTokTemplateType> TikTokController::parseTemplateType(
    const std::string& type_str) {

    if (type_str == "urgent_countdown") return TikTokTemplateType::URGENT_COUNTDOWN;
    if (type_str == "longest_waiting") return TikTokTemplateType::LONGEST_WAITING;
    if (type_str == "overlooked_angel") return TikTokTemplateType::OVERLOOKED_ANGEL;
    if (type_str == "foster_plea") return TikTokTemplateType::FOSTER_PLEA;
    if (type_str == "happy_update") return TikTokTemplateType::HAPPY_UPDATE;
    if (type_str == "educational") return TikTokTemplateType::EDUCATIONAL;

    return std::nullopt;
}

std::optional<std::chrono::system_clock::time_point> TikTokController::parseTimestamp(
    const std::string& timestamp_str) {

    if (timestamp_str.empty()) {
        return std::nullopt;
    }

    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Helper to build success response
static drogon::HttpResponsePtr successResponse(const Json::Value& data) {
    Json::Value response;
    response["success"] = true;
    response["data"] = data;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(drogon::k200OK);
    return resp;
}

// Helper to build error response
static drogon::HttpResponsePtr errorResponse(const std::string& message,
                                              drogon::HttpStatusCode status = drogon::k400BadRequest) {
    Json::Value response;
    response["success"] = false;
    response["error"]["message"] = message;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(status);
    return resp;
}

// ============================================================================
// POST GENERATION ENDPOINTS
// ============================================================================

void TikTokController::generatePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(errorResponse("Invalid JSON body"));
            return;
        }

        // Validate required fields
        if (!json->isMember("dog_id") || (*json)["dog_id"].asString().empty()) {
            callback(errorResponse("dog_id is required"));
            return;
        }

        std::string dog_id = (*json)["dog_id"].asString();

        // Get template type
        TikTokTemplateType template_type = TikTokTemplateType::EDUCATIONAL;
        if (json->isMember("template_type")) {
            auto type_opt = parseTemplateType((*json)["template_type"].asString());
            if (!type_opt) {
                callback(errorResponse("Invalid template_type"));
                return;
            }
            template_type = *type_opt;
        }

        // Get dog from service
        auto dog_opt = wtl::core::services::DogService::getInstance().findById(dog_id);
        if (!dog_opt) {
            callback(errorResponse("Dog not found", drogon::k404NotFound));
            return;
        }

        // Generate post
        auto result = TikTokEngine::getInstance().generatePost(*dog_opt, template_type);

        if (!result.success) {
            callback(errorResponse(result.error_message));
            return;
        }

        // Save post to database
        auto saved_id = TikTokEngine::getInstance().savePost(*result.post);
        if (saved_id) {
            result.post->id = *saved_id;
        }

        Json::Value data;
        data["post"] = result.post->toJson();

        Json::Value warnings(Json::arrayValue);
        for (const auto& w : result.warnings) {
            warnings.append(w);
        }
        data["warnings"] = warnings;

        callback(successResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/generate"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::generateCustomPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(errorResponse("Invalid JSON body"));
            return;
        }

        // Validate required fields
        if (!json->isMember("dog_id") || (*json)["dog_id"].asString().empty()) {
            callback(errorResponse("dog_id is required"));
            return;
        }

        if (!json->isMember("caption") || (*json)["caption"].asString().empty()) {
            callback(errorResponse("caption is required"));
            return;
        }

        std::string dog_id = (*json)["dog_id"].asString();
        std::string caption = (*json)["caption"].asString();

        std::vector<std::string> hashtags;
        if (json->isMember("hashtags") && (*json)["hashtags"].isArray()) {
            for (const auto& tag : (*json)["hashtags"]) {
                hashtags.push_back(tag.asString());
            }
        }

        // Get dog
        auto dog_opt = wtl::core::services::DogService::getInstance().findById(dog_id);
        if (!dog_opt) {
            callback(errorResponse("Dog not found", drogon::k404NotFound));
            return;
        }

        // Generate custom post
        auto result = TikTokEngine::getInstance().generateCustomPost(*dog_opt, caption, hashtags);

        if (!result.success) {
            callback(errorResponse(result.error_message));
            return;
        }

        // Save post
        auto saved_id = TikTokEngine::getInstance().savePost(*result.post);
        if (saved_id) {
            result.post->id = *saved_id;
        }

        Json::Value data;
        data["post"] = result.post->toJson();

        callback(successResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/generate/custom"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

// ============================================================================
// SCHEDULING ENDPOINTS
// ============================================================================

void TikTokController::schedulePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(errorResponse("Invalid JSON body"));
            return;
        }

        TikTokPost post;

        // Either post_id or dog_id+template_type required
        if (json->isMember("post_id")) {
            std::string post_id = (*json)["post_id"].asString();
            auto post_opt = TikTokEngine::getInstance().getPost(post_id);
            if (!post_opt) {
                callback(errorResponse("Post not found", drogon::k404NotFound));
                return;
            }
            post = *post_opt;
        } else if (json->isMember("dog_id")) {
            std::string dog_id = (*json)["dog_id"].asString();

            auto dog_opt = wtl::core::services::DogService::getInstance().findById(dog_id);
            if (!dog_opt) {
                callback(errorResponse("Dog not found", drogon::k404NotFound));
                return;
            }

            TikTokTemplateType template_type = TikTokTemplateType::EDUCATIONAL;
            if (json->isMember("template_type")) {
                auto type_opt = parseTemplateType((*json)["template_type"].asString());
                if (type_opt) {
                    template_type = *type_opt;
                }
            }

            auto gen_result = TikTokEngine::getInstance().generatePost(*dog_opt, template_type);
            if (!gen_result.success || !gen_result.post) {
                callback(errorResponse("Failed to generate post: " + gen_result.error_message));
                return;
            }
            post = *gen_result.post;
        } else {
            callback(errorResponse("post_id or dog_id required"));
            return;
        }

        // Schedule
        SchedulingResult result;
        if (json->isMember("scheduled_at")) {
            auto scheduled = parseTimestamp((*json)["scheduled_at"].asString());
            if (!scheduled) {
                callback(errorResponse("Invalid scheduled_at format (use ISO 8601)"));
                return;
            }
            result = TikTokEngine::getInstance().schedulePostAt(post, *scheduled);
        } else {
            result = TikTokEngine::getInstance().schedulePost(post);
        }

        if (!result.success) {
            callback(errorResponse(result.error_message));
            return;
        }

        callback(successResponse(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/schedule"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::getOptimalTime(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        TikTokTemplateType template_type = TikTokTemplateType::EDUCATIONAL;

        auto type_param = req->getParameter("template_type");
        if (!type_param.empty()) {
            auto type_opt = parseTemplateType(type_param);
            if (type_opt) {
                template_type = *type_opt;
            }
        }

        int count = 1;
        auto count_param = req->getParameter("count");
        if (!count_param.empty()) {
            count = std::stoi(count_param);
            if (count < 1) count = 1;
            if (count > 10) count = 10;
        }

        if (count == 1) {
            auto optimal = TikTokEngine::getInstance().getOptimalPostTime(template_type);
            callback(successResponse(optimal.toJson()));
        } else {
            auto slots = TikTokEngine::getInstance().getOptimalPostSlots(count, template_type);

            Json::Value data;
            Json::Value slots_arr(Json::arrayValue);
            for (const auto& slot : slots) {
                slots_arr.append(slot.toJson());
            }
            data["slots"] = slots_arr;

            callback(successResponse(data));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/optimal-time"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::getScheduledPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto posts = TikTokEngine::getInstance().getScheduledPosts();

        Json::Value data;
        Json::Value posts_arr(Json::arrayValue);
        for (const auto& post : posts) {
            posts_arr.append(post.toJson());
        }
        data["posts"] = posts_arr;
        data["count"] = static_cast<int>(posts.size());

        callback(successResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/scheduled"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

// ============================================================================
// POSTING ENDPOINTS
// ============================================================================

void TikTokController::postNow(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(errorResponse("Invalid JSON body"));
            return;
        }

        if (!json->isMember("post_id")) {
            callback(errorResponse("post_id is required"));
            return;
        }

        std::string post_id = (*json)["post_id"].asString();

        auto result = TikTokEngine::getInstance().postById(post_id);

        if (!result.success) {
            Json::Value data = result.toJson();
            callback(errorResponse(result.error_message));
            return;
        }

        callback(successResponse(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/post-now"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::retryPost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto result = TikTokEngine::getInstance().retryFailedPost(id);

        if (!result.success) {
            callback(errorResponse(result.error_message));
            return;
        }

        callback(successResponse(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/retry/" + id}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

// ============================================================================
// POST CRUD ENDPOINTS
// ============================================================================

void TikTokController::getAllPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int page = 1;
        int per_page = 20;

        auto page_param = req->getParameter("page");
        if (!page_param.empty()) {
            page = std::max(1, std::stoi(page_param));
        }

        auto per_page_param = req->getParameter("per_page");
        if (!per_page_param.empty()) {
            per_page = std::min(100, std::max(1, std::stoi(per_page_param)));
        }

        // Get posts filtered by status if specified
        std::vector<TikTokPost> posts;
        auto status_param = req->getParameter("status");

        if (!status_param.empty()) {
            auto status = TikTokPost::stringToStatus(status_param);
            posts = TikTokEngine::getInstance().getPostsByStatus(status, per_page * page);
        } else {
            // Get all posts - need to implement pagination properly
            posts = TikTokEngine::getInstance().getPostsByStatus(TikTokPostStatus::DRAFT, 100);
            auto scheduled = TikTokEngine::getInstance().getPostsByStatus(TikTokPostStatus::SCHEDULED, 100);
            auto published = TikTokEngine::getInstance().getPostsByStatus(TikTokPostStatus::PUBLISHED, 100);
            auto failed = TikTokEngine::getInstance().getPostsByStatus(TikTokPostStatus::FAILED, 100);

            posts.insert(posts.end(), scheduled.begin(), scheduled.end());
            posts.insert(posts.end(), published.begin(), published.end());
            posts.insert(posts.end(), failed.begin(), failed.end());
        }

        // Simple pagination
        int total = static_cast<int>(posts.size());
        int start = (page - 1) * per_page;
        int end = std::min(start + per_page, total);

        Json::Value data;
        Json::Value posts_arr(Json::arrayValue);

        for (int i = start; i < end; ++i) {
            posts_arr.append(posts[i].toJson());
        }
        data["posts"] = posts_arr;

        Json::Value meta;
        meta["total"] = total;
        meta["page"] = page;
        meta["per_page"] = per_page;
        meta["total_pages"] = (total + per_page - 1) / per_page;

        Json::Value response;
        response["success"] = true;
        response["data"] = data;
        response["meta"] = meta;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/posts"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::getPostById(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto post = TikTokEngine::getInstance().getPost(id);

        if (!post) {
            callback(errorResponse("Post not found", drogon::k404NotFound));
            return;
        }

        callback(successResponse(post->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/posts/" + id}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::updatePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(errorResponse("Invalid JSON body"));
            return;
        }

        auto post_opt = TikTokEngine::getInstance().getPost(id);
        if (!post_opt) {
            callback(errorResponse("Post not found", drogon::k404NotFound));
            return;
        }

        TikTokPost post = *post_opt;

        // Check if post is editable
        if (!post.isEditable()) {
            callback(errorResponse("Post cannot be edited in current status"));
            return;
        }

        // Update fields
        if (json->isMember("caption")) {
            post.caption = (*json)["caption"].asString();
        }

        if (json->isMember("hashtags") && (*json)["hashtags"].isArray()) {
            post.hashtags.clear();
            for (const auto& tag : (*json)["hashtags"]) {
                post.hashtags.push_back(tag.asString());
            }
        }

        if (json->isMember("scheduled_at")) {
            auto scheduled = parseTimestamp((*json)["scheduled_at"].asString());
            if (scheduled) {
                post.scheduled_at = *scheduled;
                post.status = TikTokPostStatus::SCHEDULED;
            }
        }

        post.updated_at = std::chrono::system_clock::now();

        // Save
        if (TikTokEngine::getInstance().updatePost(post)) {
            callback(successResponse(post.toJson()));
        } else {
            callback(errorResponse("Failed to update post"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/posts/" + id}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::deletePost(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        bool delete_from_tiktok = req->getParameter("delete_from_tiktok") == "true";

        auto post = TikTokEngine::getInstance().getPost(id);
        if (!post) {
            callback(errorResponse("Post not found", drogon::k404NotFound));
            return;
        }

        if (!post->isDeletable()) {
            callback(errorResponse("Post cannot be deleted in current status"));
            return;
        }

        if (TikTokEngine::getInstance().deletePost(id, delete_from_tiktok)) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            callback(resp);
        } else {
            callback(errorResponse("Failed to delete post"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/posts/" + id}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::getPostsByDog(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    try {
        auto posts = TikTokEngine::getInstance().getPostsByDog(dog_id);

        Json::Value data;
        Json::Value posts_arr(Json::arrayValue);
        for (const auto& post : posts) {
            posts_arr.append(post.toJson());
        }
        data["posts"] = posts_arr;
        data["count"] = static_cast<int>(posts.size());

        callback(successResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/posts/dog/" + dog_id}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

// ============================================================================
// ANALYTICS ENDPOINTS
// ============================================================================

void TikTokController::getAnalyticsSummary(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto summary = TikTokEngine::getInstance().getAnalyticsSummary();
        callback(successResponse(summary));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/analytics"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::getPostAnalytics(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto post = TikTokEngine::getInstance().getPost(id);

        if (!post) {
            callback(errorResponse("Post not found", drogon::k404NotFound));
            return;
        }

        callback(successResponse(post->analytics.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/analytics/" + id}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::updatePostAnalytics(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        if (TikTokEngine::getInstance().updateAnalytics(id)) {
            auto post = TikTokEngine::getInstance().getPost(id);
            if (post) {
                callback(successResponse(post->analytics.toJson()));
            } else {
                callback(errorResponse("Post not found", drogon::k404NotFound));
            }
        } else {
            callback(errorResponse("Failed to update analytics"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/analytics/" + id + "/refresh"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::getTopPosts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int count = 10;
        auto count_param = req->getParameter("count");
        if (!count_param.empty()) {
            count = std::min(50, std::max(1, std::stoi(count_param)));
        }

        std::string metric = "views";
        auto metric_param = req->getParameter("metric");
        if (!metric_param.empty()) {
            metric = metric_param;
        }

        auto posts = TikTokEngine::getInstance().getTopPerformingPosts(count, metric);

        Json::Value data;
        Json::Value posts_arr(Json::arrayValue);
        for (const auto& post : posts) {
            posts_arr.append(post.toJson());
        }
        data["posts"] = posts_arr;
        data["metric"] = metric;

        callback(successResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/analytics/top"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

// ============================================================================
// TEMPLATE ENDPOINTS
// ============================================================================

void TikTokController::getTemplates(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto templates = TemplateManager::getInstance().getAllTemplates();

        Json::Value data;
        Json::Value tmpl_arr(Json::arrayValue);
        for (const auto& tmpl : templates) {
            tmpl_arr.append(tmpl.toJson());
        }
        data["templates"] = tmpl_arr;

        callback(successResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/templates"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::getTemplateById(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    try {
        auto tmpl = TemplateManager::getInstance().getTemplateById(id);

        if (!tmpl) {
            callback(errorResponse("Template not found", drogon::k404NotFound));
            return;
        }

        callback(successResponse(tmpl->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/templates/" + id}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

// ============================================================================
// STATUS ENDPOINTS
// ============================================================================

void TikTokController::getStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        Json::Value data;
        data["engine_initialized"] = TikTokEngine::getInstance().isInitialized();
        data["client_initialized"] = TikTokClient::getInstance().isInitialized();
        data["client_authenticated"] = TikTokClient::getInstance().isAuthenticated();
        data["worker_running"] = TikTokWorker::getInstance().isRunning();
        data["circuit_open"] = TikTokClient::getInstance().isCircuitOpen();

        callback(successResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/status"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::getClientStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto stats = TikTokClient::getInstance().getStats();
        callback(successResponse(stats));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/client/stats"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::getWorkerStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto status = TikTokWorker::getInstance().getStatus();
        callback(successResponse(status));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/worker/status"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

void TikTokController::triggerWorkerRun(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        TikTokWorker::getInstance().triggerRun();

        Json::Value data;
        data["message"] = "Worker run triggered";

        callback(successResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, ErrorCategory::HTTP_REQUEST,
            (std::unordered_map<std::string, std::string>{{"endpoint", "/api/tiktok/worker/trigger"}}));
        callback(errorResponse(e.what(), drogon::k500InternalServerError));
    }
}

} // namespace wtl::controllers
