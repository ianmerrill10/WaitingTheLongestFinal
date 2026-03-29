/**
 * @file SearchController.cc
 * @brief Implementation of SearchController
 * @see SearchController.h for documentation
 */

#include "core/controllers/SearchController.h"

// Project includes
#include "core/services/SearchService.h"
#include "core/services/SearchFilters.h"
#include "core/services/SearchOptions.h"
#include "core/utils/ApiResponse.h"
#include "core/debug/ErrorCapture.h"
#include "core/models/Dog.h"

// Standard library includes
#include <sstream>

namespace wtl::core::controllers {

// ============================================================================
// MAIN SEARCH
// ============================================================================

void SearchController::searchDogs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        // Parse filters and options from query parameters
        auto filters = wtl::core::services::SearchFilters::fromQueryParams(req);
        auto options = wtl::core::services::SearchOptions::fromQueryParams(req);

        // Validate filters
        std::string validation_error = filters.validate();
        if (!validation_error.empty()) {
            callback(wtl::core::utils::ApiResponse::badRequest(validation_error));
            return;
        }

        // Validate and normalize options
        options.validate();

        // Perform search
        auto result = wtl::core::services::SearchService::getInstance().searchDogs(filters, options);

        // Build response
        Json::Value data = result.toJson();
        callback(wtl::core::utils::ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "searchDogs failed: " + std::string(e.what()),
            {{"path", req->path()}}
        );
        callback(wtl::core::utils::ApiResponse::serverError("Search failed"));
    }
}

// ============================================================================
// AUTOCOMPLETE
// ============================================================================

void SearchController::autocomplete(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        // Get query parameter
        std::string query = req->getParameter("q");
        if (query.empty()) {
            query = req->getParameter("query");
        }

        if (query.empty()) {
            callback(wtl::core::utils::ApiResponse::badRequest("Query parameter 'q' is required"));
            return;
        }

        // Minimum query length
        if (query.length() < 2) {
            callback(wtl::core::utils::ApiResponse::badRequest("Query must be at least 2 characters"));
            return;
        }

        // Get type parameter (default: breeds)
        std::string type = req->getParameter("type");
        if (type.empty()) {
            type = "breeds";  // Default to breeds
        }

        // Get limit
        int limit = parseLimit(req, 10, 20);

        // Get suggestions based on type
        std::vector<std::string> suggestions;
        auto& service = wtl::core::services::SearchService::getInstance();

        if (type == "breeds") {
            suggestions = service.suggestBreeds(query, limit);
        } else if (type == "locations") {
            suggestions = service.suggestLocations(query, limit);
        } else if (type == "names") {
            suggestions = service.suggestNames(query, limit);
        } else {
            callback(wtl::core::utils::ApiResponse::badRequest(
                "Invalid type. Must be: breeds, locations, or names"));
            return;
        }

        // Build response
        Json::Value data;
        Json::Value suggestions_array(Json::arrayValue);
        for (const auto& suggestion : suggestions) {
            suggestions_array.append(suggestion);
        }
        data["suggestions"] = suggestions_array;
        data["type"] = type;
        data["query"] = query;

        callback(wtl::core::utils::ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "autocomplete failed: " + std::string(e.what()),
            {{"path", req->path()}}
        );
        callback(wtl::core::utils::ApiResponse::serverError("Autocomplete failed"));
    }
}

// ============================================================================
// FILTER OPTIONS
// ============================================================================

void SearchController::getFilters(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        // Parse current filters (to respect existing filter selections)
        auto current_filters = wtl::core::services::SearchFilters::fromQueryParams(req);

        // Get available filter options
        auto& service = wtl::core::services::SearchService::getInstance();
        Json::Value filters = service.getAvailableFilters(current_filters);

        callback(wtl::core::utils::ApiResponse::success(filters));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "getFilters failed: " + std::string(e.what()),
            {{"path", req->path()}}
        );
        callback(wtl::core::utils::ApiResponse::serverError("Failed to get filters"));
    }
}

// ============================================================================
// BREED LIST
// ============================================================================

void SearchController::getBreeds(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int limit = parseLimit(req, 100, 500);

        // Get all breeds by searching with empty query
        auto& service = wtl::core::services::SearchService::getInstance();

        // Get breed counts from filter service
        wtl::core::services::SearchFilters empty_filters;
        auto filter_counts = service.getFilterCounts("breed", empty_filters);

        // Build response with breed list
        Json::Value data;
        Json::Value breeds_array(Json::arrayValue);
        int count = 0;
        for (const auto& [breed, breed_count] : filter_counts) {
            if (count >= limit) break;
            Json::Value breed_obj;
            breed_obj["name"] = breed;
            breed_obj["count"] = breed_count;
            breeds_array.append(breed_obj);
            count++;
        }
        data["breeds"] = breeds_array;
        data["total"] = static_cast<int>(filter_counts.size());

        callback(wtl::core::utils::ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "getBreeds failed: " + std::string(e.what()),
            {{"path", req->path()}}
        );
        callback(wtl::core::utils::ApiResponse::serverError("Failed to get breeds"));
    }
}

// ============================================================================
// SIMILAR DOGS
// ============================================================================

void SearchController::getSimilar(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    try {
        if (dog_id.empty()) {
            callback(wtl::core::utils::ApiResponse::badRequest("Dog ID is required"));
            return;
        }

        int limit = parseLimit(req, 5, 20);

        auto& service = wtl::core::services::SearchService::getInstance();
        auto dogs = service.getSimilarDogs(dog_id, limit);

        Json::Value data;
        data["dogs"] = dogsToJson(dogs);
        data["reference_dog_id"] = dog_id;

        callback(wtl::core::utils::ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "getSimilar failed: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
        callback(wtl::core::utils::ApiResponse::serverError("Failed to get similar dogs"));
    }
}

// ============================================================================
// LOCATION-BASED SEARCH
// ============================================================================

void SearchController::getNearby(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        // Parse latitude
        std::string lat_str = req->getParameter("lat");
        if (lat_str.empty()) {
            callback(wtl::core::utils::ApiResponse::badRequest("Latitude (lat) is required"));
            return;
        }

        double lat;
        try {
            lat = std::stod(lat_str);
        } catch (...) {
            callback(wtl::core::utils::ApiResponse::badRequest("Invalid latitude value"));
            return;
        }

        // Parse longitude (support both 'lon' and 'lng')
        std::string lon_str = req->getParameter("lon");
        if (lon_str.empty()) {
            lon_str = req->getParameter("lng");
        }
        if (lon_str.empty()) {
            callback(wtl::core::utils::ApiResponse::badRequest("Longitude (lon/lng) is required"));
            return;
        }

        double lon;
        try {
            lon = std::stod(lon_str);
        } catch (...) {
            callback(wtl::core::utils::ApiResponse::badRequest("Invalid longitude value"));
            return;
        }

        // Validate coordinates
        if (lat < -90 || lat > 90) {
            callback(wtl::core::utils::ApiResponse::badRequest("Latitude must be between -90 and 90"));
            return;
        }
        if (lon < -180 || lon > 180) {
            callback(wtl::core::utils::ApiResponse::badRequest("Longitude must be between -180 and 180"));
            return;
        }

        // Parse radius (default: 25 miles)
        int radius = 25;
        std::string radius_str = req->getParameter("radius");
        if (!radius_str.empty()) {
            try {
                radius = std::stoi(radius_str);
                if (radius < 1) radius = 1;
                if (radius > 500) radius = 500;  // Max 500 miles
            } catch (...) {
                // Use default
            }
        }

        int limit = parseLimit(req, 20, 100);

        auto& service = wtl::core::services::SearchService::getInstance();
        auto dogs = service.getNearby(lat, lon, radius, limit);

        Json::Value data;
        data["dogs"] = dogsToJson(dogs);
        data["location"]["lat"] = lat;
        data["location"]["lon"] = lon;
        data["radius_miles"] = radius;

        callback(wtl::core::utils::ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "getNearby failed: " + std::string(e.what()),
            {{"path", req->path()}}
        );
        callback(wtl::core::utils::ApiResponse::serverError("Failed to search nearby"));
    }
}

// ============================================================================
// QUICK SEARCHES
// ============================================================================

void SearchController::getLongestWaiting(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int limit = parseLimit(req, 10, 50);

        auto& service = wtl::core::services::SearchService::getInstance();
        auto dogs = service.getLongestWaiting(limit);

        Json::Value data;
        data["dogs"] = dogsToJson(dogs);

        callback(wtl::core::utils::ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "getLongestWaiting failed: " + std::string(e.what()),
            {}
        );
        callback(wtl::core::utils::ApiResponse::serverError("Failed to get longest waiting dogs"));
    }
}

void SearchController::getMostUrgent(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int limit = parseLimit(req, 10, 50);

        auto& service = wtl::core::services::SearchService::getInstance();
        auto dogs = service.getMostUrgent(limit);

        Json::Value data;
        data["dogs"] = dogsToJson(dogs);

        callback(wtl::core::utils::ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "getMostUrgent failed: " + std::string(e.what()),
            {}
        );
        callback(wtl::core::utils::ApiResponse::serverError("Failed to get urgent dogs"));
    }
}

void SearchController::getRecentlyAdded(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int limit = parseLimit(req, 10, 50);

        auto& service = wtl::core::services::SearchService::getInstance();
        auto dogs = service.getRecentlyAdded(limit);

        Json::Value data;
        data["dogs"] = dogsToJson(dogs);

        callback(wtl::core::utils::ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "getRecentlyAdded failed: " + std::string(e.what()),
            {}
        );
        callback(wtl::core::utils::ApiResponse::serverError("Failed to get recent dogs"));
    }
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

int SearchController::parseLimit(const drogon::HttpRequestPtr& req,
                                  int default_limit,
                                  int max_limit) {
    std::string limit_str = req->getParameter("limit");
    if (limit_str.empty()) {
        return default_limit;
    }

    try {
        int limit = std::stoi(limit_str);
        if (limit < 1) return 1;
        if (limit > max_limit) return max_limit;
        return limit;
    } catch (...) {
        return default_limit;
    }
}

Json::Value SearchController::dogsToJson(const std::vector<wtl::core::models::Dog>& dogs) {
    Json::Value array(Json::arrayValue);
    for (const auto& dog : dogs) {
        array.append(dog.toJson());
    }
    return array;
}

} // namespace wtl::core::controllers
