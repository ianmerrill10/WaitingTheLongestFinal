/**
 * @file ScheduledJob.h
 * @brief Scheduled job data model for the scheduler system
 *
 * PURPOSE:
 * Represents a scheduled job that can be executed at a specific time
 * or on a recurring schedule using cron expressions.
 *
 * USAGE:
 * ScheduledJob job;
 * job.job_type = "send_email";
 * job.scheduled_at = std::chrono::system_clock::now() + std::chrono::hours(1);
 * job.data = emailData;
 * scheduler.schedule(job);
 *
 * DEPENDENCIES:
 * - jsoncpp for JSON serialization
 * - pqxx for database row parsing
 *
 * MODIFICATION GUIDE:
 * - Add new job types to JobType enum
 * - Extend data payload for new job types
 * - Add new status transitions as needed
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

namespace wtl::workers::scheduler {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum JobStatus
 * @brief Status of a scheduled job
 */
enum class JobStatus {
    PENDING = 0,    ///< Job is scheduled but not yet executed
    RUNNING,        ///< Job is currently executing
    COMPLETED,      ///< Job completed successfully
    FAILED,         ///< Job failed to execute
    CANCELLED,      ///< Job was cancelled
    RETRYING,       ///< Job failed and is scheduled for retry
    EXPIRED         ///< Job expired without executing
};

/**
 * @enum JobType
 * @brief Types of scheduled jobs
 */
enum class JobType {
    ONE_TIME = 0,   ///< Execute once at scheduled time
    RECURRING,      ///< Execute on a recurring schedule
    DELAYED,        ///< Execute after a delay
    PRIORITY        ///< High-priority job
};

/**
 * @enum RecurrenceType
 * @brief Type of recurrence for recurring jobs
 */
enum class RecurrenceType {
    NONE = 0,       ///< No recurrence (one-time)
    CRON,           ///< Cron expression based
    INTERVAL,       ///< Fixed interval
    DAILY,          ///< Daily at specific time
    WEEKLY,         ///< Weekly on specific day
    MONTHLY         ///< Monthly on specific day
};

// ============================================================================
// SCHEDULED JOB STRUCTURE
// ============================================================================

/**
 * @struct ScheduledJob
 * @brief Represents a scheduled job
 *
 * Contains all information needed to schedule, execute, and track
 * a background job.
 */
struct ScheduledJob {
    // =========================================================================
    // IDENTITY
    // =========================================================================

    std::string id;                                   ///< Unique job ID (UUID)
    std::string job_type;                             ///< Type of job (e.g., "send_email")
    std::string entity_id;                            ///< Related entity ID (optional)
    std::string name;                                 ///< Human-readable name
    std::string description;                          ///< Job description

    // =========================================================================
    // SCHEDULING
    // =========================================================================

    std::chrono::system_clock::time_point scheduled_at; ///< When to execute
    std::optional<std::string> cron_expression;       ///< Cron expression for recurring
    RecurrenceType recurrence{RecurrenceType::NONE};  ///< Recurrence type
    std::optional<std::chrono::seconds> interval;     ///< Interval for recurring jobs
    std::optional<std::chrono::system_clock::time_point> expires_at; ///< Expiration time

    // =========================================================================
    // STATUS
    // =========================================================================

    JobStatus status{JobStatus::PENDING};             ///< Current status
    int priority{0};                                  ///< Priority (higher = more important)
    int retry_count{0};                               ///< Number of retries attempted
    int max_retries{3};                               ///< Maximum retry attempts

    // =========================================================================
    // EXECUTION TRACKING
    // =========================================================================

    std::optional<std::chrono::system_clock::time_point> last_run; ///< Last execution time
    std::optional<std::chrono::system_clock::time_point> next_run; ///< Next execution time
    std::optional<std::chrono::milliseconds> last_duration;        ///< Last execution duration
    std::optional<std::string> last_error;            ///< Last error message
    std::optional<std::string> last_result;           ///< Last execution result (JSON string)

    // =========================================================================
    // PAYLOAD
    // =========================================================================

    Json::Value data;                                 ///< Job-specific data payload
    std::optional<std::string> callback_url;          ///< Webhook URL for completion
    std::optional<std::string> callback_token;        ///< Auth token for callback

    // =========================================================================
    // METADATA
    // =========================================================================

    std::string created_by;                           ///< User who created the job
    std::chrono::system_clock::time_point created_at; ///< Creation timestamp
    std::chrono::system_clock::time_point updated_at; ///< Last update timestamp
    std::vector<std::string> tags;                    ///< Tags for filtering

    // =========================================================================
    // SERIALIZATION
    // =========================================================================

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return ScheduledJob
     */
    static ScheduledJob fromJson(const Json::Value& json);

    /**
     * @brief Create from database row
     * @param row Database row
     * @return ScheduledJob
     */
    static ScheduledJob fromDbRow(const pqxx::row& row);

    // =========================================================================
    // HELPERS
    // =========================================================================

    /**
     * @brief Check if job is due for execution
     * @return true if job should be executed now
     */
    bool isDue() const;

    /**
     * @brief Check if job is expired
     * @return true if job has expired
     */
    bool isExpired() const;

    /**
     * @brief Check if job can be retried
     * @return true if retry is possible
     */
    bool canRetry() const;

    /**
     * @brief Check if job is recurring
     * @return true if job recurs
     */
    bool isRecurring() const;

    /**
     * @brief Calculate next run time for recurring job
     * @return Next run time, or nullopt if not recurring
     */
    std::optional<std::chrono::system_clock::time_point> calculateNextRun() const;

    /**
     * @brief Create a copy with updated next run time
     * @return Updated job for next execution
     */
    ScheduledJob forNextExecution() const;
};

/**
 * @struct JobExecutionResult
 * @brief Result of executing a scheduled job
 */
struct JobExecutionResult {
    bool success{false};                              ///< Whether execution succeeded
    std::string message;                              ///< Result message
    std::chrono::milliseconds duration{0};            ///< Execution duration
    Json::Value output;                               ///< Output data
    std::optional<std::string> error;                 ///< Error message if failed
    bool should_retry{false};                         ///< Whether to retry on failure

    /**
     * @brief Create success result
     * @param msg Message
     * @param output_data Output data
     * @return Success result
     */
    static JobExecutionResult Success(const std::string& msg = "OK",
                                       const Json::Value& output_data = {}) {
        JobExecutionResult result;
        result.success = true;
        result.message = msg;
        result.output = output_data;
        return result;
    }

    /**
     * @brief Create failure result
     * @param error_msg Error message
     * @param retry Whether to retry
     * @return Failure result
     */
    static JobExecutionResult Failure(const std::string& error_msg, bool retry = true) {
        JobExecutionResult result;
        result.success = false;
        result.message = error_msg;
        result.error = error_msg;
        result.should_retry = retry;
        return result;
    }

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["success"] = success;
        json["message"] = message;
        json["duration_ms"] = static_cast<Json::Int64>(duration.count());

        if (!output.isNull()) {
            json["output"] = output;
        }

        if (error) {
            json["error"] = *error;
        }

        json["should_retry"] = should_retry;

        return json;
    }
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Convert JobStatus to string
 * @param status The status
 * @return String representation
 */
inline std::string jobStatusToString(JobStatus status) {
    switch (status) {
        case JobStatus::PENDING:   return "pending";
        case JobStatus::RUNNING:   return "running";
        case JobStatus::COMPLETED: return "completed";
        case JobStatus::FAILED:    return "failed";
        case JobStatus::CANCELLED: return "cancelled";
        case JobStatus::RETRYING:  return "retrying";
        case JobStatus::EXPIRED:   return "expired";
        default:                   return "unknown";
    }
}

/**
 * @brief Convert string to JobStatus
 * @param str The string
 * @return JobStatus
 */
inline JobStatus stringToJobStatus(const std::string& str) {
    if (str == "pending")   return JobStatus::PENDING;
    if (str == "running")   return JobStatus::RUNNING;
    if (str == "completed") return JobStatus::COMPLETED;
    if (str == "failed")    return JobStatus::FAILED;
    if (str == "cancelled") return JobStatus::CANCELLED;
    if (str == "retrying")  return JobStatus::RETRYING;
    if (str == "expired")   return JobStatus::EXPIRED;
    return JobStatus::PENDING;
}

/**
 * @brief Convert RecurrenceType to string
 * @param type The type
 * @return String representation
 */
inline std::string recurrenceTypeToString(RecurrenceType type) {
    switch (type) {
        case RecurrenceType::NONE:    return "none";
        case RecurrenceType::CRON:    return "cron";
        case RecurrenceType::INTERVAL: return "interval";
        case RecurrenceType::DAILY:   return "daily";
        case RecurrenceType::WEEKLY:  return "weekly";
        case RecurrenceType::MONTHLY: return "monthly";
        default:                      return "unknown";
    }
}

/**
 * @brief Convert string to RecurrenceType
 * @param str The string
 * @return RecurrenceType
 */
inline RecurrenceType stringToRecurrenceType(const std::string& str) {
    if (str == "none")     return RecurrenceType::NONE;
    if (str == "cron")     return RecurrenceType::CRON;
    if (str == "interval") return RecurrenceType::INTERVAL;
    if (str == "daily")    return RecurrenceType::DAILY;
    if (str == "weekly")   return RecurrenceType::WEEKLY;
    if (str == "monthly")  return RecurrenceType::MONTHLY;
    return RecurrenceType::NONE;
}

} // namespace wtl::workers::scheduler
