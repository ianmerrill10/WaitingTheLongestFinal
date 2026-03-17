/**
 * @file AnalyticsWorker.cc
 * @brief Implementation of AnalyticsWorker
 * @see AnalyticsWorker.h for documentation
 */

#include "analytics/AnalyticsWorker.h"

#include <iomanip>
#include <sstream>

#include "analytics/AnalyticsEvent.h"
#include "analytics/MetricsAggregator.h"
#include "analytics/ReportGenerator.h"
#include "analytics/SocialAnalytics.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::analytics {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// WORKER STATUS
// ============================================================================

Json::Value WorkerStatus::toJson() const {
    Json::Value json;
    json["is_running"] = is_running;
    json["started_at"] = formatTimestampISO(started_at);
    json["last_run_at"] = formatTimestampISO(last_run_at);
    json["total_runs"] = total_runs;
    json["successful_runs"] = successful_runs;
    json["failed_runs"] = failed_runs;
    json["last_error"] = last_error;
    json["last_events_aggregated"] = last_events_aggregated;
    json["last_events_cleaned"] = last_events_cleaned;
    json["last_reports_generated"] = last_reports_generated;
    json["last_social_synced"] = last_social_synced;
    return json;
}

// ============================================================================
// WORKER CONFIG
// ============================================================================

Json::Value WorkerConfig::toJson() const {
    Json::Value json;
    json["aggregation_interval"] = aggregation_interval;
    json["cleanup_interval"] = cleanup_interval;
    json["report_interval"] = report_interval;
    json["social_sync_interval"] = social_sync_interval;
    json["health_check_interval"] = health_check_interval;
    json["event_retention_days"] = event_retention_days;
    json["daily_metrics_retention_days"] = daily_metrics_retention_days;
    json["weekly_metrics_retention_days"] = weekly_metrics_retention_days;
    json["aggregation_batch_days"] = aggregation_batch_days;
    json["cleanup_batch_size"] = cleanup_batch_size;
    json["enable_aggregation"] = enable_aggregation;
    json["enable_cleanup"] = enable_cleanup;
    json["enable_reports"] = enable_reports;
    json["enable_social_sync"] = enable_social_sync;
    return json;
}

WorkerConfig WorkerConfig::fromJson(const Json::Value& json) {
    WorkerConfig config;
    config.aggregation_interval = json.get("aggregation_interval", 3600).asInt();
    config.cleanup_interval = json.get("cleanup_interval", 86400).asInt();
    config.report_interval = json.get("report_interval", 86400).asInt();
    config.social_sync_interval = json.get("social_sync_interval", 3600).asInt();
    config.health_check_interval = json.get("health_check_interval", 300).asInt();
    config.event_retention_days = json.get("event_retention_days", 90).asInt();
    config.daily_metrics_retention_days = json.get("daily_metrics_retention_days", 365).asInt();
    config.weekly_metrics_retention_days = json.get("weekly_metrics_retention_days", 730).asInt();
    config.aggregation_batch_days = json.get("aggregation_batch_days", 7).asInt();
    config.cleanup_batch_size = json.get("cleanup_batch_size", 10000).asInt();
    config.enable_aggregation = json.get("enable_aggregation", true).asBool();
    config.enable_cleanup = json.get("enable_cleanup", true).asBool();
    config.enable_reports = json.get("enable_reports", true).asBool();
    config.enable_social_sync = json.get("enable_social_sync", false).asBool();
    return config;
}

// ============================================================================
// SINGLETON
// ============================================================================

AnalyticsWorker& AnalyticsWorker::getInstance() {
    static AnalyticsWorker instance;
    return instance;
}

AnalyticsWorker::AnalyticsWorker() {
    auto now = std::chrono::system_clock::now();
    // Set last run times to now minus interval so tasks run soon after start
    last_aggregation_ = now - std::chrono::seconds(config_.aggregation_interval - 60);
    last_cleanup_ = now - std::chrono::seconds(config_.cleanup_interval - 300);
    last_report_ = now - std::chrono::seconds(config_.report_interval - 300);
    last_social_sync_ = now - std::chrono::seconds(config_.social_sync_interval - 60);
}

AnalyticsWorker::~AnalyticsWorker() {
    stop();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void AnalyticsWorker::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
        return;
    }

    running_ = true;
    status_.is_running = true;
    status_.started_at = std::chrono::system_clock::now();

    worker_thread_ = std::make_unique<std::thread>(&AnalyticsWorker::workerLoop, this);
}

void AnalyticsWorker::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
        status_.is_running = false;
    }

    cv_.notify_all();

    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
}

bool AnalyticsWorker::isRunning() const {
    return running_;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

WorkerConfig AnalyticsWorker::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void AnalyticsWorker::setConfig(const WorkerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

// ============================================================================
// STATUS
// ============================================================================

WorkerStatus AnalyticsWorker::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

Json::Value AnalyticsWorker::getStatusJson() const {
    return getStatus().toJson();
}

// ============================================================================
// MANUAL TRIGGERS
// ============================================================================

int AnalyticsWorker::triggerAggregation() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& aggregator = MetricsAggregator::getInstance();
    auto result = aggregator.aggregateYesterday();

    status_.last_events_aggregated = result.events_processed;
    return result.events_processed;
}

int AnalyticsWorker::triggerCleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    int total_deleted = 0;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Delete old events
        auto cutoff = std::chrono::system_clock::now() -
            std::chrono::hours(24 * config_.event_retention_days);
        std::string cutoff_str = formatTimestampISO(cutoff);

        auto result = txn.exec_params(
            "DELETE FROM analytics_events WHERE timestamp < $1 AND is_processed = TRUE",
            cutoff_str
        );

        total_deleted = static_cast<int>(result.affected_rows());

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to cleanup old events: " + std::string(e.what()),
            {}
        );
    }

    // Also cleanup old metrics
    auto& aggregator = MetricsAggregator::getInstance();
    int metrics_deleted = aggregator.cleanupOldMetrics(config_.daily_metrics_retention_days);

    total_deleted += metrics_deleted;
    status_.last_events_cleaned = total_deleted;

    return total_deleted;
}

bool AnalyticsWorker::triggerReport(const std::string& report_type) {
    try {
        auto& generator = ReportGenerator::getInstance();
        DateRange range;

        if (report_type == "daily") {
            range = DateRange::yesterday();
        } else if (report_type == "weekly") {
            range = DateRange::lastWeek();
        } else if (report_type == "monthly") {
            range = DateRange::lastMonth();
        } else {
            return false;
        }

        std::string report = generator.generateReport(report_type, range, "json");
        status_.last_reports_generated++;
        return !report.empty();

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::INTERNAL,
            "Failed to generate report: " + std::string(e.what()),
            {{"report_type", report_type}}
        );
        return false;
    }
}

int AnalyticsWorker::triggerSocialSync() {
    if (!config_.enable_social_sync) {
        return 0;
    }

    auto& social = SocialAnalytics::getInstance();
    int synced = social.syncAllPlatforms();

    status_.last_social_synced = synced;
    return synced;
}

void AnalyticsWorker::runAllTasks() {
    runAggregation();
    runCleanup();
    runReports();
    runSocialSync();
}

// ============================================================================
// WORKER LOOP
// ============================================================================

void AnalyticsWorker::workerLoop() {
    while (running_) {
        auto now = std::chrono::system_clock::now();

        try {
            // Check and run aggregation
            if (config_.enable_aggregation &&
                shouldRunTask(last_aggregation_, config_.aggregation_interval)) {
                runAggregation();
                last_aggregation_ = now;
            }

            // Check and run cleanup
            if (config_.enable_cleanup &&
                shouldRunTask(last_cleanup_, config_.cleanup_interval)) {
                runCleanup();
                last_cleanup_ = now;
            }

            // Check and run reports
            if (config_.enable_reports &&
                shouldRunTask(last_report_, config_.report_interval)) {
                runReports();
                last_report_ = now;
            }

            // Check and run social sync
            if (config_.enable_social_sync &&
                shouldRunTask(last_social_sync_, config_.social_sync_interval)) {
                runSocialSync();
                last_social_sync_ = now;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                status_.last_run_at = now;
                status_.total_runs++;
                status_.successful_runs++;
            }

        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            status_.failed_runs++;
            status_.last_error = e.what();
            WTL_CAPTURE_ERROR(
                ErrorCategory::INTERNAL,
                "Analytics worker error: " + std::string(e.what()),
                {}
            );
        }

        // Sleep until next check (minimum of all intervals or 60 seconds)
        int sleep_seconds = std::min({
            config_.aggregation_interval,
            config_.cleanup_interval,
            config_.health_check_interval,
            60
        });

        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(sleep_seconds), [this] {
            return !running_;
        });
    }
}

void AnalyticsWorker::runAggregation() {
    auto& aggregator = MetricsAggregator::getInstance();

    // Aggregate yesterday's data
    auto result = aggregator.aggregateYesterday();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.last_events_aggregated = result.events_processed;
    }

    // Also catch up on any missed days (up to batch limit)
    auto catch_up_result = aggregator.aggregatePending(config_.aggregation_batch_days);

    if (!result.success || !catch_up_result.success) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::INTERNAL,
            "Aggregation had errors",
            {{"yesterday_error", result.error_message},
             {"catchup_error", catch_up_result.error_message}}
        );
    }
}

void AnalyticsWorker::runCleanup() {
    triggerCleanup();
}

void AnalyticsWorker::runReports() {
    // Generate daily report
    triggerReport("daily");

    // Check if it's Monday - generate weekly report
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;

#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    if (tm_now.tm_wday == 1) {  // Monday
        triggerReport("weekly");
    }

    // Check if it's first of month - generate monthly report
    if (tm_now.tm_mday == 1) {
        triggerReport("monthly");
    }
}

void AnalyticsWorker::runSocialSync() {
    triggerSocialSync();
}

bool AnalyticsWorker::shouldRunTask(const std::chrono::system_clock::time_point& last_run,
                                     int interval_seconds) const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_run);
    return elapsed.count() >= interval_seconds;
}

} // namespace wtl::analytics
