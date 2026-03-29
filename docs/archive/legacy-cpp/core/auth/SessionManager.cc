/**
 * @file SessionManager.cc
 * @brief Implementation of session management
 * @see SessionManager.h for documentation
 */

#include "core/auth/SessionManager.h"

// Standard library includes
#include <chrono>
#include <iomanip>
#include <sstream>

// Third-party includes
#include <pqxx/pqxx>

// Project includes
#include "core/auth/PasswordUtils.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::core::auth {

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

SessionManager& SessionManager::getInstance() {
    static SessionManager instance;
    return instance;
}

// ============================================================================
// SESSION OPERATIONS
// ============================================================================

Session SessionManager::createSession(const std::string& user_id,
                                       const std::string& token,
                                       const std::string& refresh_token,
                                       const std::string& ip_address,
                                       const std::string& user_agent) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Hash tokens before storage for security
    std::string token_hash = PasswordUtils::hashToken(token);
    std::string refresh_token_hash = PasswordUtils::hashToken(refresh_token);

    // Generate session ID
    std::string session_id = generateSessionId();

    // Calculate expiration (7 days for refresh token)
    auto now = std::chrono::system_clock::now();
    auto expires_at = now + std::chrono::hours(24 * 7);

    // Convert time points to timestamps for database
    auto now_ts = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    auto expires_ts = std::chrono::duration_cast<std::chrono::seconds>(
        expires_at.time_since_epoch()).count();

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "INSERT INTO sessions (id, user_id, token_hash, refresh_token_hash, "
            "created_at, expires_at, last_used_at, ip_address, user_agent, is_active) "
            "VALUES ($1, $2, $3, $4, to_timestamp($5), to_timestamp($6), to_timestamp($7), $8, $9, true)",
            session_id,
            user_id,
            token_hash,
            refresh_token_hash,
            now_ts,
            expires_ts,
            now_ts,
            ip_address,
            user_agent
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Build and return Session object
        Session session;
        session.id = session_id;
        session.user_id = user_id;
        session.token_hash = token_hash;
        session.refresh_token_hash = refresh_token_hash;
        session.created_at = now;
        session.expires_at = expires_at;
        session.last_used_at = now;
        session.ip_address = ip_address;
        session.user_agent = user_agent;
        session.is_active = true;

        return session;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to create session: " + std::string(e.what()),
            {{"user_id", user_id}, {"operation", "createSession"}}
        );
        throw std::runtime_error("Failed to create session: " + std::string(e.what()));
    }
}

std::optional<Session> SessionManager::getSession(const std::string& token) {
    if (token.empty()) {
        return std::nullopt;
    }

    std::string token_hash = PasswordUtils::hashToken(token);

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT id, user_id, token_hash, refresh_token_hash, "
            "EXTRACT(EPOCH FROM created_at) as created_at, "
            "EXTRACT(EPOCH FROM expires_at) as expires_at, "
            "EXTRACT(EPOCH FROM last_used_at) as last_used_at, "
            "ip_address, user_agent, is_active "
            "FROM sessions "
            "WHERE token_hash = $1 AND is_active = true",
            token_hash
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return sessionFromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get session: " + std::string(e.what()),
            {{"operation", "getSession"}}
        );
        return std::nullopt;
    }
}

std::optional<Session> SessionManager::getSessionByRefreshToken(const std::string& refresh_token) {
    if (refresh_token.empty()) {
        return std::nullopt;
    }

    std::string refresh_token_hash = PasswordUtils::hashToken(refresh_token);

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT id, user_id, token_hash, refresh_token_hash, "
            "EXTRACT(EPOCH FROM created_at) as created_at, "
            "EXTRACT(EPOCH FROM expires_at) as expires_at, "
            "EXTRACT(EPOCH FROM last_used_at) as last_used_at, "
            "ip_address, user_agent, is_active "
            "FROM sessions "
            "WHERE refresh_token_hash = $1 AND is_active = true",
            refresh_token_hash
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return sessionFromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get session by refresh token: " + std::string(e.what()),
            {{"operation", "getSessionByRefreshToken"}}
        );
        return std::nullopt;
    }
}

bool SessionManager::invalidateSession(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::string token_hash = PasswordUtils::hashToken(token);

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "UPDATE sessions SET is_active = false "
            "WHERE token_hash = $1 AND is_active = true",
            token_hash
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to invalidate session: " + std::string(e.what()),
            {{"operation", "invalidateSession"}}
        );
        return false;
    }
}

int SessionManager::invalidateAllUserSessions(const std::string& user_id) {
    if (user_id.empty()) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "UPDATE sessions SET is_active = false "
            "WHERE user_id = $1 AND is_active = true",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return static_cast<int>(result.affected_rows());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to invalidate all user sessions: " + std::string(e.what()),
            {{"user_id", user_id}, {"operation", "invalidateAllUserSessions"}}
        );
        return 0;
    }
}

int SessionManager::cleanupExpiredSessions() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Delete sessions that expired more than 30 days ago
        auto result = txn.exec(
            "DELETE FROM sessions "
            "WHERE expires_at < NOW() - INTERVAL '30 days'"
        );

        // Mark expired sessions as inactive
        txn.exec(
            "UPDATE sessions SET is_active = false "
            "WHERE expires_at < NOW() AND is_active = true"
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return static_cast<int>(result.affected_rows());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to cleanup expired sessions: " + std::string(e.what()),
            {{"operation", "cleanupExpiredSessions"}}
        );
        return 0;
    }
}

bool SessionManager::isSessionValid(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    std::string token_hash = PasswordUtils::hashToken(token);

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT 1 FROM sessions "
            "WHERE token_hash = $1 AND is_active = true AND expires_at > NOW()",
            token_hash
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return !result.empty();

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to check session validity: " + std::string(e.what()),
            {{"operation", "isSessionValid"}}
        );
        return false;
    }
}

bool SessionManager::touchSession(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    std::string token_hash = PasswordUtils::hashToken(token);

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "UPDATE sessions SET last_used_at = NOW() "
            "WHERE token_hash = $1 AND is_active = true",
            token_hash
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        // Don't capture error for touch operations - too noisy
        return false;
    }
}

std::vector<Session> SessionManager::getUserActiveSessions(const std::string& user_id) {
    std::vector<Session> sessions;

    if (user_id.empty()) {
        return sessions;
    }

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT id, user_id, token_hash, refresh_token_hash, "
            "EXTRACT(EPOCH FROM created_at) as created_at, "
            "EXTRACT(EPOCH FROM expires_at) as expires_at, "
            "EXTRACT(EPOCH FROM last_used_at) as last_used_at, "
            "ip_address, user_agent, is_active "
            "FROM sessions "
            "WHERE user_id = $1 AND is_active = true AND expires_at > NOW() "
            "ORDER BY last_used_at DESC",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            sessions.push_back(sessionFromDbRow(row));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get user active sessions: " + std::string(e.what()),
            {{"user_id", user_id}, {"operation", "getUserActiveSessions"}}
        );
    }

    return sessions;
}

bool SessionManager::updateSessionTokens(const std::string& old_refresh_token,
                                          const std::string& new_token,
                                          const std::string& new_refresh_token,
                                          std::chrono::system_clock::time_point new_expires_at) {
    if (old_refresh_token.empty() || new_token.empty() || new_refresh_token.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    std::string old_refresh_hash = PasswordUtils::hashToken(old_refresh_token);
    std::string new_token_hash = PasswordUtils::hashToken(new_token);
    std::string new_refresh_hash = PasswordUtils::hashToken(new_refresh_token);

    auto expires_ts = std::chrono::duration_cast<std::chrono::seconds>(
        new_expires_at.time_since_epoch()).count();

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "UPDATE sessions SET "
            "token_hash = $1, refresh_token_hash = $2, "
            "expires_at = to_timestamp($3), last_used_at = NOW() "
            "WHERE refresh_token_hash = $4 AND is_active = true",
            new_token_hash,
            new_refresh_hash,
            expires_ts,
            old_refresh_hash
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to update session tokens: " + std::string(e.what()),
            {{"operation", "updateSessionTokens"}}
        );
        return false;
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value SessionManager::getStats() {
    Json::Value stats;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Total sessions
        auto total = txn.exec("SELECT COUNT(*) FROM sessions");
        stats["total_sessions"] = total[0][0].as<int>();

        // Active sessions
        auto active = txn.exec(
            "SELECT COUNT(*) FROM sessions WHERE is_active = true AND expires_at > NOW()"
        );
        stats["active_sessions"] = active[0][0].as<int>();

        // Expired sessions
        auto expired = txn.exec(
            "SELECT COUNT(*) FROM sessions WHERE expires_at < NOW()"
        );
        stats["expired_sessions"] = expired[0][0].as<int>();

        // Unique users with active sessions
        auto users = txn.exec(
            "SELECT COUNT(DISTINCT user_id) FROM sessions WHERE is_active = true AND expires_at > NOW()"
        );
        stats["unique_active_users"] = users[0][0].as<int>();

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        stats["error"] = e.what();
    }

    return stats;
}

int SessionManager::getActiveSessionCount(const std::string& user_id) {
    if (user_id.empty()) {
        return 0;
    }

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT COUNT(*) FROM sessions "
            "WHERE user_id = $1 AND is_active = true AND expires_at > NOW()",
            user_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return result[0][0].as<int>();

    } catch (const std::exception& e) {
        return 0;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

Session SessionManager::sessionFromDbRow(const pqxx::row& row) {
    Session session;

    session.id = row["id"].as<std::string>();
    session.user_id = row["user_id"].as<std::string>();
    session.token_hash = row["token_hash"].as<std::string>();
    session.refresh_token_hash = row["refresh_token_hash"].as<std::string>();

    // Convert timestamps (stored as seconds since epoch)
    auto created_ts = row["created_at"].as<double>();
    auto expires_ts = row["expires_at"].as<double>();
    auto last_used_ts = row["last_used_at"].as<double>();

    session.created_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(static_cast<int64_t>(created_ts)));
    session.expires_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(static_cast<int64_t>(expires_ts)));
    session.last_used_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(static_cast<int64_t>(last_used_ts)));

    session.ip_address = row["ip_address"].is_null() ? "" : row["ip_address"].as<std::string>();
    session.user_agent = row["user_agent"].is_null() ? "" : row["user_agent"].as<std::string>();
    session.is_active = row["is_active"].as<bool>();

    return session;
}

std::string SessionManager::generateSessionId() {
    // Generate a secure random UUID-like string
    return PasswordUtils::generateSecureToken(16);
}

} // namespace wtl::core::auth
