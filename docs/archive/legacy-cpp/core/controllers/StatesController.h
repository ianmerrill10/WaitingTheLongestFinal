/**
 * @file StatesController.h
 * @brief REST API endpoints for state operations
 *
 * PURPOSE:
 * Handles all HTTP requests related to US state resources. Provides endpoints
 * for listing states, accessing dogs and shelters by state, and viewing
 * state-level statistics.
 *
 * USAGE:
 * This controller is automatically registered with Drogon and handles:
 * - GET  /api/states              - List all states
 * - GET  /api/states/active       - Get states with active dogs
 * - GET  /api/states/:code        - Get state by code
 * - GET  /api/states/:code/dogs   - Get dogs in state
 * - GET  /api/states/:code/shelters - Get shelters in state
 * - GET  /api/states/:code/stats  - Get state statistics
 *
 * DEPENDENCIES:
 * - StateService (Agent 3) - Business logic for state operations
 * - DogService (Agent 3) - For retrieving dogs in state
 * - ShelterService (Agent 3) - For retrieving shelters in state
 * - ErrorCapture (Agent 1) - Error logging
 * - ApiResponse (Agent 4) - Standardized response formatting
 *
 * MODIFICATION GUIDE:
 * - Add new endpoints by adding to METHOD_LIST and implementing handler
 * - All handlers must use ApiResponse for consistent response format
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
 * @class StatesController
 * @brief HTTP controller for state-related API endpoints
 *
 * Thread-safe controller that handles all state-related HTTP requests.
 * Delegates business logic to StateService.
 */
class StatesController : public drogon::HttpController<StatesController> {
public:
    METHOD_LIST_BEGIN
    // Public read endpoints
    ADD_METHOD_TO(StatesController::getAllStates, "/api/states", drogon::Get);
    ADD_METHOD_TO(StatesController::getActiveStates, "/api/states/active", drogon::Get);
    ADD_METHOD_TO(StatesController::getStateByCode, "/api/states/{code}", drogon::Get);
    ADD_METHOD_TO(StatesController::getDogsInState, "/api/states/{code}/dogs", drogon::Get);
    ADD_METHOD_TO(StatesController::getSheltersInState, "/api/states/{code}/shelters", drogon::Get);
    ADD_METHOD_TO(StatesController::getStateStatistics, "/api/states/{code}/stats", drogon::Get);
    METHOD_LIST_END

    // ========================================================================
    // PUBLIC READ ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all states
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getAllStates(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get states with active dogs/shelters
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getActiveStates(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get a specific state by code
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param code State code (e.g., "TX", "CA")
     */
    void getStateByCode(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& code);

    /**
     * @brief Get all dogs in a specific state
     *
     * Query Parameters:
     * - page (int, default 1): Page number
     * - per_page (int, default 20, max 100): Items per page
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param code State code
     */
    void getDogsInState(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& code);

    /**
     * @brief Get all shelters in a specific state
     *
     * Query Parameters:
     * - page (int, default 1): Page number
     * - per_page (int, default 20, max 100): Items per page
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param code State code
     */
    void getSheltersInState(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& code);

    /**
     * @brief Get statistics for a specific state
     *
     * Returns aggregate statistics including:
     * - Total dogs, shelters
     * - Dogs by urgency level
     * - Dogs by size, age category
     * - Average wait time
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param code State code
     */
    void getStateStatistics(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& code);

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
     * @brief Normalize state code to uppercase
     *
     * @param code State code to normalize
     * @return Uppercase state code
     */
    std::string normalizeStateCode(const std::string& code);
};

} // namespace wtl::core::controllers
