/**
 * @file DogsController.h
 * @brief REST API endpoints for dog operations
 *
 * PURPOSE:
 * Handles all HTTP requests related to dog resources. Provides endpoints
 * for listing, searching, viewing, creating, updating, and deleting dogs.
 * Includes special endpoints for urgent dogs and longest-waiting dogs.
 *
 * USAGE:
 * This controller is automatically registered with Drogon and handles:
 * - GET  /api/dogs                 - List all dogs (paginated)
 * - GET  /api/dogs/:id             - Get dog by ID
 * - GET  /api/dogs/search          - Search dogs with filters
 * - GET  /api/dogs/urgent          - Get urgent dogs
 * - GET  /api/dogs/longest-waiting - Get longest waiting dogs
 * - POST /api/dogs                 - Create dog (admin only)
 * - PUT  /api/dogs/:id             - Update dog (admin only)
 * - DELETE /api/dogs/:id           - Delete dog (admin only)
 *
 * DEPENDENCIES:
 * - DogService (Agent 3) - Business logic for dog operations
 * - SearchService (Agent 7) - Search and filter functionality
 * - AuthMiddleware (Agent 6) - Authentication and authorization
 * - ErrorCapture (Agent 1) - Error logging
 * - ApiResponse (Agent 4) - Standardized response formatting
 *
 * MODIFICATION GUIDE:
 * - Add new endpoints by adding to METHOD_LIST and implementing handler
 * - All handlers must use ApiResponse for consistent response format
 * - Admin endpoints must use REQUIRE_ROLE macro
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
 * @class DogsController
 * @brief HTTP controller for dog-related API endpoints
 *
 * Thread-safe controller that handles all dog-related HTTP requests.
 * Delegates business logic to DogService and SearchService.
 */
class DogsController : public drogon::HttpController<DogsController> {
public:
    METHOD_LIST_BEGIN
    // Public read endpoints
    ADD_METHOD_TO(DogsController::getAllDogs, "/api/dogs", drogon::Get);
    ADD_METHOD_TO(DogsController::getDogById, "/api/dogs/{id}", drogon::Get);
    ADD_METHOD_TO(DogsController::searchDogs, "/api/dogs/search", drogon::Get);
    ADD_METHOD_TO(DogsController::getUrgentDogs, "/api/dogs/urgent", drogon::Get);
    ADD_METHOD_TO(DogsController::getLongestWaiting, "/api/dogs/longest-waiting", drogon::Get);

    // Admin write endpoints
    ADD_METHOD_TO(DogsController::createDog, "/api/dogs", drogon::Post);
    ADD_METHOD_TO(DogsController::updateDog, "/api/dogs/{id}", drogon::Put);
    ADD_METHOD_TO(DogsController::deleteDog, "/api/dogs/{id}", drogon::Delete);
    METHOD_LIST_END

    // ========================================================================
    // PUBLIC READ ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all dogs with pagination
     *
     * Query Parameters:
     * - page (int, default 1): Page number
     * - per_page (int, default 20, max 100): Items per page
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getAllDogs(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get a specific dog by ID
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param id Dog UUID
     */
    void getDogById(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& id);

    /**
     * @brief Search dogs with filters
     *
     * Query Parameters:
     * - state_code, shelter_id, breed, size, age_category, gender
     * - good_with_kids, good_with_dogs, good_with_cats, house_trained (bool)
     * - urgency_level, query (full-text search)
     * - page, per_page, sort_by, sort_order
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void searchDogs(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get dogs with urgent status (at-risk shelters, euthanasia lists)
     *
     * Query Parameters:
     * - limit (int, default 10): Maximum number of results
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getUrgentDogs(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get dogs waiting the longest for adoption
     *
     * Query Parameters:
     * - limit (int, default 10): Maximum number of results
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getLongestWaiting(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // ADMIN WRITE ENDPOINTS
    // ========================================================================

    /**
     * @brief Create a new dog (admin only)
     *
     * Request Body: Dog JSON object
     *
     * @param req HTTP request with dog data
     * @param callback Response callback
     */
    void createDog(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Update an existing dog (admin only)
     *
     * Request Body: Dog JSON object with fields to update
     *
     * @param req HTTP request with update data
     * @param callback Response callback
     * @param id Dog UUID to update
     */
    void updateDog(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& id);

    /**
     * @brief Delete a dog (admin only)
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param id Dog UUID to delete
     */
    void deleteDog(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& id);

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
     * @brief Validate dog data from request body
     *
     * @param json Dog JSON data
     * @param errors Output: validation errors
     * @return true if valid, false otherwise
     */
    bool validateDogData(const Json::Value& json,
                         std::vector<std::pair<std::string, std::string>>& errors);
};

} // namespace wtl::core::controllers
