/**
 * @file AuthToken.h
 * @brief Authentication token structures and result types
 *
 * PURPOSE:
 * Defines the core data structures for authentication tokens, including
 * access tokens, refresh tokens, and authentication operation results.
 * These structures are used throughout the auth system.
 *
 * USAGE:
 * AuthToken token;
 * token.token = jwt_string;
 * token.user_id = "user-uuid";
 * token.role = "user";
 * token.expires_at = std::chrono::system_clock::now() + std::chrono::hours(1);
 *
 * DEPENDENCIES:
 * - None (pure data structures)
 *
 * MODIFICATION GUIDE:
 * - Add new fields to AuthToken if additional claims are needed
 * - Keep structures serializable to/from JSON
 * - Maintain backward compatibility for existing token formats
 *
 * @author Agent 6 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes (alphabetical)
#include <chrono>
#include <optional>
#include <string>

// Third-party includes (alphabetical)
#include <json/json.h>

namespace wtl::core::auth {

/**
 * @struct AuthToken
 * @brief Represents a validated authentication token
 *
 * Contains the decoded information from a JWT token after validation.
 * Used to identify the authenticated user and their permissions.
 */
struct AuthToken {
    std::string token;                                    // The raw JWT token string
    std::string user_id;                                  // User's UUID
    std::string role;                                     // user, foster, shelter_admin, admin
    std::chrono::system_clock::time_point expires_at;     // Token expiration time
    std::chrono::system_clock::time_point created_at;     // Token creation time (iat claim)

    /**
     * @brief Check if the token is expired
     * @return true if token has expired, false otherwise
     */
    bool isExpired() const {
        return std::chrono::system_clock::now() >= expires_at;
    }

    /**
     * @brief Convert token to JSON representation
     * @return JSON object with token data (excludes raw token for security)
     */
    Json::Value toJson() const {
        Json::Value json;
        json["user_id"] = user_id;
        json["role"] = role;
        json["expires_at"] = static_cast<Json::Int64>(
            std::chrono::duration_cast<std::chrono::seconds>(
                expires_at.time_since_epoch()
            ).count()
        );
        json["created_at"] = static_cast<Json::Int64>(
            std::chrono::duration_cast<std::chrono::seconds>(
                created_at.time_since_epoch()
            ).count()
        );
        return json;
    }
};

/**
 * @struct AuthResult
 * @brief Result of an authentication operation (login, register, refresh)
 *
 * Contains either success data (tokens) or error information.
 * Follows a Result pattern for clean error handling.
 */
struct AuthResult {
    bool success;                                        // Whether the operation succeeded
    std::optional<AuthToken> token;                      // Access token on success
    std::optional<std::string> refresh_token;            // Refresh token on success
    std::optional<std::string> error;                    // Error message on failure
    std::optional<std::string> error_code;               // Error code for client handling

    /**
     * @brief Create a successful auth result
     * @param auth_token The access token
     * @param refresh_tok The refresh token
     * @return AuthResult indicating success
     */
    static AuthResult ok(const AuthToken& auth_token, const std::string& refresh_tok) {
        AuthResult result;
        result.success = true;
        result.token = auth_token;
        result.refresh_token = refresh_tok;
        return result;
    }

    /**
     * @brief Create a failed auth result
     * @param error_message Human-readable error message
     * @param code Machine-readable error code
     * @return AuthResult indicating failure
     */
    static AuthResult fail(const std::string& error_message,
                           const std::string& code = "AUTH_ERROR") {
        AuthResult result;
        result.success = false;
        result.error = error_message;
        result.error_code = code;
        return result;
    }

    /**
     * @brief Convert result to JSON for API responses
     * @return JSON representation of the result
     */
    Json::Value toJson() const {
        Json::Value json;
        json["success"] = success;

        if (success && token) {
            json["data"]["access_token"] = token->token;
            json["data"]["token_type"] = "Bearer";
            json["data"]["expires_at"] = static_cast<Json::Int64>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    token->expires_at.time_since_epoch()
                ).count()
            );
            json["data"]["user"]["id"] = token->user_id;
            json["data"]["user"]["role"] = token->role;

            if (refresh_token) {
                json["data"]["refresh_token"] = *refresh_token;
            }
        } else {
            json["error"]["code"] = error_code.value_or("AUTH_ERROR");
            json["error"]["message"] = error.value_or("Authentication failed");
        }

        return json;
    }
};

/**
 * @struct TokenPayload
 * @brief Decoded JWT payload contents
 *
 * Contains the claims from a decoded JWT token before full validation.
 * Used for token introspection and validation.
 */
struct TokenPayload {
    std::string user_id;                                 // sub claim - user ID
    std::string role;                                    // role claim
    std::chrono::system_clock::time_point issued_at;     // iat claim
    std::chrono::system_clock::time_point expires_at;    // exp claim
    std::string token_type;                              // typ claim (access/refresh)

    /**
     * @brief Check if the token is expired
     * @return true if token has expired
     */
    bool isExpired() const {
        return std::chrono::system_clock::now() >= expires_at;
    }

    /**
     * @brief Convert to AuthToken for validated use
     * @param raw_token The original token string
     * @return AuthToken with the decoded payload data
     */
    AuthToken toAuthToken(const std::string& raw_token) const {
        AuthToken auth_token;
        auth_token.token = raw_token;
        auth_token.user_id = user_id;
        auth_token.role = role;
        auth_token.expires_at = expires_at;
        auth_token.created_at = issued_at;
        return auth_token;
    }
};

/**
 * @struct Session
 * @brief Represents a user session stored in the database
 *
 * Sessions track active user logins and enable token revocation.
 * Tokens are hashed before storage for security.
 */
struct Session {
    std::string id;                                      // Session UUID
    std::string user_id;                                 // User who owns this session
    std::string token_hash;                              // Hashed access token
    std::string refresh_token_hash;                      // Hashed refresh token
    std::chrono::system_clock::time_point created_at;    // Session creation time
    std::chrono::system_clock::time_point expires_at;    // Session expiration time
    std::chrono::system_clock::time_point last_used_at;  // Last activity time
    std::string ip_address;                              // Client IP address
    std::string user_agent;                              // Client user agent string
    bool is_active;                                      // Whether session is active

    /**
     * @brief Check if the session is expired
     * @return true if session has expired
     */
    bool isExpired() const {
        return std::chrono::system_clock::now() >= expires_at;
    }

    /**
     * @brief Convert session to JSON (for admin APIs)
     * @return JSON representation (excludes sensitive hashes)
     */
    Json::Value toJson() const {
        Json::Value json;
        json["id"] = id;
        json["user_id"] = user_id;
        json["created_at"] = static_cast<Json::Int64>(
            std::chrono::duration_cast<std::chrono::seconds>(
                created_at.time_since_epoch()
            ).count()
        );
        json["expires_at"] = static_cast<Json::Int64>(
            std::chrono::duration_cast<std::chrono::seconds>(
                expires_at.time_since_epoch()
            ).count()
        );
        json["last_used_at"] = static_cast<Json::Int64>(
            std::chrono::duration_cast<std::chrono::seconds>(
                last_used_at.time_since_epoch()
            ).count()
        );
        json["ip_address"] = ip_address;
        json["user_agent"] = user_agent;
        json["is_active"] = is_active;
        return json;
    }
};

// ============================================================================
// ERROR CODES
// ============================================================================

namespace ErrorCodes {
    constexpr const char* INVALID_CREDENTIALS = "INVALID_CREDENTIALS";
    constexpr const char* TOKEN_EXPIRED = "TOKEN_EXPIRED";
    constexpr const char* TOKEN_INVALID = "TOKEN_INVALID";
    constexpr const char* TOKEN_REVOKED = "TOKEN_REVOKED";
    constexpr const char* USER_NOT_FOUND = "USER_NOT_FOUND";
    constexpr const char* USER_DISABLED = "USER_DISABLED";
    constexpr const char* EMAIL_NOT_VERIFIED = "EMAIL_NOT_VERIFIED";
    constexpr const char* EMAIL_ALREADY_EXISTS = "EMAIL_ALREADY_EXISTS";
    constexpr const char* INVALID_REFRESH_TOKEN = "INVALID_REFRESH_TOKEN";
    constexpr const char* SESSION_EXPIRED = "SESSION_EXPIRED";
    constexpr const char* INVALID_RESET_TOKEN = "INVALID_RESET_TOKEN";
    constexpr const char* PASSWORD_TOO_WEAK = "PASSWORD_TOO_WEAK";
    constexpr const char* RATE_LIMITED = "RATE_LIMITED";
}

// ============================================================================
// ROLE CONSTANTS
// ============================================================================

namespace Roles {
    constexpr const char* USER = "user";
    constexpr const char* FOSTER = "foster";
    constexpr const char* SHELTER_ADMIN = "shelter_admin";
    constexpr const char* ADMIN = "admin";

    /**
     * @brief Check if a role has at least the specified permission level
     * @param user_role The user's current role
     * @param required_role The minimum required role
     * @return true if user_role meets or exceeds required_role
     */
    inline bool hasPermission(const std::string& user_role, const std::string& required_role) {
        // Role hierarchy: admin > shelter_admin > foster > user
        auto getRoleLevel = [](const std::string& role) -> int {
            if (role == ADMIN) return 4;
            if (role == SHELTER_ADMIN) return 3;
            if (role == FOSTER) return 2;
            if (role == USER) return 1;
            return 0;
        };
        return getRoleLevel(user_role) >= getRoleLevel(required_role);
    }
}

} // namespace wtl::core::auth
