/**
 * @file UsersController.cc
 * @brief Implementation of UsersController
 * @see UsersController.h for documentation
 */

#include "core/controllers/UsersController.h"

// Standard library includes
#include <regex>

// Project includes
#include "core/utils/ApiResponse.h"
#include "core/services/UserService.h"
#include "core/services/DogService.h"
#include "core/auth/AuthMiddleware.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::core::controllers {

using namespace ::wtl::core::utils;
using namespace ::wtl::core::services;
using namespace ::wtl::core::auth;

// ============================================================================
// USER PROFILE ENDPOINTS
// ============================================================================

void UsersController::getCurrentUser(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require authentication
    REQUIRE_AUTH(req, callback);

    try {
        auto& user_service = UserService::getInstance();
        auto user = user_service.findById(auth_token->user_id);

        if (!user) {
            // This shouldn't happen if auth is working correctly
            WTL_CAPTURE_ERROR(
                ErrorCategory::AUTHENTICATION,
                "Authenticated user not found in database",
                {{"user_id", auth_token->user_id}}
            );
            callback(ApiResponse::serverError("User not found"));
            return;
        }

        // Return user profile (toJson excludes password_hash)
        callback(ApiResponse::success(user->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get current user: " + std::string(e.what()),
            {{"endpoint", "/api/users/me"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve user profile"));
    }
}

void UsersController::updateCurrentUser(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require authentication
    REQUIRE_AUTH(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        // Validate update data
        std::vector<std::pair<std::string, std::string>> errors;
        if (!validateProfileUpdate(*json, errors)) {
            callback(ApiResponse::validationError(errors));
            return;
        }

        auto& user_service = UserService::getInstance();
        auto user = user_service.findById(auth_token->user_id);

        if (!user) {
            callback(ApiResponse::serverError("User not found"));
            return;
        }

        // Update allowed fields
        if (json->isMember("display_name")) {
            user->display_name = (*json)["display_name"].asString();
        }

        if (json->isMember("phone")) {
            user->phone = (*json)["phone"].asString();
        }

        if (json->isMember("zip_code")) {
            user->zip_code = (*json)["zip_code"].asString();
        }

        if (json->isMember("notification_preferences")) {
            user->notification_preferences = (*json)["notification_preferences"];
        }

        if (json->isMember("search_preferences")) {
            user->search_preferences = (*json)["search_preferences"];
        }

        // Save updates
        auto updated_user = user_service.update(*user);

        callback(ApiResponse::success(updated_user.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to update current user: " + std::string(e.what()),
            {{"endpoint", "/api/users/me"}, {"method", "PUT"}}
        );
        callback(ApiResponse::serverError("Failed to update user profile"));
    }
}

// ============================================================================
// FAVORITES ENDPOINTS
// ============================================================================

void UsersController::getFavorites(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require authentication
    REQUIRE_AUTH(req, callback);

    try {
        int page, per_page;
        parsePaginationParams(req, page, per_page);

        auto& user_service = UserService::getInstance();
        auto& dog_service = DogService::getInstance();

        // Get user's favorite dog IDs
        auto favorite_ids = user_service.getFavorites(auth_token->user_id);

        // Apply pagination to favorite IDs
        int total = static_cast<int>(favorite_ids.size());
        int offset = (page - 1) * per_page;

        Json::Value dogs_json(Json::arrayValue);

        // Fetch dog details for each favorite (within page range)
        for (int i = offset; i < std::min(offset + per_page, total); i++) {
            auto dog = dog_service.findById(favorite_ids[i]);
            if (dog) {
                dogs_json.append(dog->toJson());
            }
        }

        callback(ApiResponse::success(dogs_json, total, page, per_page));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get favorites: " + std::string(e.what()),
            {{"endpoint", "/api/users/me/favorites"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve favorites"));
    }
}

void UsersController::addFavorite(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                   const std::string& dog_id) {
    // Require authentication
    REQUIRE_AUTH(req, callback);

    try {
        // Verify dog exists
        auto& dog_service = DogService::getInstance();
        auto dog = dog_service.findById(dog_id);

        if (!dog) {
            callback(ApiResponse::notFound("Dog"));
            return;
        }

        // Add to favorites
        auto& user_service = UserService::getInstance();
        bool added = user_service.addFavorite(auth_token->user_id, dog_id);

        if (!added) {
            // Dog is already in favorites
            callback(ApiResponse::conflict("Dog is already in favorites"));
            return;
        }

        // Return the dog that was added
        callback(ApiResponse::created(dog->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to add favorite: " + std::string(e.what()),
            {{"endpoint", "/api/users/me/favorites/:dog_id"}, {"method", "POST"}, {"dog_id", dog_id}}
        );
        callback(ApiResponse::serverError("Failed to add favorite"));
    }
}

void UsersController::removeFavorite(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                      const std::string& dog_id) {
    // Require authentication
    REQUIRE_AUTH(req, callback);

    try {
        auto& user_service = UserService::getInstance();
        bool removed = user_service.removeFavorite(auth_token->user_id, dog_id);

        if (!removed) {
            // Dog was not in favorites
            callback(ApiResponse::notFound("Favorite"));
            return;
        }

        callback(ApiResponse::noContent());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to remove favorite: " + std::string(e.what()),
            {{"endpoint", "/api/users/me/favorites/:dog_id"}, {"method", "DELETE"}, {"dog_id", dog_id}}
        );
        callback(ApiResponse::serverError("Failed to remove favorite"));
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void UsersController::parsePaginationParams(const drogon::HttpRequestPtr& req,
                                             int& page, int& per_page) {
    page = 1;
    per_page = 20;

    auto page_str = req->getParameter("page");
    if (!page_str.empty()) {
        try {
            page = std::max(1, std::stoi(page_str));
        } catch (...) {
            page = 1;
        }
    }

    auto per_page_str = req->getParameter("per_page");
    if (!per_page_str.empty()) {
        try {
            per_page = std::min(100, std::max(1, std::stoi(per_page_str)));
        } catch (...) {
            per_page = 20;
        }
    }
}

bool UsersController::validateProfileUpdate(const Json::Value& json,
                                             std::vector<std::pair<std::string, std::string>>& errors) {
    bool is_valid = true;

    // Validate display_name if provided
    if (json.isMember("display_name")) {
        std::string display_name = json["display_name"].asString();
        if (display_name.empty()) {
            errors.emplace_back("display_name", "Display name cannot be empty");
            is_valid = false;
        } else if (display_name.length() > 100) {
            errors.emplace_back("display_name", "Display name must be 100 characters or less");
            is_valid = false;
        }
    }

    // Validate phone if provided
    if (json.isMember("phone") && !json["phone"].isNull()) {
        std::string phone = json["phone"].asString();
        if (!phone.empty()) {
            // Basic phone format validation (allows various formats)
            std::regex phone_regex(R"(^[\d\s\-\+\(\)]{7,20}$)");
            if (!std::regex_match(phone, phone_regex)) {
                errors.emplace_back("phone", "Invalid phone number format");
                is_valid = false;
            }
        }
    }

    // Validate zip_code if provided
    if (json.isMember("zip_code") && !json["zip_code"].isNull()) {
        std::string zip_code = json["zip_code"].asString();
        if (!zip_code.empty()) {
            // US ZIP code format (5 digits or 5+4)
            std::regex zip_regex(R"(^\d{5}(-\d{4})?$)");
            if (!std::regex_match(zip_code, zip_regex)) {
                errors.emplace_back("zip_code", "Invalid ZIP code format (use 12345 or 12345-6789)");
                is_valid = false;
            }
        }
    }

    // Validate notification_preferences if provided (must be object)
    if (json.isMember("notification_preferences")) {
        if (!json["notification_preferences"].isObject()) {
            errors.emplace_back("notification_preferences", "Must be an object");
            is_valid = false;
        }
    }

    // Validate search_preferences if provided (must be object)
    if (json.isMember("search_preferences")) {
        if (!json["search_preferences"].isObject()) {
            errors.emplace_back("search_preferences", "Must be an object");
            is_valid = false;
        }
    }

    // Check for disallowed fields
    std::vector<std::string> disallowed = {"id", "email", "password", "password_hash", "role", "is_active"};
    for (const auto& field : disallowed) {
        if (json.isMember(field)) {
            errors.emplace_back(field, "Cannot modify this field via profile update");
            is_valid = false;
        }
    }

    return is_valid;
}

} // namespace wtl::core::controllers
