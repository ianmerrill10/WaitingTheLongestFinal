/**
 * @file WaitTimeWorker.h
 * @brief Background worker thread for real-time wait time updates
 *
 * PURPOSE:
 * Runs a background thread that updates wait time counters every second.
 * Calculates current wait times for all dogs with active subscribers
 * and broadcasts updates via BroadcastService.
 *
 * USAGE:
 * auto& worker = WaitTimeWorker::getInstance();
 * worker.start();  // Start the background thread
 * // ... application runs ...
 * worker.stop();   // Stop on shutdown
 *
 * DEPENDENCIES:
 * - ConnectionManager (finding dogs with subscribers)
 * - BroadcastService (sending updates)
 * - DogService (getting dog data and calculating wait times)
 *
 * MODIFICATION GUIDE:
 * - Adjust update interval by changing TICK_INTERVAL_MS
 * - Add countdown handling for urgent dogs in workerLoop()
 * - Ensure thread-safe access to all shared resources
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

// Third-party includes
#include <json/json.h>

namespace wtl::core::websocket {

/**
 * @class WaitTimeWorker
 * @brief Background thread that updates wait time counters every second
 *
 * This singleton runs a dedicated thread that:
 * 1. Gets all dogs with active WebSocket subscribers
 * 2. Calculates their current wait time (since intake)
 * 3. Broadcasts wait time updates to subscribers
 * 4. For urgent dogs, calculates and broadcasts countdowns
 */
class WaitTimeWorker {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the WaitTimeWorker singleton
     */
    static WaitTimeWorker& getInstance();

    // Prevent copying
    WaitTimeWorker(const WaitTimeWorker&) = delete;
    WaitTimeWorker& operator=(const WaitTimeWorker&) = delete;

    /**
     * @brief Start the background worker thread
     *
     * Starts a new thread that runs workerLoop() until stop() is called.
     * Safe to call multiple times - only starts if not already running.
     */
    void start();

    /**
     * @brief Stop the background worker thread
     *
     * Signals the worker to stop and waits for it to finish.
     * Safe to call multiple times.
     */
    void stop();

    /**
     * @brief Check if the worker is currently running
     * @return true if the worker thread is active
     */
    bool isRunning() const;

    /**
     * @brief Get worker statistics
     * @return JSON with tick count, last tick time, etc.
     */
    Json::Value getStats() const;

    /**
     * @brief Set the tick interval in milliseconds
     * @param interval_ms Interval between updates (default 1000)
     */
    void setTickInterval(int interval_ms);

    /**
     * @brief Get the current tick interval in milliseconds
     * @return Current tick interval
     */
    int getTickInterval() const;

private:
    WaitTimeWorker();
    ~WaitTimeWorker();

    /**
     * @brief Main worker loop that runs in the background thread
     *
     * Runs every tick interval:
     * 1. Get dogs with active subscribers
     * 2. Calculate wait times
     * 3. Broadcast updates
     * 4. Handle countdowns for urgent dogs
     */
    void workerLoop();

    /**
     * @brief Process a single tick - update all subscribers
     *
     * Called every tick interval from workerLoop()
     */
    void processTick();

    /**
     * @brief Calculate wait time for a specific dog
     * @param dog_id The dog's unique identifier
     * @param intake_timestamp The dog's intake timestamp
     * @return JSON with wait time components
     */
    Json::Value calculateWaitTime(const std::string& dog_id,
                                   std::chrono::system_clock::time_point intake_timestamp);

    /**
     * @brief Calculate countdown for an urgent dog
     * @param dog_id The dog's unique identifier
     * @param euthanasia_timestamp The scheduled euthanasia timestamp
     * @return JSON with countdown components, or null if no countdown needed
     */
    Json::Value calculateCountdown(const std::string& dog_id,
                                    std::chrono::system_clock::time_point euthanasia_timestamp);

    // Thread management
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Synchronization for graceful shutdown
    std::mutex mutex_;
    std::condition_variable cv_;

    // Configuration
    std::atomic<int> tick_interval_ms_{1000};  // 1 second default

    // Statistics
    mutable std::mutex stats_mutex_;
    size_t tick_count_ = 0;
    size_t dogs_updated_ = 0;
    std::chrono::system_clock::time_point last_tick_time_;
    std::chrono::system_clock::time_point start_time_;

    // Default tick interval
    static constexpr int DEFAULT_TICK_INTERVAL_MS = 1000;
};

} // namespace wtl::core::websocket
