/**
 * @file UsersController.h
 * @brief REST API endpoints for user operations
 *
 * PURPOSE:
 * Handles all HTTP requests related to user resources. Provides endpoints
 * for managing the current user's profile and favorite dogs. All endpoints
 * require authentication.
 *
 * USAGE:
 * This controller is automatically registered with Drogon and handles:
 * - GET  /api/users/me                    - Get current user profile
 * - PUT  /api/users/me                    - Update current user profile
 * - GET  /api/users/me/favorites          - Get user's favorite dogs
 * - POST /api/users/me/favorites/:dog_id  - Add dog to favorites
 * - DELETE /api/users/me/favorites/:dog_id - Remove dog from favorites
 *
 * DEPENDENCIES:
 * - UserService (Agent 3) - Business logic for user operations
 * - DogService (Agent 3) - For validating favorite dogs
 * - AuthMiddleware (Agent 6) - Authentication and authorization
 * - ErrorCapture (Agent 1) - Error logging
 * - ApiResponse (Agent 4) - Standardized response formatting
 *
 * MODIFICATION GUIDE:
 * - Add new endpoints by adding to METHOD_LIST and implementing handler
 * - All handlers must use ApiResponse for consistent response format
 * - All endpoints require authentication (use REQUIRE_AUTH macro)
 * - All errors must be captured with WTL_CAPTURE_ERROR
 *
 * @author Agent 4 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <string>

// Third-party includes
#include <drogon/HttpController.h>

namespace wtl::core::controllers {

/**
 * @class UsersController
 * @brief HTTP controller for user-related API endpoints
 *
 * Thread-safe controller that handles all user-related HTTP requests.
 * All endpoints require authentication. Delegates business logic to UserService.
 */
class UsersController : public drogon::HttpController<UsersController> {
public:
    METHOD_LIST_BEGIN
    // User profile endpoints (auth required)
    ADD_METHOD_TO(UsersController::getCurrentUser, "/api/users/me", drogon::Get);
    ADD_METHOD_TO(UsersController::updateCurrentUser, "/api/users/me", drogon::Put);

    // Favorites endpoints (auth required)
    ADD_METHOD_TO(UsersController::getFavorites, "/api/users/me/favorites", drogon::Get);
    ADD_METHOD_TO(UsersController::addFavorite, "/api/users/me/favorites/{dog_id}", drogon::Post);
    ADD_METHOD_TO(UsersController::removeFavorite, "/api/users/me/favorites/{dog_id}", drogon::Delete);
    METHOD_LIST_END

    // ========================================================================
    // USER PROFILE ENDPOINTS
    // ========================================================================

    /**
     * @brief Get current authenticated user's profile
     *
     * Returns the profile information for the authenticated user.
     * Excludes sensitive information like password hash.
     *
     * @param req HTTP request (must contain valid auth token)
     * @param callback Response callback
     */
    void getCurrentUser(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Update current authenticated user's profile
     *
     * Request Body: User profile fields to update
     * - display_name (string)
     * - phone (string)
     * - zip_code (string)
     * - notification_preferences (object)
     * - search_preferences (object)
     *
     * @param req HTTP request with update data
     * @param callback Response callback
     */
    void updateCurrentUser(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // FAVORITES ENDPOINTS
    // ========================================================================

    /**
     * @brief Get current user's favorite dogs
     *
     * Query Parameters:
     * - page (int, default 1): Page number
     * - per_page (int, default 20, max 100): Items per page
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getFavorites(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Add a dog to user's favorites
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param dog_id Dog UUID to add to favorites
     */
    void addFavorite(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& dog_id);

    /**
     * @brief Remove a dog from user's favorites
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param dog_id Dog UUID to remove from favorites
     */
    void removeFavorite(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& dog_id);

private:
    /**
     * @brief Parse pagination parameters from request
     *
     * @param req HTTP request
     * @param page Output: page number (1-indexed)
     * @param per_page Output: items per page
     */
    void parsePaginationParams(const drogon::HttpRequestPtr& req,
                                int& page, int& per_page);

    /**
     * @brief Validate user profile update data
     *
     * @param json User profile JSON data
     * @param errors Output: validation errors
     * @return true if valid, false otherwise
     */
    bool validateProfileUpdate(const Json::Value& json,
                                std::vector<std::pair<std::string, std::string>>& errors);
};

} // namespace wtl::core::controllers
