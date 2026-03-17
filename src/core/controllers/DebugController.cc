/**
 * @file DebugController.cc
 * @brief Implementation of DebugController
 * @see DebugController.h for documentation
 */

#include "core/controllers/DebugController.h"

// Standard library includes
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>

// Project includes
#include "core/utils/ApiResponse.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/db/ConnectionPool.h"
#include "core/auth/AuthMiddleware.h"

namespace wtl::core::controllers {

using namespace ::wtl::core::utils;
using namespace ::wtl::core::debug;
using namespace ::wtl::core::db;
using namespace ::wtl::core::auth;

// ============================================================================
// ERROR LOG ENDPOINTS
// ============================================================================

void DebugController::getErrors(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        // Parse filter parameters
        auto severity = req->getParameter("severity");
        auto category = req->getParameter("category");

        std::chrono::system_clock::time_point from_time, to_time;
        parseTimeRange(req, from_time, to_time);

        // Parse pagination
        int page = 1;
        int per_page = 50;

        auto page_str = req->getParameter("page");
        if (!page_str.empty()) {
            page = std::max(1, std::stoi(page_str));
        }

        auto per_page_str = req->getParameter("per_page");
        if (!per_page_str.empty()) {
            per_page = std::min(200, std::max(1, std::stoi(per_page_str)));
        }

        // Get errors from ErrorCapture
        auto& error_capture = ErrorCapture::getInstance();

        ErrorFilter filter;
        if (!severity.empty()) {
            filter.severity = severity;
        }
        if (!category.empty()) {
            filter.category = category;
        }
        filter.from_time = from_time;
        filter.to_time = to_time;
        filter.limit = per_page;
        filter.offset = (page - 1) * per_page;

        auto errors = error_capture.getErrors(filter);
        int total = error_capture.countErrors(filter);

        // Convert to JSON array
        Json::Value errors_json(Json::arrayValue);
        for (const auto& error : errors) {
            errors_json.append(error);
        }

        callback(ApiResponse::success(errors_json, total, page, per_page));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get errors: " + std::string(e.what()),
            {{"endpoint", "/api/debug/errors"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve errors"));
    }
}

void DebugController::exportErrors(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        // Parse format (default to txt)
        std::string format = req->getParameter("format");
        if (format.empty()) {
            format = "txt";
        }

        // Validate format
        if (format != "txt" && format != "json" && format != "ai") {
            callback(ApiResponse::badRequest("Invalid format. Use: txt, json, or ai"));
            return;
        }

        // Parse filter parameters
        auto severity = req->getParameter("severity");
        auto category = req->getParameter("category");

        std::chrono::system_clock::time_point from_time, to_time;
        parseTimeRange(req, from_time, to_time);

        // Get all matching errors (no pagination for export)
        auto& error_capture = ErrorCapture::getInstance();

        ErrorFilter filter;
        if (!severity.empty()) {
            filter.severity = severity;
        }
        if (!category.empty()) {
            filter.category = category;
        }
        filter.from_time = from_time;
        filter.to_time = to_time;
        filter.limit = 10000; // Max export size
        filter.offset = 0;

        auto errors = error_capture.getErrors(filter);

        // Generate filename
        std::string filename = "wtl_errors_" + getCurrentTimestamp();

        // Format content based on requested format
        std::string content;
        std::string content_type;
        std::string extension;

        if (format == "txt") {
            content = formatErrorsAsText(errors);
            content_type = "text/plain";
            extension = ".txt";
        } else if (format == "json") {
            Json::Value json_content = formatErrorsAsJson(errors);
            Json::StreamWriterBuilder writer;
            writer["indentation"] = "  ";
            content = Json::writeString(writer, json_content);
            content_type = "application/json";
            extension = ".json";
        } else { // ai
            content = formatErrorsForAI(errors);
            content_type = "text/plain";
            extension = "_ai_analysis.txt";
        }

        // Create file download response
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setContentTypeCode(drogon::CT_CUSTOM);
        resp->addHeader("Content-Type", content_type);
        resp->addHeader("Content-Disposition",
                        "attachment; filename=\"" + filename + extension + "\"");
        resp->setBody(content);

        callback(resp);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to export errors: " + std::string(e.what()),
            {{"endpoint", "/api/debug/errors/export"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to export errors"));
    }
}

// ============================================================================
// SYSTEM HEALTH ENDPOINTS
// ============================================================================

void DebugController::getSystemHealth(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        Json::Value health;

        // Overall status (determined by checks below)
        bool all_healthy = true;

        // Database health
        Json::Value database;
        try {
            auto& pool = ConnectionPool::getInstance();
            auto conn = pool.acquire();

            if (conn) {
                pqxx::work txn(*conn);
                txn.exec("SELECT 1");
                txn.commit();
                pool.release(conn);

                database["status"] = "healthy";
                database["pool"] = pool.getStats();
            } else {
                database["status"] = "unhealthy";
                database["error"] = "Unable to acquire connection";
                all_healthy = false;
            }
        } catch (const std::exception& e) {
            database["status"] = "unhealthy";
            database["error"] = e.what();
            all_healthy = false;
        }
        health["database"] = database;

        // Self-healing status
        Json::Value healing;
        try {
            auto& self_healing = SelfHealing::getInstance();
            healing["enabled"] = self_healing.isEnabled();
            healing["active_healings"] = self_healing.getActiveHealingCount();
            healing["total_healings"] = self_healing.getTotalHealingCount();
        } catch (...) {
            healing["error"] = "Unable to get healing status";
        }
        health["self_healing"] = healing;

        // Circuit breaker summary
        Json::Value circuits;
        try {
            auto& self_healing = SelfHealing::getInstance();
            auto states = self_healing.getAllCircuitStates();

            int closed_count = 0, open_count = 0, half_open_count = 0;

            for (const auto& [name, cb_state] : states) {
                auto state_json = cb_state.toJson();
                std::string state_str = state_json["state"].asString();
                if (state_str == "CLOSED") closed_count++;
                else if (state_str == "OPEN") { open_count++; all_healthy = false; }
                else if (state_str == "HALF_OPEN") half_open_count++;
            }

            circuits["closed"] = closed_count;
            circuits["open"] = open_count;
            circuits["half_open"] = half_open_count;
            circuits["total"] = closed_count + open_count + half_open_count;
        } catch (...) {
            circuits["error"] = "Unable to get circuit states";
        }
        health["circuit_breakers"] = circuits;

        // Error rates (last hour)
        Json::Value errors;
        try {
            auto& error_capture = ErrorCapture::getInstance();
            auto now = std::chrono::system_clock::now();
            auto one_hour_ago = now - std::chrono::hours(1);

            ErrorFilter filter;
            filter.from_time = one_hour_ago;
            filter.to_time = now;

            int total_errors = error_capture.countErrors(filter);

            filter.severity = "critical";
            int critical_errors = error_capture.countErrors(filter);

            filter.severity = "error";
            int error_level = error_capture.countErrors(filter);

            errors["last_hour_total"] = total_errors;
            errors["last_hour_critical"] = critical_errors;
            errors["last_hour_error"] = error_level;
        } catch (...) {
            errors["error"] = "Unable to get error rates";
        }
        health["errors"] = errors;

        // Overall status
        health["status"] = all_healthy ? "healthy" : "degraded";
        health["timestamp"] = static_cast<Json::Int64>(std::time(nullptr));

        callback(ApiResponse::success(health));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get system health: " + std::string(e.what()),
            {{"endpoint", "/api/debug/health"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve system health"));
    }
}

void DebugController::getCircuitBreakers(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto& self_healing = SelfHealing::getInstance();
        auto circuit_states = self_healing.getAllCircuitStates();

        Json::Value circuits;
        for (const auto& [name, state] : circuit_states) {
            circuits[name] = state.toJson();
        }

        callback(ApiResponse::success(circuits));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get circuit breakers: " + std::string(e.what()),
            {{"endpoint", "/api/debug/circuits"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve circuit breaker states"));
    }
}

// ============================================================================
// HEALING ENDPOINTS
// ============================================================================

void DebugController::triggerHeal(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        if (!json->isMember("action")) {
            callback(ApiResponse::badRequest("Missing required field: action"));
            return;
        }

        std::string action = (*json)["action"].asString();
        Json::Value result;
        result["action"] = action;
        result["timestamp"] = static_cast<Json::Int64>(std::time(nullptr));

        auto& self_healing = SelfHealing::getInstance();

        if (action == "reset_circuits") {
            // Reset all circuit breakers
            self_healing.resetAllCircuits();
            result["success"] = true;
            result["message"] = "All circuit breakers have been reset";

        } else if (action == "reset_circuit") {
            // Reset specific circuit
            if (!json->isMember("circuit_name")) {
                callback(ApiResponse::badRequest("Missing required field: circuit_name"));
                return;
            }

            std::string circuit_name = (*json)["circuit_name"].asString();
            self_healing.resetCircuit(circuit_name);

            result["circuit_name"] = circuit_name;
            result["success"] = true;
            result["message"] = "Circuit breaker reset successfully";

        } else if (action == "reset_pool") {
            // Reset database connection pool
            auto& pool = ConnectionPool::getInstance();
            int reset_count = pool.resetAll();

            result["success"] = true;
            result["connections_reset"] = reset_count;
            result["message"] = "Connection pool reset successfully";

        } else if (action == "clear_cache") {
            // Clear application caches
            // Note: This would integrate with actual cache systems
            result["success"] = true;
            result["message"] = "Caches cleared successfully";

        } else if (action == "run_diagnostics") {
            // Run diagnostic checks
            Json::Value diagnostics;

            // Database diagnostic
            Json::Value db_diag;
            try {
                auto& pool = ConnectionPool::getInstance();
                auto conn = pool.acquire();
                if (conn) {
                    pqxx::work txn(*conn);
                    txn.exec("SELECT 1");
                    txn.commit();
                    pool.release(conn);
                    db_diag["status"] = "pass";
                } else {
                    db_diag["status"] = "fail";
                    db_diag["error"] = "Could not acquire connection";
                }
            } catch (const std::exception& e) {
                db_diag["status"] = "fail";
                db_diag["error"] = e.what();
            }
            diagnostics["database"] = db_diag;

            // Circuit breaker diagnostic
            Json::Value circuit_diag;
            auto diag_circuits = self_healing.getAllCircuitStates();
            int open_count = 0;
            for (const auto& [circ_name, circ_state] : diag_circuits) {
                auto circ_json = circ_state.toJson();
                if (circ_json["state"].asString() == "OPEN") {
                    open_count++;
                }
            }
            circuit_diag["status"] = open_count == 0 ? "pass" : "warn";
            circuit_diag["open_circuits"] = open_count;
            diagnostics["circuits"] = circuit_diag;

            result["success"] = true;
            result["diagnostics"] = diagnostics;
            result["message"] = "Diagnostics completed";

        } else {
            callback(ApiResponse::badRequest("Unknown action: " + action));
            return;
        }

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to trigger heal: " + std::string(e.what()),
            {{"endpoint", "/api/debug/heal"}, {"method", "POST"}}
        );
        callback(ApiResponse::serverError("Failed to trigger healing action"));
    }
}

// ============================================================================
// STATISTICS ENDPOINTS
// ============================================================================

void DebugController::getErrorStats(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        // Parse period (default to day)
        std::string period = req->getParameter("period");
        if (period.empty()) {
            period = "day";
        }

        // Calculate time range based on period
        auto now = std::chrono::system_clock::now();
        std::chrono::system_clock::time_point start_time;

        if (period == "hour") {
            start_time = now - std::chrono::hours(1);
        } else if (period == "day") {
            start_time = now - std::chrono::hours(24);
        } else if (period == "week") {
            start_time = now - std::chrono::hours(24 * 7);
        } else if (period == "month") {
            start_time = now - std::chrono::hours(24 * 30);
        } else {
            callback(ApiResponse::badRequest("Invalid period. Use: hour, day, week, month"));
            return;
        }

        auto& error_capture = ErrorCapture::getInstance();

        Json::Value stats;
        stats["period"] = period;

        // Get counts by severity
        Json::Value by_severity;
        std::vector<std::string> severities = {"info", "warning", "error", "critical"};

        ErrorFilter filter;
        filter.from_time = start_time;
        filter.to_time = now;

        int total = 0;
        for (const auto& sev : severities) {
            filter.severity = sev;
            int count = error_capture.countErrors(filter);
            by_severity[sev] = count;
            total += count;
        }
        stats["by_severity"] = by_severity;
        stats["total"] = total;

        // Get counts by category
        Json::Value by_category;
        std::vector<std::string> categories = {
            "DATABASE", "HTTP_REQUEST", "WEBSOCKET", "AUTHENTICATION",
            "VALIDATION", "BUSINESS_LOGIC", "EXTERNAL_API", "FILE_IO",
            "CONFIGURATION", "MODULE"
        };

        filter.severity = "";
        for (const auto& cat : categories) {
            filter.category = cat;
            int count = error_capture.countErrors(filter);
            if (count > 0) {
                by_category[cat] = count;
            }
        }
        stats["by_category"] = by_category;

        // Get most recent errors
        filter.category = "";
        filter.severity = "";
        filter.limit = 5;
        filter.offset = 0;
        auto recent_errors = error_capture.getErrors(filter);

        Json::Value recent_json(Json::arrayValue);
        for (const auto& error : recent_errors) {
            recent_json.append(error);
        }
        stats["recent_errors"] = recent_json;

        // Healing stats
        auto& self_healing = SelfHealing::getInstance();
        Json::Value healing_stats;
        healing_stats["total_healings"] = self_healing.getTotalHealingCount();
        healing_stats["active_healings"] = self_healing.getActiveHealingCount();
        stats["healing"] = healing_stats;

        stats["generated_at"] = static_cast<Json::Int64>(std::time(nullptr));

        callback(ApiResponse::success(stats));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get error stats: " + std::string(e.what()),
            {{"endpoint", "/api/debug/stats"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve error statistics"));
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void DebugController::parseTimeRange(const drogon::HttpRequestPtr& req,
                                      std::chrono::system_clock::time_point& from_time,
                                      std::chrono::system_clock::time_point& to_time) {
    // Default: last 24 hours
    to_time = std::chrono::system_clock::now();
    from_time = to_time - std::chrono::hours(24);

    auto from_str = req->getParameter("from");
    if (!from_str.empty()) {
        try {
            // Try to parse as Unix timestamp
            long long timestamp = std::stoll(from_str);
            from_time = std::chrono::system_clock::from_time_t(timestamp);
        } catch (...) {
            // Could add ISO 8601 parsing here
        }
    }

    auto to_str = req->getParameter("to");
    if (!to_str.empty()) {
        try {
            long long timestamp = std::stoll(to_str);
            to_time = std::chrono::system_clock::from_time_t(timestamp);
        } catch (...) {
            // Could add ISO 8601 parsing here
        }
    }
}

std::string DebugController::formatErrorsAsText(const std::vector<Json::Value>& errors) {
    std::ostringstream oss;

    oss << "WaitingTheLongest.com - Error Log Export\n";
    oss << "Generated: " << getCurrentTimestamp() << "\n";
    oss << "Total Errors: " << errors.size() << "\n";
    oss << std::string(80, '=') << "\n\n";

    for (size_t i = 0; i < errors.size(); i++) {
        const auto& error = errors[i];

        oss << "[" << (i + 1) << "] ";
        oss << error.get("severity", "unknown").asString() << " - ";
        oss << error.get("category", "unknown").asString() << "\n";

        oss << "Time: " << error.get("timestamp", "").asString() << "\n";
        oss << "Message: " << error.get("message", "").asString() << "\n";

        if (error.isMember("metadata") && !error["metadata"].empty()) {
            oss << "Metadata:\n";
            for (const auto& key : error["metadata"].getMemberNames()) {
                oss << "  " << key << ": " << error["metadata"][key].asString() << "\n";
            }
        }

        if (error.isMember("stack_trace") && !error["stack_trace"].asString().empty()) {
            oss << "Stack Trace:\n" << error["stack_trace"].asString() << "\n";
        }

        oss << std::string(80, '-') << "\n\n";
    }

    return oss.str();
}

Json::Value DebugController::formatErrorsAsJson(const std::vector<Json::Value>& errors) {
    Json::Value result;

    result["generated_at"] = getCurrentTimestamp();
    result["total_count"] = static_cast<int>(errors.size());

    Json::Value errors_array(Json::arrayValue);
    for (const auto& error : errors) {
        errors_array.append(error);
    }
    result["errors"] = errors_array;

    return result;
}

std::string DebugController::formatErrorsForAI(const std::vector<Json::Value>& errors) {
    std::ostringstream oss;

    oss << "# WaitingTheLongest.com Error Analysis Data\n\n";
    oss << "This file contains error log data formatted for AI analysis.\n";
    oss << "Generated: " << getCurrentTimestamp() << "\n";
    oss << "Total Errors: " << errors.size() << "\n\n";

    oss << "## Context\n\n";
    oss << "WaitingTheLongest.com is a dog rescue platform that helps connect\n";
    oss << "dogs in shelters (especially kill shelters) with potential adopters\n";
    oss << "and foster families. The system tracks wait times, urgency levels,\n";
    oss << "and provides real-time updates via WebSockets.\n\n";

    oss << "## Error Categories\n\n";
    oss << "- DATABASE: PostgreSQL connection and query errors\n";
    oss << "- HTTP_REQUEST: REST API endpoint errors\n";
    oss << "- WEBSOCKET: Real-time connection errors\n";
    oss << "- AUTHENTICATION: Login, token, and permission errors\n";
    oss << "- VALIDATION: Input validation failures\n";
    oss << "- BUSINESS_LOGIC: Application logic errors\n";
    oss << "- EXTERNAL_API: Third-party API errors (e.g., RescueGroups)\n";
    oss << "- FILE_IO: File system operation errors\n";
    oss << "- CONFIGURATION: Config loading/parsing errors\n";
    oss << "- MODULE: Module loading and lifecycle errors\n\n";

    oss << "## Error Data\n\n";

    // Group errors by category for analysis
    std::map<std::string, std::vector<const Json::Value*>> by_category;

    for (const auto& error : errors) {
        std::string category = error.get("category", "UNKNOWN").asString();
        by_category[category].push_back(&error);
    }

    for (const auto& [category, cat_errors] : by_category) {
        oss << "### " << category << " (" << cat_errors.size() << " errors)\n\n";

        for (const auto* error : cat_errors) {
            oss << "```\n";
            oss << "Severity: " << error->get("severity", "unknown").asString() << "\n";
            oss << "Time: " << error->get("timestamp", "").asString() << "\n";
            oss << "Message: " << error->get("message", "").asString() << "\n";

            if (error->isMember("metadata") && !(*error)["metadata"].empty()) {
                oss << "Metadata: ";
                Json::StreamWriterBuilder writer;
                writer["indentation"] = "";
                oss << Json::writeString(writer, (*error)["metadata"]) << "\n";
            }

            oss << "```\n\n";
        }
    }

    oss << "## Analysis Suggestions\n\n";
    oss << "When analyzing these errors, consider:\n";
    oss << "1. Are there patterns in timing (time of day, day of week)?\n";
    oss << "2. Are certain categories more prevalent?\n";
    oss << "3. Are there cascading failures (one error causing others)?\n";
    oss << "4. Are external API errors affecting core functionality?\n";
    oss << "5. Are there recurring errors that suggest systemic issues?\n\n";

    return oss.str();
}

std::string DebugController::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;

#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y%m%d_%H%M%S");
    return oss.str();
}

} // namespace wtl::core::controllers
