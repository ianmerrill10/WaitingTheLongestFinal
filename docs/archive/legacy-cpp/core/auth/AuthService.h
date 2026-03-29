/**
 * @file AuthService.h
 * @brief Authentication service for user login, registration, and token management
 *
 * PURPOSE:
 * Provides the primary authentication interface for the application.
 * Handles user login, registration, password management, and token
 * operations. This is the main entry point for all auth operations.
 *
 * USAGE:
 * auto& auth = AuthService::getInstance();
 *
 * // Login
 * auto result = auth.login("user@example.com", "password");
 * if (result.success) {
 *     std::string token = result.token->token;
 * }
 *
 * // Register
 * auto result = auth.registerUser("user@example.com", "password", "John Doe");
 *
 * DEPENDENCIES:
 * - UserService (Agent 3) - User data access
 * - SessionManager - Session management
 * - JwtUtils - Token creation/validation
 * - PasswordUtils - Password hashing
 * - ConnectionPool (Agent 1) - Database access
 * - ErrorCapture (Agent 1) - Error logging
 *
 * MODIFICATION GUIDE:
 * - All auth operations should use WTL_CAPTURE_ERROR for errors
 * - Never log passwords or tokens
 * - Always validate inputs before processing
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
#include <chrono>
#include <mutex>
#include <optional>
#include <string>

// Project includes (alphabetical)
#include "core/auth/AuthToken.h"

namespace wtl::core::auth {

/**
 * @class AuthService
 * @brief Singleton service for authentication operations
 *
 * Thread-safe singleton that provides authentication services.
 * Coordinates between UserService, SessionManager, and token utilities.
 */
class AuthService {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the AuthService instance
     */
    static AuthService& getInstance();

    // Delete copy/move constructors and assignment operators
    AuthService(const AuthService&) = delete;
    AuthService& operator=(const AuthService&) = delete;
    AuthService(AuthService&&) = delete;
    AuthService& operator=(AuthService&&) = delete;

    // ========================================================================
    // AUTHENTICATION
    // ========================================================================

    /**
     * @brief Authenticate a user with email and password
     *
     * Validates credentials, creates access and refresh tokens,
     * and establishes a session in the database.
     *
     * @param email User's email address
     * @param password User's plaintext password (never logged)
     * @param ip_address Client IP for session tracking (optional)
     * @param user_agent Client user agent (optional)
     * @return AuthResult with tokens on success, error on failure
     *
     * @note Rate limiting should be applied at the controller level
     *
     * @example
     * auto result = auth.login("user@example.com", "password123");
     * if (result.success) {
     *     // Use result.token->token for API calls
     *     // Store result.refresh_token for refresh operations
     * }
     */
    AuthResult login(const std::string& email,
                     const std::string& password,
                     const std::string& ip_address = "",
                     const std::string& user_agent = "");

    /**
     * @brief Refresh an access token using a refresh token
     *
     * Validates the refresh token, invalidates the old session,
     * and creates new access and refresh tokens.
     *
     * @param refresh_token The refresh token from login
     * @return AuthResult with new tokens on success
     *
     * @note Old tokens are invalidated after successful refresh
     *
     * @example
     * auto result = auth.refreshToken(stored_refresh_token);
     * if (result.success) {
     *     // Update stored tokens
     * }
     */
    AuthResult refreshToken(const std::string& refresh_token);

    /**
     * @brief Logout a user by invalidating their session
     *
     * Marks the session as inactive, effectively revoking the tokens.
     *
     * @param token The access token to invalidate
     *
     * @example
     * auth.logout(current_token);
     */
    void logout(const std::string& token);

    /**
     * @brief Logout from all devices
     *
     * Invalidates all sessions for the user identified by the token.
     *
     * @param token Any valid access token for the user
     * @return Number of sessions invalidated
     */
    int logoutAll(const std::string& token);

    // ========================================================================
    // TOKEN VALIDATION
    // ========================================================================

    /**
     * @brief Validate an access token
     *
     * Checks token signature, expiration, and session validity.
     *
     * @param token The access token to validate
     * @return AuthToken if valid, std::nullopt if invalid
     *
     * @example
     * auto auth_token = auth.validateToken(token);
     * if (auth_token) {
     *     std::string user_id = auth_token->user_id;
     * }
     */
    std::optional<AuthToken> validateToken(const std::string& token);

    /**
     * @brief Check if a token has a specific role or higher
     *
     * Uses role hierarchy: admin > shelter_admin > foster > user
     *
     * @param token The access token
     * @param required_role The minimum required role
     * @return true if user has required role or higher
     *
     * @example
     * if (auth.hasRole(token, "foster")) {
     *     // User is foster, shelter_admin, or admin
     * }
     */
    bool hasRole(const std::string& token, const std::string& required_role);

    /**
     * @brief Check if a token belongs to an admin user
     *
     * Convenience method for admin-only operations.
     *
     * @param token The access token
     * @return true if user is an admin
     */
    bool isAdmin(const std::string& token);

    // ========================================================================
    // REGISTRATION
    // ========================================================================

    /**
     * @brief Register a new user account
     *
     * Creates a new user with the provided credentials.
     * Sends a verification email (if email service is configured).
     *
     * @param email User's email address (must be unique)
     * @param password User's chosen password
     * @param display_name User's display name
     * @return AuthResult with tokens on success (auto-login)
     *
     * @note Password is validated against security requirements
     *
     * @example
     * auto result = auth.registerUser("new@example.com", "SecurePass123!", "John Doe");
     * if (!result.success) {
     *     std::cerr << result.error.value() << std::endl;
     * }
     */
    AuthResult registerUser(const std::string& email,
                            const std::string& password,
                            const std::string& display_name);

    /**
     * @brief Verify a user's email address
     *
     * Marks the user's email as verified using the verification token.
     *
     * @param verification_token Token sent to user's email
     * @return true if verification succeeded
     *
     * @example
     * if (auth.verifyEmail(token_from_url)) {
     *     // Email verified successfully
     * }
     */
    bool verifyEmail(const std::string& verification_token);

    /**
     * @brief Resend email verification
     *
     * Generates a new verification token and sends email.
     *
     * @param email User's email address
     * @return true if email was sent
     */
    bool resendVerificationEmail(const std::string& email);

    // ========================================================================
    // PASSWORD MANAGEMENT
    // ========================================================================

    /**
     * @brief Request a password reset
     *
     * Generates a reset token and sends it to the user's email.
     * Token is valid for a limited time.
     *
     * @param email User's email address
     * @return true if request was processed (always returns true to prevent enumeration)
     *
     * @note Always returns true to prevent email enumeration attacks
     *
     * @example
     * auth.requestPasswordReset("user@example.com");
     * // Always show "check your email" message
     */
    bool requestPasswordReset(const std::string& email);

    /**
     * @brief Reset password using reset token
     *
     * Validates the reset token and updates the user's password.
     * Invalidates all existing sessions for security.
     *
     * @param reset_token Token from password reset email
     * @param new_password New password (validated against requirements)
     * @return true if password was reset successfully
     *
     * @example
     * if (auth.resetPassword(token_from_url, "NewSecurePass123!")) {
     *     // Password reset, user should log in
     * }
     */
    bool resetPassword(const std::string& reset_token,
                       const std::string& new_password);

    /**
     * @brief Change password for authenticated user
     *
     * Validates old password, updates to new password,
     * and optionally invalidates other sessions.
     *
     * @param user_id The user's UUID
     * @param old_password Current password (for verification)
     * @param new_password New password
     * @param invalidate_sessions Whether to log out other devices (default: true)
     * @return true if password was changed successfully
     *
     * @example
     * if (auth.changePassword(user_id, "OldPass", "NewPass123!")) {
     *     // Password changed
     * }
     */
    bool changePassword(const std::string& user_id,
                        const std::string& old_password,
                        const std::string& new_password,
                        bool invalidate_sessions = true);

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Initialize the auth service with JWT secret
     *
     * Should be called during application startup.
     *
     * @param jwt_secret Secret for signing JWT tokens (min 32 chars)
     */
    void initialize(const std::string& jwt_secret);

    /**
     * @brief Check if the service is properly initialized
     *
     * @return true if service is ready to use
     */
    bool isInitialized() const;

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    /**
     * @brief Private constructor for singleton
     */
    AuthService() = default;

    /**
     * @brief Private destructor
     */
    ~AuthService() = default;

    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Create tokens and session for authenticated user
     * @param user_id User's UUID
     * @param role User's role
     * @param ip_address Client IP
     * @param user_agent Client user agent
     * @return AuthResult with tokens
     */
    AuthResult createAuthSession(const std::string& user_id,
                                  const std::string& role,
                                  const std::string& ip_address = "",
                                  const std::string& user_agent = "");

    /**
     * @brief Validate email format
     * @param email Email to validate
     * @return true if email format is valid
     */
    bool isValidEmail(const std::string& email) const;

    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================

    /**
     * @brief Mutex for thread-safe operations
     */
    mutable std::mutex mutex_;

    /**
     * @brief Flag indicating if service is initialized
     */
    bool initialized_ = false;
};

} // namespace wtl::core::auth
