/**
 * @file JobProcessor.cc
 * @brief Implementation of JobProcessor class
 * @see JobProcessor.h for documentation
 */

#include "JobProcessor.h"

// Standard library includes
#include <algorithm>
#include <future>
#include <sstream>

// Project includes
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::workers::jobs {

using namespace ::wtl::core::debug;

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

JobProcessor& JobProcessor::getInstance() {
    static JobProcessor instance;
    return instance;
}

JobProcessor::~JobProcessor() {
    if (running_.load()) {
        stop(false);
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void JobProcessor::initialize(const JobProcessorConfig& config) {
    if (initialized_.load()) {
        log("JobProcessor already initialized");
        return;
    }

    config_ = config;
    initialized_.store(true);

    log("JobProcessor initialized with " + std::to_string(config_.num_workers) + " workers");

    if (config_.process_on_startup) {
        start();
    }
}

void JobProcessor::initializeFromConfig() {
    JobProcessorConfig config;

    try {
        auto& cfg = wtl::core::utils::Config::getInstance();
        config.num_workers = cfg.getInt("job_processor.num_workers", 4);
        config.poll_interval = std::chrono::milliseconds(
            cfg.getInt("job_processor.poll_interval_ms", 100));
        config.job_timeout = std::chrono::seconds(
            cfg.getInt("job_processor.job_timeout", 300));
        config.process_on_startup = cfg.getBool("job_processor.process_on_startup", true);
        config.max_jobs_per_worker = cfg.getInt("job_processor.max_jobs_per_worker", 1000);
    } catch (const std::exception& e) {
        log("Using default config: " + std::string(e.what()));
    }

    initialize(config);
}

bool JobProcessor::isInitialized() const {
    return initialized_.load();
}

void JobProcessor::start(int num_workers) {
    if (!initialized_.load()) {
        logError("Cannot start - processor not initialized");
        return;
    }

    if (running_.load()) {
        log("Processor already running");
        return;
    }

    int worker_count = num_workers > 0 ? num_workers : config_.num_workers;

    stop_requested_.store(false);
    running_.store(true);

    std::lock_guard<std::mutex> lock(workers_mutex_);
    workers_.clear();

    for (int i = 0; i < worker_count; ++i) {
        workers_.emplace_back(&JobProcessor::workerLoop, this, i);
    }

    log("Started " + std::to_string(worker_count) + " worker threads");
}

void JobProcessor::stop(bool wait_for_current) {
    if (!running_.load()) {
        return;
    }

    log("Stopping processor...");

    stop_requested_.store(true);

    // Wake up workers waiting on queue
    JobQueue::getInstance().notifyJobEnqueued();

    std::lock_guard<std::mutex> lock(workers_mutex_);

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            if (wait_for_current) {
                worker.join();
            } else {
                worker.detach();
            }
        }
    }

    workers_.clear();
    running_.store(false);

    log("Processor stopped");
}

bool JobProcessor::isRunning() const {
    return running_.load();
}

void JobProcessor::shutdown() {
    stop(true);
    initialized_.store(false);
}

// ============================================================================
// HANDLER REGISTRATION
// ============================================================================

void JobProcessor::registerHandler(const std::string& job_type, JobHandlerFunc handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[job_type] = std::move(handler);
    log("Registered handler for job type: " + job_type);
}

void JobProcessor::unregisterHandler(const std::string& job_type) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_.erase(job_type);
    log("Unregistered handler for job type: " + job_type);
}

bool JobProcessor::hasHandler(const std::string& job_type) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    return handlers_.find(job_type) != handlers_.end();
}

std::vector<std::string> JobProcessor::getHandlerTypes() const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    std::vector<std::string> types;
    types.reserve(handlers_.size());

    for (const auto& [type, _] : handlers_) {
        types.push_back(type);
    }

    return types;
}

// ============================================================================
// JOB PROCESSING
// ============================================================================

JobExecutionResult JobProcessor::processJob(const ScheduledJob& job) {
    auto start_time = std::chrono::steady_clock::now();

    // Find handler
    JobHandlerFunc handler;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        auto it = handlers_.find(job.job_type);
        if (it == handlers_.end()) {
            return JobExecutionResult::Failure(
                "No handler registered for job type: " + job.job_type, false);
        }
        handler = it->second;
    }

    // Execute handler
    JobExecutionResult result;
    try {
        result = executeWithTimeout(job);
    } catch (const std::exception& e) {
        result = JobExecutionResult::Failure(e.what(), true);
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    updateStats(result);

    return result;
}

JobExecutionResult JobProcessor::processJobById(const std::string& job_id) {
    auto& queue = JobQueue::getInstance();

    auto queued_job = queue.getJob(job_id);
    if (!queued_job) {
        return JobExecutionResult::Failure("Job not found: " + job_id, false);
    }

    return processJob(queued_job->job);
}

// ============================================================================
// WORKER MANAGEMENT
// ============================================================================

int JobProcessor::getActiveWorkerCount() const {
    return active_workers_.load();
}

void JobProcessor::scaleWorkers(int num_workers) {
    if (!running_.load()) {
        config_.num_workers = num_workers;
        return;
    }

    std::lock_guard<std::mutex> lock(workers_mutex_);

    int current = static_cast<int>(workers_.size());

    if (num_workers > current) {
        // Add workers
        for (int i = current; i < num_workers; ++i) {
            workers_.emplace_back(&JobProcessor::workerLoop, this, i);
        }
        log("Scaled up to " + std::to_string(num_workers) + " workers");
    } else if (num_workers < current) {
        // Note: Cannot easily remove workers; they will exit on their own when stop_requested
        // For now, just log and let natural attrition handle it
        log("Scale down requested to " + std::to_string(num_workers) + " workers");
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

JobProcessorStats JobProcessor::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    JobProcessorStats stats = stats_;
    stats.active_workers = active_workers_.load();
    return stats;
}

void JobProcessor::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = JobProcessorStats{};
}

// ============================================================================
// HEALTH
// ============================================================================

bool JobProcessor::isHealthy() const {
    if (!initialized_.load()) {
        return false;
    }

    // Check if workers are alive
    if (running_.load() && active_workers_.load() == 0) {
        return false;
    }

    return true;
}

Json::Value JobProcessor::getHealthDetails() const {
    Json::Value health;
    health["initialized"] = initialized_.load();
    health["running"] = running_.load();
    health["healthy"] = isHealthy();
    health["active_workers"] = active_workers_.load();
    health["configured_workers"] = config_.num_workers;
    health["stats"] = getStats().toJson();

    return health;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void JobProcessor::workerLoop(int worker_id) {
    std::string worker_name = generateWorkerId(worker_id);

    log("Worker " + worker_name + " started");

    active_workers_++;
    int jobs_processed = 0;

    auto& queue = JobQueue::getInstance();

    while (!stop_requested_.load()) {
        // Wait for a job
        bool has_job = queue.waitForJob(config_.poll_interval);

        if (stop_requested_.load()) {
            break;
        }

        if (!has_job) {
            continue;
        }

        // Dequeue a job
        auto queued_job = queue.dequeue(worker_name);
        if (!queued_job) {
            continue;
        }

        log("Worker " + worker_name + " processing job: " + queued_job->job.id);

        // Process the job
        auto result = processJob(queued_job->job);

        // Handle result
        if (result.success) {
            queue.markComplete(queued_job->job.id);
            log("Worker " + worker_name + " completed job: " + queued_job->job.id);
        } else {
            queue.markFailed(queued_job->job.id, result.message, result.should_retry);
            logError("Worker " + worker_name + " failed job: " + queued_job->job.id +
                    " - " + result.message);
        }

        jobs_processed++;

        // Check if worker should restart
        if (jobs_processed >= config_.max_jobs_per_worker) {
            log("Worker " + worker_name + " reached max jobs, restarting");
            break;
        }
    }

    active_workers_--;
    log("Worker " + worker_name + " stopped (processed " +
        std::to_string(jobs_processed) + " jobs)");
}

JobExecutionResult JobProcessor::executeWithTimeout(const ScheduledJob& job) {
    // Find handler
    JobHandlerFunc handler;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        auto it = handlers_.find(job.job_type);
        if (it == handlers_.end()) {
            return JobExecutionResult::Failure(
                "No handler registered for job type: " + job.job_type, false);
        }
        handler = it->second;
    }

    // Execute with timeout using async
    auto future = std::async(std::launch::async, [&handler, &job]() {
        return handler(job);
    });

    // Wait with timeout
    auto status = future.wait_for(config_.job_timeout);

    if (status == std::future_status::timeout) {
        // Job timed out
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.jobs_timed_out++;

        return JobExecutionResult::Failure("Job timed out after " +
            std::to_string(config_.job_timeout.count()) + " seconds", true);
    }

    try {
        return future.get();
    } catch (const std::exception& e) {
        return JobExecutionResult::Failure(e.what(), true);
    }
}

void JobProcessor::updateStats(const JobExecutionResult& result) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats_.jobs_processed++;
    stats_.total_processing_time += result.duration;

    if (result.success) {
        stats_.jobs_succeeded++;
    } else {
        stats_.jobs_failed++;
    }

    // Update average
    if (stats_.jobs_processed > 0) {
        stats_.avg_processing_time = std::chrono::milliseconds(
            stats_.total_processing_time.count() / stats_.jobs_processed);
    }
}

void JobProcessor::log(const std::string& message) {
    WTL_CAPTURE_INFO(
        ErrorCategory::BUSINESS_LOGIC,
        "[JobProcessor] " + message,
        {{"component", "JobProcessor"}}
    );
}

void JobProcessor::logError(const std::string& message) {
    WTL_CAPTURE_ERROR(
        ErrorCategory::BUSINESS_LOGIC,
        "[JobProcessor] " + message,
        {{"component", "JobProcessor"}}
    );
}

std::string JobProcessor::generateWorkerId(int index) const {
    std::ostringstream oss;
    oss << "worker-" << index;
    return oss.str();
}

} // namespace wtl::workers::jobs
