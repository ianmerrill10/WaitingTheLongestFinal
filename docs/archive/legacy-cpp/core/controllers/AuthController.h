/**
 * @file AuthController.h
 * @brief REST API controller for authentication endpoints
 *
 * PURPOSE:
 * Provides HTTP endpoints for user authentication operations including
 * login, logout, registration, password management, and token refresh.
 * All endpoints follow the standard API response format.
 *
 * ENDPOINTS:
 * POST /api/auth/register          - Register new user
 * POST /api/auth/login             - Login, returns tokens
 * POST /api/auth/logout            - Logout (invalidate session)
 * POST /api/auth/refresh           - Refresh access token
 * POST /api/auth/verify-email      - Verify email with token
 * POST /api/auth/forgot-password   - Request password reset
 * POST /api/auth/reset-password    - Reset password with token
 * POST /api/auth/change-password   - Change password (auth required)
 * GET  /api/auth/me                - Get current user info (auth required)
 *
 * USAGE:
 * // Register
 * POST /api/auth/register
 * {"email": "user@example.com", "password": "SecurePass123!", "display_name": "John"}
 *
 * // Login
 * POST /api/auth/login
 * {"email": "user@example.com", "password": "SecurePass123!"}
 *
 * DEPENDENCIES:
 * - AuthService - Authentication logic
 * - UserService (Agent 3) - User data access
 * - ApiResponse (Agent 4) - Response formatting
 *
 * MODIFICATION GUIDE:
 * - All endpoints should validate input
 * - Use AuthMiddleware macros for protected endpoints
 * - Add rate limiting notes where applicable
 * - Follow REST conventions
 *
 * @author Agent 6 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Third-party includes (alphabetical)
#include <drogon/HttpController.h>

namespace wtl::core::controllers {

/**
 * @class AuthController
 * @brief HTTP controller for authentication endpoints
 *
 * Handles all authentication-related HTTP requests.
 * Uses Drogon's controller pattern with METHOD_LIST macros.
 */
class AuthController : public drogon::HttpController<AuthController> {
public:
    // ========================================================================
    // ROUTE REGISTRATION
    // ========================================================================

    METHOD_LIST_BEGIN
    // Registration
    ADD_METHOD_TO(AuthController::registerUser, "/api/auth/register", drogon::Post);

    // Authentication
    ADD_METHOD_TO(AuthController::login, "/api/auth/login", drogon::Post);
    ADD_METHOD_TO(AuthController::logout, "/api/auth/logout", drogon::Post);
    ADD_METHOD_TO(AuthController::refresh, "/api/auth/refresh", drogon::Post);

    // Email verification
    ADD_METHOD_TO(AuthController::verifyEmail, "/api/auth/verify-email", drogon::Post);
    ADD_METHOD_TO(AuthController::resendVerification, "/api/auth/resend-verification", drogon::Post);

    // Password management
    ADD_METHOD_TO(AuthController::forgotPassword, "/api/auth/forgot-password", drogon::Post);
    ADD_METHOD_TO(AuthController::resetPassword, "/api/auth/reset-password", drogon::Post);
    ADD_METHOD_TO(AuthController::changePassword, "/api/auth/change-password", drogon::Post);

    // User info
    ADD_METHOD_TO(AuthController::me, "/api/auth/me", drogon::Get);
    ADD_METHOD_TO(AuthController::sessions, "/api/auth/sessions", drogon::Get);
    ADD_METHOD_TO(AuthController::logoutAll, "/api/auth/logout-all", drogon::Post);
    METHOD_LIST_END

    // ========================================================================
    // REGISTRATION ENDPOINTS
    // ========================================================================

    /**
     * @brief Register a new user account
     *
     * Request body:
     * {
     *     "email": "user@example.com",
     *     "password": "SecurePass123!",
     *     "display_name": "John Doe"
     * }
     *
     * Response (201):
     * {
     *     "success": true,
     *     "data": {
     *         "access_token": "...",
     *         "refresh_token": "...",
     *         "expires_at": 1706500000,
     *         "user": { "id": "...", "role": "user" }
     *     }
     * }
     *
     * Errors:
     * - 400: Invalid input (missing fields, weak password)
     * - 409: Email already registered
     * - 500: Server error
     *
     * @note Rate limiting recommended: 5 requests per minute per IP
     */
    void registerUser(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // AUTHENTICATION ENDPOINTS
    // ========================================================================

    /**
     * @brief Authenticate user and return tokens
     *
     * Request body:
     * {
     *     "email": "user@example.com",
     *     "password": "SecurePass123!"
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "access_token": "...",
     *         "refresh_token": "...",
     *         "token_type": "Bearer",
     *         "expires_at": 1706500000,
     *         "user": { "id": "...", "role": "user" }
     *     }
     * }
     *
     * Errors:
     * - 400: Missing credentials
     * - 401: Invalid credentials
     * - 403: Account disabled or unverified
     *
     * @note Rate limiting recommended: 10 requests per minute per IP
     */
    void login(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Invalidate current session
     *
     * Requires: Authorization header with Bearer token
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "message": "Logged out successfully" }
     * }
     */
    void logout(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Refresh access token using refresh token
     *
     * Request body:
     * {
     *     "refresh_token": "..."
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "access_token": "...",
     *         "refresh_token": "...",
     *         "expires_at": 1706500000
     *     }
     * }
     *
     * Errors:
     * - 400: Missing refresh token
     * - 401: Invalid or expired refresh token
     */
    void refresh(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // EMAIL VERIFICATION ENDPOINTS
    // ========================================================================

    /**
     * @brief Verify email address with token
     *
     * Request body:
     * {
     *     "token": "verification-token-from-email"
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "message": "Email verified successfully" }
     * }
     *
     * Errors:
     * - 400: Invalid or expired token
     */
    void verifyEmail(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Resend email verification
     *
     * Request body:
     * {
     *     "email": "user@example.com"
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "message": "Verification email sent if account exists" }
     * }
     *
     * @note Always returns success to prevent email enumeration
     */
    void resendVerification(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // PASSWORD MANAGEMENT ENDPOINTS
    // ========================================================================

    /**
     * @brief Request password reset email
     *
     * Request body:
     * {
     *     "email": "user@example.com"
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "message": "Password reset email sent if account exists" }
     * }
     *
     * @note Always returns success to prevent email enumeration
     * @note Rate limiting recommended: 3 requests per hour per email
     */
    void forgotPassword(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Reset password with token from email
     *
     * Request body:
     * {
     *     "token": "reset-token-from-email",
     *     "new_password": "NewSecurePass123!"
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "message": "Password reset successfully" }
     * }
     *
     * Errors:
     * - 400: Invalid/expired token or weak password
     */
    void resetPassword(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Change password for authenticated user
     *
     * Requires: Authorization header with Bearer token
     *
     * Request body:
     * {
     *     "current_password": "OldPassword123!",
     *     "new_password": "NewSecurePass123!"
     * }
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "message": "Password changed successfully" }
     * }
     *
     * Errors:
     * - 400: Invalid current password or weak new password
     * - 401: Not authenticated
     */
    void changePassword(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // USER INFO ENDPOINTS
    // ========================================================================

    /**
     * @brief Get current authenticated user info
     *
     * Requires: Authorization header with Bearer token
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "id": "user-uuid",
     *         "email": "user@example.com",
     *         "display_name": "John Doe",
     *         "role": "user",
     *         "email_verified": true,
     *         "created_at": "2024-01-28T00:00:00Z"
     *     }
     * }
     *
     * Errors:
     * - 401: Not authenticated
     */
    void me(const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get active sessions for current user
     *
     * Requires: Authorization header with Bearer token
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": {
     *         "sessions": [
     *             {
     *                 "id": "session-id",
     *                 "ip_address": "192.168.1.1",
     *                 "user_agent": "Mozilla/5.0...",
     *                 "created_at": "2024-01-28T00:00:00Z",
     *                 "last_used_at": "2024-01-28T12:00:00Z"
     *             }
     *         ]
     *     }
     * }
     */
    void sessions(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Logout from all devices
     *
     * Requires: Authorization header with Bearer token
     *
     * Response (200):
     * {
     *     "success": true,
     *     "data": { "message": "Logged out from 3 devices" }
     * }
     */
    void logoutAll(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Extract client IP address from request
     */
    std::string getClientIp(const drogon::HttpRequestPtr& req) const;

    /**
     * @brief Extract user agent from request
     */
    std::string getUserAgent(const drogon::HttpRequestPtr& req) const;
};

} // namespace wtl::core::controllers
