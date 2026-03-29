/**
 * @file AuthMiddleware.cc
 * @brief Implementation of authentication middleware
 * @see AuthMiddleware.h for documentation
 */

#include "core/auth/AuthMiddleware.h"

// Standard library includes
#include <algorithm>

// Project includes
#include "core/auth/AuthService.h"
#include "core/utils/ApiResponse.h"

namespace wtl::core::auth {

// ============================================================================
// AUTHENTICATION
// ============================================================================

std::optional<AuthToken> AuthMiddleware::authenticate(const drogon::HttpRequestPtr& req) {
    // First check if we've already authenticated this request
    auto cached_token = getAuthToken(req);
    if (cached_token) {
        return cached_token;
    }

    // Extract bearer token from Authorization header
    std::string token = extractBearerToken(req);
    if (token.empty()) {
        return std::nullopt;
    }

    // Validate the token
    auto auth_token = AuthService::getInstance().validateToken(token);
    if (auth_token) {
        // Cache the token in request attributes
        setAuthToken(req, *auth_token);
    }

    return auth_token;
}

// ============================================================================
// AUTHORIZATION
// ============================================================================

bool AuthMiddleware::requireRole(const drogon::HttpRequestPtr& req,
                                  const std::string& role,
                                  std::function<void(const drogon::HttpResponsePtr&)>& callback) {
    // First authenticate
    auto auth_token = authenticate(req);
    if (!auth_token) {
        callback(wtl::core::utils::ApiResponse::unauthorized("Authentication required"));
        return false;
    }

    // Check role permission
    if (!Roles::hasPermission(auth_token->role, role)) {
        callback(wtl::core::utils::ApiResponse::forbidden(
            "Insufficient permissions. Required role: " + role));
        return false;
    }

    return true;
}

bool AuthMiddleware::requireAdmin(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>& callback) {
    return requireRole(req, Roles::ADMIN, callback);
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

std::string AuthMiddleware::extractBearerToken(const drogon::HttpRequestPtr& req) {
    // Get the Authorization header
    std::string auth_header = req->getHeader("Authorization");
    if (auth_header.empty()) {
        return "";
    }

    // Check for Bearer prefix (case-insensitive)
    static const std::string bearer_prefix = "Bearer ";
    static const std::string bearer_prefix_lower = "bearer ";

    if (auth_header.length() <= bearer_prefix.length()) {
        return "";
    }

    // Check prefix
    std::string prefix = auth_header.substr(0, bearer_prefix.length());
    if (prefix != bearer_prefix && prefix != bearer_prefix_lower) {
        // Also handle "bearer " (all lowercase)
        std::string lower_prefix = prefix;
        std::transform(lower_prefix.begin(), lower_prefix.end(),
                      lower_prefix.begin(), ::tolower);
        if (lower_prefix != bearer_prefix_lower) {
            return "";
        }
    }

    // Extract token (everything after "Bearer ")
    std::string token = auth_header.substr(bearer_prefix.length());

    // Trim whitespace
    size_t start = token.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = token.find_last_not_of(" \t\n\r");
    token = token.substr(start, end - start + 1);

    return token;
}

std::string AuthMiddleware::getUserId(const drogon::HttpRequestPtr& req) {
    auto auth_token = getAuthToken(req);
    if (!auth_token) {
        // Try to authenticate
        auth_token = authenticate(req);
    }

    return auth_token ? auth_token->user_id : "";
}

std::string AuthMiddleware::getUserRole(const drogon::HttpRequestPtr& req) {
    auto auth_token = getAuthToken(req);
    if (!auth_token) {
        auth_token = authenticate(req);
    }

    return auth_token ? auth_token->role : "";
}

void AuthMiddleware::setAuthToken(const drogon::HttpRequestPtr& req, const AuthToken& token) {
    // Store token data in request attributes for later retrieval
    // Using individual attributes since Drogon attributes are string-based

    auto& attrs = req->attributes();
    attrs->insert(std::string(AUTH_TOKEN_KEY) + "_user_id", token.user_id);
    attrs->insert(std::string(AUTH_TOKEN_KEY) + "_role", token.role);
    attrs->insert(std::string(AUTH_TOKEN_KEY) + "_token", token.token);
    attrs->insert(std::string(AUTH_TOKEN_KEY) + "_exists", std::string("1"));

    // Store expiry as string timestamp
    auto expires_ts = std::chrono::duration_cast<std::chrono::seconds>(
        token.expires_at.time_since_epoch()).count();
    attrs->insert(std::string(AUTH_TOKEN_KEY) + "_expires", std::to_string(expires_ts));
}

std::optional<AuthToken> AuthMiddleware::getAuthToken(const drogon::HttpRequestPtr& req) {
    auto& attrs = req->attributes();

    // Check if token was stored
    auto exists = attrs->get<std::string>(std::string(AUTH_TOKEN_KEY) + "_exists");
    if (exists != "1") {
        return std::nullopt;
    }

    // Reconstruct token from attributes
    AuthToken token;
    token.user_id = attrs->get<std::string>(std::string(AUTH_TOKEN_KEY) + "_user_id");
    token.role = attrs->get<std::string>(std::string(AUTH_TOKEN_KEY) + "_role");
    token.token = attrs->get<std::string>(std::string(AUTH_TOKEN_KEY) + "_token");

    // Reconstruct expiry
    auto expires_str = attrs->get<std::string>(std::string(AUTH_TOKEN_KEY) + "_expires");
    if (!expires_str.empty()) {
        int64_t expires_ts = std::stoll(expires_str);
        token.expires_at = std::chrono::system_clock::time_point(
            std::chrono::seconds(expires_ts));
    }

    // Set created_at to now since we don't store it
    token.created_at = std::chrono::system_clock::now();

    return token;
}

} // namespace wtl::core::auth
