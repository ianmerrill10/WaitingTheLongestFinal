/**
 * @file DebugController.h
 * @brief REST API endpoints for debug and error management
 *
 * PURPOSE:
 * Provides administrative endpoints for viewing error logs, exporting logs,
 * monitoring system health, managing circuit breakers, and triggering
 * self-healing actions. All endpoints require admin privileges.
 *
 * USAGE:
 * This controller is automatically registered with Drogon and handles:
 * - GET  /api/debug/errors        - Get error logs
 * - GET  /api/debug/errors/export - Export logs as file (txt, json, ai)
 * - GET  /api/debug/health        - System health status
 * - GET  /api/debug/circuits      - Circuit breaker states
 * - POST /api/debug/heal          - Trigger healing action
 * - GET  /api/debug/stats         - Error statistics
 *
 * DEPENDENCIES:
 * - ErrorCapture (Agent 1) - Error logging and retrieval
 * - SelfHealing (Agent 1) - Self-healing and circuit breaker management
 * - ConnectionPool (Agent 1) - Database health checks
 * - AuthMiddleware (Agent 6) - Admin authentication
 * - ApiResponse (Agent 4) - Standardized response formatting
 *
 * MODIFICATION GUIDE:
 * - Add new debug endpoints by adding to METHOD_LIST
 * - All endpoints must require admin role
 * - Export formats can be extended by adding new format handlers
 * - All errors must be captured with WTL_CAPTURE_ERROR
 *
 * @author Agent 4 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <string>
#include <vector>

// Third-party includes
#include <drogon/HttpController.h>

namespace wtl::core::controllers {

/**
 * @class DebugController
 * @brief HTTP controller for debug and error management API endpoints
 *
 * All endpoints require admin authentication. Provides access to error logs,
 * circuit breaker states, and self-healing controls.
 */
class DebugController : public drogon::HttpController<DebugController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DebugController::getErrors, "/api/debug/errors", drogon::Get);
    ADD_METHOD_TO(DebugController::exportErrors, "/api/debug/errors/export", drogon::Get);
    ADD_METHOD_TO(DebugController::getSystemHealth, "/api/debug/health", drogon::Get);
    ADD_METHOD_TO(DebugController::getCircuitBreakers, "/api/debug/circuits", drogon::Get);
    ADD_METHOD_TO(DebugController::triggerHeal, "/api/debug/heal", drogon::Post);
    ADD_METHOD_TO(DebugController::getErrorStats, "/api/debug/stats", drogon::Get);
    METHOD_LIST_END

    // ========================================================================
    // ERROR LOG ENDPOINTS
    // ========================================================================

    /**
     * @brief Get error logs with optional filtering
     *
     * Query Parameters:
     * - severity (string): Filter by severity (info, warning, error, critical)
     * - category (string): Filter by category (DATABASE, HTTP_REQUEST, etc.)
     * - from (timestamp): Start time filter (ISO 8601 or Unix timestamp)
     * - to (timestamp): End time filter
     * - page (int, default 1): Page number
     * - per_page (int, default 50, max 200): Items per page
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getErrors(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Export error logs as downloadable file
     *
     * Query Parameters:
     * - format (string): Export format (txt, json, ai)
     * - severity (string): Filter by severity
     * - category (string): Filter by category
     * - from (timestamp): Start time filter
     * - to (timestamp): End time filter
     *
     * Formats:
     * - txt: Human-readable text format
     * - json: Machine-readable JSON format
     * - ai: AI-friendly format with context for analysis
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void exportErrors(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // SYSTEM HEALTH ENDPOINTS
    // ========================================================================

    /**
     * @brief Get comprehensive system health status
     *
     * Returns detailed health information including:
     * - Database connectivity
     * - Connection pool status
     * - Error rates
     * - Circuit breaker states
     * - Self-healing status
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getSystemHealth(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get all circuit breaker states
     *
     * Returns state information for each circuit breaker:
     * - state (closed, open, half_open)
     * - failure_count
     * - last_failure_time
     * - next_retry_time (if open)
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getCircuitBreakers(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // HEALING ENDPOINTS
    // ========================================================================

    /**
     * @brief Trigger a healing action
     *
     * Request Body:
     * - action (string): Healing action to perform
     *   - "reset_circuits" - Reset all circuit breakers
     *   - "reset_circuit" - Reset specific circuit (requires "circuit_name")
     *   - "reset_pool" - Reset database connection pool
     *   - "clear_cache" - Clear application caches
     *   - "run_diagnostics" - Run diagnostic checks
     *
     * @param req HTTP request with action data
     * @param callback Response callback
     */
    void triggerHeal(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // STATISTICS ENDPOINTS
    // ========================================================================

    /**
     * @brief Get error statistics
     *
     * Query Parameters:
     * - period (string): Time period (hour, day, week, month)
     *
     * Returns:
     * - Total errors by severity
     * - Total errors by category
     * - Error rate trends
     * - Most frequent errors
     * - Healing action history
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getErrorStats(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    /**
     * @brief Parse time range parameters from request
     *
     * @param req HTTP request
     * @param from_time Output: start time
     * @param to_time Output: end time
     */
    void parseTimeRange(const drogon::HttpRequestPtr& req,
                        std::chrono::system_clock::time_point& from_time,
                        std::chrono::system_clock::time_point& to_time);

    /**
     * @brief Format errors as plain text
     *
     * @param errors Vector of error entries
     * @return Formatted text string
     */
    std::string formatErrorsAsText(const std::vector<Json::Value>& errors);

    /**
     * @brief Format errors as JSON
     *
     * @param errors Vector of error entries
     * @return JSON array
     */
    Json::Value formatErrorsAsJson(const std::vector<Json::Value>& errors);

    /**
     * @brief Format errors for AI analysis
     *
     * Includes additional context and suggestions for AI analysis.
     *
     * @param errors Vector of error entries
     * @return AI-friendly formatted string
     */
    std::string formatErrorsForAI(const std::vector<Json::Value>& errors);

    /**
     * @brief Get current timestamp as ISO 8601 string
     *
     * @return ISO 8601 formatted timestamp
     */
    std::string getCurrentTimestamp();
};

} // namespace wtl::core::controllers
