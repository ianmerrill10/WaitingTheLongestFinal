/**
 * @file AuditLog.cc
 * @brief Implementation of AuditLog
 * @see AuditLog.h for documentation
 */

#include "admin/AuditLog.h"

// Standard library includes
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <random>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::admin {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// AUDIT LOG ENTRY METHODS
// ============================================================================

Json::Value AuditLogEntry::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["user_id"] = user_id;
    json["user_email"] = user_email;
    json["action"] = action;
    json["entity_type"] = entity_type;
    json["entity_id"] = entity_id;
    json["details"] = details;
    json["ip_address"] = ip_address;
    json["user_agent"] = user_agent;
    json["created_at"] = created_at;
    return json;
}

AuditLogEntry AuditLogEntry::fromDbRow(const std::string& id,
                                       const std::string& user_id,
                                       const std::string& user_email,
                                       const std::string& action,
                                       const std::string& entity_type,
                                       const std::string& entity_id,
                                       const std::string& details_json,
                                       const std::string& ip_address,
                                       const std::string& user_agent,
                                       const std::string& created_at) {
    AuditLogEntry entry;
    entry.id = id;
    entry.user_id = user_id;
    entry.user_email = user_email;
    entry.action = action;
    entry.entity_type = entity_type;
    entry.entity_id = entity_id;
    entry.ip_address = ip_address;
    entry.user_agent = user_agent;
    entry.created_at = created_at;

    // Parse details JSON
    if (!details_json.empty()) {
        Json::CharReaderBuilder builder;
        std::istringstream stream(details_json);
        std::string errors;
        Json::parseFromStream(builder, stream, &entry.details, &errors);
    }

    return entry;
}

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

AuditLog& AuditLog::getInstance() {
    static AuditLog instance;
    return instance;
}

AuditLog::AuditLog() = default;
AuditLog::~AuditLog() = default;

// ============================================================================
// LOGGING
// ============================================================================

std::string AuditLog::logAction(const std::string& user_id,
                                const std::string& action,
                                const std::string& entity_type,
                                const std::string& entity_id,
                                const Json::Value& details,
                                const std::string& ip_address,
                                const std::string& user_agent) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string id = generateId();
        std::string timestamp = getCurrentTimestamp();

        // Serialize details to JSON string
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string details_json = Json::writeString(builder, details);

        txn.exec_params(
            "INSERT INTO admin_audit_log "
            "(id, user_id, action, entity_type, entity_id, details, ip_address, user_agent, created_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
            id, user_id, action, entity_type, entity_id,
            details_json, ip_address, user_agent, timestamp
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return id;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to log audit action: " + std::string(e.what()),
                         {{"action", action}, {"user_id", user_id}});
        return "";
    }
}

std::string AuditLog::logActionWithContext(const std::string& user_id,
                                           const std::string& action,
                                           const std::string& entity_type,
                                           const std::string& entity_id,
                                           const Json::Value& details,
                                           const std::unordered_map<std::string, std::string>& request_headers) {
    std::string ip_address;
    std::string user_agent;

    // Extract IP address (check common headers)
    auto it = request_headers.find("X-Forwarded-For");
    if (it != request_headers.end()) {
        // Take first IP if multiple
        ip_address = it->second;
        auto comma_pos = ip_address.find(',');
        if (comma_pos != std::string::npos) {
            ip_address = ip_address.substr(0, comma_pos);
        }
    } else {
        it = request_headers.find("X-Real-IP");
        if (it != request_headers.end()) {
            ip_address = it->second;
        }
    }

    // Extract user agent
    it = request_headers.find("User-Agent");
    if (it != request_headers.end()) {
        user_agent = it->second;
        // Truncate very long user agents
        if (user_agent.length() > 500) {
            user_agent = user_agent.substr(0, 500);
        }
    }

    return logAction(user_id, action, entity_type, entity_id, details, ip_address, user_agent);
}

// ============================================================================
// QUERYING
// ============================================================================

Json::Value AuditLog::getAuditLog(const AuditLogFilters& filters,
                                  int page, int per_page) {
    Json::Value result;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Build query
        std::vector<std::string> params;
        std::string where_clause = buildWhereClause(filters, params);

        // Count total
        std::string count_query = "SELECT COUNT(*) FROM admin_audit_log a "
                                  "LEFT JOIN users u ON a.user_id = u.id" +
                                  where_clause;

        auto count_result = txn.exec(count_query);
        result["total"] = count_result[0][0].as<int>();

        // Get entries with pagination
        int offset = (page - 1) * per_page;
        std::string query = "SELECT a.*, COALESCE(u.email, '') as user_email "
                           "FROM admin_audit_log a "
                           "LEFT JOIN users u ON a.user_id = u.id" +
                           where_clause +
                           " ORDER BY a.created_at DESC "
                           "LIMIT " + std::to_string(per_page) +
                           " OFFSET " + std::to_string(offset);

        auto query_result = txn.exec(query);

        Json::Value entries(Json::arrayValue);
        for (const auto& row : query_result) {
            auto entry = AuditLogEntry::fromDbRow(
                row["id"].c_str(),
                row["user_id"].c_str(),
                row["user_email"].c_str(),
                row["action"].c_str(),
                row["entity_type"].c_str(),
                row["entity_id"].c_str(),
                row["details"].is_null() ? "" : row["details"].c_str(),
                row["ip_address"].is_null() ? "" : row["ip_address"].c_str(),
                row["user_agent"].is_null() ? "" : row["user_agent"].c_str(),
                row["created_at"].c_str()
            );
            entries.append(entry.toJson());
        }

        result["entries"] = entries;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get audit log: " + std::string(e.what()),
                         {});

        result["total"] = 0;
        result["entries"] = Json::Value(Json::arrayValue);
    }

    return result;
}

std::optional<AuditLogEntry> AuditLog::getById(const std::string& id) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT a.*, COALESCE(u.email, '') as user_email "
            "FROM admin_audit_log a "
            "LEFT JOIN users u ON a.user_id = u.id "
            "WHERE a.id = $1",
            id
        );

        if (result.empty()) {
            txn.commit();
            ConnectionPool::getInstance().release(conn);
            return std::nullopt;
        }

        const auto& row = result[0];
        auto entry = AuditLogEntry::fromDbRow(
            row["id"].c_str(),
            row["user_id"].c_str(),
            row["user_email"].c_str(),
            row["action"].c_str(),
            row["entity_type"].c_str(),
            row["entity_id"].c_str(),
            row["details"].is_null() ? "" : row["details"].c_str(),
            row["ip_address"].is_null() ? "" : row["ip_address"].c_str(),
            row["user_agent"].is_null() ? "" : row["user_agent"].c_str(),
            row["created_at"].c_str()
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);
        return entry;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get audit log entry: " + std::string(e.what()),
                         {{"id", id}});
        return std::nullopt;
    }
}

std::vector<AuditLogEntry> AuditLog::getByUserId(const std::string& user_id, int limit) {
    std::vector<AuditLogEntry> entries;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT a.*, COALESCE(u.email, '') as user_email "
            "FROM admin_audit_log a "
            "LEFT JOIN users u ON a.user_id = u.id "
            "WHERE a.user_id = $1 "
            "ORDER BY a.created_at DESC LIMIT $2",
            user_id, limit
        );

        for (const auto& row : result) {
            entries.push_back(AuditLogEntry::fromDbRow(
                row["id"].c_str(),
                row["user_id"].c_str(),
                row["user_email"].c_str(),
                row["action"].c_str(),
                row["entity_type"].c_str(),
                row["entity_id"].c_str(),
                row["details"].is_null() ? "" : row["details"].c_str(),
                row["ip_address"].is_null() ? "" : row["ip_address"].c_str(),
                row["user_agent"].is_null() ? "" : row["user_agent"].c_str(),
                row["created_at"].c_str()
            ));
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get audit log by user: " + std::string(e.what()),
                         {{"user_id", user_id}});
    }

    return entries;
}

std::vector<AuditLogEntry> AuditLog::getByEntity(const std::string& entity_type,
                                                 const std::string& entity_id,
                                                 int limit) {
    std::vector<AuditLogEntry> entries;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT a.*, COALESCE(u.email, '') as user_email "
            "FROM admin_audit_log a "
            "LEFT JOIN users u ON a.user_id = u.id "
            "WHERE a.entity_type = $1 AND a.entity_id = $2 "
            "ORDER BY a.created_at DESC LIMIT $3",
            entity_type, entity_id, limit
        );

        for (const auto& row : result) {
            entries.push_back(AuditLogEntry::fromDbRow(
                row["id"].c_str(),
                row["user_id"].c_str(),
                row["user_email"].c_str(),
                row["action"].c_str(),
                row["entity_type"].c_str(),
                row["entity_id"].c_str(),
                row["details"].is_null() ? "" : row["details"].c_str(),
                row["ip_address"].is_null() ? "" : row["ip_address"].c_str(),
                row["user_agent"].is_null() ? "" : row["user_agent"].c_str(),
                row["created_at"].c_str()
            ));
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get audit log by entity: " + std::string(e.what()),
                         {{"entity_type", entity_type}, {"entity_id", entity_id}});
    }

    return entries;
}

std::vector<AuditLogEntry> AuditLog::getRecent(int minutes, int limit) {
    std::vector<AuditLogEntry> entries;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT a.*, COALESCE(u.email, '') as user_email "
            "FROM admin_audit_log a "
            "LEFT JOIN users u ON a.user_id = u.id "
            "WHERE a.created_at > NOW() - INTERVAL '" + std::to_string(minutes) + " minutes' "
            "ORDER BY a.created_at DESC LIMIT $1",
            limit
        );

        for (const auto& row : result) {
            entries.push_back(AuditLogEntry::fromDbRow(
                row["id"].c_str(),
                row["user_id"].c_str(),
                row["user_email"].c_str(),
                row["action"].c_str(),
                row["entity_type"].c_str(),
                row["entity_id"].c_str(),
                row["details"].is_null() ? "" : row["details"].c_str(),
                row["ip_address"].is_null() ? "" : row["ip_address"].c_str(),
                row["user_agent"].is_null() ? "" : row["user_agent"].c_str(),
                row["created_at"].c_str()
            ));
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get recent audit log: " + std::string(e.what()),
                         {{"minutes", std::to_string(minutes)}});
    }

    return entries;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value AuditLog::getStats(const std::string& timeframe) {
    Json::Value stats;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string interval;
        if (timeframe == "24h") interval = "1 day";
        else if (timeframe == "7d") interval = "7 days";
        else if (timeframe == "30d") interval = "30 days";
        else interval = "1 day";

        // Total count
        auto result = txn.exec(
            "SELECT COUNT(*) FROM admin_audit_log "
            "WHERE created_at > NOW() - INTERVAL '" + interval + "'"
        );
        stats["total_actions"] = result[0][0].as<int>();

        // Unique users
        result = txn.exec(
            "SELECT COUNT(DISTINCT user_id) FROM admin_audit_log "
            "WHERE created_at > NOW() - INTERVAL '" + interval + "'"
        );
        stats["unique_users"] = result[0][0].as<int>();

        // Actions by type
        result = txn.exec(
            "SELECT action, COUNT(*) as count FROM admin_audit_log "
            "WHERE created_at > NOW() - INTERVAL '" + interval + "' "
            "GROUP BY action ORDER BY count DESC"
        );

        Json::Value by_action;
        for (const auto& row : result) {
            by_action[row["action"].c_str()] = row["count"].as<int>();
        }
        stats["by_action"] = by_action;

        // Actions by entity type
        result = txn.exec(
            "SELECT entity_type, COUNT(*) as count FROM admin_audit_log "
            "WHERE created_at > NOW() - INTERVAL '" + interval + "' "
            "GROUP BY entity_type ORDER BY count DESC"
        );

        Json::Value by_entity;
        for (const auto& row : result) {
            by_entity[row["entity_type"].c_str()] = row["count"].as<int>();
        }
        stats["by_entity_type"] = by_entity;

        stats["timeframe"] = timeframe;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get audit log stats: " + std::string(e.what()),
                         {{"timeframe", timeframe}});

        stats["total_actions"] = 0;
        stats["unique_users"] = 0;
    }

    return stats;
}

Json::Value AuditLog::getMostActiveUsers(int limit) {
    Json::Value users(Json::arrayValue);

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT a.user_id, u.email, COUNT(*) as action_count "
            "FROM admin_audit_log a "
            "LEFT JOIN users u ON a.user_id = u.id "
            "WHERE a.created_at > NOW() - INTERVAL '30 days' "
            "GROUP BY a.user_id, u.email "
            "ORDER BY action_count DESC LIMIT $1",
            limit
        );

        for (const auto& row : result) {
            Json::Value user;
            user["user_id"] = row["user_id"].c_str();
            user["email"] = row["email"].is_null() ? "" : row["email"].c_str();
            user["action_count"] = row["action_count"].as<int>();
            users.append(user);
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get most active users: " + std::string(e.what()),
                         {});
    }

    return users;
}

Json::Value AuditLog::getActionCounts(const std::string& timeframe) {
    Json::Value counts;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string interval;
        if (timeframe == "24h") interval = "1 day";
        else if (timeframe == "7d") interval = "7 days";
        else if (timeframe == "30d") interval = "30 days";
        else interval = "1 day";

        auto result = txn.exec(
            "SELECT action, COUNT(*) as count FROM admin_audit_log "
            "WHERE created_at > NOW() - INTERVAL '" + interval + "' "
            "GROUP BY action ORDER BY count DESC"
        );

        for (const auto& row : result) {
            counts[row["action"].c_str()] = row["count"].as<int>();
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get action counts: " + std::string(e.what()),
                         {});
    }

    return counts;
}

// ============================================================================
// EXPORT
// ============================================================================

Json::Value AuditLog::exportToJson(const AuditLogFilters& filters, int limit) {
    Json::Value entries(Json::arrayValue);

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::vector<std::string> params;
        std::string where_clause = buildWhereClause(filters, params);

        std::string query = "SELECT a.*, COALESCE(u.email, '') as user_email "
                           "FROM admin_audit_log a "
                           "LEFT JOIN users u ON a.user_id = u.id" +
                           where_clause +
                           " ORDER BY a.created_at DESC";

        if (limit > 0) {
            query += " LIMIT " + std::to_string(limit);
        }

        auto result = txn.exec(query);

        for (const auto& row : result) {
            auto entry = AuditLogEntry::fromDbRow(
                row["id"].c_str(),
                row["user_id"].c_str(),
                row["user_email"].c_str(),
                row["action"].c_str(),
                row["entity_type"].c_str(),
                row["entity_id"].c_str(),
                row["details"].is_null() ? "" : row["details"].c_str(),
                row["ip_address"].is_null() ? "" : row["ip_address"].c_str(),
                row["user_agent"].is_null() ? "" : row["user_agent"].c_str(),
                row["created_at"].c_str()
            );
            entries.append(entry.toJson());
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to export audit log to JSON: " + std::string(e.what()),
                         {});
    }

    return entries;
}

std::string AuditLog::exportToCsv(const AuditLogFilters& filters, int limit) {
    std::ostringstream csv;

    // CSV header
    csv << "id,user_id,user_email,action,entity_type,entity_id,ip_address,created_at\n";

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::vector<std::string> params;
        std::string where_clause = buildWhereClause(filters, params);

        std::string query = "SELECT a.*, COALESCE(u.email, '') as user_email "
                           "FROM admin_audit_log a "
                           "LEFT JOIN users u ON a.user_id = u.id" +
                           where_clause +
                           " ORDER BY a.created_at DESC";

        if (limit > 0) {
            query += " LIMIT " + std::to_string(limit);
        }

        auto result = txn.exec(query);

        for (const auto& row : result) {
            // Escape and quote fields for CSV
            csv << "\"" << row["id"].c_str() << "\","
                << "\"" << row["user_id"].c_str() << "\","
                << "\"" << (row["user_email"].is_null() ? "" : row["user_email"].c_str()) << "\","
                << "\"" << row["action"].c_str() << "\","
                << "\"" << row["entity_type"].c_str() << "\","
                << "\"" << row["entity_id"].c_str() << "\","
                << "\"" << (row["ip_address"].is_null() ? "" : row["ip_address"].c_str()) << "\","
                << "\"" << row["created_at"].c_str() << "\"\n";
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to export audit log to CSV: " + std::string(e.what()),
                         {});
    }

    return csv.str();
}

// ============================================================================
// CLEANUP
// ============================================================================

int AuditLog::cleanup(int days) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "DELETE FROM admin_audit_log "
            "WHERE created_at < NOW() - INTERVAL '" + std::to_string(days) + " days'"
        );

        int deleted = result.affected_rows();

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return deleted;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to cleanup audit log: " + std::string(e.what()),
                         {{"days", std::to_string(days)}});
        return 0;
    }
}

int AuditLog::getCount() {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec("SELECT COUNT(*) FROM admin_audit_log");
        int count = result[0][0].as<int>();

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return count;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get audit log count: " + std::string(e.what()),
                         {});
        return 0;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::string AuditLog::generateId() const {
    // Generate UUID-like ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string id;
    id.reserve(36);

    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            id += '-';
        } else if (i == 14) {
            id += '4';  // Version 4
        } else if (i == 19) {
            id += hex[(dis(gen) & 0x3) | 0x8];  // Variant
        } else {
            id += hex[dis(gen)];
        }
    }

    return id;
}

std::string AuditLog::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string AuditLog::buildWhereClause(const AuditLogFilters& filters,
                                       std::vector<std::string>& params) const {
    std::string where = " WHERE 1=1";

    if (!filters.user_id.empty()) {
        where += " AND a.user_id = '" + filters.user_id + "'";
    }

    if (!filters.action.empty()) {
        where += " AND a.action = '" + filters.action + "'";
    }

    if (!filters.entity_type.empty()) {
        where += " AND a.entity_type = '" + filters.entity_type + "'";
    }

    if (!filters.entity_id.empty()) {
        where += " AND a.entity_id = '" + filters.entity_id + "'";
    }

    if (!filters.start_date.empty()) {
        where += " AND a.created_at >= '" + filters.start_date + "'";
    }

    if (!filters.end_date.empty()) {
        where += " AND a.created_at <= '" + filters.end_date + "'";
    }

    if (!filters.search.empty()) {
        where += " AND (a.action ILIKE '%" + filters.search + "%' "
                 "OR a.entity_id ILIKE '%" + filters.search + "%')";
    }

    return where;
}

Json::Value AuditLog::parseDetails(const std::string& details_json) const {
    if (details_json.empty()) {
        return Json::Value();
    }

    try {
        Json::CharReaderBuilder builder;
        std::istringstream stream(details_json);
        Json::Value details;
        std::string errors;
        Json::parseFromStream(builder, stream, &details, &errors);
        return details;
    } catch (...) {
        return Json::Value();
    }
}

} // namespace wtl::admin
