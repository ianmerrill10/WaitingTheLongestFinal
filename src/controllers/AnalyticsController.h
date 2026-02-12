/**
 * @file AnalyticsController.h
 * @brief REST API controller for analytics endpoints
 *
 * PURPOSE:
 * Provides REST API endpoints for:
 * - Tracking analytics events
 * - Querying dashboard statistics
 * - Generating reports
 * - Viewing impact metrics
 * - Social media analytics
 *
 * USAGE:
 * Endpoints are automatically registered with Drogon via METHOD_LIST_BEGIN.
 * POST /api/analytics/event - Track an event
 * GET /api/analytics/dashboard - Get dashboard stats
 *
 * DEPENDENCIES:
 * - AnalyticsService (event tracking and queries)
 * - AuthMiddleware (protected endpoints)
 * - ApiResponse (response formatting)
 *
 * MODIFICATION GUIDE:
 * - Add new endpoints to METHOD_LIST_BEGIN
 * - Use REQUIRE_AUTH for admin-only endpoints
 * - Follow standard response format
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <drogon/HttpController.h>

namespace wtl::controllers {

/**
 * @class AnalyticsController
 * @brief REST API controller for analytics operations
 *
 * Provides endpoints for tracking events, querying statistics,
 * and generating reports from the analytics system.
 */
class AnalyticsController : public drogon::HttpController<AnalyticsController> {
public:
    METHOD_LIST_BEGIN
    // Event tracking
    ADD_METHOD_TO(AnalyticsController::trackEvent, "/api/analytics/event", drogon::Post);
    ADD_METHOD_TO(AnalyticsController::trackPageView, "/api/analytics/pageview", drogon::Post);
    ADD_METHOD_TO(AnalyticsController::trackBatch, "/api/analytics/batch", drogon::Post);

    // Dashboard statistics
    ADD_METHOD_TO(AnalyticsController::getDashboard, "/api/analytics/dashboard", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::getRealTimeStats, "/api/analytics/realtime", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::getTrends, "/api/analytics/trends", drogon::Get);

    // Entity-specific statistics
    ADD_METHOD_TO(AnalyticsController::getDogStats, "/api/analytics/dogs/{id}", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::getShelterStats, "/api/analytics/shelters/{id}", drogon::Get);

    // Social media analytics
    ADD_METHOD_TO(AnalyticsController::getSocialStats, "/api/analytics/social", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::getSocialPlatformStats, "/api/analytics/social/{platform}", drogon::Get);

    // Impact tracking
    ADD_METHOD_TO(AnalyticsController::getImpactStats, "/api/analytics/impact", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::getRecentImpact, "/api/analytics/impact/recent", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::getLivesSaved, "/api/analytics/impact/lives-saved", drogon::Get);

    // Reports
    ADD_METHOD_TO(AnalyticsController::generateReport, "/api/analytics/reports/{type}", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::getReportTypes, "/api/analytics/reports", drogon::Get);

    // Top performers
    ADD_METHOD_TO(AnalyticsController::getTopDogs, "/api/analytics/top/dogs", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::getTopShelters, "/api/analytics/top/shelters", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::getTopContent, "/api/analytics/top/content", drogon::Get);

    // Admin endpoints (require authentication)
    ADD_METHOD_TO(AnalyticsController::getWorkerStatus, "/api/analytics/admin/worker", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::triggerAggregation, "/api/analytics/admin/aggregate", drogon::Post);
    ADD_METHOD_TO(AnalyticsController::triggerCleanup, "/api/analytics/admin/cleanup", drogon::Post);
    ADD_METHOD_TO(AnalyticsController::getEventsByEntity, "/api/analytics/admin/events/{entity_type}/{entity_id}", drogon::Get);
    METHOD_LIST_END

    // =========================================================================
    // EVENT TRACKING HANDLERS
    // =========================================================================

    /**
     * @brief Track a single analytics event
     * POST /api/analytics/event
     */
    void trackEvent(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Track a page view event
     * POST /api/analytics/pageview
     */
    void trackPageView(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Track multiple events in batch
     * POST /api/analytics/batch
     */
    void trackBatch(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // =========================================================================
    // DASHBOARD HANDLERS
    // =========================================================================

    /**
     * @brief Get dashboard statistics
     * GET /api/analytics/dashboard?start_date=&end_date=
     */
    void getDashboard(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get real-time statistics
     * GET /api/analytics/realtime
     */
    void getRealTimeStats(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get trend data
     * GET /api/analytics/trends?metric=&start_date=&end_date=
     */
    void getTrends(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // =========================================================================
    // ENTITY STATISTICS HANDLERS
    // =========================================================================

    /**
     * @brief Get statistics for a specific dog
     * GET /api/analytics/dogs/:id
     */
    void getDogStats(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& id);

    /**
     * @brief Get statistics for a specific shelter
     * GET /api/analytics/shelters/:id
     */
    void getShelterStats(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                         const std::string& id);

    // =========================================================================
    // SOCIAL ANALYTICS HANDLERS
    // =========================================================================

    /**
     * @brief Get overall social media statistics
     * GET /api/analytics/social
     */
    void getSocialStats(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get statistics for a specific social platform
     * GET /api/analytics/social/:platform
     */
    void getSocialPlatformStats(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                const std::string& platform);

    // =========================================================================
    // IMPACT HANDLERS
    // =========================================================================

    /**
     * @brief Get impact statistics
     * GET /api/analytics/impact
     */
    void getImpactStats(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get recent impact events
     * GET /api/analytics/impact/recent?limit=
     */
    void getRecentImpact(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get total lives saved
     * GET /api/analytics/impact/lives-saved
     */
    void getLivesSaved(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // =========================================================================
    // REPORT HANDLERS
    // =========================================================================

    /**
     * @brief Generate a report
     * GET /api/analytics/reports/:type?start_date=&end_date=&format=
     */
    void generateReport(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& type);

    /**
     * @brief Get available report types
     * GET /api/analytics/reports
     */
    void getReportTypes(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // =========================================================================
    // TOP PERFORMERS HANDLERS
    // =========================================================================

    /**
     * @brief Get top dogs by metric
     * GET /api/analytics/top/dogs?metric=&limit=
     */
    void getTopDogs(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get top shelters by metric
     * GET /api/analytics/top/shelters?metric=&limit=
     */
    void getTopShelters(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get top performing content
     * GET /api/analytics/top/content?metric=&limit=
     */
    void getTopContent(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // =========================================================================
    // ADMIN HANDLERS
    // =========================================================================

    /**
     * @brief Get analytics worker status
     * GET /api/analytics/admin/worker
     */
    void getWorkerStatus(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Trigger manual aggregation
     * POST /api/analytics/admin/aggregate
     */
    void triggerAggregation(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Trigger manual cleanup
     * POST /api/analytics/admin/cleanup
     */
    void triggerCleanup(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get events for a specific entity
     * GET /api/analytics/admin/events/:entity_type/:entity_id
     */
    void getEventsByEntity(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                           const std::string& entity_type,
                           const std::string& entity_id);
};

} // namespace wtl::controllers
