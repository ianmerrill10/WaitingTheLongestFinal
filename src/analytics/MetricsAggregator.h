/**
 * @file MetricsAggregator.h
 * @brief Metrics aggregation service for analytics reporting
 *
 * PURPOSE:
 * Aggregates raw analytics events into daily, weekly, and monthly metrics
 * for efficient reporting and dashboard display. Calculates views, favorites,
 * shares, applications, and adoptions grouped by dog, shelter, state, and
 * time period.
 *
 * USAGE:
 * auto& aggregator = MetricsAggregator::getInstance();
 * aggregator.aggregateDaily(yesterday);
 * auto metrics = aggregator.getDailyMetrics("2024-01-28");
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - SelfHealing (retry logic)
 * - DashboardStats (data structures)
 *
 * MODIFICATION GUIDE:
 * - Add new metric types to AggregatedMetrics struct
 * - Update aggregation SQL queries for new metrics
 * - Add new aggregation methods as needed
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <json/json.h>

#include "analytics/DashboardStats.h"
#include "analytics/EventType.h"

namespace wtl::analytics {

/**
 * @struct AggregatedMetrics
 * @brief Aggregated metrics for a specific period and dimension
 */
struct AggregatedMetrics {
    // Time period
    std::string period;               // Date string (YYYY-MM-DD) or week/month
    std::string period_type;          // daily, weekly, monthly

    // Dimension (optional - for breakdown)
    std::string dimension_type;       // dog, shelter, state, or empty for total
    std::string dimension_id;

    // View metrics
    int page_views = 0;
    int unique_visitors = 0;
    int dog_views = 0;
    int shelter_views = 0;
    int search_count = 0;

    // Engagement metrics
    int favorites_added = 0;
    int favorites_removed = 0;
    int shares = 0;
    int contact_clicks = 0;
    int direction_clicks = 0;

    // Application metrics
    int foster_applications = 0;
    int foster_approved = 0;
    int foster_started = 0;
    int foster_ended = 0;
    int adoption_applications = 0;

    // Outcome metrics
    int adoptions = 0;
    int rescues = 0;
    int transports = 0;

    // User metrics
    int new_users = 0;
    int active_users = 0;
    int returning_users = 0;

    // Social metrics
    int social_views = 0;
    int social_shares = 0;
    int social_engagement = 0;
    int social_referrals = 0;

    // Notification metrics
    int notifications_sent = 0;
    int notifications_opened = 0;
    int emails_sent = 0;
    int emails_opened = 0;

    // Timestamps
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from database row
     */
    static AggregatedMetrics fromDbRow(const pqxx::row& row);

    /**
     * @brief Merge another metrics object into this one (add values)
     */
    void merge(const AggregatedMetrics& other);
};

/**
 * @struct AggregationResult
 * @brief Result of an aggregation operation
 */
struct AggregationResult {
    bool success = false;
    int events_processed = 0;
    int metrics_created = 0;
    int metrics_updated = 0;
    std::string error_message;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;

    Json::Value toJson() const;
};

/**
 * @class MetricsAggregator
 * @brief Singleton service for aggregating analytics metrics
 *
 * Processes raw analytics events and aggregates them into daily, weekly,
 * and monthly metrics. Supports breakdowns by dog, shelter, state, and
 * various other dimensions.
 */
class MetricsAggregator {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the MetricsAggregator instance
     */
    static MetricsAggregator& getInstance();

    // Prevent copying
    MetricsAggregator(const MetricsAggregator&) = delete;
    MetricsAggregator& operator=(const MetricsAggregator&) = delete;

    // =========================================================================
    // AGGREGATION METHODS
    // =========================================================================

    /**
     * @brief Aggregate metrics for a specific date
     * @param date The date to aggregate (YYYY-MM-DD)
     * @return Result of the aggregation operation
     */
    AggregationResult aggregateDaily(const std::string& date);

    /**
     * @brief Aggregate metrics for yesterday
     * @return Result of the aggregation operation
     */
    AggregationResult aggregateYesterday();

    /**
     * @brief Aggregate metrics for a specific week
     * @param year The year
     * @param week The ISO week number (1-53)
     * @return Result of the aggregation operation
     */
    AggregationResult aggregateWeekly(int year, int week);

    /**
     * @brief Aggregate metrics for the previous week
     * @return Result of the aggregation operation
     */
    AggregationResult aggregateLastWeek();

    /**
     * @brief Aggregate metrics for a specific month
     * @param year The year
     * @param month The month (1-12)
     * @return Result of the aggregation operation
     */
    AggregationResult aggregateMonthly(int year, int month);

    /**
     * @brief Aggregate metrics for the previous month
     * @return Result of the aggregation operation
     */
    AggregationResult aggregateLastMonth();

    /**
     * @brief Aggregate all pending events (catch-up aggregation)
     * @param max_days Maximum days to process (default: 30)
     * @return Result of the aggregation operation
     */
    AggregationResult aggregatePending(int max_days = 30);

    // =========================================================================
    // QUERY METHODS
    // =========================================================================

    /**
     * @brief Get daily metrics for a date
     * @param date The date (YYYY-MM-DD)
     * @return Aggregated metrics or nullopt if not found
     */
    std::optional<AggregatedMetrics> getDailyMetrics(const std::string& date);

    /**
     * @brief Get daily metrics for a date range
     * @param start_date Start date (YYYY-MM-DD)
     * @param end_date End date (YYYY-MM-DD)
     * @return Vector of daily metrics
     */
    std::vector<AggregatedMetrics> getDailyMetricsRange(
        const std::string& start_date,
        const std::string& end_date);

    /**
     * @brief Get weekly metrics for a specific week
     * @param year The year
     * @param week The ISO week number
     * @return Aggregated metrics or nullopt if not found
     */
    std::optional<AggregatedMetrics> getWeeklyMetrics(int year, int week);

    /**
     * @brief Get monthly metrics for a specific month
     * @param year The year
     * @param month The month
     * @return Aggregated metrics or nullopt if not found
     */
    std::optional<AggregatedMetrics> getMonthlyMetrics(int year, int month);

    /**
     * @brief Get metrics by dog
     * @param dog_id The dog ID
     * @param range Date range to query
     * @return Aggregated metrics for the dog
     */
    AggregatedMetrics getMetricsByDog(const std::string& dog_id, const DateRange& range);

    /**
     * @brief Get metrics by shelter
     * @param shelter_id The shelter ID
     * @param range Date range to query
     * @return Aggregated metrics for the shelter
     */
    AggregatedMetrics getMetricsByShelter(const std::string& shelter_id, const DateRange& range);

    /**
     * @brief Get metrics by state
     * @param state_code The state code
     * @param range Date range to query
     * @return Aggregated metrics for the state
     */
    AggregatedMetrics getMetricsByState(const std::string& state_code, const DateRange& range);

    /**
     * @brief Get top dogs by a specific metric
     * @param metric_name The metric to sort by (views, shares, favorites, etc.)
     * @param range Date range to query
     * @param limit Maximum number of results
     * @return Vector of metrics sorted by the specified metric
     */
    std::vector<AggregatedMetrics> getTopDogsByMetric(
        const std::string& metric_name,
        const DateRange& range,
        int limit = 10);

    /**
     * @brief Get top shelters by a specific metric
     * @param metric_name The metric to sort by
     * @param range Date range to query
     * @param limit Maximum number of results
     * @return Vector of metrics sorted by the specified metric
     */
    std::vector<AggregatedMetrics> getTopSheltersByMetric(
        const std::string& metric_name,
        const DateRange& range,
        int limit = 10);

    /**
     * @brief Get trend data for a metric
     * @param metric_name The metric to get trends for
     * @param range Date range to query
     * @return Vector of trend data points
     */
    std::vector<TrendData> getTrendData(
        const std::string& metric_name,
        const DateRange& range);

    // =========================================================================
    // COMPARISON METHODS
    // =========================================================================

    /**
     * @brief Calculate week-over-week change for a metric
     * @param metric_name The metric to compare
     * @param end_date End date of current week
     * @return Percentage change (positive = growth)
     */
    double getWeekOverWeekChange(const std::string& metric_name, const std::string& end_date);

    /**
     * @brief Calculate month-over-month change for a metric
     * @param metric_name The metric to compare
     * @param year Year
     * @param month Month
     * @return Percentage change (positive = growth)
     */
    double getMonthOverMonthChange(const std::string& metric_name, int year, int month);

    // =========================================================================
    // MAINTENANCE METHODS
    // =========================================================================

    /**
     * @brief Clean up old aggregated metrics
     * @param retention_days Number of days to keep daily metrics
     * @return Number of records deleted
     */
    int cleanupOldMetrics(int retention_days = 90);

    /**
     * @brief Recalculate metrics for a date range (force refresh)
     * @param start_date Start date
     * @param end_date End date
     * @return Result of the recalculation
     */
    AggregationResult recalculateMetrics(const std::string& start_date, const std::string& end_date);

    /**
     * @brief Mark events as processed
     * @param event_ids Vector of event IDs to mark
     * @return Number of events marked
     */
    int markEventsProcessed(const std::vector<std::string>& event_ids);

private:
    /**
     * @brief Private constructor for singleton
     */
    MetricsAggregator();

    /**
     * @brief Aggregate events for a time range into metrics
     * @param start Start time
     * @param end End time
     * @param period_type Type of period (daily, weekly, monthly)
     * @param period_label Label for the period
     * @return Result of aggregation
     */
    AggregationResult aggregateEventsForPeriod(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        const std::string& period_type,
        const std::string& period_label);

    /**
     * @brief Count events of a specific type in the time range
     * @param start Start time
     * @param end End time
     * @param event_type Event type to count
     * @return Count of events
     */
    int countEvents(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        AnalyticsEventType event_type);

    /**
     * @brief Count events by dimension
     * @param start Start time
     * @param end End time
     * @param event_type Event type to count
     * @param dimension_type dog, shelter, or state
     * @return Map of dimension_id to count
     */
    std::map<std::string, int> countEventsByDimension(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        AnalyticsEventType event_type,
        const std::string& dimension_type);

    /**
     * @brief Save aggregated metrics to database
     * @param metrics The metrics to save
     * @return True if saved successfully
     */
    bool saveMetrics(const AggregatedMetrics& metrics);

    /**
     * @brief Upsert metrics (insert or update)
     * @param metrics The metrics to upsert
     * @return True if successful
     */
    bool upsertMetrics(const AggregatedMetrics& metrics);

    /** Mutex for thread safety */
    std::mutex mutex_;

    /** Last aggregation time */
    std::chrono::system_clock::time_point last_aggregation_;
};

} // namespace wtl::analytics
