/**
 * @file Scheduler.cc
 * @brief Implementation of Scheduler class
 * @see Scheduler.h for documentation
 */

#include "Scheduler.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>

// Project includes
#include "CronParser.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/utils/Config.h"

namespace wtl::workers::scheduler {

using namespace ::wtl::core::debug;
using namespace ::wtl::core::db;

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

Scheduler& Scheduler::getInstance() {
    static Scheduler instance;
    return instance;
}

Scheduler::~Scheduler() {
    if (running_.load()) {
        stop(false);
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void Scheduler::initialize(const SchedulerConfig& config) {
    if (initialized_.load()) {
        log("Scheduler already initialized");
        return;
    }

    config_ = config;

    // Load jobs from database if configured
    if (config_.load_on_startup && config_.persist_to_database) {
        try {
            loadJobs();
        } catch (const std::exception& e) {
            logError("Failed to load jobs from database: " + std::string(e.what()));
        }
    }

    initialized_.store(true);
    log("Scheduler initialized");
}

void Scheduler::initializeFromConfig() {
    SchedulerConfig config;

    try {
        auto& cfg = wtl::core::utils::Config::getInstance();
        config.poll_interval = std::chrono::seconds(
            cfg.getInt("scheduler.poll_interval", 1));
        config.max_concurrent_jobs = cfg.getInt("scheduler.max_concurrent_jobs", 10);
        config.persist_to_database = cfg.getBool("scheduler.persist_to_database", true);
        config.load_on_startup = cfg.getBool("scheduler.load_on_startup", true);
        config.job_timeout = std::chrono::seconds(
            cfg.getInt("scheduler.job_timeout", 300));
        config.max_retries = cfg.getInt("scheduler.max_retries", 3);
        config.retry_delay = std::chrono::seconds(
            cfg.getInt("scheduler.retry_delay", 60));
    } catch (const std::exception& e) {
        log("Using default config: " + std::string(e.what()));
    }

    initialize(config);
}

bool Scheduler::isInitialized() const {
    return initialized_.load();
}

void Scheduler::start() {
    if (!initialized_.load()) {
        logError("Cannot start - scheduler not initialized");
        return;
    }

    if (running_.load()) {
        log("Scheduler already running");
        return;
    }

    stop_requested_.store(false);
    running_.store(true);

    process_thread_ = std::make_unique<std::thread>(&Scheduler::runLoop, this);

    log("Scheduler started");
}

void Scheduler::stop(bool wait_for_running) {
    if (!running_.load()) {
        return;
    }

    log("Stopping scheduler...");

    stop_requested_.store(true);

    // Wake up the processing thread
    {
        std::lock_guard<std::mutex> lock(process_mutex_);
        process_cv_.notify_all();
    }

    if (process_thread_ && process_thread_->joinable()) {
        if (wait_for_running) {
            process_thread_->join();
        } else {
            process_thread_->detach();
        }
    }

    process_thread_.reset();
    running_.store(false);

    // Persist jobs if configured
    if (config_.persist_to_database) {
        try {
            persistJobs();
        } catch (const std::exception& e) {
            logError("Failed to persist jobs on shutdown: " + std::string(e.what()));
        }
    }

    log("Scheduler stopped");
}

bool Scheduler::isRunning() const {
    return running_.load();
}

void Scheduler::shutdown() {
    stop(true);
    initialized_.store(false);
}

// ============================================================================
// JOB SCHEDULING
// ============================================================================

std::string Scheduler::schedule(ScheduledJob job,
                                 std::chrono::system_clock::time_point time) {
    if (job.id.empty()) {
        job.id = generateJobId();
    }

    job.scheduled_at = time;
    job.next_run = time;
    job.status = JobStatus::PENDING;
    job.recurrence = RecurrenceType::NONE;
    job.created_at = std::chrono::system_clock::now();
    job.updated_at = job.created_at;

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        jobs_[job.id] = job;
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_jobs_scheduled++;
        stats_.current_pending++;
    }

    if (config_.persist_to_database) {
        saveJob(job);
    }

    log("Scheduled job '" + job.id + "' (" + job.job_type + ")");

    return job.id;
}

std::string Scheduler::scheduleDelayed(ScheduledJob job,
                                        std::chrono::seconds delay) {
    auto execute_time = std::chrono::system_clock::now() + delay;
    return schedule(std::move(job), execute_time);
}

std::string Scheduler::scheduleRecurring(ScheduledJob job,
                                          const std::string& cron_expression) {
    // Validate cron expression
    CronParser parser;
    if (!parser.parse(cron_expression)) {
        logError("Invalid cron expression: " + cron_expression +
                 " - " + parser.getError());
        return "";
    }

    // Get next run time
    auto next_run = parser.getNextRunTime();
    if (!next_run) {
        logError("Could not calculate next run time for: " + cron_expression);
        return "";
    }

    if (job.id.empty()) {
        job.id = generateJobId();
    }

    job.scheduled_at = *next_run;
    job.next_run = *next_run;
    job.cron_expression = cron_expression;
    job.recurrence = RecurrenceType::CRON;
    job.status = JobStatus::PENDING;
    job.created_at = std::chrono::system_clock::now();
    job.updated_at = job.created_at;

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        jobs_[job.id] = job;
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_jobs_scheduled++;
        stats_.current_pending++;
    }

    if (config_.persist_to_database) {
        saveJob(job);
    }

    log("Scheduled recurring job '" + job.id + "' with cron: " + cron_expression);

    return job.id;
}

std::string Scheduler::scheduleInterval(ScheduledJob job,
                                         std::chrono::seconds interval) {
    if (job.id.empty()) {
        job.id = generateJobId();
    }

    auto now = std::chrono::system_clock::now();
    job.scheduled_at = now + interval;
    job.next_run = job.scheduled_at;
    job.interval = interval;
    job.recurrence = RecurrenceType::INTERVAL;
    job.status = JobStatus::PENDING;
    job.created_at = now;
    job.updated_at = now;

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        jobs_[job.id] = job;
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_jobs_scheduled++;
        stats_.current_pending++;
    }

    if (config_.persist_to_database) {
        saveJob(job);
    }

    log("Scheduled interval job '" + job.id + "' every " +
        std::to_string(interval.count()) + " seconds");

    return job.id;
}

bool Scheduler::cancel(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    auto it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        return false;
    }

    if (it->second.status == JobStatus::RUNNING) {
        logError("Cannot cancel running job: " + job_id);
        return false;
    }

    it->second.status = JobStatus::CANCELLED;
    it->second.updated_at = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.cancelled_jobs++;
        stats_.current_pending--;
    }

    if (config_.persist_to_database) {
        updateJob(it->second);
    }

    log("Cancelled job: " + job_id);

    return true;
}

int Scheduler::cancelByType(const std::string& job_type) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    int count = 0;
    for (auto& [id, job] : jobs_) {
        if (job.job_type == job_type &&
            job.status != JobStatus::RUNNING &&
            job.status != JobStatus::CANCELLED &&
            job.status != JobStatus::COMPLETED) {

            job.status = JobStatus::CANCELLED;
            job.updated_at = std::chrono::system_clock::now();
            count++;

            if (config_.persist_to_database) {
                updateJob(job);
            }
        }
    }

    if (count > 0) {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.cancelled_jobs += count;
        stats_.current_pending -= count;
    }

    log("Cancelled " + std::to_string(count) + " jobs of type: " + job_type);

    return count;
}

int Scheduler::cancelByEntity(const std::string& entity_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    int count = 0;
    for (auto& [id, job] : jobs_) {
        if (job.entity_id == entity_id &&
            job.status != JobStatus::RUNNING &&
            job.status != JobStatus::CANCELLED &&
            job.status != JobStatus::COMPLETED) {

            job.status = JobStatus::CANCELLED;
            job.updated_at = std::chrono::system_clock::now();
            count++;

            if (config_.persist_to_database) {
                updateJob(job);
            }
        }
    }

    if (count > 0) {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.cancelled_jobs += count;
        stats_.current_pending -= count;
    }

    log("Cancelled " + std::to_string(count) + " jobs for entity: " + entity_id);

    return count;
}

bool Scheduler::reschedule(const std::string& job_id,
                            std::chrono::system_clock::time_point new_time) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    auto it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        return false;
    }

    if (it->second.status == JobStatus::RUNNING) {
        logError("Cannot reschedule running job: " + job_id);
        return false;
    }

    it->second.scheduled_at = new_time;
    it->second.next_run = new_time;
    it->second.status = JobStatus::PENDING;
    it->second.updated_at = std::chrono::system_clock::now();

    if (config_.persist_to_database) {
        updateJob(it->second);
    }

    log("Rescheduled job: " + job_id);

    return true;
}

// ============================================================================
// JOB QUERIES
// ============================================================================

std::optional<ScheduledJob> Scheduler::getJob(const std::string& job_id) const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    auto it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<ScheduledJob> Scheduler::getScheduledJobs(bool include_completed) const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    std::vector<ScheduledJob> result;
    for (const auto& [id, job] : jobs_) {
        if (include_completed || (job.status != JobStatus::COMPLETED &&
                                   job.status != JobStatus::CANCELLED)) {
            result.push_back(job);
        }
    }

    // Sort by scheduled time
    std::sort(result.begin(), result.end(),
              [](const ScheduledJob& a, const ScheduledJob& b) {
                  return a.scheduled_at < b.scheduled_at;
              });

    return result;
}

std::vector<ScheduledJob> Scheduler::getPendingJobs() const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    std::vector<ScheduledJob> result;
    for (const auto& [id, job] : jobs_) {
        if (job.status == JobStatus::PENDING || job.status == JobStatus::RETRYING) {
            result.push_back(job);
        }
    }

    // Sort by priority (descending) then scheduled time
    std::sort(result.begin(), result.end(),
              [](const ScheduledJob& a, const ScheduledJob& b) {
                  if (a.priority != b.priority) return a.priority > b.priority;
                  return a.scheduled_at < b.scheduled_at;
              });

    return result;
}

std::vector<ScheduledJob> Scheduler::getJobsByType(const std::string& job_type) const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    std::vector<ScheduledJob> result;
    for (const auto& [id, job] : jobs_) {
        if (job.job_type == job_type) {
            result.push_back(job);
        }
    }

    return result;
}

std::vector<ScheduledJob> Scheduler::getJobsByEntity(const std::string& entity_id) const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    std::vector<ScheduledJob> result;
    for (const auto& [id, job] : jobs_) {
        if (job.entity_id == entity_id) {
            result.push_back(job);
        }
    }

    return result;
}

std::vector<ScheduledJob> Scheduler::getDueJobs() const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    std::vector<ScheduledJob> result;

    for (const auto& [id, job] : jobs_) {
        if ((job.status == JobStatus::PENDING || job.status == JobStatus::RETRYING) &&
            job.isDue()) {
            result.push_back(job);
        }
    }

    // Sort by priority then scheduled time
    std::sort(result.begin(), result.end(),
              [](const ScheduledJob& a, const ScheduledJob& b) {
                  if (a.priority != b.priority) return a.priority > b.priority;
                  return a.scheduled_at < b.scheduled_at;
              });

    return result;
}

int Scheduler::getJobCount(JobStatus status) const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    int count = 0;
    for (const auto& [id, job] : jobs_) {
        if (job.status == status) {
            count++;
        }
    }

    return count;
}

// ============================================================================
// JOB HANDLERS
// ============================================================================

void Scheduler::registerHandler(const std::string& job_type, JobHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[job_type] = std::move(handler);
    log("Registered handler for job type: " + job_type);
}

void Scheduler::unregisterHandler(const std::string& job_type) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_.erase(job_type);
}

bool Scheduler::hasHandler(const std::string& job_type) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    return handlers_.find(job_type) != handlers_.end();
}

// ============================================================================
// PROCESSING
// ============================================================================

void Scheduler::processScheduledJobs() {
    auto due_jobs = getDueJobs();

    if (due_jobs.empty()) {
        return;
    }

    // Check concurrent limit
    int current_running;
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        current_running = stats_.current_running;
    }

    int available = config_.max_concurrent_jobs - current_running;
    if (available <= 0) {
        return;
    }

    // Execute due jobs (up to available slots)
    int executed = 0;
    for (auto& job : due_jobs) {
        if (executed >= available) break;

        // Check if expired
        if (job.isExpired()) {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            auto it = jobs_.find(job.id);
            if (it != jobs_.end()) {
                it->second.status = JobStatus::EXPIRED;
                it->second.updated_at = std::chrono::system_clock::now();

                std::lock_guard<std::mutex> slock(stats_mutex_);
                stats_.expired_jobs++;
                stats_.current_pending--;

                if (config_.persist_to_database) {
                    updateJob(it->second);
                }
            }
            continue;
        }

        // Execute job
        auto result = executeJob(job);

        // Handle result
        if (result.success) {
            handleJobComplete(job, result);
        } else {
            handleJobFailure(job, result);
        }

        executed++;
    }
}

JobExecutionResult Scheduler::executeJobNow(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    auto it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        return JobExecutionResult::Failure("Job not found: " + job_id, false);
    }

    auto result = executeJob(it->second);

    if (result.success) {
        handleJobComplete(it->second, result);
    } else {
        handleJobFailure(it->second, result);
    }

    return result;
}

// ============================================================================
// STATISTICS
// ============================================================================

SchedulerStats Scheduler::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void Scheduler::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = SchedulerStats{};

    // Recalculate current counts
    std::lock_guard<std::mutex> jlock(jobs_mutex_);
    for (const auto& [id, job] : jobs_) {
        if (job.status == JobStatus::PENDING || job.status == JobStatus::RETRYING) {
            stats_.current_pending++;
        } else if (job.status == JobStatus::RUNNING) {
            stats_.current_running++;
        }
    }
}

// ============================================================================
// PERSISTENCE
// ============================================================================

void Scheduler::persistJobs() {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    for (const auto& [id, job] : jobs_) {
        saveJob(job);
    }

    log("Persisted " + std::to_string(jobs_.size()) + " jobs");
}

void Scheduler::loadJobs() {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT * FROM scheduled_jobs WHERE status NOT IN ('completed', 'cancelled', 'expired')");

        std::lock_guard<std::mutex> lock(jobs_mutex_);
        jobs_.clear();

        for (const auto& row : result) {
            auto job = ScheduledJob::fromDbRow(row);
            jobs_[job.id] = job;
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        log("Loaded " + std::to_string(jobs_.size()) + " jobs from database");

    } catch (const std::exception& e) {
        logError("Failed to load jobs: " + std::string(e.what()));
    }
}

void Scheduler::clearAllJobs(bool include_database) {
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        jobs_.clear();
    }

    if (include_database) {
        try {
            auto conn = ConnectionPool::getInstance().acquire();
            pqxx::work txn(*conn);

            txn.exec("DELETE FROM scheduled_jobs");

            txn.commit();
            ConnectionPool::getInstance().release(conn);

        } catch (const std::exception& e) {
            logError("Failed to clear jobs from database: " + std::string(e.what()));
        }
    }

    resetStats();
    log("Cleared all jobs");
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void Scheduler::runLoop() {
    log("Scheduler processing loop started");

    while (!stop_requested_.load()) {
        try {
            processScheduledJobs();
        } catch (const std::exception& e) {
            logError("Error in processing loop: " + std::string(e.what()));
        }

        // Wait for next poll interval or stop signal
        std::unique_lock<std::mutex> lock(process_mutex_);
        process_cv_.wait_for(lock, config_.poll_interval, [this]() {
            return stop_requested_.load();
        });
    }

    log("Scheduler processing loop stopped");
}

JobExecutionResult Scheduler::executeJob(ScheduledJob& job) {
    auto start_time = std::chrono::steady_clock::now();

    // Mark as running
    job.status = JobStatus::RUNNING;
    job.updated_at = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.current_running++;
        stats_.current_pending--;
    }

    // Find handler
    JobHandler handler;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        auto it = handlers_.find(job.job_type);
        if (it == handlers_.end()) {
            // No handler registered
            job.status = JobStatus::FAILED;
            return JobExecutionResult::Failure(
                "No handler registered for job type: " + job.job_type, false);
        }
        handler = it->second;
    }

    // Execute handler
    JobExecutionResult result;
    try {
        result = handler(job);
    } catch (const std::exception& e) {
        result = JobExecutionResult::Failure(e.what(), true);
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    job.last_run = std::chrono::system_clock::now();
    job.last_duration = result.duration;

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.current_running--;
        stats_.total_jobs_executed++;
        stats_.last_run = std::chrono::system_clock::now();
    }

    return result;
}

void Scheduler::handleJobComplete(ScheduledJob& job, const JobExecutionResult& result) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    auto it = jobs_.find(job.id);
    if (it == jobs_.end()) return;

    it->second.last_run = std::chrono::system_clock::now();
    it->second.last_duration = result.duration;
    it->second.last_result = Json::writeString(Json::StreamWriterBuilder(), result.output);

    // Handle recurring jobs
    if (it->second.isRecurring()) {
        // Schedule next run
        if (it->second.recurrence == RecurrenceType::CRON && it->second.cron_expression) {
            CronParser parser(*it->second.cron_expression);
            auto next = parser.getNextRunTime();
            if (next) {
                it->second.next_run = *next;
                it->second.scheduled_at = *next;
            }
        } else if (it->second.recurrence == RecurrenceType::INTERVAL && it->second.interval) {
            it->second.next_run = std::chrono::system_clock::now() + *it->second.interval;
            it->second.scheduled_at = *it->second.next_run;
        } else {
            auto next = it->second.calculateNextRun();
            if (next) {
                it->second.next_run = *next;
                it->second.scheduled_at = *next;
            }
        }

        it->second.status = JobStatus::PENDING;
        it->second.retry_count = 0;

        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.successful_jobs++;
        stats_.current_pending++;
    } else {
        it->second.status = JobStatus::COMPLETED;

        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.successful_jobs++;
    }

    it->second.updated_at = std::chrono::system_clock::now();

    if (config_.persist_to_database) {
        updateJob(it->second);
    }
}

void Scheduler::handleJobFailure(ScheduledJob& job, const JobExecutionResult& result) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);

    auto it = jobs_.find(job.id);
    if (it == jobs_.end()) return;

    it->second.last_run = std::chrono::system_clock::now();
    it->second.last_duration = result.duration;
    it->second.last_error = result.error;
    it->second.retry_count++;

    // Check if should retry
    if (result.should_retry && it->second.canRetry()) {
        it->second.status = JobStatus::RETRYING;
        it->second.next_run = std::chrono::system_clock::now() + config_.retry_delay;

        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.retried_jobs++;
        stats_.current_pending++;
    } else {
        it->second.status = JobStatus::FAILED;

        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.failed_jobs++;
    }

    it->second.updated_at = std::chrono::system_clock::now();

    if (config_.persist_to_database) {
        updateJob(it->second);
    }

    logError("Job failed: " + job.id + " - " + result.message);
}

std::string Scheduler::generateJobId() const {
    // Generate UUID-like ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    const char* hex = "0123456789abcdef";
    std::string uuid;
    uuid.reserve(36);

    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else if (i == 14) {
            uuid += '4';  // Version 4
        } else if (i == 19) {
            uuid += hex[(dis(gen) & 0x3) | 0x8];  // Variant
        } else {
            uuid += hex[dis(gen)];
        }
    }

    return uuid;
}

void Scheduler::saveJob(const ScheduledJob& job) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Convert data to JSON string
        Json::StreamWriterBuilder writer;
        std::string data_str = Json::writeString(writer, job.data);

        // Convert tags to PostgreSQL array format
        std::string tags_str = "{";
        for (size_t i = 0; i < job.tags.size(); ++i) {
            if (i > 0) tags_str += ",";
            tags_str += "\"" + job.tags[i] + "\"";
        }
        tags_str += "}";

        auto tp_to_str = [](const std::chrono::system_clock::time_point& tp) -> std::string {
            auto time_t_val = std::chrono::system_clock::to_time_t(tp);
            std::tm tm_val{};
            gmtime_r(&time_t_val, &tm_val);
            std::ostringstream oss;
            oss << std::put_time(&tm_val, "%Y-%m-%d %H:%M:%S");
            return oss.str();
        };

        txn.exec_params(
            R"(INSERT INTO scheduled_jobs (
                id, job_type, entity_id, name, description,
                scheduled_at, cron_expression, recurrence, interval_seconds, expires_at,
                status, priority, retry_count, max_retries,
                last_run, next_run, last_duration_ms, last_error, last_result,
                data, callback_url, callback_token,
                created_by, created_at, updated_at, tags
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22, $23, $24, $25, $26)
            ON CONFLICT (id) DO UPDATE SET
                status = EXCLUDED.status,
                scheduled_at = EXCLUDED.scheduled_at,
                next_run = EXCLUDED.next_run,
                last_run = EXCLUDED.last_run,
                last_duration_ms = EXCLUDED.last_duration_ms,
                last_error = EXCLUDED.last_error,
                last_result = EXCLUDED.last_result,
                retry_count = EXCLUDED.retry_count,
                updated_at = EXCLUDED.updated_at)",
            job.id,
            job.job_type,
            job.entity_id,
            job.name,
            job.description,
            tp_to_str(job.scheduled_at),
            job.cron_expression.value_or(""),
            recurrenceTypeToString(job.recurrence),
            job.interval ? static_cast<int64_t>(job.interval->count()) : 0,
            tp_to_str(job.expires_at.value_or(std::chrono::system_clock::time_point{})),
            jobStatusToString(job.status),
            job.priority,
            job.retry_count,
            job.max_retries,
            tp_to_str(job.last_run.value_or(std::chrono::system_clock::time_point{})),
            tp_to_str(job.next_run.value_or(std::chrono::system_clock::time_point{})),
            job.last_duration ? static_cast<int64_t>(job.last_duration->count()) : 0,
            job.last_error.value_or(""),
            job.last_result.value_or(""),
            data_str,
            job.callback_url.value_or(""),
            job.callback_token.value_or(""),
            job.created_by,
            tp_to_str(job.created_at),
            tp_to_str(job.updated_at),
            tags_str
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logError("Failed to save job: " + std::string(e.what()));
    }
}

void Scheduler::deleteJob(const std::string& job_id) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params("DELETE FROM scheduled_jobs WHERE id = $1", job_id);

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logError("Failed to delete job: " + std::string(e.what()));
    }
}

void Scheduler::updateJob(const ScheduledJob& job) {
    saveJob(job);  // Uses upsert
}

void Scheduler::log(const std::string& message) {
    WTL_CAPTURE_INFO(
        ErrorCategory::BUSINESS_LOGIC,
        "[Scheduler] " + message,
        {{"component", "Scheduler"}}
    );
}

void Scheduler::logError(const std::string& message) {
    WTL_CAPTURE_ERROR(
        ErrorCategory::BUSINESS_LOGIC,
        "[Scheduler] " + message,
        {{"component", "Scheduler"}}
    );
}

} // namespace wtl::workers::scheduler
