/**
 * @file AggregatorController.h
 * @brief REST API controller for aggregator management (admin only)
 *
 * PURPOSE:
 * Provides admin endpoints for managing and monitoring data source
 * aggregators, triggering syncs, and viewing statistics.
 *
 * USAGE:
 * Endpoints auto-register with Drogon via HttpController inheritance.
 *
 * DEPENDENCIES:
 * - Drogon framework
 * - AggregatorManager
 * - AuthMiddleware for admin authentication
 *
 * MODIFICATION GUIDE:
 * - Add new endpoints using ADD_METHOD_TO macro
 * - All endpoints require admin role
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Third-party includes
#include <drogon/HttpController.h>

namespace wtl::controllers {

/**
 * @class AggregatorController
 * @brief REST API controller for aggregator management
 *
 * All endpoints are admin-only. Provides:
 * - List aggregators
 * - Get aggregator status
 * - Trigger syncs
 * - View statistics and errors
 */
class AggregatorController : public drogon::HttpController<AggregatorController> {
public:
    METHOD_LIST_BEGIN
    // List all aggregators
    ADD_METHOD_TO(AggregatorController::getAllAggregators, "/api/aggregators", drogon::Get);

    // Get specific aggregator status
    ADD_METHOD_TO(AggregatorController::getAggregator, "/api/aggregators/{name}", drogon::Get);

    // Trigger full sync for all aggregators
    ADD_METHOD_TO(AggregatorController::syncAll, "/api/aggregators/sync", drogon::Post);

    // Trigger sync for specific aggregator
    ADD_METHOD_TO(AggregatorController::syncOne, "/api/aggregators/{name}/sync", drogon::Post);

    // Get sync statistics
    ADD_METHOD_TO(AggregatorController::getStats, "/api/aggregators/stats", drogon::Get);

    // Get recent sync errors
    ADD_METHOD_TO(AggregatorController::getErrors, "/api/aggregators/errors", drogon::Get);

    // Cancel running sync
    ADD_METHOD_TO(AggregatorController::cancelSync, "/api/aggregators/{name}/cancel", drogon::Post);

    // Cancel all running syncs
    ADD_METHOD_TO(AggregatorController::cancelAllSyncs, "/api/aggregators/cancel", drogon::Post);

    // Enable/disable aggregator
    ADD_METHOD_TO(AggregatorController::setAggregatorEnabled, "/api/aggregators/{name}/enabled", drogon::Put);

    // Get aggregator health details
    ADD_METHOD_TO(AggregatorController::getHealth, "/api/aggregators/{name}/health", drogon::Get);

    // Test aggregator connection
    ADD_METHOD_TO(AggregatorController::testConnection, "/api/aggregators/{name}/test", drogon::Post);

    // Get sync history
    ADD_METHOD_TO(AggregatorController::getSyncHistory, "/api/aggregators/history", drogon::Get);

    // Auto-sync management
    ADD_METHOD_TO(AggregatorController::getAutoSyncStatus, "/api/aggregators/auto-sync", drogon::Get);
    ADD_METHOD_TO(AggregatorController::setAutoSync, "/api/aggregators/auto-sync", drogon::Put);

    METHOD_LIST_END

    // =========================================================================
    // HANDLER METHODS
    // =========================================================================

    /**
     * @brief GET /api/aggregators - List all aggregators
     */
    void getAllAggregators(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief GET /api/aggregators/{name} - Get specific aggregator status
     */
    void getAggregator(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       const std::string& name);

    /**
     * @brief POST /api/aggregators/sync - Trigger full sync for all
     */
    void syncAll(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief POST /api/aggregators/{name}/sync - Sync specific aggregator
     */
    void syncOne(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                 const std::string& name);

    /**
     * @brief GET /api/aggregators/stats - Get sync statistics
     */
    void getStats(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief GET /api/aggregators/errors - Get recent sync errors
     */
    void getErrors(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief POST /api/aggregators/{name}/cancel - Cancel running sync
     */
    void cancelSync(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& name);

    /**
     * @brief POST /api/aggregators/cancel - Cancel all running syncs
     */
    void cancelAllSyncs(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief PUT /api/aggregators/{name}/enabled - Enable/disable aggregator
     */
    void setAggregatorEnabled(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                               const std::string& name);

    /**
     * @brief GET /api/aggregators/{name}/health - Get health details
     */
    void getHealth(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& name);

    /**
     * @brief POST /api/aggregators/{name}/test - Test connection
     */
    void testConnection(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& name);

    /**
     * @brief GET /api/aggregators/history - Get sync history
     */
    void getSyncHistory(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief GET /api/aggregators/auto-sync - Get auto-sync status
     */
    void getAutoSyncStatus(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief PUT /api/aggregators/auto-sync - Configure auto-sync
     */
    void setAutoSync(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace wtl::controllers
