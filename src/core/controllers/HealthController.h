/**
 * @file HealthController.h
 * @brief REST API endpoints for health checks
 *
 * PURPOSE:
 * Provides health check endpoints for monitoring the application status.
 * Used by load balancers, container orchestration, and monitoring systems
 * to verify the application is running and healthy.
 *
 * USAGE:
 * This controller is automatically registered with Drogon and handles:
 * - GET /api/health          - Basic health check (public)
 * - GET /api/health/detailed - Detailed health check (admin only)
 *
 * DEPENDENCIES:
 * - ConnectionPool (Agent 1) - For database health check
 * - AuthMiddleware (Agent 6) - For admin-only detailed health
 * - ErrorCapture (Agent 1) - Error logging
 * - ApiResponse (Agent 4) - Standardized response formatting
 *
 * MODIFICATION GUIDE:
 * - Add new health checks to the detailed endpoint
 * - Keep basic endpoint fast and lightweight for load balancer probes
 * - All errors must be captured with WTL_CAPTURE_ERROR
 *
 * @author Agent 4 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <string>

// Third-party includes
#include <drogon/HttpController.h>

namespace wtl::core::controllers {

/**
 * @class HealthController
 * @brief HTTP controller for health check API endpoints
 *
 * Provides endpoints for monitoring application health.
 * The basic endpoint is public, while the detailed endpoint requires admin access.
 */
class HealthController : public drogon::HttpController<HealthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HealthController::getHealth, "/api/health", drogon::Get);
    ADD_METHOD_TO(HealthController::getDetailedHealth, "/api/health/detailed", drogon::Get);
    METHOD_LIST_END

    /**
     * @brief Basic health check endpoint
     *
     * Returns a simple health status for load balancer probes.
     * Does not perform database checks for speed.
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getHealth(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Detailed health check endpoint (admin only)
     *
     * Returns detailed health status including:
     * - Application version
     * - Uptime
     * - Database connection status
     * - Connection pool stats
     * - Memory usage
     * - Active connections
     * - Circuit breaker states
     *
     * @param req HTTP request
     * @param callback Response callback
     */
    void getDetailedHealth(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    /**
     * @brief Check database connectivity
     *
     * @return true if database is accessible, false otherwise
     */
    bool checkDatabaseHealth();

    /**
     * @brief Get application uptime
     *
     * @return Formatted uptime string
     */
    std::string getUptime();

    /**
     * @brief Get memory usage statistics
     *
     * @return JSON object with memory stats
     */
    Json::Value getMemoryStats();
};

} // namespace wtl::core::controllers
