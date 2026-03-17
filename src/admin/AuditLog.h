/**
 * @file AuditLog.h
 * @brief Admin action audit logging for compliance and tracking
 *
 * PURPOSE:
 * Logs all administrative actions for compliance, security auditing,
 * and tracking who made what changes when. Essential for accountability
 * and debugging issues.
 *
 * USAGE:
 * auto& audit = AuditLog::getInstance();
 * audit.logAction(user_id, "UPDATE_DOG", "dog", dog_id, details);
 * auto logs = audit.getAuditLog(filters, page, per_page);
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - Add new action types as ActionType enum values
 * - Ensure all admin actions are logged
 * - Keep details JSON structured for searchability
 *
 * @author Agent 20 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::admin {

/**
 * @struct AuditLogEntry
 * @brief Represents a single audit log entry
 */
struct AuditLogEntry {
    std::string id;
    std::string user_id;
    std::string user_email;
    std::string action;
    std::string entity_type;
    std::string entity_id;
    Json::Value details;
    std::string ip_address;
    std::string user_agent;
    std::string created_at;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from database row
     */
    static AuditLogEntry fromDbRow(const std::string& id,
                                   const std::string& user_id,
                                   const std::string& user_email,
                                   const std::string& action,
                                   const std::string& entity_type,
                                   const std::string& entity_id,
                                   const std::string& details_json,
                                   const std::string& ip_address,
                                   const std::string& user_agent,
                                   const std::string& created_at);
};

/**
 * @struct AuditLogFilters
 * @brief Filters for querying audit logs
 */
struct AuditLogFilters {
    std::string user_id;
    std::string action;
    std::string entity_type;
    std::string entity_id;
    std::string start_date;
    std::string end_date;
    std::string search;
};

/**
 * @class AuditLog
 * @brief Singleton service for audit logging
 *
 * Thread-safe audit logging with:
 * - Async write to database
 * - Query capabilities
 * - Search functionality
 * - Export support
 */
class AuditLog {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the AuditLog instance
     */
    static AuditLog& getInstance();

    // Prevent copying and moving
    AuditLog(const AuditLog&) = delete;
    AuditLog& operator=(const AuditLog&) = delete;
    AuditLog(AuditLog&&) = delete;
    AuditLog& operator=(AuditLog&&) = delete;

    // ========================================================================
    // LOGGING
    // ========================================================================

    /**
     * @brief Log an admin action
     *
     * @param user_id ID of the user performing the action
     * @param action Action type (e.g., "UPDATE_DOG", "DELETE_USER")
     * @param entity_type Type of entity (e.g., "dog", "shelter", "user")
     * @param entity_id ID of the entity being acted upon
     * @param details Additional JSON details about the action
     * @param ip_address IP address of the requester (optional)
     * @param user_agent User agent string (optional)
     * @return Audit log entry ID
     */
    std::string logAction(const std::string& user_id,
                         const std::string& action,
                         const std::string& entity_type,
                         const std::string& entity_id,
                         const Json::Value& details,
                         const std::string& ip_address = "",
                         const std::string& user_agent = "");

    /**
     * @brief Log an action with request context
     *
     * Extracts IP and user agent from request headers.
     *
     * @param user_id ID of the user
     * @param action Action type
     * @param entity_type Entity type
     * @param entity_id Entity ID
     * @param details Additional details
     * @param request_headers Map of HTTP headers
     * @return Audit log entry ID
     */
    std::string logActionWithContext(const std::string& user_id,
                                     const std::string& action,
                                     const std::string& entity_type,
                                     const std::string& entity_id,
                                     const Json::Value& details,
                                     const std::unordered_map<std::string, std::string>& request_headers);

    // ========================================================================
    // QUERYING
    // ========================================================================

    /**
     * @brief Get audit log entries with filtering and pagination
     *
     * @param filters Filters to apply
     * @param page Page number (1-indexed)
     * @param per_page Items per page
     * @return JSON object with "entries" array and "total" count
     */
    Json::Value getAuditLog(const AuditLogFilters& filters,
                           int page = 1, int per_page = 20);

    /**
     * @brief Get a specific audit log entry
     *
     * @param id Audit log entry ID
     * @return Entry if found
     */
    std::optional<AuditLogEntry> getById(const std::string& id);

    /**
     * @brief Get all actions for a user
     *
     * @param user_id User ID
     * @param limit Maximum entries to return
     * @return Vector of audit log entries
     */
    std::vector<AuditLogEntry> getByUserId(const std::string& user_id,
                                           int limit = 100);

    /**
     * @brief Get all actions on an entity
     *
     * @param entity_type Entity type
     * @param entity_id Entity ID
     * @param limit Maximum entries to return
     * @return Vector of audit log entries
     */
    std::vector<AuditLogEntry> getByEntity(const std::string& entity_type,
                                           const std::string& entity_id,
                                           int limit = 100);

    /**
     * @brief Get recent audit log entries
     *
     * @param minutes Minutes to look back
     * @param limit Maximum entries
     * @return Vector of recent entries
     */
    std::vector<AuditLogEntry> getRecent(int minutes = 60, int limit = 100);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get audit log statistics
     *
     * Returns counts by action, entity type, user, etc.
     *
     * @param timeframe Timeframe string ("24h", "7d", "30d")
     * @return JSON object with statistics
     */
    Json::Value getStats(const std::string& timeframe = "24h");

    /**
     * @brief Get most active admin users
     *
     * @param limit Number of users to return
     * @return JSON array of users with action counts
     */
    Json::Value getMostActiveUsers(int limit = 10);

    /**
     * @brief Get action counts by type
     *
     * @param timeframe Timeframe string
     * @return JSON object with action type counts
     */
    Json::Value getActionCounts(const std::string& timeframe = "24h");

    // ========================================================================
    // EXPORT
    // ========================================================================

    /**
     * @brief Export audit log to JSON
     *
     * @param filters Filters to apply
     * @param limit Maximum entries (0 = all)
     * @return JSON array of entries
     */
    Json::Value exportToJson(const AuditLogFilters& filters, int limit = 0);

    /**
     * @brief Export audit log to CSV format
     *
     * @param filters Filters to apply
     * @param limit Maximum entries
     * @return CSV string
     */
    std::string exportToCsv(const AuditLogFilters& filters, int limit = 0);

    // ========================================================================
    // CLEANUP
    // ========================================================================

    /**
     * @brief Delete old audit log entries
     *
     * @param days Keep entries newer than this many days
     * @return Number of entries deleted
     */
    int cleanup(int days = 90);

    /**
     * @brief Get total count of audit log entries
     *
     * @return Total count
     */
    int getCount();

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    AuditLog();
    ~AuditLog();

    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Generate a unique ID for audit entry
     */
    std::string generateId() const;

    /**
     * @brief Get current timestamp string
     */
    std::string getCurrentTimestamp() const;

    /**
     * @brief Build query WHERE clause from filters
     */
    std::string buildWhereClause(const AuditLogFilters& filters,
                                 std::vector<std::string>& params) const;

    /**
     * @brief Parse JSON details from database
     */
    Json::Value parseDetails(const std::string& details_json) const;

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    std::mutex mutex_;
    mutable uint64_t id_counter_{0};
};

// ============================================================================
// ACTION TYPE CONSTANTS
// ============================================================================

/**
 * Common audit action types for consistency
 */
namespace AuditActions {
    // User actions
    constexpr const char* USER_LOGIN = "USER_LOGIN";
    constexpr const char* USER_LOGOUT = "USER_LOGOUT";
    constexpr const char* USER_REGISTER = "USER_REGISTER";
    constexpr const char* PASSWORD_CHANGE = "PASSWORD_CHANGE";
    constexpr const char* PASSWORD_RESET = "PASSWORD_RESET";

    // Admin user management
    constexpr const char* CREATE_USER = "CREATE_USER";
    constexpr const char* UPDATE_USER = "UPDATE_USER";
    constexpr const char* DELETE_USER = "DELETE_USER";
    constexpr const char* DISABLE_USER = "DISABLE_USER";
    constexpr const char* ENABLE_USER = "ENABLE_USER";
    constexpr const char* CHANGE_USER_ROLE = "CHANGE_USER_ROLE";

    // Dog management
    constexpr const char* CREATE_DOG = "CREATE_DOG";
    constexpr const char* UPDATE_DOG = "UPDATE_DOG";
    constexpr const char* DELETE_DOG = "DELETE_DOG";
    constexpr const char* BATCH_UPDATE_DOGS = "BATCH_UPDATE_DOGS";

    // Shelter management
    constexpr const char* CREATE_SHELTER = "CREATE_SHELTER";
    constexpr const char* UPDATE_SHELTER = "UPDATE_SHELTER";
    constexpr const char* DELETE_SHELTER = "DELETE_SHELTER";
    constexpr const char* VERIFY_SHELTER = "VERIFY_SHELTER";
    constexpr const char* UNVERIFY_SHELTER = "UNVERIFY_SHELTER";

    // Configuration
    constexpr const char* UPDATE_CONFIG = "UPDATE_CONFIG";
    constexpr const char* SET_CONFIG = "SET_CONFIG";
    constexpr const char* SET_FEATURE_FLAG = "SET_FEATURE_FLAG";

    // Content management
    constexpr const char* APPROVE_CONTENT = "APPROVE_CONTENT";
    constexpr const char* REJECT_CONTENT = "REJECT_CONTENT";
    constexpr const char* CREATE_CONTENT = "CREATE_CONTENT";
    constexpr const char* UPDATE_CONTENT = "UPDATE_CONTENT";
    constexpr const char* DELETE_CONTENT = "DELETE_CONTENT";

    // System actions
    constexpr const char* RESTART_WORKER = "RESTART_WORKER";
    constexpr const char* FORCE_RECALCULATE = "FORCE_RECALCULATE";
    constexpr const char* EXPORT_DATA = "EXPORT_DATA";
    constexpr const char* VIEW_DASHBOARD = "VIEW_DASHBOARD";

    // Foster management
    constexpr const char* APPROVE_FOSTER = "APPROVE_FOSTER";
    constexpr const char* REJECT_FOSTER = "REJECT_FOSTER";
    constexpr const char* START_PLACEMENT = "START_PLACEMENT";
    constexpr const char* END_PLACEMENT = "END_PLACEMENT";
}

// ============================================================================
// ENTITY TYPE CONSTANTS
// ============================================================================

namespace AuditEntities {
    constexpr const char* DOG = "dog";
    constexpr const char* SHELTER = "shelter";
    constexpr const char* USER = "user";
    constexpr const char* FOSTER_HOME = "foster_home";
    constexpr const char* FOSTER_PLACEMENT = "foster_placement";
    constexpr const char* CONFIG = "config";
    constexpr const char* FEATURE_FLAG = "feature_flag";
    constexpr const char* CONTENT = "content";
    constexpr const char* WORKER = "worker";
    constexpr const char* SYSTEM = "system";
}

} // namespace wtl::admin
