/**
 * @file CleanupWorker.cc
 * @brief Implementation of CleanupWorker
 */

#include "CleanupWorker.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/utils/Config.h"

namespace wtl::workers {

using namespace ::wtl::core::debug;
using namespace ::wtl::core::db;

CleanupWorker::CleanupWorker()
    : BaseWorker("cleanup_worker",
                 "Cleans up expired sessions, old logs, and orphaned data",
                 std::chrono::seconds(86400),  // Daily
                 WorkerPriority::LOW)
{
    try {
        auto& cfg = wtl::core::utils::Config::getInstance();
        cleanup_config_.session_retention_days = cfg.getInt("cleanup.session_retention_days", 7);
        cleanup_config_.error_log_retention_days = cfg.getInt("cleanup.error_log_retention_days", 30);
        cleanup_config_.analytics_retention_days = cfg.getInt("cleanup.analytics_retention_days", 90);
        cleanup_config_.media_retention_days = cfg.getInt("cleanup.media_retention_days", 365);
        cleanup_config_.vacuum_database = cfg.getBool("cleanup.vacuum_database", false);
    } catch (...) {
        // Use defaults
    }
}

CleanupWorker::CleanupWorker(const CleanupConfig& config)
    : BaseWorker("cleanup_worker",
                 "Cleans up expired sessions, old logs, and orphaned data",
                 std::chrono::seconds(86400),
                 WorkerPriority::LOW)
    , cleanup_config_(config)
{
}

CleanupResult CleanupWorker::getLastResult() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return last_result_;
}

CleanupResult CleanupWorker::cleanupNow() {
    CleanupResult result;

    logInfo("Starting cleanup operations...");

    try {
        cleanupExpiredSessions(result);
        archiveOldErrorLogs(result);
        archiveOldAnalytics(result);
        cleanupOrphanedMedia(result);
        cleanupOrphanedRecords(result);

        if (cleanup_config_.vacuum_database) {
            vacuumDatabase();
        }
    } catch (const std::exception& e) {
        logError("Cleanup failed: " + std::string(e.what()), {});
    }

    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        last_result_ = result;
    }

    logInfo("Cleanup complete: " + std::to_string(result.sessions_deleted) +
            " sessions, " + std::to_string(result.error_logs_archived) + " logs archived");

    return result;
}

void CleanupWorker::setCleanupConfig(const CleanupConfig& config) {
    cleanup_config_ = config;
}

WorkerResult CleanupWorker::doExecute() {
    CleanupResult result = cleanupNow();

    int total = result.sessions_deleted + result.error_logs_archived +
                result.analytics_archived + result.orphaned_records_cleaned;

    WorkerResult wr = WorkerResult::Success(
        "Cleaned up " + std::to_string(total) + " items", total);
    wr.metadata = result.toJson();

    return wr;
}

void CleanupWorker::cleanupExpiredSessions(CleanupResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto deleted = txn.exec_params(
            "DELETE FROM sessions WHERE expires_at < NOW() - INTERVAL '" +
            std::to_string(cleanup_config_.session_retention_days) + " days' "
            "RETURNING id");

        result.sessions_deleted = deleted.size();

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logWarning("Session cleanup failed: " + std::string(e.what()));
    }
}

void CleanupWorker::archiveOldErrorLogs(CleanupResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Move old logs to archive table
        auto archived = txn.exec(
            "WITH moved AS ("
            "  DELETE FROM error_logs "
            "  WHERE created_at < NOW() - INTERVAL '" +
            std::to_string(cleanup_config_.error_log_retention_days) + " days' "
            "  RETURNING *"
            ") "
            "INSERT INTO error_logs_archive SELECT * FROM moved "
            "RETURNING id");

        result.error_logs_archived = archived.size();

        // Clean very old archives (3x retention period)
        txn.exec(
            "DELETE FROM error_logs_archive "
            "WHERE created_at < NOW() - INTERVAL '" +
            std::to_string(cleanup_config_.error_log_retention_days * 3) + " days'");

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logWarning("Error log archiving failed: " + std::string(e.what()));
    }
}

void CleanupWorker::archiveOldAnalytics(CleanupResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Aggregate old daily analytics to monthly
        auto archived = txn.exec(
            "WITH aggregated AS ("
            "  DELETE FROM analytics_daily "
            "  WHERE date < CURRENT_DATE - INTERVAL '" +
            std::to_string(cleanup_config_.analytics_retention_days) + " days' "
            "  RETURNING *"
            ") "
            "INSERT INTO analytics_monthly (year, month, page_views, unique_visitors, dog_views) "
            "SELECT "
            "  EXTRACT(YEAR FROM date)::int, "
            "  EXTRACT(MONTH FROM date)::int, "
            "  SUM(page_views), "
            "  SUM(unique_visitors), "
            "  SUM(dog_views) "
            "FROM aggregated "
            "GROUP BY EXTRACT(YEAR FROM date), EXTRACT(MONTH FROM date) "
            "ON CONFLICT (year, month) DO UPDATE SET "
            "  page_views = analytics_monthly.page_views + EXCLUDED.page_views, "
            "  unique_visitors = analytics_monthly.unique_visitors + EXCLUDED.unique_visitors, "
            "  dog_views = analytics_monthly.dog_views + EXCLUDED.dog_views "
            "RETURNING id");

        result.analytics_archived = archived.size();

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logWarning("Analytics archiving failed: " + std::string(e.what()));
    }
}

void CleanupWorker::cleanupOrphanedMedia(CleanupResult& result) {
    // This would typically involve filesystem operations
    // For now, we just clean up database references to missing files
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find media records for deleted dogs
        auto deleted = txn.exec(
            "DELETE FROM dog_media "
            "WHERE dog_id NOT IN (SELECT id FROM dogs) "
            "RETURNING id, file_path");

        result.media_files_deleted = deleted.size();

        // Estimate bytes freed (placeholder)
        result.bytes_freed = deleted.size() * 500000;  // ~500KB per file estimate

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logWarning("Media cleanup failed: " + std::string(e.what()));
    }
}

void CleanupWorker::cleanupOrphanedRecords(CleanupResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        int total_cleaned = 0;

        // Clean favorites for non-existent dogs
        auto r1 = txn.exec(
            "DELETE FROM favorites WHERE dog_id NOT IN (SELECT id FROM dogs)");
        total_cleaned += r1.affected_rows();

        // Clean foster placements for non-existent dogs
        auto r2 = txn.exec(
            "DELETE FROM foster_placements "
            "WHERE dog_id NOT IN (SELECT id FROM dogs) "
            "AND status IN ('cancelled', 'completed')");
        total_cleaned += r2.affected_rows();

        // Clean old job history
        auto r3 = txn.exec(
            "DELETE FROM job_history WHERE completed_at < NOW() - INTERVAL '30 days'");
        total_cleaned += r3.affected_rows();

        result.orphaned_records_cleaned = total_cleaned;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logWarning("Orphaned record cleanup failed: " + std::string(e.what()));
    }
}

void CleanupWorker::vacuumDatabase() {
    logInfo("Running database vacuum...");

    try {
        auto conn = ConnectionPool::getInstance().acquire();

        // VACUUM requires autocommit mode
        pqxx::nontransaction ntxn(*conn);
        ntxn.exec("VACUUM ANALYZE");

        ConnectionPool::getInstance().release(conn);

        logInfo("Database vacuum completed");

    } catch (const std::exception& e) {
        logWarning("Database vacuum failed: " + std::string(e.what()));
    }
}

} // namespace wtl::workers
