/**
 * @file FosterController.cc
 * @brief Implementation of FosterController
 * @see FosterController.h for documentation
 */

#include "core/controllers/FosterController.h"

#include "core/services/FosterService.h"
#include "core/services/FosterMatcher.h"
#include "core/services/FosterApplicationHandler.h"
#include "core/auth/AuthMiddleware.h"
#include "core/utils/ApiResponse.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::core::controllers {

using namespace drogon;
using namespace ::wtl::core::services;
using namespace ::wtl::core::auth;
using namespace ::wtl::core::utils;

// ============================================================================
// USER ENDPOINTS
// ============================================================================

void FosterController::registerFoster(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    // Require authentication
    REQUIRE_AUTH(req, callback);

    try {
        // Parse request body
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        // Create foster home from JSON
        auto home = models::FosterHome::fromJson(*json);
        home.user_id = auth_token->user_id;

        // Attempt registration
        bool success = FosterService::getInstance().registerAsFoster(
            auth_token->user_id,
            home
        );

        if (!success) {
            callback(ApiResponse::conflict("You are already registered as a foster home"));
            return;
        }

        // Get the created home
        auto created = FosterService::getInstance().findByUserId(auth_token->user_id);
        if (created.has_value()) {
            callback(ApiResponse::created(created->toJson()));
        } else {
            callback(ApiResponse::serverError("Failed to retrieve created foster home"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in registerFoster: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to register as foster"));
    }
}

void FosterController::getProfile(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    REQUIRE_AUTH(req, callback);

    try {
        auto home = FosterService::getInstance().findByUserId(auth_token->user_id);

        if (!home.has_value()) {
            callback(ApiResponse::notFound("Foster profile not found. Please register first."));
            return;
        }

        callback(ApiResponse::success(home->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in getProfile: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to get foster profile"));
    }
}

void FosterController::updateProfile(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    REQUIRE_AUTH(req, callback);

    try {
        // Get existing profile
        auto existing = FosterService::getInstance().findByUserId(auth_token->user_id);
        if (!existing.has_value()) {
            callback(ApiResponse::notFound("Foster profile not found. Please register first."));
            return;
        }

        // Parse updates
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        // Merge updates with existing
        auto updated = models::FosterHome::fromJson(*json);
        updated.id = existing->id;
        updated.user_id = existing->user_id;
        updated.created_at = existing->created_at;

        // Preserve statistics
        updated.fosters_completed = existing->fosters_completed;
        updated.fosters_converted_to_adoption = existing->fosters_converted_to_adoption;

        // Update
        auto result = FosterService::getInstance().update(updated);
        callback(ApiResponse::success(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in updateProfile: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to update foster profile"));
    }
}

void FosterController::getMatches(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    REQUIRE_AUTH(req, callback);

    try {
        // Get foster home
        auto home = FosterService::getInstance().findByUserId(auth_token->user_id);
        if (!home.has_value()) {
            callback(ApiResponse::notFound("Foster profile not found. Please register first."));
            return;
        }

        // Get limit parameter
        int limit = 10;
        auto limit_param = req->getParameter("limit");
        if (!limit_param.empty()) {
            limit = std::stoi(limit_param);
            limit = std::min(limit, 50);  // Cap at 50
        }

        // Get matches
        auto matches = FosterMatcher::getInstance().findDogsForFoster(home->id, limit);

        // Convert to JSON
        Json::Value matches_json(Json::arrayValue);
        for (const auto& match : matches) {
            matches_json.append(match.toJson());
        }

        Json::Value response;
        response["matches"] = matches_json;
        response["total"] = static_cast<int>(matches.size());

        callback(ApiResponse::success(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in getMatches: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to get matches"));
    }
}

void FosterController::getPlacements(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    REQUIRE_AUTH(req, callback);

    try {
        // Get foster home
        auto home = FosterService::getInstance().findByUserId(auth_token->user_id);
        if (!home.has_value()) {
            callback(ApiResponse::notFound("Foster profile not found. Please register first."));
            return;
        }

        // Get placements
        auto placements = FosterService::getInstance().getPlacementsByHome(home->id);

        // Convert to JSON
        Json::Value placements_json(Json::arrayValue);
        for (const auto& placement : placements) {
            placements_json.append(placement.toJson());
        }

        callback(ApiResponse::success(placements_json));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in getPlacements: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to get placements"));
    }
}

void FosterController::applyToFoster(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    REQUIRE_AUTH(req, callback);

    try {
        // Parse message from body
        auto json = req->getJsonObject();
        std::string message;
        if (json && json->isMember("message")) {
            message = (*json)["message"].asString();
        }

        if (message.empty()) {
            callback(ApiResponse::badRequest("Please include a message with your application"));
            return;
        }

        // Submit application
        auto result = FosterApplicationHandler::getInstance().submit(
            auth_token->user_id,
            dog_id,
            message
        );

        if (result.success) {
            callback(ApiResponse::created(result.application->toJson()));
        } else {
            callback(ApiResponse::badRequest(result.error.value_or("Application failed")));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in applyToFoster: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}, {"dog_id", dog_id}}
        );
        callback(ApiResponse::serverError("Failed to submit application"));
    }
}

void FosterController::getApplications(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    REQUIRE_AUTH(req, callback);

    try {
        // Get foster home
        auto home = FosterService::getInstance().findByUserId(auth_token->user_id);
        if (!home.has_value()) {
            callback(ApiResponse::notFound("Foster profile not found. Please register first."));
            return;
        }

        // Get applications
        auto applications = FosterService::getInstance().getApplicationsByHome(home->id);

        // Convert to JSON
        Json::Value apps_json(Json::arrayValue);
        for (const auto& app : applications) {
            apps_json.append(app.toJson());
        }

        callback(ApiResponse::success(apps_json));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in getApplications: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}}
        );
        callback(ApiResponse::serverError("Failed to get applications"));
    }
}

void FosterController::withdrawApplication(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_AUTH(req, callback);

    try {
        // Verify the application belongs to the user
        auto app = FosterService::getInstance().findApplicationById(id);
        if (!app.has_value()) {
            callback(ApiResponse::notFound("Application not found"));
            return;
        }

        auto home = FosterService::getInstance().findByUserId(auth_token->user_id);
        if (!home.has_value() || home->id != app->foster_home_id) {
            callback(ApiResponse::forbidden("You can only withdraw your own applications"));
            return;
        }

        // Withdraw
        bool success = FosterService::getInstance().withdrawApplication(id);
        if (success) {
            callback(ApiResponse::noContent());
        } else {
            callback(ApiResponse::badRequest("Application cannot be withdrawn (may not be pending)"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in withdrawApplication: " + std::string(e.what()),
            {{"user_id", auth_token->user_id}, {"app_id", id}}
        );
        callback(ApiResponse::serverError("Failed to withdraw application"));
    }
}

// ============================================================================
// SHELTER/ADMIN ENDPOINTS
// ============================================================================

void FosterController::getPendingApplications(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    REQUIRE_ROLE(req, callback, "shelter_admin");

    try {
        auto applications = FosterService::getInstance().getPendingApplications();

        Json::Value apps_json(Json::arrayValue);
        for (const auto& app : applications) {
            apps_json.append(app.toJson());
        }

        callback(ApiResponse::success(apps_json));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in getPendingApplications: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to get pending applications"));
    }
}

void FosterController::approveApplication(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ROLE(req, callback, "shelter_admin");

    try {
        // Parse response message
        auto json = req->getJsonObject();
        std::string response_msg;
        if (json && json->isMember("response")) {
            response_msg = (*json)["response"].asString();
        }

        // Process approval
        auto result = FosterApplicationHandler::getInstance().process(
            id,
            true,  // approve
            response_msg
        );

        if (result.success) {
            callback(ApiResponse::success(result.application->toJson()));
        } else {
            callback(ApiResponse::badRequest(result.error.value_or("Approval failed")));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in approveApplication: " + std::string(e.what()),
            {{"app_id", id}}
        );
        callback(ApiResponse::serverError("Failed to approve application"));
    }
}

void FosterController::rejectApplication(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ROLE(req, callback, "shelter_admin");

    try {
        // Parse response message
        auto json = req->getJsonObject();
        std::string response_msg;
        if (json && json->isMember("response")) {
            response_msg = (*json)["response"].asString();
        }

        if (response_msg.empty()) {
            callback(ApiResponse::badRequest("Please provide a reason for rejection"));
            return;
        }

        // Process rejection
        auto result = FosterApplicationHandler::getInstance().process(
            id,
            false,  // reject
            response_msg
        );

        if (result.success) {
            callback(ApiResponse::success(result.application->toJson()));
        } else {
            callback(ApiResponse::badRequest(result.error.value_or("Rejection failed")));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in rejectApplication: " + std::string(e.what()),
            {{"app_id", id}}
        );
        callback(ApiResponse::serverError("Failed to reject application"));
    }
}

void FosterController::listFosterHomes(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    REQUIRE_ROLE(req, callback, "admin");

    try {
        // Parse pagination
        int limit = 100;
        int offset = 0;

        auto limit_param = req->getParameter("limit");
        if (!limit_param.empty()) {
            limit = std::stoi(limit_param);
            limit = std::min(limit, 500);
        }

        auto offset_param = req->getParameter("offset");
        if (!offset_param.empty()) {
            offset = std::stoi(offset_param);
        }

        auto homes = FosterService::getInstance().findAll(limit, offset);

        Json::Value homes_json(Json::arrayValue);
        for (const auto& home : homes) {
            homes_json.append(home.toJson());
        }

        Json::Value response;
        response["homes"] = homes_json;
        response["limit"] = limit;
        response["offset"] = offset;
        response["count"] = static_cast<int>(homes.size());

        callback(ApiResponse::success(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in listFosterHomes: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to list foster homes"));
    }
}

void FosterController::getAllPlacements(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    REQUIRE_ROLE(req, callback, "admin");

    try {
        // Check for status filter
        auto status_param = req->getParameter("status");

        std::vector<models::FosterPlacement> placements;

        if (status_param == "active") {
            placements = FosterService::getInstance().getActivePlacements();
        } else {
            // For now, just return active placements
            // Full list would need pagination
            placements = FosterService::getInstance().getActivePlacements();
        }

        Json::Value placements_json(Json::arrayValue);
        for (const auto& placement : placements) {
            placements_json.append(placement.toJson());
        }

        callback(ApiResponse::success(placements_json));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in getAllPlacements: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to get placements"));
    }
}

void FosterController::getStatistics(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto stats = FosterService::getInstance().getStatistics();
        callback(ApiResponse::success(stats));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error in getStatistics: " + std::string(e.what()),
            {}
        );
        callback(ApiResponse::serverError("Failed to get statistics"));
    }
}

} // namespace wtl::core::controllers
