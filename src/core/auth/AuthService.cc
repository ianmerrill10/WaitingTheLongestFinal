/**
 * @file AuthService.cc
 * @brief Implementation of authentication service
 * @see AuthService.h for documentation
 */

#include "core/auth/AuthService.h"

// Standard library includes
#include <algorithm>
#include <chrono>
#include <regex>

// Third-party includes
#include <pqxx/pqxx>

// Project includes
#include "core/auth/JwtUtils.h"
#include "core/auth/PasswordUtils.h"
#include "core/auth/SessionManager.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/services/UserService.h"

namespace wtl::core::auth {

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

AuthService& AuthService::getInstance() {
    static AuthService instance;
    return instance;
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

AuthResult AuthService::login(const std::string& email,
                               const std::string& password,
                               const std::string& ip_address,
                               const std::string& user_agent) {
    // Security: Never log password

    // Validate inputs
    if (email.empty()) {
        return AuthResult::fail("Email is required", ErrorCodes::INVALID_CREDENTIALS);
    }
    if (password.empty()) {
        return AuthResult::fail("Password is required", ErrorCodes::INVALID_CREDENTIALS);
    }

    // Normalize email (lowercase, trim)
    std::string normalized_email = email;
    std::transform(normalized_email.begin(), normalized_email.end(),
                   normalized_email.begin(), ::tolower);

    try {
        // Get user by email
        auto& user_service = wtl::core::services::UserService::getInstance();
        auto user = user_service.findByEmail(normalized_email);

        if (!user) {
            // Don't reveal whether email exists
            return AuthResult::fail("Invalid email or password", ErrorCodes::INVALID_CREDENTIALS);
        }

        // Check if user is active
        if (!user->is_active) {
            return AuthResult::fail("Account is disabled", ErrorCodes::USER_DISABLED);
        }

        // Verify password
        if (!PasswordUtils::verifyPassword(password, user->password_hash)) {
            return AuthResult::fail("Invalid email or password", ErrorCodes::INVALID_CREDENTIALS);
        }

        // Check email verification if required
        // Note: You might want to make this configurable
        // if (!user->email_verified) {
        //     return AuthResult::fail("Please verify your email address", ErrorCodes::EMAIL_NOT_VERIFIED);
        // }

        // Update last login timestamp
        user_service.updateLastLogin(user->id);

        // Create tokens and session
        return createAuthSession(user->id, user->role, ip_address, user_agent);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Login failed: " + std::string(e.what()),
            {{"email", normalized_email}, {"operation", "login"}}
        );
        return AuthResult::fail("An error occurred during login", "AUTH_ERROR");
    }
}

AuthResult AuthService::refreshToken(const std::string& refresh_token) {
    if (refresh_token.empty()) {
        return AuthResult::fail("Refresh token is required", ErrorCodes::INVALID_REFRESH_TOKEN);
    }

    try {
        // Validate refresh token format and signature
        auto payload = JwtUtils::validateToken(refresh_token);
        if (!payload) {
            return AuthResult::fail("Invalid refresh token", ErrorCodes::INVALID_REFRESH_TOKEN);
        }

        // Check if it's actually a refresh token
        if (!JwtUtils::isRefreshToken(refresh_token)) {
            return AuthResult::fail("Invalid token type", ErrorCodes::INVALID_REFRESH_TOKEN);
        }

        // Check session exists and is valid
        auto& session_manager = SessionManager::getInstance();
        auto session = session_manager.getSessionByRefreshToken(refresh_token);
        if (!session) {
            return AuthResult::fail("Session not found or expired", ErrorCodes::SESSION_EXPIRED);
        }

        // Get user to verify they're still active and get current role
        auto& user_service = wtl::core::services::UserService::getInstance();
        auto user = user_service.findById(payload->user_id);
        if (!user) {
            return AuthResult::fail("User not found", ErrorCodes::USER_NOT_FOUND);
        }

        if (!user->is_active) {
            // Invalidate the session since user is disabled
            session_manager.invalidateSession(session->token_hash);
            return AuthResult::fail("Account is disabled", ErrorCodes::USER_DISABLED);
        }

        // Create new tokens
        std::string new_access_token = JwtUtils::createToken(user->id, user->role);
        std::string new_refresh_token = JwtUtils::createRefreshToken(user->id);

        auto new_expires_at = std::chrono::system_clock::now() + std::chrono::hours(24 * 7);

        // Update session with new tokens
        bool updated = session_manager.updateSessionTokens(
            refresh_token,
            new_access_token,
            new_refresh_token,
            new_expires_at
        );

        if (!updated) {
            return AuthResult::fail("Failed to refresh session", "AUTH_ERROR");
        }

        // Build result
        AuthToken auth_token;
        auth_token.token = new_access_token;
        auth_token.user_id = user->id;
        auth_token.role = user->role;
        auth_token.created_at = std::chrono::system_clock::now();
        auth_token.expires_at = auth_token.created_at + std::chrono::hours(1);

        return AuthResult::ok(auth_token, new_refresh_token);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Token refresh failed: " + std::string(e.what()),
            {{"operation", "refreshToken"}}
        );
        return AuthResult::fail("Failed to refresh token", "AUTH_ERROR");
    }
}

void AuthService::logout(const std::string& token) {
    if (token.empty()) {
        return;
    }

    try {
        SessionManager::getInstance().invalidateSession(token);
    } catch (const std::exception& e) {
        // Log but don't throw - logout should always "succeed"
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Logout failed: " + std::string(e.what()),
            {{"operation", "logout"}}
        );
    }
}

int AuthService::logoutAll(const std::string& token) {
    if (token.empty()) {
        return 0;
    }

    try {
        // Validate token to get user ID
        auto payload = JwtUtils::validateToken(token);
        if (!payload) {
            return 0;
        }

        return SessionManager::getInstance().invalidateAllUserSessions(payload->user_id);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Logout all failed: " + std::string(e.what()),
            {{"operation", "logoutAll"}}
        );
        return 0;
    }
}

// ============================================================================
// TOKEN VALIDATION
// ============================================================================

std::optional<AuthToken> AuthService::validateToken(const std::string& token) {
    if (token.empty()) {
        return std::nullopt;
    }

    try {
        // Validate JWT signature and expiration
        auto payload = JwtUtils::validateToken(token);
        if (!payload) {
            return std::nullopt;
        }

        // Check session is still valid (not revoked)
        if (!SessionManager::getInstance().isSessionValid(token)) {
            return std::nullopt;
        }

        // Update session activity
        SessionManager::getInstance().touchSession(token);

        // Convert to AuthToken
        return payload->toAuthToken(token);

    } catch (const std::exception& e) {
        // Invalid token
        return std::nullopt;
    }
}

bool AuthService::hasRole(const std::string& token, const std::string& required_role) {
    auto auth_token = validateToken(token);
    if (!auth_token) {
        return false;
    }

    return Roles::hasPermission(auth_token->role, required_role);
}

bool AuthService::isAdmin(const std::string& token) {
    return hasRole(token, Roles::ADMIN);
}

// ============================================================================
// REGISTRATION
// ============================================================================

AuthResult AuthService::registerUser(const std::string& email,
                                      const std::string& password,
                                      const std::string& display_name) {
    // Security: Never log password

    // Validate email
    if (email.empty()) {
        return AuthResult::fail("Email is required", ErrorCodes::INVALID_CREDENTIALS);
    }

    if (!isValidEmail(email)) {
        return AuthResult::fail("Invalid email format", ErrorCodes::INVALID_CREDENTIALS);
    }

    // Normalize email
    std::string normalized_email = email;
    std::transform(normalized_email.begin(), normalized_email.end(),
                   normalized_email.begin(), ::tolower);

    // Validate password
    std::string password_error = PasswordUtils::validatePassword(password);
    if (!password_error.empty()) {
        return AuthResult::fail(password_error, ErrorCodes::PASSWORD_TOO_WEAK);
    }

    // Validate display name
    if (display_name.empty()) {
        return AuthResult::fail("Display name is required", ErrorCodes::INVALID_CREDENTIALS);
    }

    if (display_name.length() > 100) {
        return AuthResult::fail("Display name is too long", ErrorCodes::INVALID_CREDENTIALS);
    }

    try {
        auto& user_service = wtl::core::services::UserService::getInstance();

        // Check if email already exists
        auto existing = user_service.findByEmail(normalized_email);
        if (existing) {
            return AuthResult::fail("Email already registered", ErrorCodes::EMAIL_ALREADY_EXISTS);
        }

        // Hash password
        std::string password_hash = PasswordUtils::hashPassword(password);

        // Create user
        wtl::core::models::User new_user;
        new_user.email = normalized_email;
        new_user.password_hash = password_hash;
        new_user.display_name = display_name;
        new_user.role = Roles::USER;  // Default role
        new_user.is_active = true;
        new_user.email_verified = false;  // Requires email verification

        auto saved_user = user_service.save(new_user);

        // Generate verification token and store it
        std::string verification_token = PasswordUtils::generateSecureToken();
        // Store verification token and send email
        try {
            auto conn = wtl::core::db::ConnectionPool::getInstance().getConnection();
            pqxx::work txn(*conn);
            txn.exec_params(
                "UPDATE users SET email_verification_token = $1, email_verified = false WHERE id = $2",
                verification_token_hash, user_id);
            txn.commit();

            LOG_INFO << "Email verification token stored for user: " << user_id;
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to store verification token: " << e.what();
        }

        // Auto-login: create tokens and session
        return createAuthSession(saved_user.id, saved_user.role);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Registration failed: " + std::string(e.what()),
            {{"email", normalized_email}, {"operation", "registerUser"}}
        );
        return AuthResult::fail("An error occurred during registration", "AUTH_ERROR");
    }
}

bool AuthService::verifyEmail(const std::string& verification_token) {
    if (verification_token.empty()) {
        return false;
    }

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find user with this verification token
        // Hash the token for lookup
        std::string token_hash = PasswordUtils::hashToken(verification_token);

        auto result = txn.exec_params(
            "UPDATE users SET email_verified = true, "
            "email_verification_token = NULL, updated_at = NOW() "
            "WHERE email_verification_token = $1 AND email_verified = false "
            "RETURNING id",
            token_hash
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return !result.empty();

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Email verification failed: " + std::string(e.what()),
            {{"operation", "verifyEmail"}}
        );
        return false;
    }
}

bool AuthService::resendVerificationEmail(const std::string& email) {
    if (email.empty()) {
        return false;
    }

    std::string normalized_email = email;
    std::transform(normalized_email.begin(), normalized_email.end(),
                   normalized_email.begin(), ::tolower);

    try {
        auto& user_service = wtl::core::services::UserService::getInstance();
        auto user = user_service.findByEmail(normalized_email);

        if (!user || user->email_verified) {
            // Always return true to prevent enumeration
            return true;
        }

        // Generate new verification token
        std::string verification_token = PasswordUtils::generateSecureToken();
        std::string token_hash = PasswordUtils::hashToken(verification_token);

        // Store token
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE users SET email_verification_token = $1, updated_at = NOW() "
            "WHERE id = $2",
            token_hash,
            user->id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Send verification email via notification service
        try {
            LOG_INFO << "Sending verification email to: " << email;
            // Email will be sent when NotificationService is configured with SendGrid
            // For now, log the token for manual verification in development
            LOG_DEBUG << "Verification token (dev only): " << verification_token;
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to send verification email: " << e.what();
        }

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Resend verification failed: " + std::string(e.what()),
            {{"email", normalized_email}, {"operation", "resendVerificationEmail"}}
        );
        // Return true to prevent enumeration
        return true;
    }
}

// ============================================================================
// PASSWORD MANAGEMENT
// ============================================================================

bool AuthService::requestPasswordReset(const std::string& email) {
    // Always return true to prevent email enumeration
    if (email.empty()) {
        return true;
    }

    std::string normalized_email = email;
    std::transform(normalized_email.begin(), normalized_email.end(),
                   normalized_email.begin(), ::tolower);

    try {
        auto& user_service = wtl::core::services::UserService::getInstance();
        auto user = user_service.findByEmail(normalized_email);

        if (!user || !user->is_active) {
            // Return true to prevent enumeration
            return true;
        }

        // Generate reset token (valid for 1 hour)
        std::string reset_token = PasswordUtils::generateSecureToken();
        std::string token_hash = PasswordUtils::hashToken(reset_token);

        auto expires_at = std::chrono::system_clock::now() + std::chrono::hours(1);
        auto expires_ts = std::chrono::duration_cast<std::chrono::seconds>(
            expires_at.time_since_epoch()).count();

        // Store token in database
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE users SET password_reset_token = $1, "
            "password_reset_expires = to_timestamp($2), updated_at = NOW() "
            "WHERE id = $3",
            token_hash,
            expires_ts,
            user->id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Send password reset email
        try {
            LOG_INFO << "Sending password reset email to: " << email;
            // Email will be sent when NotificationService is configured with SendGrid
            // For now, log the token for manual reset in development
            LOG_DEBUG << "Reset token (dev only): " << reset_token;
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to send reset email: " << e.what();
        }

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Password reset request failed: " + std::string(e.what()),
            {{"email", normalized_email}, {"operation", "requestPasswordReset"}}
        );
        // Return true to prevent enumeration
        return true;
    }
}

bool AuthService::resetPassword(const std::string& reset_token,
                                 const std::string& new_password) {
    // Security: Never log password

    if (reset_token.empty()) {
        return false;
    }

    // Validate new password
    std::string password_error = PasswordUtils::validatePassword(new_password);
    if (!password_error.empty()) {
        return false;
    }

    try {
        std::string token_hash = PasswordUtils::hashToken(reset_token);

        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find user with valid reset token
        auto result = txn.exec_params(
            "SELECT id FROM users "
            "WHERE password_reset_token = $1 "
            "AND password_reset_expires > NOW() "
            "AND is_active = true",
            token_hash
        );

        if (result.empty()) {
            txn.commit();
            wtl::core::db::ConnectionPool::getInstance().release(conn);
            return false;
        }

        std::string user_id = result[0]["id"].as<std::string>();

        // Hash new password
        std::string password_hash = PasswordUtils::hashPassword(new_password);

        // Update password and clear reset token
        txn.exec_params(
            "UPDATE users SET password_hash = $1, "
            "password_reset_token = NULL, password_reset_expires = NULL, "
            "updated_at = NOW() WHERE id = $2",
            password_hash,
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Invalidate all sessions for security
        SessionManager::getInstance().invalidateAllUserSessions(user_id);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Password reset failed: " + std::string(e.what()),
            {{"operation", "resetPassword"}}
        );
        return false;
    }
}

bool AuthService::changePassword(const std::string& user_id,
                                  const std::string& old_password,
                                  const std::string& new_password,
                                  bool invalidate_sessions) {
    // Security: Never log passwords

    if (user_id.empty() || old_password.empty() || new_password.empty()) {
        return false;
    }

    // Validate new password
    std::string password_error = PasswordUtils::validatePassword(new_password);
    if (!password_error.empty()) {
        return false;
    }

    try {
        auto& user_service = wtl::core::services::UserService::getInstance();
        auto user = user_service.findById(user_id);

        if (!user || !user->is_active) {
            return false;
        }

        // Verify old password
        if (!PasswordUtils::verifyPassword(old_password, user->password_hash)) {
            return false;
        }

        // Hash new password
        std::string password_hash = PasswordUtils::hashPassword(new_password);

        // Update password in database
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE users SET password_hash = $1, updated_at = NOW() WHERE id = $2",
            password_hash,
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Optionally invalidate other sessions
        if (invalidate_sessions) {
            SessionManager::getInstance().invalidateAllUserSessions(user_id);
        }

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Password change failed: " + std::string(e.what()),
            {{"user_id", user_id}, {"operation", "changePassword"}}
        );
        return false;
    }
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void AuthService::initialize(const std::string& jwt_secret) {
    std::lock_guard<std::mutex> lock(mutex_);

    JwtUtils::setSecret(jwt_secret);
    initialized_ = true;
}

bool AuthService::isInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_ && JwtUtils::isSecretConfigured();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

AuthResult AuthService::createAuthSession(const std::string& user_id,
                                           const std::string& role,
                                           const std::string& ip_address,
                                           const std::string& user_agent) {
    try {
        // Create access and refresh tokens
        std::string access_token = JwtUtils::createToken(user_id, role);
        std::string refresh_token = JwtUtils::createRefreshToken(user_id);

        // Create session in database
        SessionManager::getInstance().createSession(
            user_id, access_token, refresh_token, ip_address, user_agent
        );

        // Build auth token
        AuthToken auth_token;
        auth_token.token = access_token;
        auth_token.user_id = user_id;
        auth_token.role = role;
        auth_token.created_at = std::chrono::system_clock::now();
        auth_token.expires_at = auth_token.created_at + std::chrono::hours(1);

        return AuthResult::ok(auth_token, refresh_token);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::AUTHENTICATION,
            "Failed to create auth session: " + std::string(e.what()),
            {{"user_id", user_id}, {"operation", "createAuthSession"}}
        );
        return AuthResult::fail("Failed to create session", "AUTH_ERROR");
    }
}

bool AuthService::isValidEmail(const std::string& email) const {
    // Basic email validation regex
    static const std::regex email_regex(
        R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})"
    );

    return std::regex_match(email, email_regex);
}

} // namespace wtl::core::auth
