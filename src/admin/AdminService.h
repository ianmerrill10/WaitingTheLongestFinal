/**
 * @file AdminService.h
 * @brief Admin dashboard service for aggregated statistics and management
 *
 * PURPOSE:
 * Provides centralized access to dashboard statistics, system health,
 * analytics, and management operations for the admin interface.
 *
 * USAGE:
 * auto& service = AdminService::getInstance();
 * auto stats = service.getDashboardStats();
 * auto health = service.getSystemHealth();
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - DogService, ShelterService, UserService (entity data)
 * - ErrorCapture (error logging)
 * - SelfHealing (retry logic)
 * - HealthCheck (system health)
 *
 * MODIFICATION GUIDE:
 * - Add new statistics methods as needed
 * - All database operations should use self-healing
 * - Aggregate complex queries for performance
 *
 * @author Agent 20 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::admin {

/**
 * @class AdminService
 * @brief Singleton service for admin dashboard operations
 *
 * Thread-safe singleton providing:
 * - Dashboard statistics aggregation
 * - System health monitoring
 * - User activity tracking
 * - Content and social statistics
 * - Analytics and metrics
 */
class AdminService {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the AdminService instance
     */
    static AdminService& getInstance();

    // Prevent copying and moving
    AdminService(const AdminService&) = delete;
    AdminService& operator=(const AdminService&) = delete;
    AdminService(AdminService&&) = delete;
    AdminService& operator=(AdminService&&) = delete;

    // ========================================================================
    // DASHBOARD STATISTICS
    // ========================================================================

    /**
     * @brief Get comprehensive dashboard statistics
     *
     * Returns aggregated stats including:
     * - Total counts (dogs, shelters, users, fosters)
     * - Active user metrics
     * - Urgency breakdown
     * - Recent activity
     * - System health summary
     *
     * @return JSON object with all dashboard statistics
     */
    Json::Value getDashboardStats();

    /**
     * @brief Get system health status
     *
     * Returns detailed health information:
     * - Database connectivity and pool stats
     * - Worker status (urgency, wait time, etc.)
     * - Error rates (last hour, day)
     * - Circuit breaker states
     * - Memory/disk usage if available
     *
     * @return JSON object with system health data
     */
    Json::Value getSystemHealth();

    /**
     * @brief Get active users statistics
     *
     * @param timeframe Timeframe string ("24h", "7d", "30d")
     * @return JSON object with active user metrics
     */
    Json::Value getActiveUsers(const std::string& timeframe = "24h");

    /**
     * @brief Get content statistics
     *
     * Returns stats about:
     * - TikTok posts
     * - Blog posts
     * - Social posts
     * - Pending moderation queue
     *
     * @return JSON object with content statistics
     */
    Json::Value getContentStats();

    /**
     * @brief Get social media statistics
     *
     * Returns metrics about:
     * - Post engagement
     * - Follower growth
     * - Top performing content
     * - Platform breakdown
     *
     * @return JSON object with social statistics
     */
    Json::Value getSocialStats();

    /**
     * @brief Get urgency overview
     *
     * Returns breakdown of dogs by urgency level:
     * - Critical count (<24h)
     * - High count (1-3 days)
     * - Medium count (4-7 days)
     * - Normal count
     * - Trends over time
     *
     * @return JSON object with urgency overview
     */
    Json::Value getUrgencyOverview();

    // ========================================================================
    // USER MANAGEMENT
    // ========================================================================

    /**
     * @brief Get users with filtering and pagination
     *
     * @param page Page number (1-indexed)
     * @param per_page Items per page
     * @param role Filter by role (empty for all)
     * @param search Search query
     * @return JSON object with users array and total count
     */
    Json::Value getUsers(int page, int per_page,
                        const std::string& role = "",
                        const std::string& search = "");

    // ========================================================================
    // ANALYTICS
    // ========================================================================

    /**
     * @brief Get analytics data for specified period
     *
     * @param period Period string ("day", "week", "month", "year")
     * @return JSON object with analytics data
     */
    Json::Value getAnalytics(const std::string& period);

    /**
     * @brief Get traffic statistics
     *
     * Returns:
     * - Page views
     * - Unique visitors
     * - API calls
     * - Popular pages
     *
     * @return JSON object with traffic stats
     */
    Json::Value getTrafficStats();

    /**
     * @brief Get adoption funnel metrics
     *
     * Returns conversion rates through:
     * - View dog profile
     * - Add to favorites
     * - Submit application
     * - Complete adoption
     *
     * @return JSON object with funnel data
     */
    Json::Value getAdoptionFunnel();

    /**
     * @brief Get impact metrics
     *
     * Returns:
     * - Lives saved
     * - Dogs adopted
     * - Dogs fostered
     * - Average wait time reduction
     * - Success stories
     *
     * @return JSON object with impact metrics
     */
    Json::Value getImpactMetrics();

    // ========================================================================
    // CONTENT MANAGEMENT
    // ========================================================================

    /**
     * @brief Get content moderation queue
     *
     * @return JSON array of pending content items
     */
    Json::Value getContentQueue();

    /**
     * @brief Approve a content item
     *
     * @param content_id Content item ID
     * @return true if approved successfully
     */
    bool approveContent(const std::string& content_id);

    /**
     * @brief Reject a content item
     *
     * @param content_id Content item ID
     * @param reason Rejection reason
     * @return true if rejected successfully
     */
    bool rejectContent(const std::string& content_id, const std::string& reason);

    // ========================================================================
    // WORKER MANAGEMENT
    // ========================================================================

    /**
     * @brief Get status of all background workers
     *
     * @return JSON array with worker status information
     */
    Json::Value getWorkerStatus();

    /**
     * @brief Restart a specific worker
     *
     * @param worker_name Name of the worker to restart
     * @return true if restart initiated successfully
     */
    bool restartWorker(const std::string& worker_name);

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    AdminService();
    ~AdminService();

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Execute database query with self-healing
     */
    template<typename T>
    T executeWithHealing(const std::string& operation_name,
                        std::function<T()> operation);

    /**
     * @brief Get count from database table
     */
    int getTableCount(const std::string& table_name,
                     const std::string& where_clause = "");

    /**
     * @brief Calculate percentage change between two values
     */
    static double calculatePercentageChange(int current, int previous);

    /**
     * @brief Get timestamp for timeframe
     */
    static std::string getTimeframeTimestamp(const std::string& timeframe);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    std::mutex mutex_;  ///< Mutex for thread-safe operations
};

} // namespace wtl::admin
