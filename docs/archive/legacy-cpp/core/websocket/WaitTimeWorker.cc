/**
 * @file WaitTimeWorker.cc
 * @brief Implementation of WaitTimeWorker background thread
 * @see WaitTimeWorker.h for documentation
 */

#include "core/websocket/WaitTimeWorker.h"

#include <drogon/drogon.h>
#include <iomanip>
#include <sstream>

#include "core/websocket/BroadcastService.h"
#include "core/websocket/ConnectionManager.h"

namespace wtl::core::websocket {

// ============================================================================
// SINGLETON
// ============================================================================

WaitTimeWorker& WaitTimeWorker::getInstance() {
    static WaitTimeWorker instance;
    return instance;
}

WaitTimeWorker::WaitTimeWorker() = default;

WaitTimeWorker::~WaitTimeWorker() {
    stop();
}

// ============================================================================
// THREAD MANAGEMENT
// ============================================================================

void WaitTimeWorker::start() {
    // Don't start if already running
    if (running_.load()) {
        return;
    }

    // Reset stop flag
    stop_requested_.store(false);
    running_.store(true);

    // Record start time
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        start_time_ = std::chrono::system_clock::now();
        tick_count_ = 0;
        dogs_updated_ = 0;
    }

    // Start worker thread
    worker_thread_ = std::thread(&WaitTimeWorker::workerLoop, this);

    LOG_INFO << "WaitTimeWorker: Started background worker thread";
}

void WaitTimeWorker::stop() {
    if (!running_.load()) {
        return;
    }

    // Signal stop
    stop_requested_.store(true);

    // Wake up the worker thread
    cv_.notify_all();

    // Wait for thread to finish
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    running_.store(false);

    LOG_INFO << "WaitTimeWorker: Stopped background worker thread";
}

bool WaitTimeWorker::isRunning() const {
    return running_.load();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void WaitTimeWorker::setTickInterval(int interval_ms) {
    if (interval_ms > 0) {
        tick_interval_ms_.store(interval_ms);
    }
}

int WaitTimeWorker::getTickInterval() const {
    return tick_interval_ms_.load();
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value WaitTimeWorker::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Json::Value stats;
    stats["is_running"] = running_.load();
    stats["tick_count"] = static_cast<Json::UInt64>(tick_count_);
    stats["dogs_updated"] = static_cast<Json::UInt64>(dogs_updated_);
    stats["tick_interval_ms"] = tick_interval_ms_.load();

    if (tick_count_ > 0) {
        auto last_tick_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            last_tick_time_.time_since_epoch()
        ).count();
        stats["last_tick_timestamp_ms"] = static_cast<Json::Int64>(last_tick_ms);

        auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - start_time_
        ).count();
        stats["uptime_seconds"] = static_cast<Json::Int64>(uptime_seconds);
    }

    return stats;
}

// ============================================================================
// WORKER LOOP
// ============================================================================

void WaitTimeWorker::workerLoop() {
    LOG_DEBUG << "WaitTimeWorker: Entering worker loop";

    while (!stop_requested_.load()) {
        auto tick_start = std::chrono::steady_clock::now();

        // Process this tick
        processTick();

        // Calculate time until next tick
        auto tick_end = std::chrono::steady_clock::now();
        auto tick_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            tick_end - tick_start
        );

        int sleep_time_ms = tick_interval_ms_.load() - static_cast<int>(tick_duration.count());

        if (sleep_time_ms > 0) {
            // Wait for next tick or stop signal
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(sleep_time_ms), [this]() {
                return stop_requested_.load();
            });
        }
    }

    LOG_DEBUG << "WaitTimeWorker: Exiting worker loop";
}

void WaitTimeWorker::processTick() {
    auto& manager = ConnectionManager::getInstance();
    auto& broadcast = BroadcastService::getInstance();

    // Get all dogs with active subscribers
    auto dogs_with_subs = manager.getDogsWithSubscribers();

    if (dogs_with_subs.empty()) {
        // Update stats even when no work done
        std::lock_guard<std::mutex> lock(stats_mutex_);
        tick_count_++;
        last_tick_time_ = std::chrono::system_clock::now();
        return;
    }

    size_t updated = 0;
    auto now = std::chrono::system_clock::now();

    for (const auto& dog_id : dogs_with_subs) {
        // In production, we would fetch the dog's intake_date from DogService:
        // auto& dogService = DogService::getInstance();
        // auto dog = dogService.findById(dog_id);
        // if (dog) {
        //     auto wait_time = calculateWaitTime(dog_id, dog->intake_date);
        //     broadcast.broadcastWaitTimeUpdate(dog_id, wait_time);
        //
        //     if (dog->euthanasia_date) {
        //         auto countdown = calculateCountdown(dog_id, *dog->euthanasia_date);
        //         if (!countdown.isNull()) {
        //             broadcast.broadcastCountdownUpdate(dog_id, countdown);
        //         }
        //     }
        //     updated++;
        // }

        // For now, calculate wait time assuming dog was added some time ago
        // This demonstrates the calculation logic - real implementation gets intake_date from DB
        auto simulated_intake = now - std::chrono::hours(24 * 30 * 3);  // 3 months ago for demo
        auto wait_time = calculateWaitTime(dog_id, simulated_intake);
        broadcast.broadcastWaitTimeUpdate(dog_id, wait_time);
        updated++;
    }

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        tick_count_++;
        dogs_updated_ += updated;
        last_tick_time_ = now;
    }

    LOG_TRACE << "WaitTimeWorker: Tick " << tick_count_
              << " - updated " << updated << " dogs";
}

// ============================================================================
// WAIT TIME CALCULATION
// ============================================================================

Json::Value WaitTimeWorker::calculateWaitTime(
    const std::string& dog_id,
    std::chrono::system_clock::time_point intake_timestamp
) {
    auto now = std::chrono::system_clock::now();
    auto duration = now - intake_timestamp;

    // Convert to total seconds
    auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    if (total_seconds < 0) {
        total_seconds = 0;  // Handle future dates gracefully
    }

    // Calculate components
    int years = static_cast<int>(total_seconds / (365 * 24 * 60 * 60));
    int remaining = static_cast<int>(total_seconds % (365 * 24 * 60 * 60));

    int months = remaining / (30 * 24 * 60 * 60);
    remaining %= (30 * 24 * 60 * 60);

    int days = remaining / (24 * 60 * 60);
    remaining %= (24 * 60 * 60);

    int hours = remaining / (60 * 60);
    remaining %= (60 * 60);

    int minutes = remaining / 60;
    int seconds = remaining % 60;

    // Format as "YY:MM:DD:HH:MM:SS"
    std::ostringstream formatted;
    formatted << std::setfill('0') << std::setw(2) << years << ":"
              << std::setfill('0') << std::setw(2) << months << ":"
              << std::setfill('0') << std::setw(2) << days << ":"
              << std::setfill('0') << std::setw(2) << hours << ":"
              << std::setfill('0') << std::setw(2) << minutes << ":"
              << std::setfill('0') << std::setw(2) << seconds;

    Json::Value result;
    result["years"] = years;
    result["months"] = months;
    result["days"] = days;
    result["hours"] = hours;
    result["minutes"] = minutes;
    result["seconds"] = seconds;
    result["formatted"] = formatted.str();
    result["total_seconds"] = static_cast<Json::Int64>(total_seconds);

    return result;
}

Json::Value WaitTimeWorker::calculateCountdown(
    const std::string& dog_id,
    std::chrono::system_clock::time_point euthanasia_timestamp
) {
    auto now = std::chrono::system_clock::now();
    auto duration = euthanasia_timestamp - now;

    auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    // If already past, return null
    if (total_seconds <= 0) {
        return Json::Value(Json::nullValue);
    }

    // Calculate components
    int days = static_cast<int>(total_seconds / (24 * 60 * 60));
    int remaining = static_cast<int>(total_seconds % (24 * 60 * 60));

    int hours = remaining / (60 * 60);
    remaining %= (60 * 60);

    int minutes = remaining / 60;
    int seconds = remaining % 60;

    // Determine if critical (< 24 hours)
    bool is_critical = (total_seconds < 24 * 60 * 60);

    // Determine urgency level
    std::string urgency_level;
    if (total_seconds < 24 * 60 * 60) {
        urgency_level = "critical";
    } else if (total_seconds < 72 * 60 * 60) {  // < 3 days
        urgency_level = "high";
    } else {
        urgency_level = "medium";
    }

    // Format as "DD:HH:MM:SS"
    std::ostringstream formatted;
    formatted << std::setfill('0') << std::setw(2) << days << ":"
              << std::setfill('0') << std::setw(2) << hours << ":"
              << std::setfill('0') << std::setw(2) << minutes << ":"
              << std::setfill('0') << std::setw(2) << seconds;

    Json::Value result;
    result["days"] = days;
    result["hours"] = hours;
    result["minutes"] = minutes;
    result["seconds"] = seconds;
    result["formatted"] = formatted.str();
    result["is_critical"] = is_critical;
    result["urgency_level"] = urgency_level;
    result["total_seconds_remaining"] = static_cast<Json::Int64>(total_seconds);

    return result;
}

} // namespace wtl::core::websocket
