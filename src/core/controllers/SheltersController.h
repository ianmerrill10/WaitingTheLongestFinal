/**
 * @file SheltersController.h
 * @brief REST API endpoints for shelter operations
 *
 * PURPOSE:
 * Handles all HTTP requests related to shelter resources. Provides endpoints
 * for listing, viewing, and managing shelters, as well as accessing dogs
 * at specific shelters.
 *
 * USAGE:
 * This controller is automatically registered with Drogon and handles:
 * - GET  /api/shelters               - List all shelters (paginated)
 * - GET  /api/shelters/:id           - Get shelter by ID
 * - GET  /api/shelters/:id/dogs      - Get dogs at shelter
 * - GET  /api/shelters/kill-shelters - Get kill shelters
 * - POST /api/shelters               - Create shelter (admin only)
 * - PUT  /api/shelters/:id           - Update shelter (admin only)
 *
 * DEPENDENCIES:
 * - ShelterService (Agent 3) - Business logic for shelter operations
 * - DogService (Agent 3) - For retrieving dogs at shelter
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
 * @class SheltersController
 * @brief HTTP controller for shelter-related API endpoints
 *
 * Thread-safe controller that handles all shelter-related HTTP requests.
 * Delegates business logic to ShelterService.
 */
class SheltersController : public drogon::HttpController<SheltersController> {
public:
    METHOD_LIST_BEGIN
    // Public read endpoints
    ADD_METHOD_TO(SheltersController::getAllShelters, "/api/shelters", drogon::Get);
    ADD_METHOD_TO(SheltersController::getShelterById, "/api/shelters/{id}", drogon::Get);
    ADD_METHOD_TO(SheltersController::getDogsAtShelter, "/api/shelters/{id}/dogs", drogon::Get);
    ADD_METHOD_TO(SheltersController::getKillShelters, "/api/shelters/kill-shelters", drogon::Get);

    // Admin write endpoints
    ADD_METHOD_TO(SheltersController::createShelter, "/api/shelters", drogon::Post);
    ADD_METHOD_TO(SheltersController::updateShelter, "/api/shelters/{id}", drogon::Put);
    METHOD_LIST_END

    // ========================================================================
    // PUBLIC READ ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all shelters with pagination
     *
     * Query Parameters:
     * - page (int, default 1): Page number
     * - per_page (int, default 20, max 100): Items per page
     * - state_code (string, optional): Filter by state
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getAllShelters(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get a specific shelter by ID
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param id Shelter UUID
     */
    void getShelterById(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& id);

    /**
     * @brief Get all dogs at a specific shelter
     *
     * Query Parameters:
     * - page (int, default 1): Page number
     * - per_page (int, default 20, max 100): Items per page
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param id Shelter UUID
     */
    void getDogsAtShelter(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          const std::string& id);

    /**
     * @brief Get all kill shelters (higher urgency)
     *
     * Query Parameters:
     * - page (int, default 1): Page number
     * - per_page (int, default 20, max 100): Items per page
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getKillShelters(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // ADMIN WRITE ENDPOINTS
    // ========================================================================

    /**
     * @brief Create a new shelter (admin only)
     *
     * Request Body: Shelter JSON object
     *
     * @param req HTTP request with shelter data
     * @param callback Response callback
     */
    void createShelter(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Update an existing shelter (admin only)
     *
     * Request Body: Shelter JSON object with fields to update
     *
     * @param req HTTP request with update data
     * @param callback Response callback
     * @param id Shelter UUID to update
     */
    void updateShelter(const drogon::HttpRequestPtr& req,
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
     * @brief Validate shelter data from request body
     *
     * @param json Shelter JSON data
     * @param errors Output: validation errors
     * @return true if valid, false otherwise
     */
    bool validateShelterData(const Json::Value& json,
                              std::vector<std::pair<std::string, std::string>>& errors);
};

} // namespace wtl::core::controllers
