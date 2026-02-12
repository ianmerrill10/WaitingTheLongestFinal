/**
 * @file ScheduledJob.cc
 * @brief Implementation of ScheduledJob struct
 * @see ScheduledJob.h for documentation
 */

#include "ScheduledJob.h"

// Standard library includes
#include <ctime>
#include <iomanip>
#include <sstream>

namespace wtl::workers::scheduler {

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

namespace {

/**
 * @brief Format time point as ISO 8601 string
 */
std::string formatTimePoint(const std::chrono::system_clock::time_point& tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

/**
 * @brief Parse ISO 8601 string to time point
 */
std::chrono::system_clock::time_point parseTimePoint(const std::string& str) {
    std::tm tm = {};
    std::istringstream iss(str);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

    if (iss.fail()) {
        // Try alternative format
        iss.clear();
        iss.str(str);
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    }

    #ifdef _WIN32
    auto time_t_val = _mkgmtime(&tm);
    #else
    auto time_t_val = timegm(&tm);
    #endif

    return std::chrono::system_clock::from_time_t(time_t_val);
}

/**
 * @brief Parse JSON array to vector of strings
 */
std::vector<std::string> parseStringArray(const Json::Value& array) {
    std::vector<std::string> result;
    if (array.isArray()) {
        for (const auto& item : array) {
            if (item.isString()) {
                result.push_back(item.asString());
            }
        }
    }
    return result;
}

} // anonymous namespace

// ============================================================================
// SCHEDULEDJOB SERIALIZATION
// ============================================================================

Json::Value ScheduledJob::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    json["job_type"] = job_type;
    json["entity_id"] = entity_id;
    json["name"] = name;
    json["description"] = description;

    // Scheduling
    json["scheduled_at"] = formatTimePoint(scheduled_at);

    if (cron_expression) {
        json["cron_expression"] = *cron_expression;
    }

    json["recurrence"] = recurrenceTypeToString(recurrence);

    if (interval) {
        json["interval_seconds"] = static_cast<Json::Int64>(interval->count());
    }

    if (expires_at) {
        json["expires_at"] = formatTimePoint(*expires_at);
    }

    // Status
    json["status"] = jobStatusToString(status);
    json["priority"] = priority;
    json["retry_count"] = retry_count;
    json["max_retries"] = max_retries;

    // Execution tracking
    if (last_run) {
        json["last_run"] = formatTimePoint(*last_run);
    }

    if (next_run) {
        json["next_run"] = formatTimePoint(*next_run);
    }

    if (last_duration) {
        json["last_duration_ms"] = static_cast<Json::Int64>(last_duration->count());
    }

    if (last_error) {
        json["last_error"] = *last_error;
    }

    if (last_result) {
        json["last_result"] = *last_result;
    }

    // Payload
    if (!data.isNull()) {
        json["data"] = data;
    }

    if (callback_url) {
        json["callback_url"] = *callback_url;
    }

    if (callback_token) {
        json["callback_token"] = *callback_token;
    }

    // Metadata
    json["created_by"] = created_by;
    json["created_at"] = formatTimePoint(created_at);
    json["updated_at"] = formatTimePoint(updated_at);

    Json::Value tags_array(Json::arrayValue);
    for (const auto& tag : tags) {
        tags_array.append(tag);
    }
    json["tags"] = tags_array;

    return json;
}

ScheduledJob ScheduledJob::fromJson(const Json::Value& json) {
    ScheduledJob job;

    // Identity
    job.id = json.get("id", "").asString();
    job.job_type = json.get("job_type", "").asString();
    job.entity_id = json.get("entity_id", "").asString();
    job.name = json.get("name", "").asString();
    job.description = json.get("description", "").asString();

    // Scheduling
    if (json.isMember("scheduled_at") && !json["scheduled_at"].isNull()) {
        job.scheduled_at = parseTimePoint(json["scheduled_at"].asString());
    } else {
        job.scheduled_at = std::chrono::system_clock::now();
    }

    if (json.isMember("cron_expression") && !json["cron_expression"].isNull()) {
        job.cron_expression = json["cron_expression"].asString();
    }

    job.recurrence = stringToRecurrenceType(json.get("recurrence", "none").asString());

    if (json.isMember("interval_seconds") && !json["interval_seconds"].isNull()) {
        job.interval = std::chrono::seconds(json["interval_seconds"].asInt64());
    }

    if (json.isMember("expires_at") && !json["expires_at"].isNull()) {
        job.expires_at = parseTimePoint(json["expires_at"].asString());
    }

    // Status
    job.status = stringToJobStatus(json.get("status", "pending").asString());
    job.priority = json.get("priority", 0).asInt();
    job.retry_count = json.get("retry_count", 0).asInt();
    job.max_retries = json.get("max_retries", 3).asInt();

    // Execution tracking
    if (json.isMember("last_run") && !json["last_run"].isNull()) {
        job.last_run = parseTimePoint(json["last_run"].asString());
    }

    if (json.isMember("next_run") && !json["next_run"].isNull()) {
        job.next_run = parseTimePoint(json["next_run"].asString());
    }

    if (json.isMember("last_duration_ms") && !json["last_duration_ms"].isNull()) {
        job.last_duration = std::chrono::milliseconds(json["last_duration_ms"].asInt64());
    }

    if (json.isMember("last_error") && !json["last_error"].isNull()) {
        job.last_error = json["last_error"].asString();
    }

    if (json.isMember("last_result") && !json["last_result"].isNull()) {
        job.last_result = json["last_result"].asString();
    }

    // Payload
    if (json.isMember("data")) {
        job.data = json["data"];
    }

    if (json.isMember("callback_url") && !json["callback_url"].isNull()) {
        job.callback_url = json["callback_url"].asString();
    }

    if (json.isMember("callback_token") && !json["callback_token"].isNull()) {
        job.callback_token = json["callback_token"].asString();
    }

    // Metadata
    job.created_by = json.get("created_by", "").asString();

    if (json.isMember("created_at") && !json["created_at"].isNull()) {
        job.created_at = parseTimePoint(json["created_at"].asString());
    } else {
        job.created_at = std::chrono::system_clock::now();
    }

    if (json.isMember("updated_at") && !json["updated_at"].isNull()) {
        job.updated_at = parseTimePoint(json["updated_at"].asString());
    } else {
        job.updated_at = std::chrono::system_clock::now();
    }

    job.tags = parseStringArray(json["tags"]);

    return job;
}

ScheduledJob ScheduledJob::fromDbRow(const pqxx::row& row) {
    ScheduledJob job;

    // Identity
    job.id = row["id"].as<std::string>("");
    job.job_type = row["job_type"].as<std::string>("");
    job.entity_id = row["entity_id"].as<std::string>("");
    job.name = row["name"].as<std::string>("");
    job.description = row["description"].as<std::string>("");

    // Scheduling - parse timestamp
    if (!row["scheduled_at"].is_null()) {
        job.scheduled_at = parseTimePoint(row["scheduled_at"].as<std::string>());
    }

    if (!row["cron_expression"].is_null()) {
        job.cron_expression = row["cron_expression"].as<std::string>();
    }

    job.recurrence = stringToRecurrenceType(row["recurrence"].as<std::string>("none"));

    if (!row["interval_seconds"].is_null()) {
        job.interval = std::chrono::seconds(row["interval_seconds"].as<int64_t>());
    }

    if (!row["expires_at"].is_null()) {
        job.expires_at = parseTimePoint(row["expires_at"].as<std::string>());
    }

    // Status
    job.status = stringToJobStatus(row["status"].as<std::string>("pending"));
    job.priority = row["priority"].as<int>(0);
    job.retry_count = row["retry_count"].as<int>(0);
    job.max_retries = row["max_retries"].as<int>(3);

    // Execution tracking
    if (!row["last_run"].is_null()) {
        job.last_run = parseTimePoint(row["last_run"].as<std::string>());
    }

    if (!row["next_run"].is_null()) {
        job.next_run = parseTimePoint(row["next_run"].as<std::string>());
    }

    if (!row["last_duration_ms"].is_null()) {
        job.last_duration = std::chrono::milliseconds(row["last_duration_ms"].as<int64_t>());
    }

    if (!row["last_error"].is_null()) {
        job.last_error = row["last_error"].as<std::string>();
    }

    if (!row["last_result"].is_null()) {
        job.last_result = row["last_result"].as<std::string>();
    }

    // Payload - parse JSON
    if (!row["data"].is_null()) {
        Json::Value data_json;
        Json::CharReaderBuilder builder;
        std::string errors;
        std::istringstream data_stream(row["data"].as<std::string>());

        if (Json::parseFromStream(builder, data_stream, &data_json, &errors)) {
            job.data = data_json;
        }
    }

    if (!row["callback_url"].is_null()) {
        job.callback_url = row["callback_url"].as<std::string>();
    }

    if (!row["callback_token"].is_null()) {
        job.callback_token = row["callback_token"].as<std::string>();
    }

    // Metadata
    job.created_by = row["created_by"].as<std::string>("");

    if (!row["created_at"].is_null()) {
        job.created_at = parseTimePoint(row["created_at"].as<std::string>());
    }

    if (!row["updated_at"].is_null()) {
        job.updated_at = parseTimePoint(row["updated_at"].as<std::string>());
    }

    // Parse tags from PostgreSQL array
    if (!row["tags"].is_null()) {
        std::string tags_str = row["tags"].as<std::string>();
        // Parse PostgreSQL array format {tag1,tag2,tag3}
        if (tags_str.size() >= 2 && tags_str.front() == '{' && tags_str.back() == '}') {
            tags_str = tags_str.substr(1, tags_str.size() - 2);
            std::istringstream tag_stream(tags_str);
            std::string tag;
            while (std::getline(tag_stream, tag, ',')) {
                // Remove quotes if present
                if (tag.size() >= 2 && tag.front() == '"' && tag.back() == '"') {
                    tag = tag.substr(1, tag.size() - 2);
                }
                if (!tag.empty()) {
                    job.tags.push_back(tag);
                }
            }
        }
    }

    return job;
}

// ============================================================================
// SCHEDULEDJOB HELPERS
// ============================================================================

bool ScheduledJob::isDue() const {
    if (status != JobStatus::PENDING && status != JobStatus::RETRYING) {
        return false;
    }

    auto now = std::chrono::system_clock::now();

    // Check expiration
    if (expires_at && now > *expires_at) {
        return false;
    }

    // Check if next_run is set and due
    if (next_run) {
        return now >= *next_run;
    }

    // Check scheduled_at
    return now >= scheduled_at;
}

bool ScheduledJob::isExpired() const {
    if (!expires_at) {
        return false;
    }

    return std::chrono::system_clock::now() > *expires_at;
}

bool ScheduledJob::canRetry() const {
    return retry_count < max_retries &&
           status == JobStatus::FAILED &&
           !isExpired();
}

bool ScheduledJob::isRecurring() const {
    return recurrence != RecurrenceType::NONE;
}

std::optional<std::chrono::system_clock::time_point> ScheduledJob::calculateNextRun() const {
    if (!isRecurring()) {
        return std::nullopt;
    }

    auto base_time = last_run.value_or(scheduled_at);
    auto now = std::chrono::system_clock::now();

    switch (recurrence) {
        case RecurrenceType::INTERVAL: {
            if (interval) {
                auto next = base_time + *interval;
                // Ensure next run is in the future
                while (next <= now) {
                    next += *interval;
                }
                return next;
            }
            break;
        }

        case RecurrenceType::DAILY: {
            auto next = base_time + std::chrono::hours(24);
            while (next <= now) {
                next += std::chrono::hours(24);
            }
            return next;
        }

        case RecurrenceType::WEEKLY: {
            auto next = base_time + std::chrono::hours(24 * 7);
            while (next <= now) {
                next += std::chrono::hours(24 * 7);
            }
            return next;
        }

        case RecurrenceType::MONTHLY: {
            // Approximate 30 days
            auto next = base_time + std::chrono::hours(24 * 30);
            while (next <= now) {
                next += std::chrono::hours(24 * 30);
            }
            return next;
        }

        case RecurrenceType::CRON: {
            // Cron calculation is handled by CronParser
            // This would be called externally
            break;
        }

        default:
            break;
    }

    return std::nullopt;
}

ScheduledJob ScheduledJob::forNextExecution() const {
    ScheduledJob next_job = *this;

    // Reset execution state
    next_job.status = JobStatus::PENDING;
    next_job.retry_count = 0;
    next_job.last_error = std::nullopt;
    next_job.last_result = std::nullopt;
    next_job.last_duration = std::nullopt;
    next_job.last_run = std::chrono::system_clock::now();

    // Calculate next run time
    auto next_time = calculateNextRun();
    if (next_time) {
        next_job.scheduled_at = *next_time;
        next_job.next_run = *next_time;
    }

    next_job.updated_at = std::chrono::system_clock::now();

    return next_job;
}

} // namespace wtl::workers::scheduler
