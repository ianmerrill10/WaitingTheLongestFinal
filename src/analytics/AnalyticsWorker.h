/**
 * @file AnalyticsWorker.h
 * @brief Background worker for analytics processing
 *
 * PURPOSE:
 * Background worker that runs periodically to:
 * - Aggregate daily metrics from raw events
 * - Clean up old events past retention period
 * - Generate scheduled reports
 * - Sync social media analytics from external APIs
 *
 * USAGE:
 * auto& worker = AnalyticsWorker::getInstance();
 * worker.start();
 * // Worker runs in background
 * worker.stop();
 *
 * DEPENDENCIES:
 * - MetricsAggregator (aggregation)
 * - ReportGenerator (reports)
 * - SocialAnalytics (social sync)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - Add new scheduled tasks to the run loop
 * - Adjust intervals as needed for scale
 * - Keep tasks lightweight to avoid blocking
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <json/json.h>

namespace wtl::analytics {

/**
 * @struct WorkerStatus
 * @brief Current status of the analytics worker
 */
struct WorkerStatus {
    bool is_running = false;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point last_run_at;
    int total_runs = 0;
    int successful_runs = 0;
    int failed_runs = 0;
    std::string last_error;

    // Last task results
    int last_events_aggregated = 0;
    int last_events_cleaned = 0;
    int last_reports_generated = 0;
    int last_social_synced = 0;

    Json::Value toJson() const;
};

/**
 * @struct WorkerConfig
 * @brief Configuration for the analytics worker
 */
struct WorkerConfig {
    // Intervals (in seconds)
    int aggregation_interval = 3600;      // 1 hour
    int cleanup_interval = 86400;         // 24 hours
    int report_interval = 86400;          // 24 hours
    int social_sync_interval = 3600;      // 1 hour
    int health_check_interval = 300;      // 5 minutes

    // Retention
    int event_retention_days = 90;        // Keep raw events for 90 days
    int daily_metrics_retention_days = 365; // Keep daily metrics for 1 year
    int weekly_metrics_retention_days = 730; // Keep weekly metrics for 2 years

    // Batch sizes
    int aggregation_batch_days = 7;       // Aggregate up to 7 days at once
    int cleanup_batch_size = 10000;       // Delete 10k events per batch

    // Features
    bool enable_aggregation = true;
    bool enable_cleanup = true;
    bool enable_reports = true;
    bool enable_social_sync = false;      // Disabled until API credentials set

    Json::Value toJson() const;
    static WorkerConfig fromJson(const Json::Value& json);
};

/**
 * @class AnalyticsWorker
 * @brief Singleton background worker for analytics processing
 *
 * Runs scheduled tasks for analytics system maintenance:
 * - Aggregates raw events into daily/weekly/monthly metrics
 * - Cleans up old events to manage database size
 * - Generates scheduled reports (daily summary, weekly impact)
 * - Syncs social media analytics from external platforms
 */
class AnalyticsWorker {
public:
    /**
     * @brief Get singleton instance
     */
    static AnalyticsWorker& getInstance();

    // Prevent copying
    AnalyticsWorker(const AnalyticsWorker&) = delete;
    AnalyticsWorker& operator=(const AnalyticsWorker&) = delete;

    // =========================================================================
    // LIFECYCLE
    // =========================================================================

    /**
     * @brief Start the background worker
     */
    void start();

    /**
     * @brief Stop the background worker gracefully
     */
    void stop();

    /**
     * @brief Check if worker is running
     * @return True if running
     */
    bool isRunning() const;

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Get current configuration
     * @return Worker configuration
     */
    WorkerConfig getConfig() const;

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void setConfig(const WorkerConfig& config);

    // =========================================================================
    // STATUS
    // =========================================================================

    /**
     * @brief Get current worker status
     * @return Worker status
     */
    WorkerStatus getStatus() const;

    /**
     * @brief Get JSON representation of status
     * @return Status as JSON
     */
    Json::Value getStatusJson() const;

    // =========================================================================
    // MANUAL TRIGGERS
    // =========================================================================

    /**
     * @brief Manually trigger aggregation
     * @return Number of events aggregated
     */
    int triggerAggregation();

    /**
     * @brief Manually trigger cleanup
     * @return Number of events cleaned up
     */
    int triggerCleanup();

    /**
     * @brief Manually trigger report generation
     * @param report_type Type of report (daily, weekly, monthly)
     * @return True if report generated successfully
     */
    bool triggerReport(const std::string& report_type);

    /**
     * @brief Manually trigger social sync
     * @return Number of content pieces synced
     */
    int triggerSocialSync();

    /**
     * @brief Run all tasks immediately
     */
    void runAllTasks();

private:
    /**
     * @brief Private constructor for singleton
     */
    AnalyticsWorker();

    /**
     * @brief Destructor
     */
    ~AnalyticsWorker();

    /**
     * @brief Main worker loop
     */
    void workerLoop();

    /**
     * @brief Run aggregation task
     */
    void runAggregation();

    /**
     * @brief Run cleanup task
     */
    void runCleanup();

    /**
     * @brief Run report generation task
     */
    void runReports();

    /**
     * @brief Run social sync task
     */
    void runSocialSync();

    /**
     * @brief Check if a task should run based on last run time
     * @param last_run Last run time
     * @param interval_seconds Interval in seconds
     * @return True if task should run
     */
    bool shouldRunTask(const std::chrono::system_clock::time_point& last_run,
                       int interval_seconds) const;

    /** Worker thread */
    std::unique_ptr<std::thread> worker_thread_;

    /** Running flag */
    std::atomic<bool> running_{false};

    /** Mutex for thread safety */
    mutable std::mutex mutex_;

    /** Condition variable for wake-up */
    std::condition_variable cv_;

    /** Configuration */
    WorkerConfig config_;

    /** Status */
    WorkerStatus status_;

    /** Last run times for each task */
    std::chrono::system_clock::time_point last_aggregation_;
    std::chrono::system_clock::time_point last_cleanup_;
    std::chrono::system_clock::time_point last_report_;
    std::chrono::system_clock::time_point last_social_sync_;
};

} // namespace wtl::analytics
