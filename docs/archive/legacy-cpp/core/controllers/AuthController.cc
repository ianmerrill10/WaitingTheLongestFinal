/**
 * @file AuthController.cc
 * @brief Implementation of authentication REST API controller
 * @see AuthController.h for documentation
 */

#include "core/controllers/AuthController.h"

// Standard library includes for rate limiting
#include <chrono>
#include <mutex>
#include <unordered_map>

// Project includes
#include "core/auth/AuthMiddleware.h"
#include "core/auth/AuthService.h"
#include "core/auth/PasswordUtils.h"
#include "core/auth/SessionManager.h"
#include "core/debug/ErrorCapture.h"
#include "core/services/UserService.h"
#include "core/utils/ApiResponse.h"

namespace wtl::core::controllers {

using namespace ::wtl::core::auth;
using namespace ::wtl::core::utils;

// ============================================================================
// REGISTRATION ENDPOINTS
// ============================================================================

void AuthController::registerUser(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // TODO: Rate limiting - 5 requests per minute per IP
    // Consider using a rate limiter middleware

    // Simple rate check using request attributes
    {
        auto clientIp = req->getPeerAddr().toIp();
        static std::unordered_map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> registerAttempts;
        static std::mutex registerMutex;
        std::lock_guard<std::mutex> lock(registerMutex);
        auto now = std::chrono::steady_clock::now();
        auto& [count, windowStart] = registerAttempts[clientIp];
        if (now - windowStart > std::chrono::minutes(1)) {
            count = 0;
            windowStart = now;
        }
        if (++count > 5) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                Json::Value(Json::objectValue));
            resp->setStatusCode(drogon::k429TooManyRequests);
            resp->addHeader("Retry-After", "60");
            callback(resp);
            return;
        }
    }

    try {
        // Parse request body
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON in request body"));
            return;
        }

        // Extract and validate required fields
        std::string email = (*json).get("email", "").asString();
        std::string password = (*json).get("password", "").asString();
        std::string display_name = (*json).get("display_name", "").asString();

        if (email.empty()) {
            callback(ApiResponse::badRequest("Email is required"));
            return;
        }
        if (password.empty()) {
            callback(ApiResponse::badRequest("Password is required"));
            return;
        }
        if (display_name.empty()) {
            callback(ApiResponse::badRequest("Display name is required"));
            return;
        }

        // Attempt registration
        auto result = AuthService::getInstance().registerUser(email, password, display_name);

        if (result.success) {
            // Return 201 Created with tokens
            auto response = drogon::HttpResponse::newHttpJsonResponse(result.toJson());
            response->setStatusCode(drogon::k201Created);
            callback(response);
        } else {
            // Determine appropriate error code
            if (result.error_code == ErrorCodes::EMAIL_ALREADY_EXISTS) {
                callback(ApiResponse::conflict(*result.error));
            } else if (result.error_code == ErrorCodes::PASSWORD_TOO_WEAK) {
                callback(ApiResponse::badRequest(*result.error));
            } else {
                callback(ApiResponse::badRequest(result.error.value_or("Registration failed")));
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Registration endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/register"}}
        );
        callback(ApiResponse::serverError("An error occurred during registration"));
    }
}

// ============================================================================
// AUTHENTICATION ENDPOINTS
// ============================================================================

void AuthController::login(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // TODO: Rate limiting - 10 requests per minute per IP
    // Consider implementing exponential backoff for failed attempts

    // Simple rate check using request attributes
    {
        auto clientIp = req->getPeerAddr().toIp();
        static std::unordered_map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> loginAttempts;
        static std::mutex loginMutex;
        std::lock_guard<std::mutex> lock(loginMutex);
        auto now = std::chrono::steady_clock::now();
        auto& [count, windowStart] = loginAttempts[clientIp];
        if (now - windowStart > std::chrono::minutes(1)) {
            count = 0;
            windowStart = now;
        }
        if (++count > 10) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                Json::Value(Json::objectValue));
            resp->setStatusCode(drogon::k429TooManyRequests);
            resp->addHeader("Retry-After", "60");
            callback(resp);
            return;
        }
    }

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON in request body"));
            return;
        }

        std::string email = (*json).get("email", "").asString();
        std::string password = (*json).get("password", "").asString();

        if (email.empty()) {
            callback(ApiResponse::badRequest("Email is required"));
            return;
        }
        if (password.empty()) {
            callback(ApiResponse::badRequest("Password is required"));
            return;
        }

        // Get client info for session tracking
        std::string ip_address = getClientIp(req);
        std::string user_agent = getUserAgent(req);

        // Attempt login
        auto result = AuthService::getInstance().login(email, password, ip_address, user_agent);

        if (result.success) {
            callback(ApiResponse::success(result.toJson()["data"]));
        } else {
            // Use 401 for invalid credentials
            callback(ApiResponse::unauthorized(result.error.value_or("Invalid credentials")));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Login endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/login"}}
        );
        callback(ApiResponse::serverError("An error occurred during login"));
    }
}

void AuthController::logout(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        // Extract token from header
        std::string token = AuthMiddleware::extractBearerToken(req);

        if (!token.empty()) {
            AuthService::getInstance().logout(token);
        }

        // Always return success to prevent information leakage
        Json::Value data;
        data["message"] = "Logged out successfully";
        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        // Log but don't fail - logout should always "succeed" from client perspective
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Logout endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/logout"}}
        );

        Json::Value data;
        data["message"] = "Logged out successfully";
        callback(ApiResponse::success(data));
    }
}

void AuthController::refresh(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON in request body"));
            return;
        }

        std::string refresh_token = (*json).get("refresh_token", "").asString();

        if (refresh_token.empty()) {
            callback(ApiResponse::badRequest("Refresh token is required"));
            return;
        }

        auto result = AuthService::getInstance().refreshToken(refresh_token);

        if (result.success) {
            callback(ApiResponse::success(result.toJson()["data"]));
        } else {
            callback(ApiResponse::unauthorized(result.error.value_or("Invalid refresh token")));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Token refresh endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/refresh"}}
        );
        callback(ApiResponse::serverError("An error occurred during token refresh"));
    }
}

// ============================================================================
// EMAIL VERIFICATION ENDPOINTS
// ============================================================================

void AuthController::verifyEmail(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON in request body"));
            return;
        }

        std::string token = (*json).get("token", "").asString();

        if (token.empty()) {
            callback(ApiResponse::badRequest("Verification token is required"));
            return;
        }

        if (AuthService::getInstance().verifyEmail(token)) {
            Json::Value data;
            data["message"] = "Email verified successfully";
            callback(ApiResponse::success(data));
        } else {
            callback(ApiResponse::badRequest("Invalid or expired verification token"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Email verification endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/verify-email"}}
        );
        callback(ApiResponse::serverError("An error occurred during email verification"));
    }
}

void AuthController::resendVerification(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // TODO: Rate limiting - 1 request per minute per email

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON in request body"));
            return;
        }

        std::string email = (*json).get("email", "").asString();

        if (email.empty()) {
            callback(ApiResponse::badRequest("Email is required"));
            return;
        }

        // Simple rate check using email address
        {
            static std::unordered_map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> resendAttempts;
            static std::mutex resendMutex;
            std::lock_guard<std::mutex> lock(resendMutex);
            auto now = std::chrono::steady_clock::now();
            auto& [count, windowStart] = resendAttempts[email];
            if (now - windowStart > std::chrono::minutes(1)) {
                count = 0;
                windowStart = now;
            }
            if (++count > 1) {
                auto resp = drogon::HttpResponse::newHttpJsonResponse(
                    Json::Value(Json::objectValue));
                resp->setStatusCode(drogon::k429TooManyRequests);
                resp->addHeader("Retry-After", "60");
                callback(resp);
                return;
            }
        }

        // Always return success to prevent enumeration
        AuthService::getInstance().resendVerificationEmail(email);

        Json::Value data;
        data["message"] = "Verification email sent if account exists";
        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Resend verification endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/resend-verification"}}
        );

        // Still return success to prevent enumeration
        Json::Value data;
        data["message"] = "Verification email sent if account exists";
        callback(ApiResponse::success(data));
    }
}

// ============================================================================
// PASSWORD MANAGEMENT ENDPOINTS
// ============================================================================

void AuthController::forgotPassword(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // TODO: Rate limiting - 3 requests per hour per email

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON in request body"));
            return;
        }

        std::string email = (*json).get("email", "").asString();

        if (email.empty()) {
            callback(ApiResponse::badRequest("Email is required"));
            return;
        }

        // Simple rate check using email address
        {
            static std::unordered_map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> forgotAttempts;
            static std::mutex forgotMutex;
            std::lock_guard<std::mutex> lock(forgotMutex);
            auto now = std::chrono::steady_clock::now();
            auto& [count, windowStart] = forgotAttempts[email];
            if (now - windowStart > std::chrono::hours(1)) {
                count = 0;
                windowStart = now;
            }
            if (++count > 3) {
                auto resp = drogon::HttpResponse::newHttpJsonResponse(
                    Json::Value(Json::objectValue));
                resp->setStatusCode(drogon::k429TooManyRequests);
                resp->addHeader("Retry-After", "3600");
                callback(resp);
                return;
            }
        }

        // Always return success to prevent enumeration
        AuthService::getInstance().requestPasswordReset(email);

        Json::Value data;
        data["message"] = "Password reset email sent if account exists";
        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Forgot password endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/forgot-password"}}
        );

        // Still return success to prevent enumeration
        Json::Value data;
        data["message"] = "Password reset email sent if account exists";
        callback(ApiResponse::success(data));
    }
}

void AuthController::resetPassword(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON in request body"));
            return;
        }

        std::string token = (*json).get("token", "").asString();
        std::string new_password = (*json).get("new_password", "").asString();

        if (token.empty()) {
            callback(ApiResponse::badRequest("Reset token is required"));
            return;
        }
        if (new_password.empty()) {
            callback(ApiResponse::badRequest("New password is required"));
            return;
        }

        // Validate password strength
        std::string password_error = PasswordUtils::validatePassword(new_password);
        if (!password_error.empty()) {
            callback(ApiResponse::badRequest(password_error));
            return;
        }

        if (AuthService::getInstance().resetPassword(token, new_password)) {
            Json::Value data;
            data["message"] = "Password reset successfully. Please log in with your new password.";
            callback(ApiResponse::success(data));
        } else {
            callback(ApiResponse::badRequest("Invalid or expired reset token"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Reset password endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/reset-password"}}
        );
        callback(ApiResponse::serverError("An error occurred during password reset"));
    }
}

void AuthController::changePassword(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        // Require authentication
        REQUIRE_AUTH(req, callback);

        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON in request body"));
            return;
        }

        std::string current_password = (*json).get("current_password", "").asString();
        std::string new_password = (*json).get("new_password", "").asString();

        if (current_password.empty()) {
            callback(ApiResponse::badRequest("Current password is required"));
            return;
        }
        if (new_password.empty()) {
            callback(ApiResponse::badRequest("New password is required"));
            return;
        }

        // Validate password strength
        std::string password_error = PasswordUtils::validatePassword(new_password);
        if (!password_error.empty()) {
            callback(ApiResponse::badRequest(password_error));
            return;
        }

        if (AuthService::getInstance().changePassword(auth_token->user_id, current_password, new_password)) {
            Json::Value data;
            data["message"] = "Password changed successfully";
            callback(ApiResponse::success(data));
        } else {
            callback(ApiResponse::badRequest("Invalid current password"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Change password endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/change-password"}}
        );
        callback(ApiResponse::serverError("An error occurred during password change"));
    }
}

// ============================================================================
// USER INFO ENDPOINTS
// ============================================================================

void AuthController::me(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        // Require authentication
        REQUIRE_AUTH(req, callback);

        // Get user details
        auto& user_service = wtl::core::services::UserService::getInstance();
        auto user = user_service.findById(auth_token->user_id);

        if (!user) {
            callback(ApiResponse::notFound("User"));
            return;
        }

        // Return user info (excludes password_hash)
        callback(ApiResponse::success(user->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Get user info endpoint error: " + std::string(e.what()),
            {{"endpoint", "GET /api/auth/me"}}
        );
        callback(ApiResponse::serverError("An error occurred"));
    }
}

void AuthController::sessions(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        // Require authentication
        REQUIRE_AUTH(req, callback);

        // Get active sessions
        auto active_sessions = SessionManager::getInstance().getUserActiveSessions(auth_token->user_id);

        Json::Value data;
        Json::Value sessions_array(Json::arrayValue);

        for (const auto& session : active_sessions) {
            Json::Value session_json;
            session_json["id"] = session.id;
            session_json["ip_address"] = session.ip_address;
            session_json["user_agent"] = session.user_agent;
            session_json["created_at"] = static_cast<Json::Int64>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    session.created_at.time_since_epoch()
                ).count()
            );
            session_json["last_used_at"] = static_cast<Json::Int64>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    session.last_used_at.time_since_epoch()
                ).count()
            );
            sessions_array.append(session_json);
        }

        data["sessions"] = sessions_array;
        data["count"] = static_cast<int>(active_sessions.size());

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Get sessions endpoint error: " + std::string(e.what()),
            {{"endpoint", "GET /api/auth/sessions"}}
        );
        callback(ApiResponse::serverError("An error occurred"));
    }
}

void AuthController::logoutAll(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        // Require authentication
        REQUIRE_AUTH(req, callback);

        int invalidated = AuthService::getInstance().logoutAll(auth_token->token);

        Json::Value data;
        data["message"] = "Logged out from " + std::to_string(invalidated) + " device(s)";
        data["devices_logged_out"] = invalidated;

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Logout all endpoint error: " + std::string(e.what()),
            {{"endpoint", "POST /api/auth/logout-all"}}
        );
        callback(ApiResponse::serverError("An error occurred"));
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

std::string AuthController::getClientIp(const drogon::HttpRequestPtr& req) const {
    // Check for forwarded IP (when behind proxy)
    std::string ip = req->getHeader("X-Forwarded-For");
    if (!ip.empty()) {
        // Take first IP if multiple
        size_t comma = ip.find(',');
        if (comma != std::string::npos) {
            ip = ip.substr(0, comma);
        }
        // Trim whitespace
        size_t start = ip.find_first_not_of(" ");
        size_t end = ip.find_last_not_of(" ");
        if (start != std::string::npos) {
            return ip.substr(start, end - start + 1);
        }
    }

    // Check X-Real-IP header
    ip = req->getHeader("X-Real-IP");
    if (!ip.empty()) {
        return ip;
    }

    // Fall back to peer address
    return req->getPeerAddr().toIp();
}

std::string AuthController::getUserAgent(const drogon::HttpRequestPtr& req) const {
    std::string user_agent = req->getHeader("User-Agent");

    // Truncate if too long
    if (user_agent.length() > 500) {
        user_agent = user_agent.substr(0, 500);
    }

    return user_agent;
}

} // namespace wtl::core::controllers
