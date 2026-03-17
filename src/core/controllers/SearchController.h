/**
 * @file SearchController.h
 * @brief REST API controller for dog search operations
 *
 * PURPOSE:
 * Provides HTTP endpoints for searching dogs, autocomplete suggestions,
 * and retrieving available filter options. Handles request parsing,
 * validation, and response formatting.
 *
 * ENDPOINTS:
 * GET /api/search/dogs           - Main search with filters and pagination
 * GET /api/search/autocomplete   - Autocomplete suggestions (breeds, locations, names)
 * GET /api/search/filters        - Get available filter options with counts
 * GET /api/search/breeds         - List all available breeds
 * GET /api/search/similar/:id    - Get dogs similar to a specific dog
 * GET /api/search/nearby         - Get dogs near a geographic location
 * GET /api/search/longest        - Get dogs waiting the longest
 * GET /api/search/urgent         - Get most urgent dogs
 * GET /api/search/recent         - Get recently added dogs
 *
 * USAGE:
 * Controller auto-registers with Drogon. Endpoints are available when server starts.
 *
 * DEPENDENCIES:
 * - SearchService (Agent 7) - Search business logic
 * - SearchFilters, SearchOptions (Agent 7) - Request parsing
 * - ApiResponse (Agent 4) - Response formatting
 *
 * MODIFICATION GUIDE:
 * - To add new endpoints, add to METHOD_LIST and implement handler
 * - All handlers should use ApiResponse for consistent formatting
 * - Parse query params using SearchFilters/SearchOptions where possible
 *
 * @author Agent 7 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>
#include <vector>

// Third-party includes
#include <drogon/HttpController.h>
#include <json/json.h>

// Project includes
#include "core/models/Dog.h"

namespace wtl::core::controllers {

/**
 * @class SearchController
 * @brief HTTP controller for dog search endpoints
 *
 * Provides REST API endpoints for searching dogs with various filters,
 * autocomplete functionality, and faceted search for the UI.
 */
class SearchController : public drogon::HttpController<SearchController> {
public:
    METHOD_LIST_BEGIN
    // Main search
    ADD_METHOD_TO(SearchController::searchDogs, "/api/search/dogs", drogon::Get);

    // Autocomplete
    ADD_METHOD_TO(SearchController::autocomplete, "/api/search/autocomplete", drogon::Get);

    // Filter options
    ADD_METHOD_TO(SearchController::getFilters, "/api/search/filters", drogon::Get);

    // Breed list
    ADD_METHOD_TO(SearchController::getBreeds, "/api/search/breeds", drogon::Get);

    // Similar dogs
    ADD_METHOD_TO(SearchController::getSimilar, "/api/search/similar/{dog_id}", drogon::Get);

    // Location-based search
    ADD_METHOD_TO(SearchController::getNearby, "/api/search/nearby", drogon::Get);

    // Quick searches
    ADD_METHOD_TO(SearchController::getLongestWaiting, "/api/search/longest", drogon::Get);
    ADD_METHOD_TO(SearchController::getMostUrgent, "/api/search/urgent", drogon::Get);
    ADD_METHOD_TO(SearchController::getRecentlyAdded, "/api/search/recent", drogon::Get);
    METHOD_LIST_END

    // ========================================================================
    // MAIN SEARCH
    // ========================================================================

    /**
     * @brief Search dogs with filters and pagination
     *
     * Query parameters:
     * - Filters: state, shelter_id, breed, size, age_category, gender, color,
     *            good_with_kids, good_with_dogs, good_with_cats, house_trained,
     *            urgency_level, min_age, max_age, min_weight, max_weight, q/query
     * - Pagination: page, per_page/limit
     * - Sorting: sort_by/sort, sort_order/order
     *
     * Response:
     * {
     *     "success": true,
     *     "data": {
     *         "items": [...],
     *         "pagination": {
     *             "total": 150,
     *             "page": 1,
     *             "per_page": 20,
     *             "total_pages": 8,
     *             "has_next": true,
     *             "has_prev": false
     *         }
     *     }
     * }
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void searchDogs(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // AUTOCOMPLETE
    // ========================================================================

    /**
     * @brief Get autocomplete suggestions
     *
     * Query parameters:
     * - q: Search query (required)
     * - type: Type of suggestions (breeds, locations, names)
     * - limit: Maximum number of suggestions (default: 10)
     *
     * Response:
     * {
     *     "success": true,
     *     "data": {
     *         "suggestions": ["Labrador Retriever", "Labrador Mix", ...]
     *     }
     * }
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void autocomplete(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // FILTER OPTIONS
    // ========================================================================

    /**
     * @brief Get available filter options with counts
     *
     * Returns all filter options with counts based on currently applied filters.
     * Used to populate filter dropdowns in the UI.
     *
     * Query parameters (optional current filters):
     * - state, breed, size, age_category, gender, urgency_level
     *
     * Response:
     * {
     *     "success": true,
     *     "data": {
     *         "breeds": [{"name": "Labrador", "count": 45}, ...],
     *         "sizes": [{"name": "large", "count": 120}, ...],
     *         "age_categories": [{"name": "adult", "count": 200}, ...],
     *         "genders": [{"name": "male", "count": 150}, ...],
     *         "urgency_levels": [{"name": "high", "count": 30}, ...],
     *         "states": [{"code": "TX", "count": 89}, ...]
     *     }
     * }
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getFilters(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // BREED LIST
    // ========================================================================

    /**
     * @brief Get list of all available breeds
     *
     * Query parameters:
     * - limit: Maximum number of breeds (default: 100)
     *
     * Response:
     * {
     *     "success": true,
     *     "data": {
     *         "breeds": ["Labrador Retriever", "German Shepherd", ...]
     *     }
     * }
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getBreeds(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // SIMILAR DOGS
    // ========================================================================

    /**
     * @brief Get dogs similar to a specific dog
     *
     * Path parameters:
     * - dog_id: UUID of the dog to find similar dogs for
     *
     * Query parameters:
     * - limit: Maximum number of dogs (default: 5)
     *
     * Response:
     * {
     *     "success": true,
     *     "data": {
     *         "dogs": [...]
     *     }
     * }
     *
     * @param req HTTP request
     * @param callback Response callback
     * @param dog_id UUID of the reference dog
     */
    void getSimilar(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& dog_id);

    // ========================================================================
    // LOCATION-BASED SEARCH
    // ========================================================================

    /**
     * @brief Get dogs near a geographic location
     *
     * Query parameters:
     * - lat: Latitude (required)
     * - lon/lng: Longitude (required)
     * - radius: Radius in miles (default: 25)
     * - limit: Maximum number of dogs (default: 20)
     *
     * Response:
     * {
     *     "success": true,
     *     "data": {
     *         "dogs": [...]
     *     }
     * }
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getNearby(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // QUICK SEARCHES
    // ========================================================================

    /**
     * @brief Get dogs waiting the longest for adoption
     *
     * Query parameters:
     * - limit: Maximum number of dogs (default: 10)
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getLongestWaiting(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get most urgent dogs (highest priority for adoption)
     *
     * Query parameters:
     * - limit: Maximum number of dogs (default: 10)
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getMostUrgent(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get recently added dogs
     *
     * Query parameters:
     * - limit: Maximum number of dogs (default: 10)
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getRecentlyAdded(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    /**
     * @brief Parse limit parameter with default and max
     *
     * @param req HTTP request
     * @param default_limit Default limit if not specified
     * @param max_limit Maximum allowed limit
     * @return Parsed and validated limit value
     */
    static int parseLimit(const drogon::HttpRequestPtr& req,
                          int default_limit = 10,
                          int max_limit = 100);

    /**
     * @brief Convert dog vector to JSON array
     *
     * @param dogs Vector of dogs
     * @return JSON array of dog objects
     */
    static Json::Value dogsToJson(const std::vector<wtl::core::models::Dog>& dogs);
};

} // namespace wtl::core::controllers
