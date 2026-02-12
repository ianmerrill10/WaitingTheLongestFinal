/**
 * @file JwtUtils.h
 * @brief JWT token creation and validation utilities
 *
 * PURPOSE:
 * Provides JWT (JSON Web Token) creation, validation, and decoding
 * utilities using the HS256 algorithm. Handles all token operations
 * for the authentication system.
 *
 * USAGE:
 * // Create a token
 * std::string token = JwtUtils::createToken("user-id", "user", 1);
 *
 * // Validate a token
 * auto payload = JwtUtils::validateToken(token);
 * if (payload) {
 *     // Token is valid
 * }
 *
 * DEPENDENCIES:
 * - jwt-cpp library for JWT operations
 * - Config (Agent 1) - JWT secret configuration
 * - ErrorCapture (Agent 1) - Error logging
 *
 * MODIFICATION GUIDE:
 * - Use HS256 algorithm as specified
 * - Include user_id, role, iat, exp in payload
 * - Token expiry: access 1 hour, refresh 7 days
 *
 * @author Agent 6 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes (alphabetical)
#include <chrono>
#include <optional>
#include <string>

// Project includes (alphabetical)
#include "core/auth/AuthToken.h"

namespace wtl::core::auth {

/**
 * @class JwtUtils
 * @brief Static utility class for JWT operations
 *
 * Provides methods for creating, validating, and decoding JWT tokens.
 * All methods are static and thread-safe.
 */
class JwtUtils {
public:
    // ========================================================================
    // CONSTANTS
    // ========================================================================

    /**
     * @brief Default access token expiry in hours
     */
    static constexpr int DEFAULT_ACCESS_EXPIRY_HOURS = 1;

    /**
     * @brief Default refresh token expiry in days
     */
    static constexpr int DEFAULT_REFRESH_EXPIRY_DAYS = 7;

    /**
     * @brief Token type identifiers
     */
    static constexpr const char* TOKEN_TYPE_ACCESS = "access";
    static constexpr const char* TOKEN_TYPE_REFRESH = "refresh";

    /**
     * @brief JWT issuer identifier
     */
    static constexpr const char* ISSUER = "waitingthelongest.com";

    // ========================================================================
    // PUBLIC METHODS
    // ========================================================================

    /**
     * @brief Create a JWT access token
     *
     * Creates a signed JWT token containing the user's ID and role.
     * Uses HS256 algorithm with the configured secret.
     *
     * @param user_id The user's UUID
     * @param role The user's role (user, foster, shelter_admin, admin)
     * @param expiry_hours Token validity in hours (default: 1)
     * @return Signed JWT token string
     * @throws std::runtime_error if token creation fails
     *
     * @example
     * std::string token = JwtUtils::createToken("user-uuid", "user");
     * // Returns: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
     */
    static std::string createToken(const std::string& user_id,
                                    const std::string& role,
                                    int expiry_hours = DEFAULT_ACCESS_EXPIRY_HOURS);

    /**
     * @brief Create a JWT refresh token
     *
     * Creates a longer-lived refresh token for obtaining new access tokens.
     * Contains minimal claims for security.
     *
     * @param user_id The user's UUID
     * @param expiry_days Token validity in days (default: 7)
     * @return Signed JWT refresh token string
     * @throws std::runtime_error if token creation fails
     *
     * @example
     * std::string refresh = JwtUtils::createRefreshToken("user-uuid");
     */
    static std::string createRefreshToken(const std::string& user_id,
                                          int expiry_days = DEFAULT_REFRESH_EXPIRY_DAYS);

    /**
     * @brief Validate a JWT token and return its payload
     *
     * Verifies the token signature and checks expiration.
     * Returns the decoded payload if valid.
     *
     * @param token The JWT token string to validate
     * @return TokenPayload if valid, std::nullopt if invalid
     *
     * @note Does not check if token has been revoked (use SessionManager for that)
     *
     * @example
     * auto payload = JwtUtils::validateToken(token);
     * if (payload) {
     *     std::cout << "User: " << payload->user_id << std::endl;
     * }
     */
    static std::optional<TokenPayload> validateToken(const std::string& token);

    /**
     * @brief Decode a JWT token without validation
     *
     * Extracts the payload from a token without verifying the signature.
     * Useful for extracting information from expired tokens.
     *
     * @param token The JWT token string to decode
     * @return TokenPayload extracted from token
     * @throws std::runtime_error if token format is invalid
     *
     * @warning This does not verify the token - use validateToken for security
     *
     * @example
     * TokenPayload payload = JwtUtils::decodeToken(expired_token);
     */
    static TokenPayload decodeToken(const std::string& token);

    /**
     * @brief Check if a token is expired
     *
     * Quick check for token expiration without full validation.
     *
     * @param token The JWT token string
     * @return true if token is expired or invalid, false if still valid
     *
     * @example
     * if (JwtUtils::isExpired(token)) {
     *     // Prompt for login or refresh
     * }
     */
    static bool isExpired(const std::string& token);

    /**
     * @brief Get the expiration time of a token
     *
     * Extracts the expiration timestamp from a token.
     *
     * @param token The JWT token string
     * @return Expiration time point
     * @throws std::runtime_error if token format is invalid
     *
     * @example
     * auto expires_at = JwtUtils::getTokenExpiry(token);
     * auto remaining = expires_at - std::chrono::system_clock::now();
     */
    static std::chrono::system_clock::time_point getTokenExpiry(const std::string& token);

    /**
     * @brief Extract user ID from a token without validation
     *
     * Quick extraction of user ID for logging or lookup.
     *
     * @param token The JWT token string
     * @return User ID if extractable, empty string otherwise
     */
    static std::string extractUserId(const std::string& token);

    /**
     * @brief Check if a token is a refresh token
     *
     * @param token The JWT token string
     * @return true if token type is "refresh"
     */
    static bool isRefreshToken(const std::string& token);

    /**
     * @brief Set the JWT secret (for initialization)
     *
     * Should be called during application startup with the secret
     * from configuration. Must be at least 32 characters.
     *
     * @param secret The HS256 signing secret
     * @throws std::invalid_argument if secret is too short
     */
    static void setSecret(const std::string& secret);

    /**
     * @brief Check if the JWT secret has been configured
     *
     * @return true if setSecret has been called with a valid secret
     */
    static bool isSecretConfigured();

private:
    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================

    /**
     * @brief The HS256 signing secret
     *
     * Must be set via setSecret() before any token operations.
     * Minimum 32 characters for security.
     */
    static std::string jwt_secret_;

    /**
     * @brief Flag indicating if secret is configured
     */
    static bool secret_configured_;

    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Get the configured secret or throw
     * @return The JWT secret
     * @throws std::runtime_error if secret not configured
     */
    static const std::string& getSecret();

    // Prevent instantiation - all methods are static
    JwtUtils() = delete;
    ~JwtUtils() = delete;
};

} // namespace wtl::core::auth
