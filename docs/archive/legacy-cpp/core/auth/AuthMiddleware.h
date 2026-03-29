/**
 * @file AuthMiddleware.h
 * @brief Authentication middleware for HTTP request authorization
 *
 * PURPOSE:
 * Provides middleware functions to authenticate and authorize HTTP requests.
 * Extracts tokens from Authorization headers and validates them against
 * the session store. Provides convenience macros for controllers.
 *
 * USAGE:
 * // In a controller method
 * void MyController::protectedEndpoint(const HttpRequestPtr& req,
 *                                       std::function<void(const HttpResponsePtr&)>&& callback) {
 *     REQUIRE_AUTH(req, callback);
 *     // auth_token is now available
 *     std::string user_id = auth_token->user_id;
 * }
 *
 * // For admin-only endpoints
 * void MyController::adminEndpoint(...) {
 *     REQUIRE_ADMIN(req, callback);
 *     // Only admins reach here
 * }
 *
 * DEPENDENCIES:
 * - AuthService - Token validation
 * - ApiResponse (Agent 4) - Error responses
 * - Drogon HTTP framework
 *
 * MODIFICATION GUIDE:
 * - Keep macros simple and focused
 * - Add new role checks as static methods
 * - All error responses should use ApiResponse utilities
 *
 * PUBLIC INTERFACE - Used by other agents
 * Other agents depend on these methods. Do not change signatures
 * without coordinating via INTEGRATION_CONTRACTS.md
 *
 * @author Agent 6 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes (alphabetical)
#include <functional>
#include <optional>
#include <string>

// Third-party includes (alphabetical)
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

// Project includes (alphabetical)
#include "core/auth/AuthToken.h"
#include "core/utils/ApiResponse.h"

namespace wtl::core::auth {

/**
 * @class AuthMiddleware
 * @brief Static middleware class for authentication/authorization
 *
 * Provides static methods to authenticate requests and verify roles.
 * Designed to work with Drogon's controller pattern.
 */
class AuthMiddleware {
public:
    // ========================================================================
    // AUTHENTICATION
    // ========================================================================

    /**
     * @brief Extract and validate token from request
     *
     * Extracts the Bearer token from the Authorization header and
     * validates it. Returns the authenticated token if valid.
     *
     * @param req The HTTP request
     * @return AuthToken if authenticated, std::nullopt otherwise
     *
     * @note Expects header format: "Authorization: Bearer <token>"
     *
     * @example
     * auto auth_token = AuthMiddleware::authenticate(req);
     * if (auth_token) {
     *     std::cout << "User: " << auth_token->user_id << std::endl;
     * }
     */
    static std::optional<AuthToken> authenticate(const drogon::HttpRequestPtr& req);

    // ========================================================================
    // AUTHORIZATION
    // ========================================================================

    /**
     * @brief Check if authenticated user has required role
     *
     * Validates the token and checks if the user has the required role
     * or higher. Sends appropriate error response if not authorized.
     *
     * @param req The HTTP request
     * @param role The required role (user, foster, shelter_admin, admin)
     * @param callback Response callback (used to send error if unauthorized)
     * @return true if authorized, false if error response was sent
     *
     * @note If false is returned, callback has been invoked with error response
     *
     * @example
     * if (!AuthMiddleware::requireRole(req, "foster", callback)) {
     *     return;  // Error response already sent
     * }
     * // User is foster, shelter_admin, or admin
     */
    static bool requireRole(const drogon::HttpRequestPtr& req,
                           const std::string& role,
                           std::function<void(const drogon::HttpResponsePtr&)>& callback);

    /**
     * @brief Check if authenticated user is an admin
     *
     * Convenience method for admin-only endpoints.
     *
     * @param req The HTTP request
     * @param callback Response callback
     * @return true if user is admin, false if error response was sent
     *
     * @example
     * if (!AuthMiddleware::requireAdmin(req, callback)) {
     *     return;
     * }
     */
    static bool requireAdmin(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>& callback);

    // ========================================================================
    // UTILITY METHODS
    // ========================================================================

    /**
     * @brief Extract bearer token from Authorization header
     *
     * Parses the Authorization header and returns the token part.
     *
     * @param req The HTTP request
     * @return Token string if present, empty string otherwise
     */
    static std::string extractBearerToken(const drogon::HttpRequestPtr& req);

    /**
     * @brief Get the authenticated user ID from request
     *
     * Convenience method to get user ID from an already authenticated request.
     * Stores auth token in request attributes for later retrieval.
     *
     * @param req The HTTP request
     * @return User ID if authenticated, empty string otherwise
     */
    static std::string getUserId(const drogon::HttpRequestPtr& req);

    /**
     * @brief Get the authenticated user's role from request
     *
     * @param req The HTTP request
     * @return Role if authenticated, empty string otherwise
     */
    static std::string getUserRole(const drogon::HttpRequestPtr& req);

    /**
     * @brief Store auth token in request attributes
     *
     * Stores the validated token in request attributes so it can be
     * retrieved later without re-validation.
     *
     * @param req The HTTP request
     * @param token The authenticated token
     */
    static void setAuthToken(const drogon::HttpRequestPtr& req, const AuthToken& token);

    /**
     * @brief Get stored auth token from request attributes
     *
     * @param req The HTTP request
     * @return AuthToken if stored, std::nullopt otherwise
     */
    static std::optional<AuthToken> getAuthToken(const drogon::HttpRequestPtr& req);

private:
    // Prevent instantiation - all methods are static
    AuthMiddleware() = delete;
    ~AuthMiddleware() = delete;

    // ========================================================================
    // CONSTANTS
    // ========================================================================

    /**
     * @brief Request attribute key for stored auth token
     */
    static constexpr const char* AUTH_TOKEN_KEY = "auth_token";
};

// ============================================================================
// CONVENIENCE MACROS
// ============================================================================

/**
 * @def REQUIRE_AUTH
 * @brief Macro to require authentication in a controller method
 *
 * Authenticates the request and returns early with 401 if not authenticated.
 * Defines auth_token variable for use in the rest of the method.
 *
 * @param req The HTTP request variable
 * @param callback The callback variable
 *
 * @example
 * void Controller::method(const HttpRequestPtr& req,
 *                         std::function<void(const HttpResponsePtr&)>&& callback) {
 *     REQUIRE_AUTH(req, callback);
 *     // auth_token is now available
 *     std::string user_id = auth_token->user_id;
 * }
 */
#define REQUIRE_AUTH(req, callback) \
    auto auth_token = ::wtl::core::auth::AuthMiddleware::authenticate(req); \
    if (!auth_token) { \
        callback(::wtl::core::utils::ApiResponse::unauthorized("Authentication required")); \
        return; \
    } \
    ::wtl::core::auth::AuthMiddleware::setAuthToken(req, *auth_token)

/**
 * @def REQUIRE_ADMIN
 * @brief Macro to require admin role in a controller method
 *
 * Authenticates the request and verifies admin role.
 * Returns 401 if not authenticated, 403 if not admin.
 *
 * @param req The HTTP request variable
 * @param callback The callback variable
 */
#define REQUIRE_ADMIN(req, callback) \
    REQUIRE_AUTH(req, callback); \
    if (auth_token->role != ::wtl::core::auth::Roles::ADMIN) { \
        callback(::wtl::core::utils::ApiResponse::forbidden("Admin access required")); \
        return; \
    }

/**
 * @def REQUIRE_ROLE
 * @brief Macro to require specific role or higher in a controller method
 *
 * Authenticates the request and verifies user has required role level.
 *
 * @param req The HTTP request variable
 * @param callback The callback variable
 * @param role The required role string
 */
#define REQUIRE_ROLE(req, callback, required_role) \
    REQUIRE_AUTH(req, callback); \
    if (!::wtl::core::auth::Roles::hasPermission(auth_token->role, required_role)) { \
        callback(::wtl::core::utils::ApiResponse::forbidden( \
            std::string("Requires role: ") + required_role)); \
        return; \
    }

/**
 * @def OPTIONAL_AUTH
 * @brief Macro to optionally authenticate without requiring it
 *
 * Attempts to authenticate but doesn't fail if no token is present.
 * auth_token will be std::nullopt if not authenticated.
 *
 * @param req The HTTP request variable
 */
#define OPTIONAL_AUTH(req) \
    auto auth_token = ::wtl::core::auth::AuthMiddleware::authenticate(req); \
    if (auth_token) { \
        ::wtl::core::auth::AuthMiddleware::setAuthToken(req, *auth_token); \
    }

} // namespace wtl::core::auth
