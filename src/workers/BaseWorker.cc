/**
 * @file BaseWorker.cc
 * @brief Implementation of BaseWorker class
 * @see BaseWorker.h for documentation
 */

#include "BaseWorker.h"

// Standard library includes
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>

// Project includes
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::workers {

using namespace ::wtl::core::debug;

// ============================================================================
// CONSTRUCTION
// ============================================================================

BaseWorker::BaseWorker(
    const std::string& name,
    const std::string& description,
    std::chrono::seconds interval,
    WorkerPriority priority)
    : name_(name)
    , description_(description)
    , interval_(interval)
    , priority_(priority)
{
    // Initialize stats
    stats_.last_run = std::chrono::system_clock::time_point{};
    stats_.next_run = std::chrono::system_clock::time_point{};
}

BaseWorker::~BaseWorker() {
    // Stop the run loop if it's running
    if (isRunning()) {
        stopRunLoop(true);
    }
}

// ============================================================================
// IWORKER INTERFACE
// ============================================================================

std::string BaseWorker::getName() const {
    return name_;
}

std::string BaseWorker::getDescription() const {
    return description_;
}

std::chrono::seconds BaseWorker::getInterval() const {
    return interval_;
}

WorkerPriority BaseWorker::getPriority() const {
    return priority_;
}

WorkerResult BaseWorker::execute() {
    // Check if enabled
    if (!enabled_.load()) {
        return WorkerResult::Failure("Worker is disabled");
    }

    // Check if paused
    if (paused_.load()) {
        return WorkerResult::Failure("Worker is paused");
    }

    auto start_time = std::chrono::steady_clock::now();
    WorkerResult result;
    result.success = false;
    int attempts = 0;
    std::string last_error;

    // Pre-execution hook
    try {
        beforeExecute();
    } catch (const std::exception& e) {
        logWarning("beforeExecute() threw exception: " + std::string(e.what()));
    }

    // Execute with retries
    while (attempts <= config_.max_retries) {
        try {
            result = doExecute();

            if (result.success) {
                // Success - update stats and break
                auto end_time = std::chrono::steady_clock::now();
                result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);

                updateStats(result);

                // Log successful execution
                if (config_.log_executions) {
                    std::ostringstream oss;
                    oss << "Worker '" << name_ << "' executed successfully. "
                        << "Processed: " << result.items_processed
                        << ", Time: " << result.execution_time.count() << "ms";
                    logInfo(oss.str());
                }

                break;
            } else {
                // Execution returned failure
                last_error = result.message;

                if (!shouldRetry(attempts, std::runtime_error(result.message))) {
                    break;
                }

                attempts++;
                if (attempts <= config_.max_retries) {
                    auto delay = calculateRetryDelay(attempts);
                    std::this_thread::sleep_for(delay);
                }
            }
        } catch (const std::exception& e) {
            last_error = e.what();

            // Call error handler
            onError(e);

            // Check if we should retry
            if (!shouldRetry(attempts, e)) {
                result = WorkerResult::Failure(last_error);
                break;
            }

            attempts++;
            if (attempts <= config_.max_retries) {
                auto delay = calculateRetryDelay(attempts);
                logWarning("Retry " + std::to_string(attempts) + "/" +
                          std::to_string(config_.max_retries) +
                          " after " + std::to_string(delay.count()) + "ms");
                std::this_thread::sleep_for(delay);
            }
        }
    }

    // If all retries failed
    if (!result.success) {
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        result.message = last_error;
        result.errors.push_back(last_error);

        updateStats(result);

        if (config_.capture_errors) {
            logError("Worker execution failed: " + last_error,
                    {{"attempts", std::to_string(attempts + 1)}});
        }

        // Check for too many consecutive failures
        if (stats_.consecutive_failures >= config_.max_consecutive_failures) {
            logError("Worker stopped due to " +
                    std::to_string(stats_.consecutive_failures) +
                    " consecutive failures", {});
            status_.store(WorkerStatus::ERROR);
        }
    }

    // Store last result
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        last_result_ = result;
    }

    // Post-execution hook
    try {
        afterExecute(result);
    } catch (const std::exception& e) {
        logWarning("afterExecute() threw exception: " + std::string(e.what()));
    }

    return result;
}

void BaseWorker::onError(const std::exception& e) {
    if (config_.capture_errors) {
        logError("Worker execution error: " + std::string(e.what()), {});
    }
}

bool BaseWorker::isHealthy() const {
    // Worker is healthy if:
    // 1. Not in ERROR status
    // 2. Consecutive failures below threshold
    // 3. Enabled

    if (status_.load() == WorkerStatus::ERROR) {
        return false;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (stats_.consecutive_failures >= config_.max_consecutive_failures) {
        return false;
    }

    return true;
}

Json::Value BaseWorker::getHealthDetails() const {
    Json::Value details;
    details["name"] = name_;
    details["healthy"] = isHealthy();
    details["status"] = workerStatusToString(status_.load());
    details["enabled"] = enabled_.load();
    details["paused"] = paused_.load();

    std::lock_guard<std::mutex> lock(stats_mutex_);
    details["consecutive_failures"] = stats_.consecutive_failures;
    details["max_consecutive_failures"] = config_.max_consecutive_failures;

    if (!stats_.last_error.empty()) {
        details["last_error"] = stats_.last_error;
    }

    return details;
}

void BaseWorker::onStart() {
    logInfo("Worker '" + name_ + "' starting");
    status_.store(WorkerStatus::STARTING);
}

void BaseWorker::onStop() {
    logInfo("Worker '" + name_ + "' stopping");
    status_.store(WorkerStatus::STOPPING);
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

WorkerStatus BaseWorker::getStatus() const {
    return status_.load();
}

WorkerStats BaseWorker::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

WorkerResult BaseWorker::getLastResult() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return last_result_;
}

void BaseWorker::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = WorkerStats{};
    stats_.next_run = now() + interval_;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void BaseWorker::setConfig(const BaseWorkerConfig& config) {
    config_ = config;
}

BaseWorkerConfig BaseWorker::getConfig() const {
    return config_;
}

void BaseWorker::setInterval(std::chrono::seconds interval) {
    interval_ = interval;
    scheduleNextRun();
}

void BaseWorker::setEnabled(bool enabled) {
    enabled_.store(enabled);
    if (enabled) {
        logInfo("Worker '" + name_ + "' enabled");
    } else {
        logInfo("Worker '" + name_ + "' disabled");
    }
}

bool BaseWorker::isEnabled() const {
    return enabled_.load();
}

// ============================================================================
// RUN LOOP CONTROL
// ============================================================================

void BaseWorker::startRunLoop() {
    if (isRunning()) {
        logWarning("Run loop already running for worker '" + name_ + "'");
        return;
    }

    stop_requested_.store(false);
    onStart();

    run_thread_ = std::make_unique<std::thread>(&BaseWorker::runLoop, this);

    status_.store(WorkerStatus::RUNNING);
    scheduleNextRun();
}

void BaseWorker::stopRunLoop(bool wait) {
    if (!isRunning()) {
        return;
    }

    onStop();

    stop_requested_.store(true);

    // Wake up the run loop if it's waiting
    {
        std::lock_guard<std::mutex> lock(run_mutex_);
        run_cv_.notify_all();
    }

    if (wait && run_thread_ && run_thread_->joinable()) {
        run_thread_->join();
    }

    run_thread_.reset();
    status_.store(WorkerStatus::STOPPED);
}

bool BaseWorker::isRunning() const {
    return run_thread_ != nullptr && run_thread_->joinable();
}

void BaseWorker::pause() {
    if (!paused_.load()) {
        paused_.store(true);
        onPause();
        status_.store(WorkerStatus::PAUSED);
        logInfo("Worker '" + name_ + "' paused");
    }
}

void BaseWorker::resume() {
    if (paused_.load()) {
        paused_.store(false);
        onResume();
        status_.store(WorkerStatus::RUNNING);

        // Wake up the run loop
        {
            std::lock_guard<std::mutex> lock(run_mutex_);
            run_cv_.notify_all();
        }

        logInfo("Worker '" + name_ + "' resumed");
    }
}

bool BaseWorker::isPaused() const {
    return paused_.load();
}

void BaseWorker::requestImmediateExecution() {
    immediate_execution_requested_.store(true);

    // Wake up the run loop
    {
        std::lock_guard<std::mutex> lock(run_mutex_);
        run_cv_.notify_all();
    }
}

// ============================================================================
// PROTECTED METHODS
// ============================================================================

bool BaseWorker::shouldRetry(int attempt, const std::exception& e) {
    (void)e;  // Default implementation doesn't use the exception

    // Don't retry if max retries reached
    if (attempt >= config_.max_retries) {
        return false;
    }

    return true;
}

void BaseWorker::logInfo(const std::string& message) {
    WTL_CAPTURE_INFO(
        ErrorCategory::BUSINESS_LOGIC,
        "[Worker:" + name_ + "] " + message,
        {{"worker", name_}}
    );
}

void BaseWorker::logWarning(const std::string& message) {
    WTL_CAPTURE_WARNING(
        ErrorCategory::BUSINESS_LOGIC,
        "[Worker:" + name_ + "] " + message,
        {{"worker", name_}}
    );
}

void BaseWorker::logError(const std::string& message,
                          const std::unordered_map<std::string, std::string>& metadata) {
    auto meta = metadata;
    meta["worker"] = name_;

    WTL_CAPTURE_ERROR(
        ErrorCategory::BUSINESS_LOGIC,
        "[Worker:" + name_ + "] " + message,
        meta
    );
}

void BaseWorker::scheduleNextRun() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.next_run = now() + interval_;
}

std::chrono::system_clock::time_point BaseWorker::now() const {
    return std::chrono::system_clock::now();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void BaseWorker::runLoop() {
    logInfo("Run loop started");

    while (!stop_requested_.load()) {
        // Check if paused
        if (paused_.load()) {
            std::unique_lock<std::mutex> lock(run_mutex_);
            run_cv_.wait(lock, [this]() {
                return stop_requested_.load() || !paused_.load();
            });
            continue;
        }

        // Check if immediate execution requested
        bool immediate = immediate_execution_requested_.exchange(false);

        if (!immediate) {
            // Wait for the next scheduled run
            std::unique_lock<std::mutex> lock(run_mutex_);

            // Wait for the next scheduled interval, or until notified
            run_cv_.wait_for(lock, interval_, [this]() {
                return stop_requested_.load() ||
                       immediate_execution_requested_.load() ||
                       !paused_.load();
            });

            // Check if we should stop
            if (stop_requested_.load()) {
                break;
            }

            // If we woke up due to immediate execution request, handle it
            if (immediate_execution_requested_.load()) {
                immediate_execution_requested_.store(false);
            }
        }

        // Check if we should run now
        if (!shouldRunNow()) {
            continue;
        }

        // Execute the worker
        if (enabled_.load() && !paused_.load()) {
            try {
                execute();
            } catch (const std::exception& e) {
                logError("Unhandled exception in run loop: " + std::string(e.what()), {});
            }

            scheduleNextRun();
        }
    }

    logInfo("Run loop stopped");
}

void BaseWorker::updateStats(const WorkerResult& result) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats_.total_executions++;
    stats_.last_run = now();

    if (result.success) {
        stats_.successful_executions++;
        stats_.consecutive_failures = 0;
    } else {
        stats_.failed_executions++;
        stats_.consecutive_failures++;
        stats_.last_error = result.message;
    }

    stats_.total_items_processed += result.items_processed;
    stats_.total_items_failed += result.items_failed;

    stats_.total_execution_time += result.execution_time;

    // Update average
    if (stats_.total_executions > 0) {
        stats_.avg_execution_time = std::chrono::milliseconds(
            stats_.total_execution_time.count() / stats_.total_executions);
    }

    // Update max
    if (result.execution_time > stats_.max_execution_time) {
        stats_.max_execution_time = result.execution_time;
    }
}

std::chrono::milliseconds BaseWorker::calculateRetryDelay(int attempt) const {
    // Calculate base delay with exponential backoff
    auto base = config_.retry_delay.count();
    auto delay = static_cast<long long>(
        base * std::pow(config_.retry_backoff_multiplier, attempt));

    // Cap at max delay
    delay = std::min(delay, static_cast<long long>(config_.max_retry_delay.count()));

    // Add jitter (10% of delay)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<long long> jitter_dist(0, delay / 10);
    delay += jitter_dist(gen);

    return std::chrono::milliseconds(delay);
}

} // namespace wtl::workers
