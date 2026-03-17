/**
 * @file JobQueue.cc
 * @brief Implementation of JobQueue class
 * @see JobQueue.h for documentation
 */

#include "JobQueue.h"

// Standard library includes
#include <algorithm>
#include <random>

// Project includes
#include "core/debug/ErrorCapture.h"
#include "core/utils/Config.h"

namespace wtl::workers::jobs {

using namespace ::wtl::core::debug;

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

JobQueue& JobQueue::getInstance() {
    static JobQueue instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void JobQueue::initialize(const JobQueueConfig& config) {
    if (initialized_.load()) {
        log("JobQueue already initialized");
        return;
    }

    config_ = config;

    // Note: Redis integration would go here if config_.use_redis is true
    // For now, we use in-memory queue only

    initialized_.store(true);
    log("JobQueue initialized (in-memory mode)");
}

void JobQueue::initializeFromConfig() {
    JobQueueConfig config;

    try {
        auto& cfg = wtl::core::utils::Config::getInstance();
        config.redis_host = cfg.getString("redis.host", "localhost");
        config.redis_port = cfg.getInt("redis.port", 6379);
        config.redis_password = cfg.getString("redis.password", "");
        config.redis_db = cfg.getInt("redis.db", 0);
        config.queue_name = cfg.getString("job_queue.name", "wtl:job_queue");
        config.visibility_timeout = std::chrono::seconds(
            cfg.getInt("job_queue.visibility_timeout", 300));
        config.max_retries = cfg.getInt("job_queue.max_retries", 3);
        config.use_redis = cfg.getBool("job_queue.use_redis", false);
        config.max_queue_size = static_cast<size_t>(
            cfg.getInt("job_queue.max_queue_size", 10000));
    } catch (const std::exception& e) {
        log("Using default config: " + std::string(e.what()));
    }

    initialize(config);
}

bool JobQueue::isInitialized() const {
    return initialized_.load();
}

void JobQueue::shutdown() {
    if (!initialized_.load()) {
        return;
    }

    // Clear queues
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        pending_queue_.clear();
        processing_jobs_.clear();
        dead_letter_queue_.clear();
        job_ids_.clear();
    }

    initialized_.store(false);
    log("JobQueue shutdown");
}

// ============================================================================
// QUEUE OPERATIONS
// ============================================================================

std::string JobQueue::enqueue(const ScheduledJob& job) {
    return enqueueWithPriority(job, job.priority);
}

std::string JobQueue::enqueueWithPriority(const ScheduledJob& job, int priority) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    // Check queue size limit
    if (pending_queue_.size() >= config_.max_queue_size) {
        logError("Queue is full, cannot enqueue job");
        return "";
    }

    QueuedJob queued_job;
    queued_job.job = job;

    // Generate ID if not set
    if (queued_job.job.id.empty()) {
        queued_job.job.id = generateJobId();
    }

    // Check for duplicates
    if (job_ids_.find(queued_job.job.id) != job_ids_.end()) {
        logError("Job with ID " + queued_job.job.id + " already exists");
        return "";
    }

    queued_job.job.priority = priority;
    queued_job.queued_at = std::chrono::system_clock::now();
    queued_job.attempts = 0;

    // Insert in priority order
    auto insert_pos = std::find_if(pending_queue_.begin(), pending_queue_.end(),
        [priority](const QueuedJob& existing) {
            return existing.job.priority < priority;
        });

    pending_queue_.insert(insert_pos, queued_job);
    job_ids_.insert(queued_job.job.id);

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.total_enqueued++;
        stats_.current_pending++;
    }

    // Notify waiting workers
    job_cv_.notify_one();

    log("Enqueued job: " + queued_job.job.id + " (type: " + queued_job.job.job_type + ")");

    return queued_job.job.id;
}

std::optional<QueuedJob> JobQueue::dequeue(const std::string& worker_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (pending_queue_.empty()) {
        return std::nullopt;
    }

    QueuedJob job = pending_queue_.front();
    pending_queue_.pop_front();

    job.processing_since = std::chrono::system_clock::now();
    job.worker_id = worker_id;
    job.attempts++;

    processing_jobs_[job.job.id] = job;

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.total_dequeued++;
        stats_.current_pending--;
        stats_.current_processing++;
    }

    log("Dequeued job: " + job.job.id + " (worker: " + worker_id + ")");

    return job;
}

std::optional<QueuedJob> JobQueue::dequeueByType(const std::string& job_type,
                                                   const std::string& worker_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = std::find_if(pending_queue_.begin(), pending_queue_.end(),
        [&job_type](const QueuedJob& job) {
            return job.job.job_type == job_type;
        });

    if (it == pending_queue_.end()) {
        return std::nullopt;
    }

    QueuedJob job = *it;
    pending_queue_.erase(it);

    job.processing_since = std::chrono::system_clock::now();
    job.worker_id = worker_id;
    job.attempts++;

    processing_jobs_[job.job.id] = job;

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.total_dequeued++;
        stats_.current_pending--;
        stats_.current_processing++;
    }

    return job;
}

std::optional<QueuedJob> JobQueue::peek() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (pending_queue_.empty()) {
        return std::nullopt;
    }

    return pending_queue_.front();
}

bool JobQueue::markComplete(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = processing_jobs_.find(job_id);
    if (it == processing_jobs_.end()) {
        logError("Job not found in processing: " + job_id);
        return false;
    }

    processing_jobs_.erase(it);
    job_ids_.erase(job_id);

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.total_completed++;
        stats_.current_processing--;
    }

    log("Completed job: " + job_id);

    return true;
}

bool JobQueue::markFailed(const std::string& job_id, const std::string& error, bool retry_flag) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = processing_jobs_.find(job_id);
    if (it == processing_jobs_.end()) {
        logError("Job not found in processing: " + job_id);
        return false;
    }

    QueuedJob job = it->second;
    processing_jobs_.erase(it);

    job.job.last_error = error;
    job.job.status = JobStatus::FAILED;

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.total_failed++;
        stats_.current_processing--;
    }

    if (retry_flag && job.attempts < config_.max_retries) {
        // Requeue for retry
        job.processing_since = std::nullopt;
        job.worker_id.clear();
        pending_queue_.push_back(job);

        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.total_retried++;
        stats_.current_pending++;

        log("Job failed, requeued for retry: " + job_id + " (attempt " +
            std::to_string(job.attempts) + ")");
    } else {
        // Move to dead letter queue
        moveToDeadLetter(job);
        log("Job failed, moved to DLQ: " + job_id);
    }

    return true;
}

bool JobQueue::retry(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    // Check processing jobs first
    auto proc_it = processing_jobs_.find(job_id);
    if (proc_it != processing_jobs_.end()) {
        QueuedJob job = proc_it->second;
        processing_jobs_.erase(proc_it);

        job.processing_since = std::nullopt;
        job.worker_id.clear();
        pending_queue_.push_back(job);

        {
            std::lock_guard<std::mutex> slock(stats_mutex_);
            stats_.total_retried++;
            stats_.current_processing--;
            stats_.current_pending++;
        }

        return true;
    }

    // Check dead letter queue
    auto dlq_it = std::find_if(dead_letter_queue_.begin(), dead_letter_queue_.end(),
        [&job_id](const QueuedJob& job) {
            return job.job.id == job_id;
        });

    if (dlq_it != dead_letter_queue_.end()) {
        QueuedJob job = *dlq_it;
        dead_letter_queue_.erase(dlq_it);

        job.attempts = 0;  // Reset attempts
        job.processing_since = std::nullopt;
        job.worker_id.clear();
        job.job.status = JobStatus::PENDING;

        pending_queue_.push_back(job);

        {
            std::lock_guard<std::mutex> slock(stats_mutex_);
            stats_.total_retried++;
            stats_.current_dead_letter--;
            stats_.current_pending++;
        }

        log("Retried job from DLQ: " + job_id);
        return true;
    }

    return false;
}

bool JobQueue::extendTimeout(const std::string& job_id, std::chrono::seconds extension) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = processing_jobs_.find(job_id);
    if (it == processing_jobs_.end()) {
        return false;
    }

    // Extend the processing_since to effectively extend the timeout
    it->second.processing_since = std::chrono::system_clock::now();

    return true;
}

bool JobQueue::release(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = processing_jobs_.find(job_id);
    if (it == processing_jobs_.end()) {
        return false;
    }

    QueuedJob job = it->second;
    processing_jobs_.erase(it);

    job.processing_since = std::nullopt;
    job.worker_id.clear();

    // Put at front of queue
    pending_queue_.push_front(job);

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.current_processing--;
        stats_.current_pending++;
    }

    log("Released job: " + job_id);

    return true;
}

// ============================================================================
// QUEUE QUERIES
// ============================================================================

size_t JobQueue::size() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return pending_queue_.size();
}

bool JobQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return pending_queue_.empty();
}

std::vector<QueuedJob> JobQueue::getPendingJobs() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return std::vector<QueuedJob>(pending_queue_.begin(), pending_queue_.end());
}

std::vector<QueuedJob> JobQueue::getProcessingJobs() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    std::vector<QueuedJob> result;
    result.reserve(processing_jobs_.size());

    for (const auto& [id, job] : processing_jobs_) {
        result.push_back(job);
    }

    return result;
}

std::vector<QueuedJob> JobQueue::getDeadLetterJobs() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return std::vector<QueuedJob>(dead_letter_queue_.begin(), dead_letter_queue_.end());
}

std::optional<QueuedJob> JobQueue::getJob(const std::string& job_id) const {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    // Check pending queue
    for (const auto& job : pending_queue_) {
        if (job.job.id == job_id) {
            return job;
        }
    }

    // Check processing jobs
    auto proc_it = processing_jobs_.find(job_id);
    if (proc_it != processing_jobs_.end()) {
        return proc_it->second;
    }

    // Check dead letter queue
    for (const auto& job : dead_letter_queue_) {
        if (job.job.id == job_id) {
            return job;
        }
    }

    return std::nullopt;
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

void JobQueue::clear(bool include_processing) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    for (const auto& job : pending_queue_) {
        job_ids_.erase(job.job.id);
    }
    pending_queue_.clear();

    if (include_processing) {
        for (const auto& [id, job] : processing_jobs_) {
            job_ids_.erase(id);
        }
        processing_jobs_.clear();
    }

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.current_pending = 0;
        if (include_processing) {
            stats_.current_processing = 0;
        }
    }

    log("Queue cleared" + std::string(include_processing ? " (including processing)" : ""));
}

int JobQueue::clearDeadLetter() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    int count = static_cast<int>(dead_letter_queue_.size());

    for (const auto& job : dead_letter_queue_) {
        job_ids_.erase(job.job.id);
    }
    dead_letter_queue_.clear();

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.current_dead_letter = 0;
    }

    log("Cleared " + std::to_string(count) + " jobs from DLQ");

    return count;
}

int JobQueue::requeueDeadLetter() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    int count = 0;
    while (!dead_letter_queue_.empty()) {
        QueuedJob job = dead_letter_queue_.front();
        dead_letter_queue_.pop_front();

        job.attempts = 0;
        job.processing_since = std::nullopt;
        job.worker_id.clear();
        job.job.status = JobStatus::PENDING;
        job.job.last_error = std::nullopt;

        pending_queue_.push_back(job);
        count++;
    }

    {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.current_dead_letter = 0;
        stats_.current_pending += count;
    }

    log("Requeued " + std::to_string(count) + " jobs from DLQ");

    return count;
}

int JobQueue::recoverStaleJobs() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto now = std::chrono::system_clock::now();
    int recovered = 0;

    std::vector<std::string> stale_ids;

    for (const auto& [id, job] : processing_jobs_) {
        if (job.processing_since) {
            auto elapsed = now - *job.processing_since;
            if (elapsed > config_.visibility_timeout) {
                stale_ids.push_back(id);
            }
        }
    }

    for (const auto& id : stale_ids) {
        auto it = processing_jobs_.find(id);
        if (it != processing_jobs_.end()) {
            QueuedJob job = it->second;
            processing_jobs_.erase(it);

            job.processing_since = std::nullopt;
            job.worker_id.clear();

            pending_queue_.push_back(job);
            recovered++;
        }
    }

    if (recovered > 0) {
        std::lock_guard<std::mutex> slock(stats_mutex_);
        stats_.current_processing -= recovered;
        stats_.current_pending += recovered;

        log("Recovered " + std::to_string(recovered) + " stale jobs");
    }

    return recovered;
}

// ============================================================================
// STATISTICS
// ============================================================================

JobQueueStats JobQueue::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void JobQueue::resetStats() {
    std::lock_guard<std::mutex> slock(stats_mutex_);

    // Preserve current counts
    int pending = stats_.current_pending;
    int processing = stats_.current_processing;
    int dlq = stats_.current_dead_letter;

    stats_ = JobQueueStats{};
    stats_.current_pending = pending;
    stats_.current_processing = processing;
    stats_.current_dead_letter = dlq;
}

// ============================================================================
// WAITING
// ============================================================================

bool JobQueue::waitForJob(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    return job_cv_.wait_for(lock, timeout, [this]() {
        return !pending_queue_.empty();
    });
}

void JobQueue::notifyJobEnqueued() {
    job_cv_.notify_one();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::string JobQueue::generateJobId() const {
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
            uuid += '4';
        } else if (i == 19) {
            uuid += hex[(dis(gen) & 0x3) | 0x8];
        } else {
            uuid += hex[dis(gen)];
        }
    }

    return uuid;
}

void JobQueue::moveToDeadLetter(const QueuedJob& job) {
    dead_letter_queue_.push_back(job);

    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_dead_lettered++;
    stats_.current_dead_letter++;
}

void JobQueue::log(const std::string& message) {
    WTL_CAPTURE_INFO(
        ErrorCategory::BUSINESS_LOGIC,
        "[JobQueue] " + message,
        {{"component", "JobQueue"}}
    );
}

void JobQueue::logError(const std::string& message) {
    WTL_CAPTURE_ERROR(
        ErrorCategory::BUSINESS_LOGIC,
        "[JobQueue] " + message,
        {{"component", "JobQueue"}}
    );
}

} // namespace wtl::workers::jobs
