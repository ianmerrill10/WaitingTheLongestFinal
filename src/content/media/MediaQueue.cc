/**
 * @file MediaQueue.cc
 * @brief Implementation of asynchronous media processing queue
 *
 * @author Agent 15 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "content/media/MediaQueue.h"

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

// Standard library includes
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace wtl::content::media {

// ============================================================================
// MEDIA JOB IMPLEMENTATION
// ============================================================================

Json::Value MediaJob::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["type"] = jobTypeToString(type);
    json["status"] = jobStatusToString(status);
    json["priority"] = jobPriorityToString(priority);
    json["dog_id"] = dog_id;
    json["input_data"] = input_data;
    json["result_data"] = result_data;

    // Format timestamps
    auto format_time = [](const std::chrono::system_clock::time_point& tp) -> std::string {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    };

    json["created_at"] = format_time(created_at);
    json["started_at"] = format_time(started_at);
    json["completed_at"] = format_time(completed_at);

    json["progress"] = progress;
    json["current_stage"] = current_stage;
    json["error_message"] = error_message;
    json["retry_count"] = retry_count;
    json["max_retries"] = max_retries;
    json["callback_url"] = callback_url;

    return json;
}

MediaJob MediaJob::fromJson(const Json::Value& json) {
    MediaJob job;

    job.id = json.get("id", "").asString();
    job.type = stringToJobType(json.get("type", "video_generation").asString());
    job.status = stringToJobStatus(json.get("status", "pending").asString());
    job.priority = stringToJobPriority(json.get("priority", "normal").asString());
    job.dog_id = json.get("dog_id", "").asString();
    job.input_data = json.get("input_data", Json::Value());
    job.result_data = json.get("result_data", Json::Value());

    // Parse timestamps (simplified - use current time as default)
    job.created_at = std::chrono::system_clock::now();
    job.started_at = std::chrono::system_clock::time_point{};
    job.completed_at = std::chrono::system_clock::time_point{};

    job.progress = json.get("progress", 0.0).asDouble();
    job.current_stage = json.get("current_stage", "").asString();
    job.error_message = json.get("error_message", "").asString();
    job.retry_count = json.get("retry_count", 0).asInt();
    job.max_retries = json.get("max_retries", 3).asInt();
    job.callback_url = json.get("callback_url", "").asString();

    return job;
}

MediaJob MediaJob::fromDbRow(const std::vector<std::string>& row) {
    MediaJob job;

    if (row.size() < 14) {
        return job;
    }

    job.id = row[0];
    job.type = stringToJobType(row[1]);
    job.status = stringToJobStatus(row[2]);
    job.priority = stringToJobPriority(row[3]);
    job.dog_id = row[4];

    // Parse JSON fields
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errors;

    if (!row[5].empty()) {
        reader->parse(row[5].c_str(), row[5].c_str() + row[5].size(), &job.input_data, &errors);
    }
    if (!row[6].empty()) {
        reader->parse(row[6].c_str(), row[6].c_str() + row[6].size(), &job.result_data, &errors);
    }

    // Parse timestamps (simplified)
    job.created_at = std::chrono::system_clock::now();
    job.started_at = std::chrono::system_clock::time_point{};
    job.completed_at = std::chrono::system_clock::time_point{};

    job.progress = std::stod(row[10].empty() ? "0" : row[10]);
    job.current_stage = row[11];
    job.error_message = row[12];
    job.retry_count = std::stoi(row[13].empty() ? "0" : row[13]);

    if (row.size() > 14) {
        job.max_retries = std::stoi(row[14].empty() ? "3" : row[14]);
    }
    if (row.size() > 15) {
        job.callback_url = row[15];
    }

    return job;
}

// ============================================================================
// QUEUE STATS IMPLEMENTATION
// ============================================================================

Json::Value QueueStats::toJson() const {
    Json::Value json;
    json["pending_count"] = pending_count;
    json["processing_count"] = processing_count;
    json["completed_count"] = completed_count;
    json["failed_count"] = failed_count;
    json["avg_processing_time_ms"] = avg_processing_time_ms;
    json["total_processed"] = total_processed;
    return json;
}

// ============================================================================
// MEDIA QUEUE IMPLEMENTATION
// ============================================================================

MediaQueue& MediaQueue::getInstance() {
    static MediaQueue instance;
    return instance;
}

bool MediaQueue::initialize(int max_queue_size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return true;
    }

    max_queue_size_ = max_queue_size;

    // Verify database connection
    auto& pool = wtl::core::db::ConnectionPool::getInstance();
    if (!pool.isInitialized()) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "MediaQueue::initialize - Database connection pool not initialized", {});
        return false;
    }

    // Create jobs table if not exists (should be done by migration, but check)
    const std::string check_sql = R"(
        SELECT EXISTS (
            SELECT FROM information_schema.tables
            WHERE table_schema = 'public'
            AND table_name = 'media_jobs'
        )
    )";

    try {
        auto conn = pool.acquire();
        if (conn) {
            pqxx::work txn(*conn);
            auto result = txn.exec(check_sql);
            txn.commit();
            pool.release(std::move(conn));

            if (!result.empty() && result[0][0].as<bool>()) {
                initialized_ = true;
                return true;
            }
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::initialize - Database check failed: ") + e.what(), {});
    }

    // Table doesn't exist, log warning but continue
    // The migration should create the table
    WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "MediaQueue::initialize - media_jobs table not found - run migrations", {});

    initialized_ = true;
    return true;
}

bool MediaQueue::initializeFromConfig() {
    // Load configuration from media_config.json
    int max_queue_size = 1000;

    std::ifstream config_file("config/media_config.json");
    if (config_file.is_open()) {
        Json::Value config;
        Json::CharReaderBuilder builder;
        std::string errors;

        if (Json::parseFromStream(builder, config_file, &config, &errors)) {
            if (config.isMember("queue") && config["queue"].isMember("max_size")) {
                max_queue_size = config["queue"]["max_size"].asInt();
            }
        }
        config_file.close();
    }

    return initialize(max_queue_size);
}

bool MediaQueue::isInitialized() const {
    return initialized_;
}

// ============================================================================
// ENQUEUE OPERATIONS
// ============================================================================

std::string MediaQueue::enqueue(const MediaJob& job) {
    if (!initialized_) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "MediaQueue::enqueue - Queue not initialized", {});
        return "";
    }

    MediaJob new_job = job;

    // Generate ID if not set
    if (new_job.id.empty()) {
        new_job.id = generateJobId();
    }

    // Set creation time
    new_job.created_at = std::chrono::system_clock::now();
    new_job.status = JobStatus::PENDING;

    // Save to database
    if (!saveJob(new_job)) {
        return "";
    }

    total_enqueued_++;

    // Notify waiting workers
    notifyActivity();

    return new_job.id;
}

std::string MediaQueue::enqueueVideoGeneration(
    const std::string& dog_id,
    const std::vector<std::string>& photos,
    const std::string& template_id,
    JobPriority priority
) {
    MediaJob job;
    job.type = JobType::VIDEO_GENERATION;
    job.dog_id = dog_id;
    job.priority = priority;

    // Store photos in input data
    Json::Value photos_array(Json::arrayValue);
    for (const auto& photo : photos) {
        photos_array.append(photo);
    }
    job.input_data["photos"] = photos_array;
    job.input_data["template_id"] = template_id;

    return enqueue(job);
}

std::string MediaQueue::enqueueImageProcessing(
    const std::string& dog_id,
    const std::string& image_path,
    const std::string& operation,
    const Json::Value& options
) {
    MediaJob job;
    job.type = JobType::IMAGE_PROCESSING;
    job.dog_id = dog_id;
    job.priority = JobPriority::NORMAL;

    job.input_data["image_path"] = image_path;
    job.input_data["operation"] = operation;
    job.input_data["options"] = options;

    return enqueue(job);
}

std::string MediaQueue::enqueueShareCard(
    const std::string& dog_id,
    const std::string& platform
) {
    MediaJob job;
    job.type = JobType::SHARE_CARD;
    job.dog_id = dog_id;
    job.priority = JobPriority::NORMAL;

    job.input_data["platform"] = platform;

    return enqueue(job);
}

std::string MediaQueue::enqueueThumbnail(
    const std::string& dog_id,
    const std::string& video_path
) {
    MediaJob job;
    job.type = JobType::THUMBNAIL;
    job.dog_id = dog_id;
    job.priority = JobPriority::HIGH;  // Thumbnails are quick and needed soon

    job.input_data["video_path"] = video_path;

    return enqueue(job);
}

std::string MediaQueue::enqueuePlatformTranscode(
    const std::string& dog_id,
    const std::string& video_path,
    const std::vector<std::string>& platforms
) {
    MediaJob job;
    job.type = JobType::PLATFORM_TRANSCODE;
    job.dog_id = dog_id;
    job.priority = JobPriority::NORMAL;

    job.input_data["video_path"] = video_path;

    Json::Value platforms_array(Json::arrayValue);
    for (const auto& platform : platforms) {
        platforms_array.append(platform);
    }
    job.input_data["platforms"] = platforms_array;

    return enqueue(job);
}

// ============================================================================
// DEQUEUE OPERATIONS
// ============================================================================

std::optional<MediaJob> MediaQueue::dequeue() {
    if (!initialized_) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Get next pending job with highest priority
    const std::string sql = R"(
        UPDATE media_jobs
        SET status = 'processing',
            started_at = NOW()
        WHERE id = (
            SELECT id FROM media_jobs
            WHERE status = 'pending'
            ORDER BY
                CASE priority
                    WHEN 'urgent' THEN 0
                    WHEN 'high' THEN 1
                    WHEN 'normal' THEN 2
                    WHEN 'low' THEN 3
                END,
                created_at ASC
            LIMIT 1
            FOR UPDATE SKIP LOCKED
        )
        RETURNING id, job_type, status, priority, dog_id,
                  input_data, result_data, created_at, started_at,
                  completed_at, progress, current_stage, error_message,
                  retry_count, max_retries, callback_url
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "MediaQueue::dequeue - Failed to get database connection", {});
            return std::nullopt;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec(sql);
        txn.commit();
        pool.release(std::move(conn));

        if (result.empty()) {
            return std::nullopt;
        }

        // Convert row to MediaJob
        std::vector<std::string> row;
        for (const auto& field : result[0]) {
            row.push_back(field.is_null() ? "" : field.c_str());
        }

        return MediaJob::fromDbRow(row);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::dequeue - Database error: ") + e.what(), {});
        return std::nullopt;
    }
}

std::optional<MediaJob> MediaQueue::dequeue(JobType type) {
    if (!initialized_) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    std::string type_str = jobTypeToString(type);

    const std::string sql = R"(
        UPDATE media_jobs
        SET status = 'processing',
            started_at = NOW()
        WHERE id = (
            SELECT id FROM media_jobs
            WHERE status = 'pending' AND job_type = $1
            ORDER BY
                CASE priority
                    WHEN 'urgent' THEN 0
                    WHEN 'high' THEN 1
                    WHEN 'normal' THEN 2
                    WHEN 'low' THEN 3
                END,
                created_at ASC
            LIMIT 1
            FOR UPDATE SKIP LOCKED
        )
        RETURNING id, job_type, status, priority, dog_id,
                  input_data, result_data, created_at, started_at,
                  completed_at, progress, current_stage, error_message,
                  retry_count, max_retries, callback_url
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return std::nullopt;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec_params(sql, type_str);
        txn.commit();
        pool.release(std::move(conn));

        if (result.empty()) {
            return std::nullopt;
        }

        std::vector<std::string> row;
        for (const auto& field : result[0]) {
            row.push_back(field.is_null() ? "" : field.c_str());
        }

        return MediaJob::fromDbRow(row);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::dequeue - Database error: ") + e.what(), {});
        return std::nullopt;
    }
}

std::optional<MediaJob> MediaQueue::peek() const {
    if (!initialized_) {
        return std::nullopt;
    }

    const std::string sql = R"(
        SELECT id, job_type, status, priority, dog_id,
               input_data, result_data, created_at, started_at,
               completed_at, progress, current_stage, error_message,
               retry_count, max_retries, callback_url
        FROM media_jobs
        WHERE status = 'pending'
        ORDER BY
            CASE priority
                WHEN 'urgent' THEN 0
                WHEN 'high' THEN 1
                WHEN 'normal' THEN 2
                WHEN 'low' THEN 3
            END,
            created_at ASC
        LIMIT 1
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return std::nullopt;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec(sql);
        txn.commit();
        pool.release(std::move(conn));

        if (result.empty()) {
            return std::nullopt;
        }

        std::vector<std::string> row;
        for (const auto& field : result[0]) {
            row.push_back(field.is_null() ? "" : field.c_str());
        }

        return MediaJob::fromDbRow(row);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::peek - Database error: ") + e.what(), {});
        return std::nullopt;
    }
}

// ============================================================================
// JOB MANAGEMENT
// ============================================================================

std::optional<MediaJob> MediaQueue::getStatus(const std::string& job_id) {
    if (!initialized_) {
        return std::nullopt;
    }

    return loadJob(job_id);
}

bool MediaQueue::updateStatus(
    const std::string& job_id,
    JobStatus status,
    const Json::Value& result,
    const std::string& error_message
) {
    if (!initialized_) {
        return false;
    }

    std::string status_str = jobStatusToString(status);

    Json::StreamWriterBuilder writer;
    std::string result_json = Json::writeString(writer, result);

    std::string sql;
    if (status == JobStatus::COMPLETED || status == JobStatus::FAILED) {
        sql = R"(
            UPDATE media_jobs
            SET status = $1,
                result_data = $2::jsonb,
                error_message = $3,
                completed_at = NOW(),
                progress = 100.0
            WHERE id = $4
        )";
    } else {
        sql = R"(
            UPDATE media_jobs
            SET status = $1,
                result_data = $2::jsonb,
                error_message = $3
            WHERE id = $4
        )";
    }

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return false;
        }

        pqxx::work txn(*conn);
        txn.exec_params(sql, status_str, result_json, error_message, job_id);
        txn.commit();
        pool.release(std::move(conn));

        // Update statistics
        if (status == JobStatus::COMPLETED) {
            total_completed_++;
        } else if (status == JobStatus::FAILED) {
            total_failed_++;
        }

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::updateStatus - Database error: ") + e.what(), {});
        return false;
    }
}

bool MediaQueue::updateProgress(
    const std::string& job_id,
    double progress,
    const std::string& stage
) {
    if (!initialized_) {
        return false;
    }

    const std::string sql = R"(
        UPDATE media_jobs
        SET progress = $1,
            current_stage = $2
        WHERE id = $3
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return false;
        }

        pqxx::work txn(*conn);
        txn.exec_params(sql, progress, stage, job_id);
        txn.commit();
        pool.release(std::move(conn));

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::updateProgress - Database error: ") + e.what(), {});
        return false;
    }
}

bool MediaQueue::cancel(const std::string& job_id) {
    if (!initialized_) {
        return false;
    }

    // Can only cancel pending jobs
    const std::string sql = R"(
        UPDATE media_jobs
        SET status = 'cancelled',
            completed_at = NOW()
        WHERE id = $1 AND status = 'pending'
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return false;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec_params(sql, job_id);
        txn.commit();
        pool.release(std::move(conn));

        return result.affected_rows() > 0;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::cancel - Database error: ") + e.what(), {});
        return false;
    }
}

bool MediaQueue::retry(const std::string& job_id) {
    if (!initialized_) {
        return false;
    }

    // Can only retry failed jobs that haven't exceeded max retries
    const std::string sql = R"(
        UPDATE media_jobs
        SET status = 'pending',
            retry_count = retry_count + 1,
            error_message = '',
            progress = 0,
            current_stage = ''
        WHERE id = $1
          AND status = 'failed'
          AND retry_count < max_retries
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return false;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec_params(sql, job_id);
        txn.commit();
        pool.release(std::move(conn));

        if (result.affected_rows() > 0) {
            notifyActivity();
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::retry - Database error: ") + e.what(), {});
        return false;
    }
}

std::vector<MediaJob> MediaQueue::getJobsForDog(const std::string& dog_id) {
    std::vector<MediaJob> jobs;

    if (!initialized_) {
        return jobs;
    }

    const std::string sql = R"(
        SELECT id, job_type, status, priority, dog_id,
               input_data, result_data, created_at, started_at,
               completed_at, progress, current_stage, error_message,
               retry_count, max_retries, callback_url
        FROM media_jobs
        WHERE dog_id = $1
        ORDER BY created_at DESC
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return jobs;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec_params(sql, dog_id);
        txn.commit();
        pool.release(std::move(conn));

        for (const auto& db_row : result) {
            std::vector<std::string> row;
            for (const auto& field : db_row) {
                row.push_back(field.is_null() ? "" : field.c_str());
            }
            jobs.push_back(MediaJob::fromDbRow(row));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::getJobsForDog - Database error: ") + e.what(), {});
    }

    return jobs;
}

std::vector<MediaJob> MediaQueue::getPendingJobs(int limit) {
    std::vector<MediaJob> jobs;

    if (!initialized_) {
        return jobs;
    }

    const std::string sql = R"(
        SELECT id, job_type, status, priority, dog_id,
               input_data, result_data, created_at, started_at,
               completed_at, progress, current_stage, error_message,
               retry_count, max_retries, callback_url
        FROM media_jobs
        WHERE status = 'pending'
        ORDER BY
            CASE priority
                WHEN 'urgent' THEN 0
                WHEN 'high' THEN 1
                WHEN 'normal' THEN 2
                WHEN 'low' THEN 3
            END,
            created_at ASC
        LIMIT $1
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return jobs;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec_params(sql, limit);
        txn.commit();
        pool.release(std::move(conn));

        for (const auto& db_row : result) {
            std::vector<std::string> row;
            for (const auto& field : db_row) {
                row.push_back(field.is_null() ? "" : field.c_str());
            }
            jobs.push_back(MediaJob::fromDbRow(row));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::getPendingJobs - Database error: ") + e.what(), {});
    }

    return jobs;
}

std::vector<MediaJob> MediaQueue::getProcessingJobs() {
    std::vector<MediaJob> jobs;

    if (!initialized_) {
        return jobs;
    }

    const std::string sql = R"(
        SELECT id, job_type, status, priority, dog_id,
               input_data, result_data, created_at, started_at,
               completed_at, progress, current_stage, error_message,
               retry_count, max_retries, callback_url
        FROM media_jobs
        WHERE status = 'processing'
        ORDER BY started_at ASC
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return jobs;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec(sql);
        txn.commit();
        pool.release(std::move(conn));

        for (const auto& db_row : result) {
            std::vector<std::string> row;
            for (const auto& field : db_row) {
                row.push_back(field.is_null() ? "" : field.c_str());
            }
            jobs.push_back(MediaJob::fromDbRow(row));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::getProcessingJobs - Database error: ") + e.what(), {});
    }

    return jobs;
}

std::vector<MediaJob> MediaQueue::getFailedJobs(int limit) {
    std::vector<MediaJob> jobs;

    if (!initialized_) {
        return jobs;
    }

    const std::string sql = R"(
        SELECT id, job_type, status, priority, dog_id,
               input_data, result_data, created_at, started_at,
               completed_at, progress, current_stage, error_message,
               retry_count, max_retries, callback_url
        FROM media_jobs
        WHERE status = 'failed'
        ORDER BY completed_at DESC
        LIMIT $1
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return jobs;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec_params(sql, limit);
        txn.commit();
        pool.release(std::move(conn));

        for (const auto& db_row : result) {
            std::vector<std::string> row;
            for (const auto& field : db_row) {
                row.push_back(field.is_null() ? "" : field.c_str());
            }
            jobs.push_back(MediaJob::fromDbRow(row));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::getFailedJobs - Database error: ") + e.what(), {});
    }

    return jobs;
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

int MediaQueue::size() const {
    if (!initialized_) {
        return 0;
    }

    const std::string sql = "SELECT COUNT(*) FROM media_jobs WHERE status = 'pending'";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return 0;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec(sql);
        txn.commit();
        pool.release(std::move(conn));

        if (!result.empty()) {
            return result[0][0].as<int>();
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::size - Database error: ") + e.what(), {});
    }

    return 0;
}

bool MediaQueue::isEmpty() const {
    return size() == 0;
}

int MediaQueue::clear() {
    if (!initialized_) {
        return 0;
    }

    const std::string sql = "DELETE FROM media_jobs WHERE status = 'pending'";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return 0;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec(sql);
        int deleted = result.affected_rows();
        txn.commit();
        pool.release(std::move(conn));

        return deleted;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::clear - Database error: ") + e.what(), {});
        return 0;
    }
}

QueueStats MediaQueue::getStats() const {
    QueueStats stats;

    if (!initialized_) {
        return stats;
    }

    const std::string sql = R"(
        SELECT
            COUNT(*) FILTER (WHERE status = 'pending') as pending,
            COUNT(*) FILTER (WHERE status = 'processing') as processing,
            COUNT(*) FILTER (WHERE status = 'completed') as completed,
            COUNT(*) FILTER (WHERE status = 'failed') as failed,
            AVG(EXTRACT(EPOCH FROM (completed_at - started_at)) * 1000)
                FILTER (WHERE status = 'completed') as avg_time,
            COUNT(*) FILTER (WHERE status IN ('completed', 'failed')) as total
        FROM media_jobs
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return stats;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec(sql);
        txn.commit();
        pool.release(std::move(conn));

        if (!result.empty()) {
            stats.pending_count = result[0][0].is_null() ? 0 : result[0][0].as<int>();
            stats.processing_count = result[0][1].is_null() ? 0 : result[0][1].as<int>();
            stats.completed_count = result[0][2].is_null() ? 0 : result[0][2].as<int>();
            stats.failed_count = result[0][3].is_null() ? 0 : result[0][3].as<int>();
            stats.avg_processing_time_ms = result[0][4].is_null() ? 0.0 : result[0][4].as<double>();
            stats.total_processed = result[0][5].is_null() ? 0 : result[0][5].as<int>();
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::getStats - Database error: ") + e.what(), {});
    }

    return stats;
}

int MediaQueue::cleanup(int days_old) {
    if (!initialized_) {
        return 0;
    }

    const std::string sql = R"(
        DELETE FROM media_jobs
        WHERE status IN ('completed', 'failed', 'cancelled')
          AND completed_at < NOW() - INTERVAL '1 day' * $1
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return 0;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec_params(sql, days_old);
        int deleted = result.affected_rows();
        txn.commit();
        pool.release(std::move(conn));

        return deleted;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::cleanup - Database error: ") + e.what(), {});
        return 0;
    }
}

int MediaQueue::resetStuckJobs(int timeout_minutes) {
    if (!initialized_) {
        return 0;
    }

    const std::string sql = R"(
        UPDATE media_jobs
        SET status = 'pending',
            started_at = NULL,
            progress = 0,
            current_stage = ''
        WHERE status = 'processing'
          AND started_at < NOW() - INTERVAL '1 minute' * $1
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return 0;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec_params(sql, timeout_minutes);
        int reset = result.affected_rows();
        txn.commit();
        pool.release(std::move(conn));

        if (reset > 0) {
            notifyActivity();
        }

        return reset;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::resetStuckJobs - Database error: ") + e.what(), {});
        return 0;
    }
}

// ============================================================================
// NOTIFICATION
// ============================================================================

bool MediaQueue::waitForActivity(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return activity_cv_.wait_for(lock, timeout, [this]() {
        return !isEmpty();
    });
}

void MediaQueue::notifyActivity() {
    activity_cv_.notify_all();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::string MediaQueue::generateJobId() {
    // Generate UUID-like ID: mjob_<timestamp>_<random>
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFF);

    std::ostringstream oss;
    oss << "mjob_" << std::hex << timestamp << "_" << dis(gen);
    return oss.str();
}

bool MediaQueue::saveJob(const MediaJob& job) {
    Json::StreamWriterBuilder writer;
    std::string input_json = Json::writeString(writer, job.input_data);
    std::string result_json = Json::writeString(writer, job.result_data);

    const std::string sql = R"(
        INSERT INTO media_jobs (
            id, job_type, status, priority, dog_id,
            input_data, result_data, created_at, progress,
            current_stage, error_message, retry_count, max_retries,
            callback_url
        ) VALUES (
            $1, $2, $3, $4, $5,
            $6::jsonb, $7::jsonb, NOW(), $8,
            $9, $10, $11, $12, $13
        )
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "MediaQueue::saveJob - Failed to get database connection", {});
            return false;
        }

        pqxx::work txn(*conn);
        txn.exec_params(
            sql,
            job.id,
            jobTypeToString(job.type),
            jobStatusToString(job.status),
            jobPriorityToString(job.priority),
            job.dog_id,
            input_json,
            result_json,
            job.progress,
            job.current_stage,
            job.error_message,
            job.retry_count,
            job.max_retries,
            job.callback_url
        );
        txn.commit();
        pool.release(std::move(conn));

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::saveJob - Database error: ") + e.what(), {});
        return false;
    }
}

std::optional<MediaJob> MediaQueue::loadJob(const std::string& job_id) {
    const std::string sql = R"(
        SELECT id, job_type, status, priority, dog_id,
               input_data, result_data, created_at, started_at,
               completed_at, progress, current_stage, error_message,
               retry_count, max_retries, callback_url
        FROM media_jobs
        WHERE id = $1
    )";

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (!conn) {
            return std::nullopt;
        }

        pqxx::work txn(*conn);
        auto result = txn.exec_params(sql, job_id);
        txn.commit();
        pool.release(std::move(conn));

        if (result.empty()) {
            return std::nullopt;
        }

        std::vector<std::string> row;
        for (const auto& field : result[0]) {
            row.push_back(field.is_null() ? "" : field.c_str());
        }

        return MediaJob::fromDbRow(row);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, std::string("MediaQueue::loadJob - Database error: ") + e.what(), {});
        return std::nullopt;
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::string jobTypeToString(JobType type) {
    switch (type) {
        case JobType::VIDEO_GENERATION: return "video_generation";
        case JobType::IMAGE_PROCESSING: return "image_processing";
        case JobType::SHARE_CARD: return "share_card";
        case JobType::THUMBNAIL: return "thumbnail";
        case JobType::PLATFORM_TRANSCODE: return "platform_transcode";
        case JobType::BATCH_PROCESS: return "batch_process";
        case JobType::CLEANUP: return "cleanup";
        default: return "video_generation";
    }
}

JobType stringToJobType(const std::string& str) {
    if (str == "video_generation") return JobType::VIDEO_GENERATION;
    if (str == "image_processing") return JobType::IMAGE_PROCESSING;
    if (str == "share_card") return JobType::SHARE_CARD;
    if (str == "thumbnail") return JobType::THUMBNAIL;
    if (str == "platform_transcode") return JobType::PLATFORM_TRANSCODE;
    if (str == "batch_process") return JobType::BATCH_PROCESS;
    if (str == "cleanup") return JobType::CLEANUP;
    return JobType::VIDEO_GENERATION;
}

std::string jobStatusToString(JobStatus status) {
    switch (status) {
        case JobStatus::PENDING: return "pending";
        case JobStatus::PROCESSING: return "processing";
        case JobStatus::COMPLETED: return "completed";
        case JobStatus::FAILED: return "failed";
        case JobStatus::CANCELLED: return "cancelled";
        default: return "pending";
    }
}

JobStatus stringToJobStatus(const std::string& str) {
    if (str == "pending") return JobStatus::PENDING;
    if (str == "processing") return JobStatus::PROCESSING;
    if (str == "completed") return JobStatus::COMPLETED;
    if (str == "failed") return JobStatus::FAILED;
    if (str == "cancelled") return JobStatus::CANCELLED;
    return JobStatus::PENDING;
}

std::string jobPriorityToString(JobPriority priority) {
    switch (priority) {
        case JobPriority::LOW: return "low";
        case JobPriority::NORMAL: return "normal";
        case JobPriority::HIGH: return "high";
        case JobPriority::URGENT: return "urgent";
        default: return "normal";
    }
}

JobPriority stringToJobPriority(const std::string& str) {
    if (str == "low") return JobPriority::LOW;
    if (str == "normal") return JobPriority::NORMAL;
    if (str == "high") return JobPriority::HIGH;
    if (str == "urgent") return JobPriority::URGENT;
    return JobPriority::NORMAL;
}

} // namespace wtl::content::media
