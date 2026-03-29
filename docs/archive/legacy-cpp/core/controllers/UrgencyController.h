/**
 * @file UrgencyController.h
 * @brief REST API controller for urgency engine endpoints
 *
 * PURPOSE:
 * Provides HTTP API endpoints for accessing urgency data, alerts, and
 * statistics. This controller exposes the urgency engine's functionality
 * to the frontend and external systems.
 *
 * ENDPOINTS:
 * - GET  /api/urgency/critical        - Get critical dogs (< 24 hours)
 * - GET  /api/urgency/high            - Get high urgency dogs (1-3 days)
 * - GET  /api/urgency/alerts          - Get active alerts (admin)
 * - GET  /api/urgency/shelter/:id     - Get urgency status for shelter
 * - GET  /api/urgency/statistics      - Get urgency statistics
 * - POST /api/urgency/recalculate     - Force recalculation (admin)
 * - POST /api/urgency/acknowledge/:id - Acknowledge alert (admin)
 *
 * USAGE:
 * Controller auto-registers with Drogon via HttpController inheritance.
 * Endpoints are available once the application starts.
 *
 * DEPENDENCIES:
 * - UrgencyCalculator, KillShelterMonitor, AlertService, UrgencyWorker (this agent)
 * - ApiResponse utility (Agent 4)
 * - AuthMiddleware (Agent 6) for protected endpoints
 *
 * MODIFICATION GUIDE:
 * - To add new endpoints: Add to METHOD_LIST, declare and implement handler
 * - To modify responses: Update the JSON building in each handler
 * - Protected endpoints should use REQUIRE_AUTH or REQUIRE_ROLE macros
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Third-party includes
#include <drogon/HttpController.h>
#include <drogon/HttpTypes.h>

// Standard includes
#include <functional>
#include <string>

namespace wtl::core::controllers {

/**
 * @class UrgencyController
 * @brief HTTP controller for urgency engine API endpoints
 *
 * Provides REST API access to urgency data, alerts, and management functions.
 * Some endpoints are public (for displaying urgent dogs) while others
 * require admin authentication.
 */
class UrgencyController : public drogon::HttpController<UrgencyController> {
public:
    // ========================================================================
    // ROUTE DEFINITIONS
    // ========================================================================

    METHOD_LIST_BEGIN

    // Public endpoints - show urgent dogs to everyone
    ADD_METHOD_TO(UrgencyController::getCriticalDogs,
                  "/api/urgency/critical",
                  drogon::Get);

    ADD_METHOD_TO(UrgencyController::getHighUrgencyDogs,
                  "/api/urgency/high",
                  drogon::Get);

    ADD_METHOD_TO(UrgencyController::getUrgentDogs,
                  "/api/urgency/urgent",
                  drogon::Get);

    ADD_METHOD_TO(UrgencyController::getDogsAtRisk,
                  "/api/urgency/at-risk",
                  drogon::Get);

    // Shelter-specific endpoints
    ADD_METHOD_TO(UrgencyController::getShelterUrgencyStatus,
                  "/api/urgency/shelter/{shelter_id}",
                  drogon::Get);

    ADD_METHOD_TO(UrgencyController::getMostUrgentShelters,
                  "/api/urgency/shelters",
                  drogon::Get);

    // Alert endpoints (some require admin)
    ADD_METHOD_TO(UrgencyController::getActiveAlerts,
                  "/api/urgency/alerts",
                  drogon::Get);

    ADD_METHOD_TO(UrgencyController::getCriticalAlerts,
                  "/api/urgency/alerts/critical",
                  drogon::Get);

    ADD_METHOD_TO(UrgencyController::acknowledgeAlert,
                  "/api/urgency/alerts/{alert_id}/acknowledge",
                  drogon::Post);

    ADD_METHOD_TO(UrgencyController::resolveAlert,
                  "/api/urgency/alerts/{alert_id}/resolve",
                  drogon::Post);

    // Statistics endpoints
    ADD_METHOD_TO(UrgencyController::getStatistics,
                  "/api/urgency/statistics",
                  drogon::Get);

    ADD_METHOD_TO(UrgencyController::getWorkerStatus,
                  "/api/urgency/worker/status",
                  drogon::Get);

    // Admin endpoints (require authentication)
    ADD_METHOD_TO(UrgencyController::forceRecalculate,
                  "/api/urgency/recalculate",
                  drogon::Post);

    ADD_METHOD_TO(UrgencyController::forceRecalculateShelter,
                  "/api/urgency/recalculate/{shelter_id}",
                  drogon::Post);

    // Dog-specific urgency
    ADD_METHOD_TO(UrgencyController::getDogUrgency,
                  "/api/urgency/dog/{dog_id}",
                  drogon::Get);

    ADD_METHOD_TO(UrgencyController::getDogCountdown,
                  "/api/urgency/dog/{dog_id}/countdown",
                  drogon::Get);

    METHOD_LIST_END

    // ========================================================================
    // PUBLIC DOG ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all critical dogs (< 24 hours)
     *
     * GET /api/urgency/critical
     *
     * Returns dogs that have less than 24 hours until scheduled euthanasia.
     * These are the most urgent cases requiring immediate action.
     *
     * Query params:
     * - state: Filter by state code (optional)
     * - limit: Maximum results (default 50)
     */
    void getCriticalDogs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get high urgency dogs (1-3 days)
     *
     * GET /api/urgency/high
     *
     * Returns dogs that have 1-3 days until scheduled euthanasia.
     *
     * Query params:
     * - state: Filter by state code (optional)
     * - limit: Maximum results (default 50)
     */
    void getHighUrgencyDogs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get all urgent dogs (critical + high)
     *
     * GET /api/urgency/urgent
     *
     * Returns all dogs with HIGH or CRITICAL urgency levels.
     */
    void getUrgentDogs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get dogs at risk within specified days
     *
     * GET /api/urgency/at-risk
     *
     * Query params:
     * - days: Days threshold (default 7)
     * - state: Filter by state code (optional)
     * - shelter_id: Filter by shelter (optional)
     * - limit: Maximum results (default 100)
     */
    void getDogsAtRisk(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // SHELTER ENDPOINTS
    // ========================================================================

    /**
     * @brief Get urgency status for a specific shelter
     *
     * GET /api/urgency/shelter/:shelter_id
     *
     * Returns detailed urgency breakdown for the shelter including
     * counts by urgency level and list of dogs at risk.
     */
    void getShelterUrgencyStatus(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& shelter_id);

    /**
     * @brief Get shelters sorted by urgency
     *
     * GET /api/urgency/shelters
     *
     * Returns kill shelters sorted by urgency score (most urgent first).
     *
     * Query params:
     * - limit: Maximum shelters (default 20)
     * - state: Filter by state (optional)
     */
    void getMostUrgentShelters(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // ALERT ENDPOINTS
    // ========================================================================

    /**
     * @brief Get active alerts
     *
     * GET /api/urgency/alerts
     *
     * Returns all unresolved urgency alerts.
     * Requires authentication for sensitive data.
     *
     * Query params:
     * - shelter_id: Filter by shelter (optional)
     * - state: Filter by state (optional)
     * - type: Filter by alert type (optional)
     */
    void getActiveAlerts(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get critical alerts only
     *
     * GET /api/urgency/alerts/critical
     */
    void getCriticalAlerts(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Acknowledge an alert
     *
     * POST /api/urgency/alerts/:alert_id/acknowledge
     *
     * Marks an alert as acknowledged. Requires admin authentication.
     */
    void acknowledgeAlert(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& alert_id);

    /**
     * @brief Resolve an alert
     *
     * POST /api/urgency/alerts/:alert_id/resolve
     *
     * Marks an alert as resolved with an outcome. Requires admin authentication.
     *
     * Request body:
     * {
     *   "resolution": "adopted" | "fostered" | "rescued" | "other",
     *   "notes": "Optional notes"
     * }
     */
    void resolveAlert(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& alert_id);

    // ========================================================================
    // STATISTICS ENDPOINTS
    // ========================================================================

    /**
     * @brief Get urgency statistics
     *
     * GET /api/urgency/statistics
     *
     * Returns comprehensive statistics about urgency levels across the system.
     */
    void getStatistics(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get background worker status
     *
     * GET /api/urgency/worker/status
     *
     * Returns status of the background urgency worker.
     */
    void getWorkerStatus(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // ADMIN ENDPOINTS
    // ========================================================================

    /**
     * @brief Force recalculation of all urgency levels
     *
     * POST /api/urgency/recalculate
     *
     * Triggers immediate recalculation of urgency levels for all dogs.
     * Requires admin authentication.
     */
    void forceRecalculate(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Force recalculation for a specific shelter
     *
     * POST /api/urgency/recalculate/:shelter_id
     *
     * Triggers immediate recalculation for dogs at a specific shelter.
     * Requires admin authentication.
     */
    void forceRecalculateShelter(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& shelter_id);

    // ========================================================================
    // DOG-SPECIFIC ENDPOINTS
    // ========================================================================

    /**
     * @brief Get urgency details for a specific dog
     *
     * GET /api/urgency/dog/:dog_id
     *
     * Returns detailed urgency information for a single dog.
     */
    void getDogUrgency(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id);

    /**
     * @brief Get countdown for a dog
     *
     * GET /api/urgency/dog/:dog_id/countdown
     *
     * Returns countdown components for WebSocket-like polling.
     */
    void getDogCountdown(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& dog_id);

private:
    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Create a success response with data
     */
    static drogon::HttpResponsePtr successResponse(const Json::Value& data);

    /**
     * @brief Create a success response with data and metadata
     */
    static drogon::HttpResponsePtr successResponse(
        const Json::Value& data,
        int total,
        int page,
        int per_page);

    /**
     * @brief Create an error response
     */
    static drogon::HttpResponsePtr errorResponse(
        const std::string& code,
        const std::string& message,
        drogon::HttpStatusCode status = drogon::k400BadRequest);

    /**
     * @brief Create a not found response
     */
    static drogon::HttpResponsePtr notFoundResponse(const std::string& resource);

    /**
     * @brief Parse integer query parameter with default
     */
    static int getIntParam(
        const drogon::HttpRequestPtr& req,
        const std::string& name,
        int default_value);

    /**
     * @brief Parse string query parameter
     */
    static std::string getStringParam(
        const drogon::HttpRequestPtr& req,
        const std::string& name,
        const std::string& default_value = "");
};

} // namespace wtl::core::controllers
