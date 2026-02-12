/**
 * @file AdminController.h
 * @brief REST API controller for admin dashboard operations
 *
 * PURPOSE:
 * Provides HTTP API endpoints for the admin dashboard including system stats,
 * dog management, shelter management, user management, and system configuration.
 * All endpoints require admin authentication.
 *
 * ENDPOINTS:
 * - GET  /api/admin/dashboard           - Main dashboard statistics
 * - GET  /api/admin/dogs                - Dog management (paginated)
 * - PUT  /api/admin/dogs/{id}           - Update dog
 * - DELETE /api/admin/dogs/{id}         - Remove dog
 * - GET  /api/admin/shelters            - Shelter management
 * - PUT  /api/admin/shelters/{id}       - Update shelter
 * - GET  /api/admin/users               - User management
 * - PUT  /api/admin/users/{id}          - Update user
 * - PUT  /api/admin/users/{id}/role     - Change user role
 * - GET  /api/admin/config              - Get system configuration
 * - PUT  /api/admin/config              - Update system configuration
 * - GET  /api/admin/audit-log           - Get audit log entries
 *
 * USAGE:
 * Controller auto-registers with Drogon via HttpController inheritance.
 * All endpoints require admin authentication via REQUIRE_ADMIN macro.
 *
 * DEPENDENCIES:
 * - AdminService (dashboard stats, system health)
 * - SystemConfig (configuration management)
 * - AuditLog (action logging)
 * - AuthMiddleware (authentication)
 * - DogService, ShelterService, UserService (entity management)
 * - ApiResponse (response formatting)
 *
 * MODIFICATION GUIDE:
 * - To add new endpoints: Add to METHOD_LIST, declare and implement handler
 * - All admin actions must be logged via AuditLog
 * - All responses must use ApiResponse utility
 *
 * @author Agent 20 - WaitingTheLongest Build System
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
 * @class AdminController
 * @brief HTTP controller for admin dashboard API endpoints
 *
 * Provides comprehensive admin dashboard functionality including:
 * - Dashboard statistics and system health
 * - Dog, shelter, and user management
 * - System configuration
 * - Audit logging
 *
 * All endpoints require admin authentication.
 */
class AdminController : public drogon::HttpController<AdminController> {
public:
    // ========================================================================
    // ROUTE DEFINITIONS
    // ========================================================================

    METHOD_LIST_BEGIN

    // Dashboard endpoints
    ADD_METHOD_TO(AdminController::getDashboardStats,
                  "/api/admin/dashboard",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getSystemHealth,
                  "/api/admin/health",
                  drogon::Get);

    // Dog management endpoints
    ADD_METHOD_TO(AdminController::getDogs,
                  "/api/admin/dogs",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getDogById,
                  "/api/admin/dogs/{id}",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::updateDog,
                  "/api/admin/dogs/{id}",
                  drogon::Put);

    ADD_METHOD_TO(AdminController::deleteDog,
                  "/api/admin/dogs/{id}",
                  drogon::Delete);

    ADD_METHOD_TO(AdminController::batchUpdateDogs,
                  "/api/admin/dogs/batch",
                  drogon::Put);

    // Shelter management endpoints
    ADD_METHOD_TO(AdminController::getShelters,
                  "/api/admin/shelters",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getShelterById,
                  "/api/admin/shelters/{id}",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::updateShelter,
                  "/api/admin/shelters/{id}",
                  drogon::Put);

    ADD_METHOD_TO(AdminController::verifyShelter,
                  "/api/admin/shelters/{id}/verify",
                  drogon::Post);

    ADD_METHOD_TO(AdminController::getShelterStats,
                  "/api/admin/shelters/{id}/stats",
                  drogon::Get);

    // User management endpoints
    ADD_METHOD_TO(AdminController::getUsers,
                  "/api/admin/users",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getUserById,
                  "/api/admin/users/{id}",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::updateUser,
                  "/api/admin/users/{id}",
                  drogon::Put);

    ADD_METHOD_TO(AdminController::changeUserRole,
                  "/api/admin/users/{id}/role",
                  drogon::Put);

    ADD_METHOD_TO(AdminController::disableUser,
                  "/api/admin/users/{id}/disable",
                  drogon::Post);

    ADD_METHOD_TO(AdminController::enableUser,
                  "/api/admin/users/{id}/enable",
                  drogon::Post);

    // System configuration endpoints
    ADD_METHOD_TO(AdminController::getConfig,
                  "/api/admin/config",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::updateConfig,
                  "/api/admin/config",
                  drogon::Put);

    ADD_METHOD_TO(AdminController::getConfigByKey,
                  "/api/admin/config/{key}",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::setConfigValue,
                  "/api/admin/config/{key}",
                  drogon::Put);

    // Audit log endpoints
    ADD_METHOD_TO(AdminController::getAuditLog,
                  "/api/admin/audit-log",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getAuditLogByUser,
                  "/api/admin/audit-log/user/{user_id}",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getAuditLogByEntity,
                  "/api/admin/audit-log/entity/{entity_type}/{entity_id}",
                  drogon::Get);

    // Content management endpoints
    ADD_METHOD_TO(AdminController::getContentQueue,
                  "/api/admin/content/queue",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::approveContent,
                  "/api/admin/content/{id}/approve",
                  drogon::Post);

    ADD_METHOD_TO(AdminController::rejectContent,
                  "/api/admin/content/{id}/reject",
                  drogon::Post);

    // Analytics endpoints
    ADD_METHOD_TO(AdminController::getAnalytics,
                  "/api/admin/analytics",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getTrafficStats,
                  "/api/admin/analytics/traffic",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getAdoptionFunnel,
                  "/api/admin/analytics/adoption-funnel",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getSocialPerformance,
                  "/api/admin/analytics/social",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::getImpactMetrics,
                  "/api/admin/analytics/impact",
                  drogon::Get);

    // Worker management endpoints
    ADD_METHOD_TO(AdminController::getWorkerStatus,
                  "/api/admin/workers",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::restartWorker,
                  "/api/admin/workers/{name}/restart",
                  drogon::Post);

    // Feature flags endpoints
    ADD_METHOD_TO(AdminController::getFeatureFlags,
                  "/api/admin/features",
                  drogon::Get);

    ADD_METHOD_TO(AdminController::setFeatureFlag,
                  "/api/admin/features/{key}",
                  drogon::Put);

    METHOD_LIST_END

    // ========================================================================
    // DASHBOARD ENDPOINTS
    // ========================================================================

    /**
     * @brief Get main dashboard statistics
     *
     * GET /api/admin/dashboard
     *
     * Returns comprehensive statistics for the admin dashboard including:
     * - Total dogs, shelters, users counts
     * - Active users in last 24h/7d/30d
     * - Urgency overview (critical, high, medium, normal)
     * - Recent activity summary
     * - System health indicators
     */
    void getDashboardStats(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get system health status
     *
     * GET /api/admin/health
     *
     * Returns detailed system health information including:
     * - Database connectivity and pool stats
     * - Worker status
     * - Error rates
     * - Memory/CPU usage (if available)
     * - Circuit breaker states
     */
    void getSystemHealth(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // DOG MANAGEMENT ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all dogs with pagination and filtering
     *
     * GET /api/admin/dogs
     *
     * Query params:
     * - page: Page number (default 1)
     * - per_page: Items per page (default 20, max 100)
     * - status: Filter by status (available, adopted, fostered, etc.)
     * - shelter_id: Filter by shelter
     * - urgency: Filter by urgency level
     * - search: Search by name or breed
     * - sort_by: Sort field (name, intake_date, urgency, wait_time)
     * - sort_dir: Sort direction (asc, desc)
     */
    void getDogs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get a specific dog by ID
     *
     * GET /api/admin/dogs/:id
     */
    void getDogById(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Update a dog's information
     *
     * PUT /api/admin/dogs/:id
     *
     * Request body: Dog object with fields to update
     */
    void updateDog(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Delete a dog from the system
     *
     * DELETE /api/admin/dogs/:id
     *
     * Soft deletes the dog record. Requires confirmation in request.
     */
    void deleteDog(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Batch update multiple dogs
     *
     * PUT /api/admin/dogs/batch
     *
     * Request body:
     * {
     *   "dog_ids": ["id1", "id2", ...],
     *   "updates": {"status": "adopted", ...}
     * }
     */
    void batchUpdateDogs(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // SHELTER MANAGEMENT ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all shelters with pagination
     *
     * GET /api/admin/shelters
     *
     * Query params:
     * - page, per_page: Pagination
     * - state: Filter by state code
     * - verified: Filter by verification status
     * - is_kill_shelter: Filter by kill shelter status
     * - search: Search by name
     */
    void getShelters(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get a specific shelter by ID
     *
     * GET /api/admin/shelters/:id
     */
    void getShelterById(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Update shelter information
     *
     * PUT /api/admin/shelters/:id
     */
    void updateShelter(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Verify or unverify a shelter
     *
     * POST /api/admin/shelters/:id/verify
     *
     * Request body:
     * {
     *   "verified": true/false,
     *   "notes": "Verification notes"
     * }
     */
    void verifyShelter(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Get detailed statistics for a shelter
     *
     * GET /api/admin/shelters/:id/stats
     */
    void getShelterStats(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    // ========================================================================
    // USER MANAGEMENT ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all users with pagination
     *
     * GET /api/admin/users
     *
     * Query params:
     * - page, per_page: Pagination
     * - role: Filter by role
     * - status: Filter by status (active, disabled)
     * - search: Search by name or email
     */
    void getUsers(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get a specific user by ID
     *
     * GET /api/admin/users/:id
     */
    void getUserById(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Update user information
     *
     * PUT /api/admin/users/:id
     */
    void updateUser(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Change a user's role
     *
     * PUT /api/admin/users/:id/role
     *
     * Request body:
     * {
     *   "role": "foster" | "shelter_admin" | "admin" | "user"
     * }
     */
    void changeUserRole(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Disable a user account
     *
     * POST /api/admin/users/:id/disable
     */
    void disableUser(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Enable a user account
     *
     * POST /api/admin/users/:id/enable
     */
    void enableUser(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    // ========================================================================
    // SYSTEM CONFIGURATION ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all system configuration
     *
     * GET /api/admin/config
     */
    void getConfig(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Update multiple configuration values
     *
     * PUT /api/admin/config
     *
     * Request body:
     * {
     *   "key1": "value1",
     *   "key2": "value2"
     * }
     */
    void updateConfig(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get a specific configuration value
     *
     * GET /api/admin/config/:key
     */
    void getConfigByKey(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& key);

    /**
     * @brief Set a specific configuration value
     *
     * PUT /api/admin/config/:key
     *
     * Request body:
     * {
     *   "value": "new_value"
     * }
     */
    void setConfigValue(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& key);

    // ========================================================================
    // AUDIT LOG ENDPOINTS
    // ========================================================================

    /**
     * @brief Get audit log entries
     *
     * GET /api/admin/audit-log
     *
     * Query params:
     * - page, per_page: Pagination
     * - action: Filter by action type
     * - entity_type: Filter by entity type (dog, shelter, user, config)
     * - user_id: Filter by user who performed action
     * - start_date, end_date: Date range filter
     */
    void getAuditLog(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get audit log entries for a specific user
     *
     * GET /api/admin/audit-log/user/:user_id
     */
    void getAuditLogByUser(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& user_id);

    /**
     * @brief Get audit log entries for a specific entity
     *
     * GET /api/admin/audit-log/entity/:entity_type/:entity_id
     */
    void getAuditLogByEntity(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& entity_type,
        const std::string& entity_id);

    // ========================================================================
    // CONTENT MANAGEMENT ENDPOINTS
    // ========================================================================

    /**
     * @brief Get content moderation queue
     *
     * GET /api/admin/content/queue
     */
    void getContentQueue(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Approve content item
     *
     * POST /api/admin/content/:id/approve
     */
    void approveContent(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    /**
     * @brief Reject content item
     *
     * POST /api/admin/content/:id/reject
     *
     * Request body:
     * {
     *   "reason": "Rejection reason"
     * }
     */
    void rejectContent(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& id);

    // ========================================================================
    // ANALYTICS ENDPOINTS
    // ========================================================================

    /**
     * @brief Get comprehensive analytics data
     *
     * GET /api/admin/analytics
     *
     * Query params:
     * - period: day, week, month, year
     * - start_date, end_date: Custom date range
     */
    void getAnalytics(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get traffic statistics
     *
     * GET /api/admin/analytics/traffic
     */
    void getTrafficStats(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get adoption funnel metrics
     *
     * GET /api/admin/analytics/adoption-funnel
     */
    void getAdoptionFunnel(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get social media performance metrics
     *
     * GET /api/admin/analytics/social
     */
    void getSocialPerformance(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Get impact metrics (lives saved, etc.)
     *
     * GET /api/admin/analytics/impact
     */
    void getImpactMetrics(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ========================================================================
    // WORKER MANAGEMENT ENDPOINTS
    // ========================================================================

    /**
     * @brief Get status of all background workers
     *
     * GET /api/admin/workers
     */
    void getWorkerStatus(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Restart a specific worker
     *
     * POST /api/admin/workers/:name/restart
     */
    void restartWorker(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& name);

    // ========================================================================
    // FEATURE FLAGS ENDPOINTS
    // ========================================================================

    /**
     * @brief Get all feature flags
     *
     * GET /api/admin/features
     */
    void getFeatureFlags(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Set a feature flag
     *
     * PUT /api/admin/features/:key
     *
     * Request body:
     * {
     *   "enabled": true/false,
     *   "rollout_percentage": 100
     * }
     */
    void setFeatureFlag(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& key);

private:
    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Parse pagination parameters from request
     *
     * @param req HTTP request
     * @param page Output page number (1-indexed)
     * @param per_page Output items per page
     */
    static void parsePagination(const drogon::HttpRequestPtr& req,
                                int& page, int& per_page);

    /**
     * @brief Parse sort parameters from request
     *
     * @param req HTTP request
     * @param sort_by Output sort field
     * @param sort_dir Output sort direction
     * @param default_sort Default sort field if not specified
     */
    static void parseSortParams(const drogon::HttpRequestPtr& req,
                                std::string& sort_by, std::string& sort_dir,
                                const std::string& default_sort = "created_at");

    /**
     * @brief Get string query parameter with default
     */
    static std::string getStringParam(const drogon::HttpRequestPtr& req,
                                      const std::string& name,
                                      const std::string& default_value = "");

    /**
     * @brief Get integer query parameter with default
     */
    static int getIntParam(const drogon::HttpRequestPtr& req,
                          const std::string& name,
                          int default_value = 0);

    /**
     * @brief Get boolean query parameter with default
     */
    static bool getBoolParam(const drogon::HttpRequestPtr& req,
                            const std::string& name,
                            bool default_value = false);
};

} // namespace wtl::core::controllers
