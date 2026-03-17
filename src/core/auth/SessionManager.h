/**
 * @file SessionManager.h
 * @brief Database-backed session management for user authentication
 *
 * PURPOSE:
 * Manages user sessions in the database, enabling token revocation,
 * session tracking, and multi-device login management. Sessions are
 * the authoritative source for token validity.
 *
 * USAGE:
 * auto& manager = SessionManager::getInstance();
 * auto session = manager.createSession(user_id, token, refresh_token);
 *
 * // Validate session exists
 * if (manager.isSessionValid(token)) {
 *     // Token is valid and not revoked
 * }
 *
 * DEPENDENCIES:
 * - ConnectionPool (Agent 1) - Database connections
 * - PasswordUtils - Token hashing
 * - ErrorCapture (Agent 1) - Error logging
 *
 * MODIFICATION GUIDE:
 * - Sessions are stored with hashed tokens
 * - Always hash tokens before database operations
 * - Use transactions for session creation/deletion
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
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

// Project includes (alphabetical)
#include "core/auth/AuthToken.h"

namespace wtl::core::auth {

/**
 * @class SessionManager
 * @brief Singleton service for session management
 *
 * Thread-safe singleton that manages user sessions in the database.
 * Provides session creation, validation, and revocation capabilities.
 */
class SessionManager {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the SessionManager instance
     */
    static SessionManager& getInstance();

    // Delete copy/move constructors and assignment operators
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    SessionManager(SessionManager&&) = delete;
    SessionManager& operator=(SessionManager&&) = delete;

    // ========================================================================
    // SESSION OPERATIONS
    // ========================================================================

    /**
     * @brief Create a new session for a user
     *
     * Creates a session record in the database with hashed tokens.
     * Optionally stores client information for security tracking.
     *
     * @param user_id The user's UUID
     * @param token The access token
     * @param refresh_token The refresh token
     * @param ip_address Client IP address (optional)
     * @param user_agent Client user agent (optional)
     * @return The created Session object
     * @throws std::runtime_error if database operation fails
     *
     * @example
     * auto session = manager.createSession(user_id, token, refresh_token, "192.168.1.1", "Mozilla/5.0");
     */
    Session createSession(const std::string& user_id,
                          const std::string& token,
                          const std::string& refresh_token,
                          const std::string& ip_address = "",
                          const std::string& user_agent = "");

    /**
     * @brief Get a session by access token
     *
     * Retrieves session information for the given access token.
     * Token is hashed before lookup.
     *
     * @param token The access token
     * @return Session if found, std::nullopt otherwise
     *
     * @example
     * auto session = manager.getSession(access_token);
     * if (session) {
     *     std::cout << "Session user: " << session->user_id << std::endl;
     * }
     */
    std::optional<Session> getSession(const std::string& token);

    /**
     * @brief Get a session by refresh token
     *
     * Retrieves session information for the given refresh token.
     * Token is hashed before lookup.
     *
     * @param refresh_token The refresh token
     * @return Session if found, std::nullopt otherwise
     */
    std::optional<Session> getSessionByRefreshToken(const std::string& refresh_token);

    /**
     * @brief Invalidate (revoke) a session by access token
     *
     * Marks the session as inactive, effectively revoking the token.
     * The session record is kept for audit purposes.
     *
     * @param token The access token to revoke
     * @return true if session was found and invalidated
     *
     * @example
     * manager.invalidateSession(token);  // User logged out
     */
    bool invalidateSession(const std::string& token);

    /**
     * @brief Invalidate all sessions for a user
     *
     * Revokes all active sessions for the user, forcing re-login
     * on all devices. Used for password changes or security events.
     *
     * @param user_id The user's UUID
     * @return Number of sessions invalidated
     *
     * @example
     * int count = manager.invalidateAllUserSessions(user_id);
     * std::cout << "Logged out from " << count << " devices" << std::endl;
     */
    int invalidateAllUserSessions(const std::string& user_id);

    /**
     * @brief Remove expired sessions from the database
     *
     * Deletes session records that have passed their expiration time.
     * Should be called periodically (e.g., by a scheduled task).
     *
     * @return Number of sessions cleaned up
     *
     * @example
     * int cleaned = manager.cleanupExpiredSessions();
     */
    int cleanupExpiredSessions();

    /**
     * @brief Check if a session is valid (exists and active)
     *
     * Quick check for session validity without returning full session.
     * Token is hashed before lookup.
     *
     * @param token The access token to check
     * @return true if session exists, is active, and not expired
     *
     * @example
     * if (!manager.isSessionValid(token)) {
     *     // Token has been revoked or doesn't exist
     * }
     */
    bool isSessionValid(const std::string& token);

    /**
     * @brief Update the last_used_at timestamp for a session
     *
     * Called when a token is used to track activity.
     *
     * @param token The access token
     * @return true if session was found and updated
     */
    bool touchSession(const std::string& token);

    /**
     * @brief Get all active sessions for a user
     *
     * Returns all non-expired, active sessions for the user.
     * Useful for displaying active devices/sessions to user.
     *
     * @param user_id The user's UUID
     * @return Vector of active sessions
     */
    std::vector<Session> getUserActiveSessions(const std::string& user_id);

    /**
     * @brief Update session tokens after refresh
     *
     * Updates the session with new access and refresh tokens.
     * Old tokens are replaced with new hashed values.
     *
     * @param old_refresh_token The current refresh token
     * @param new_token The new access token
     * @param new_refresh_token The new refresh token
     * @param new_expires_at New expiration time
     * @return true if session was found and updated
     */
    bool updateSessionTokens(const std::string& old_refresh_token,
                             const std::string& new_token,
                             const std::string& new_refresh_token,
                             std::chrono::system_clock::time_point new_expires_at);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get session statistics
     *
     * @return JSON with active_sessions, total_sessions, etc.
     */
    Json::Value getStats();

    /**
     * @brief Get active session count for a user
     *
     * @param user_id The user's UUID
     * @return Number of active sessions
     */
    int getActiveSessionCount(const std::string& user_id);

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    /**
     * @brief Private constructor for singleton
     */
    SessionManager() = default;

    /**
     * @brief Private destructor
     */
    ~SessionManager() = default;

    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Convert database row to Session object
     * @param row The database row
     * @return Session object
     */
    Session sessionFromDbRow(const pqxx::row& row);

    /**
     * @brief Generate a unique session ID
     * @return UUID string
     */
    std::string generateSessionId();

    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================

    /**
     * @brief Mutex for thread-safe operations
     */
    std::mutex mutex_;
};

} // namespace wtl::core::auth
