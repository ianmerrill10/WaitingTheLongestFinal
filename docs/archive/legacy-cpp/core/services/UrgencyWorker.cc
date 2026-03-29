/**
 * @file UrgencyWorker.cc
 * @brief Implementation of UrgencyWorker background service
 * @see UrgencyWorker.h for documentation
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/UrgencyWorker.h"

// Standard library includes
#include <algorithm>

// Project includes
#include "core/services/UrgencyCalculator.h"
#include "core/services/KillShelterMonitor.h"
#include "core/services/AlertService.h"
#include "core/services/EuthanasiaTracker.h"

// These will be provided by other agents
// #include "core/models/Dog.h"
// #include "core/models/Shelter.h"
// #include "core/services/DogService.h"
// #include "core/services/ShelterService.h"
// #include "core/db/ConnectionPool.h"
// #include "core/debug/ErrorCapture.h"
// #include "core/EventBus.h"

namespace wtl::core::services {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

UrgencyWorker::UrgencyWorker() {
    // Initialize next run time to now
    next_run_time_ = std::chrono::system_clock::now();
}

UrgencyWorker::~UrgencyWorker() {
    // Ensure worker is stopped on destruction
    if (running_) {
        stop();
    }
}

UrgencyWorker& UrgencyWorker::getInstance() {
    static UrgencyWorker instance;
    return instance;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UrgencyWorker::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
        return;  // Already running
    }

    running_ = true;
    stop_requested_ = false;
    next_run_time_ = std::chrono::system_clock::now();

    // Start worker thread
    worker_thread_ = std::thread(&UrgencyWorker::workerLoop, this);
}

void UrgencyWorker::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!running_) {
            return;  // Not running
        }

        stop_requested_ = true;
    }

    // Wake up the worker thread
    cv_.notify_one();

    // Wait for thread to finish
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    running_ = false;
}

bool UrgencyWorker::isRunning() const {
    return running_;
}

// ============================================================================
// MANUAL TRIGGERS
// ============================================================================

WorkerRunResult UrgencyWorker::runNow() {
    // Run immediately, bypassing the scheduler
    return processAllDogs();
}

WorkerRunResult UrgencyWorker::runForShelter(const std::string& shelter_id) {
    return processShelter(shelter_id);
}

void UrgencyWorker::triggerRun() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Set next run time to now to trigger immediate run
        next_run_time_ = std::chrono::system_clock::now();
    }
    cv_.notify_one();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UrgencyWorker::setInterval(int minutes) {
    std::lock_guard<std::mutex> lock(mutex_);
    interval_ = std::chrono::minutes(minutes);
}

std::chrono::minutes UrgencyWorker::getInterval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return interval_;
}

void UrgencyWorker::setAutoAlert(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto_alert_enabled_ = enable;
}

// ============================================================================
// STATUS AND STATISTICS
// ============================================================================

std::optional<WorkerRunResult> UrgencyWorker::getLastRunResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_run_result_;
}

std::chrono::seconds UrgencyWorker::getTimeUntilNextRun() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    if (next_run_time_ <= now) {
        return std::chrono::seconds(0);
    }

    return std::chrono::duration_cast<std::chrono::seconds>(next_run_time_ - now);
}

Json::Value UrgencyWorker::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    stats["running"] = running_.load();
    stats["interval_minutes"] = static_cast<int>(interval_.count());
    stats["auto_alert_enabled"] = auto_alert_enabled_;

    // Time until next run
    auto now = std::chrono::system_clock::now();
    if (next_run_time_ > now) {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
            next_run_time_ - now).count();
        stats["seconds_until_next_run"] = static_cast<Json::Int64>(seconds);
    } else {
        stats["seconds_until_next_run"] = 0;
    }

    // Totals
    stats["total_runs"] = total_runs_;
    stats["total_errors"] = total_errors_;
    stats["total_dogs_processed"] = total_dogs_processed_;
    stats["total_urgency_changes"] = total_urgency_changes_;
    stats["total_alerts_triggered"] = total_alerts_triggered_;

    // Last run
    if (last_run_result_.has_value()) {
        stats["last_run"] = last_run_result_->toJson();
    }

    // Recent history summary
    if (!run_history_.empty()) {
        int recent_success = 0;
        int recent_errors = 0;
        int recent_critical = 0;

        int count = std::min(static_cast<int>(run_history_.size()), 10);
        for (int i = 0; i < count; ++i) {
            if (run_history_[i].success) {
                recent_success++;
            } else {
                recent_errors++;
            }
            recent_critical += run_history_[i].new_critical;
        }

        Json::Value recent;
        recent["runs"] = count;
        recent["successes"] = recent_success;
        recent["errors"] = recent_errors;
        recent["critical_dogs_found"] = recent_critical;
        stats["recent_history_summary"] = recent;
    }

    return stats;
}

std::vector<WorkerRunResult> UrgencyWorker::getRunHistory(int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (limit <= 0 || run_history_.empty()) {
        return {};
    }

    int count = std::min(limit, static_cast<int>(run_history_.size()));
    return std::vector<WorkerRunResult>(
        run_history_.begin(),
        run_history_.begin() + count);
}

// ============================================================================
// WORKER THREAD METHODS
// ============================================================================

void UrgencyWorker::workerLoop() {
    while (!stop_requested_) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait until next run time or stop requested
        cv_.wait_until(lock, next_run_time_, [this] {
            return stop_requested_.load() ||
                   std::chrono::system_clock::now() >= next_run_time_;
        });

        if (stop_requested_) {
            break;
        }

        // Release lock during processing
        lock.unlock();

        // Process all dogs
        auto result = processAllDogs();

        // Record result
        recordRunResult(result);

        // Schedule next run
        lock.lock();
        next_run_time_ = std::chrono::system_clock::now() + interval_;
    }
}

WorkerRunResult UrgencyWorker::processAllDogs() {
    WorkerRunResult result;
    result.run_time = std::chrono::system_clock::now();
    result.dogs_processed = 0;
    result.urgency_changes = 0;
    result.new_critical = 0;
    result.alerts_triggered = 0;
    result.success = false;

    auto start_time = std::chrono::steady_clock::now();

    try {
        // Get list of currently critical dogs before processing
        std::vector<std::string> previous_critical;
        auto& monitor = KillShelterMonitor::getInstance();
        (void)monitor;
        // TODO: When models available:
        // auto critical_dogs = monitor.getCriticalDogs();
        // for (const auto& dog : critical_dogs) {
        //     previous_critical.push_back(dog.id);
        // }

        // Recalculate urgency for all dogs
        auto& calculator = UrgencyCalculator::getInstance();
        int changes = calculator.recalculateAll();
        result.urgency_changes = changes;

        // TODO: Track dogs processed when DogService is available
        // auto& dog_service = DogService::getInstance();
        // result.dogs_processed = dog_service.countAll();

        // Check for newly critical dogs
        result.new_critical = checkForNewCritical(previous_critical);

        // Update shelter status caches
        updateShelterStatuses();

        // Update totals
        total_runs_++;
        total_dogs_processed_ += result.dogs_processed;
        total_urgency_changes_ += result.urgency_changes;
        total_alerts_triggered_ += result.alerts_triggered;

        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        total_errors_++;

        // Log error
        // WTL_CAPTURE_ERROR(ErrorCategory::INTERNAL,
        //     "UrgencyWorker run failed: " + std::string(e.what()),
        //     {{"operation", "processAllDogs"}});
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

WorkerRunResult UrgencyWorker::processShelter(const std::string& shelter_id) {
    WorkerRunResult result;
    result.run_time = std::chrono::system_clock::now();
    result.dogs_processed = 0;
    result.urgency_changes = 0;
    result.new_critical = 0;
    result.alerts_triggered = 0;
    result.success = false;

    auto start_time = std::chrono::steady_clock::now();

    try {
        // Get list of currently critical dogs at this shelter before processing
        std::vector<std::string> previous_critical;
        auto& monitor = KillShelterMonitor::getInstance();
        // TODO: When models available:
        // auto dogs_at_risk = monitor.getDogsAtRiskByShelter(shelter_id, 0);
        // for (const auto& dog : dogs_at_risk) {
        //     if (stringToUrgency(dog.urgency_level) == UrgencyLevel::CRITICAL) {
        //         previous_critical.push_back(dog.id);
        //     }
        // }

        // Recalculate urgency for shelter
        auto& calculator = UrgencyCalculator::getInstance();
        int changes = calculator.recalculateForShelter(shelter_id);
        result.urgency_changes = changes;

        // TODO: Track dogs processed when DogService is available
        // auto& dog_service = DogService::getInstance();
        // result.dogs_processed = dog_service.countByShelterId(shelter_id);

        // Check for newly critical dogs
        result.new_critical = checkForNewCritical(previous_critical);

        // Refresh shelter status
        monitor.refreshShelterStatus(shelter_id);

        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();

        // Log error
        // WTL_CAPTURE_ERROR(ErrorCategory::INTERNAL,
        //     "UrgencyWorker shelter run failed: " + std::string(e.what()),
        //     {{"operation", "processShelter"}, {"shelter_id", shelter_id}});
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

int UrgencyWorker::checkForNewCritical(
    const std::vector<std::string>& previous_critical) {

    if (!auto_alert_enabled_) {
        return 0;
    }

    int new_critical_count = 0;

    // TODO: Implementation when services are available
    // auto& monitor = KillShelterMonitor::getInstance();
    // auto& alert_service = AlertService::getInstance();
    // auto& shelter_service = ShelterService::getInstance();
    //
    // auto current_critical = monitor.getCriticalDogs();
    //
    // for (const auto& dog : current_critical) {
    //     // Check if this dog was not previously critical
    //     bool was_critical = std::find(
    //         previous_critical.begin(),
    //         previous_critical.end(),
    //         dog.id
    //     ) != previous_critical.end();
    //
    //     if (!was_critical) {
    //         // New critical dog! Trigger alert
    //         auto shelter = shelter_service.findById(dog.shelter_id);
    //         if (shelter) {
    //             alert_service.triggerCriticalAlert(dog, *shelter);
    //             new_critical_count++;
    //
    //             // Emit event
    //             emitEvent(EventType::DOG_BECAME_CRITICAL, dog.id, {
    //                 {"dog_name", dog.name},
    //                 {"shelter_id", dog.shelter_id},
    //                 {"shelter_name", shelter->name}
    //             }, "urgency_worker");
    //         }
    //     }
    // }

    (void)previous_critical;
    return new_critical_count;
}

void UrgencyWorker::updateShelterStatuses() {
    auto& monitor = KillShelterMonitor::getInstance();
    monitor.refreshStatuses();
}

void UrgencyWorker::recordRunResult(const WorkerRunResult& result) {
    std::lock_guard<std::mutex> lock(mutex_);

    last_run_result_ = result;

    // Add to history (most recent first)
    run_history_.insert(run_history_.begin(), result);

    // Trim history if too long
    if (run_history_.size() > MAX_HISTORY_SIZE) {
        run_history_.resize(MAX_HISTORY_SIZE);
    }
}

} // namespace wtl::core::services
