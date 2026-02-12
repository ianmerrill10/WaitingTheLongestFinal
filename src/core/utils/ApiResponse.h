/**
 * @file ApiResponse.h
 * @brief Standardized API response helper for consistent JSON responses
 *
 * PURPOSE:
 * Provides a centralized way to create consistent HTTP responses across all
 * API controllers. Ensures all responses follow the same format for success,
 * error, and pagination scenarios.
 *
 * USAGE:
 * auto response = ApiResponse::success(dogJson);
 * auto response = ApiResponse::success(dogsArray, total, page, per_page);
 * auto response = ApiResponse::notFound("Dog");
 * auto response = ApiResponse::validationError({{"name", "required"}});
 *
 * DEPENDENCIES:
 * - Drogon HttpResponse (framework)
 * - jsoncpp (JSON handling)
 *
 * MODIFICATION GUIDE:
 * - To add new response types, follow the pattern of existing methods
 * - All responses must set appropriate HTTP status codes
 * - All responses must follow the success/error JSON structure
 *
 * @author Agent 4 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>
#include <utility>
#include <vector>

// Third-party includes
#include <drogon/HttpResponse.h>
#include <json/json.h>

namespace wtl::core::utils {

/**
 * @class ApiResponse
 * @brief Static utility class for creating standardized HTTP responses
 *
 * All API endpoints should use this class to create responses, ensuring
 * consistent format across the entire API. Responses follow the format:
 *
 * Success: {"success": true, "data": {...}, "meta": {...}}
 * Error: {"success": false, "error": {"code": "...", "message": "...", "details": {...}}}
 */
class ApiResponse {
public:
    // ========================================================================
    // SUCCESS RESPONSES
    // ========================================================================

    /**
     * @brief Create a success response with data
     *
     * @param data The JSON data to return
     * @return HTTP 200 response with {"success": true, "data": ...}
     */
    static drogon::HttpResponsePtr success(const Json::Value& data);

    /**
     * @brief Create a success response with pagination metadata
     *
     * @param data The JSON array data to return
     * @param total Total number of items across all pages
     * @param page Current page number (1-indexed)
     * @param per_page Number of items per page
     * @return HTTP 200 response with data and pagination meta
     */
    static drogon::HttpResponsePtr success(const Json::Value& data,
                                            int total, int page, int per_page);

    /**
     * @brief Create a created response (201)
     *
     * @param data The created resource data
     * @return HTTP 201 response
     */
    static drogon::HttpResponsePtr created(const Json::Value& data);

    /**
     * @brief Create a no content response (204)
     *
     * @return HTTP 204 response with empty body
     */
    static drogon::HttpResponsePtr noContent();

    // ========================================================================
    // ERROR RESPONSES
    // ========================================================================

    /**
     * @brief Create a bad request response (400)
     *
     * @param message Error message describing the issue
     * @return HTTP 400 response
     */
    static drogon::HttpResponsePtr badRequest(const std::string& message);

    /**
     * @brief Create a bad request response with error code (400)
     *
     * @param code Error code identifier
     * @param message Error message describing the issue
     * @return HTTP 400 response
     */
    static drogon::HttpResponsePtr badRequest(const std::string& code, const std::string& message);

    /**
     * @brief Create an unauthorized response (401)
     *
     * @param message Error message (default: "Unauthorized")
     * @return HTTP 401 response
     */
    static drogon::HttpResponsePtr unauthorized(const std::string& message = "Unauthorized");

    /**
     * @brief Create a forbidden response (403)
     *
     * @param message Error message (default: "Forbidden")
     * @return HTTP 403 response
     */
    static drogon::HttpResponsePtr forbidden(const std::string& message = "Forbidden");

    /**
     * @brief Create a not found response (404)
     *
     * @param resource The resource type that was not found (e.g., "Dog", "Shelter")
     * @return HTTP 404 response
     */
    static drogon::HttpResponsePtr notFound(const std::string& resource);

    /**
     * @brief Create a conflict response (409)
     *
     * @param message Error message describing the conflict
     * @return HTTP 409 response
     */
    static drogon::HttpResponsePtr conflict(const std::string& message);

    /**
     * @brief Create a server error response (500)
     *
     * @param message Error message (internal details should be logged, not exposed)
     * @return HTTP 500 response
     */
    static drogon::HttpResponsePtr serverError(const std::string& message);

    /**
     * @brief Create a validation error response (422)
     *
     * @param errors Vector of field/message pairs describing validation failures
     * @return HTTP 422 response with detailed validation errors
     */
    static drogon::HttpResponsePtr validationError(
        const std::vector<std::pair<std::string, std::string>>& errors);

private:
    /**
     * @brief Create an error response with specified status code
     *
     * @param code Error code identifier
     * @param message Human-readable error message
     * @param httpStatus HTTP status code
     * @param details Additional error details (optional)
     * @return HTTP response with error format
     */
    static drogon::HttpResponsePtr error(const std::string& code,
                                          const std::string& message,
                                          int httpStatus,
                                          const Json::Value& details = Json::Value());
};

} // namespace wtl::core::utils
