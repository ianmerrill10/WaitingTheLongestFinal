/**
 * @file CleanupWorker.h
 * @brief Worker for cleaning up old data
 *
 * PURPOSE:
 * Performs periodic cleanup of expired sessions, old analytics,
 * archived data, and orphaned media files.
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <string>
#include <json/json.h>
#include "BaseWorker.h"

namespace wtl::workers {

/**
 * @struct CleanupResult
 */
struct CleanupResult {
    int sessions_deleted{0};
    int error_logs_archived{0};
    int analytics_archived{0};
    int media_files_deleted{0};
    int orphaned_records_cleaned{0};
    int64_t bytes_freed{0};

    Json::Value toJson() const {
        Json::Value json;
        json["sessions_deleted"] = sessions_deleted;
        json["error_logs_archived"] = error_logs_archived;
        json["analytics_archived"] = analytics_archived;
        json["media_files_deleted"] = media_files_deleted;
        json["orphaned_records_cleaned"] = orphaned_records_cleaned;
        json["bytes_freed"] = static_cast<Json::Int64>(bytes_freed);
        return json;
    }
};

/**
 * @struct CleanupConfig
 */
struct CleanupConfig {
    int session_retention_days{7};
    int error_log_retention_days{30};
    int analytics_retention_days{90};
    int media_retention_days{365};
    bool vacuum_database{false};
};

/**
 * @class CleanupWorker
 * @brief Worker for daily cleanup operations
 */
class CleanupWorker : public BaseWorker {
public:
    /**
     * @brief Constructor - runs daily by default
     */
    CleanupWorker();
    explicit CleanupWorker(const CleanupConfig& config);

    ~CleanupWorker() override = default;

    CleanupResult getLastResult() const;
    CleanupResult cleanupNow();

    void setCleanupConfig(const CleanupConfig& config);

protected:
    WorkerResult doExecute() override;

private:
    void cleanupExpiredSessions(CleanupResult& result);
    void archiveOldErrorLogs(CleanupResult& result);
    void archiveOldAnalytics(CleanupResult& result);
    void cleanupOrphanedMedia(CleanupResult& result);
    void cleanupOrphanedRecords(CleanupResult& result);
    void vacuumDatabase();

    CleanupConfig cleanup_config_;
    mutable std::mutex result_mutex_;
    CleanupResult last_result_;
};

} // namespace wtl::workers
