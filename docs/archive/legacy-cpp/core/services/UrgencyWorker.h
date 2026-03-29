/**
 * @file UrgencyWorker.h
 * @brief Background worker for updating urgency levels and monitoring shelters
 *
 * PURPOSE:
 * Runs as a background thread to continuously update urgency levels for all dogs,
 * detect newly critical dogs, and trigger alerts. This is the heartbeat of the
 * urgency engine - running every 15 minutes to ensure no dog slips through the cracks.
 *
 * KEY RESPONSIBILITIES:
 * - Recalculate urgency levels for all dogs periodically
 * - Detect dogs that have become critical since last run
 * - Update shelter status caches
 * - Trigger alerts for newly critical dogs
 * - Emit events for urgency changes
 *
 * USAGE:
 * auto& worker = UrgencyWorker::getInstance();
 * worker.start();
 * // ... application runs ...
 * worker.stop();
 *
 * DEPENDENCIES:
 * - UrgencyCalculator (this agent)
 * - KillShelterMonitor (this agent)
 * - AlertService (this agent)
 * - EuthanasiaTracker (this agent)
 * - Dog, Shelter models (Agent 3)
 * - DogService, ShelterService (Agent 3)
 * - EventBus (Agent 10)
 * - ConnectionPool (Agent 1)
 * - ErrorCapture (Agent 1)
 *
 * MODIFICATION GUIDE:
 * - To change interval: Update interval_ or add configuration
 * - To add new checks: Add to workerLoop() or create new processing method
 * - All operations must be thread-safe
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::core::services {

/**
 * @struct WorkerRunResult
 * @brief Result of a single worker run
 */
struct WorkerRunResult {
    /// Timestamp of the run
    std::chrono::system_clock::time_point run_time;

    /// Duration of the run
    std::chrono::milliseconds duration;

    /// Number of dogs processed
    int dogs_processed;

    /// Number of urgency levels that changed
    int urgency_changes;

    /// Number of new critical dogs detected
    int new_critical;

    /// Number of alerts triggered
    int alerts_triggered;

    /// Whether run completed successfully
    bool success;

    /// Error message if failed
    std::string error_message;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const {
        Json::Value json;
        auto time_t_val = std::chrono::system_clock::to_time_t(run_time);
        json["run_time"] = static_cast<Json::Int64>(time_t_val);
        json["duration_ms"] = static_cast<Json::Int64>(duration.count());
        json["dogs_processed"] = dogs_processed;
        json["urgency_changes"] = urgency_changes;
        json["new_critical"] = new_critical;
        json["alerts_triggered"] = alerts_triggered;
        json["success"] = success;
        if (!error_message.empty()) {
            json["error_message"] = error_message;
        }
        return json;
    }
};

/**
 * @class UrgencyWorker
 * @brief Singleton background worker for urgency monitoring
 *
 * Thread-safe singleton that runs a background thread to periodically
 * update urgency levels and monitor for critical dogs.
 */
class UrgencyWorker {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to the UrgencyWorker instance
     */
    static UrgencyWorker& getInstance();

    // Prevent copying and assignment
    UrgencyWorker(const UrgencyWorker&) = delete;
    UrgencyWorker& operator=(const UrgencyWorker&) = delete;

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Start the background worker
     *
     * Spawns a background thread that runs processAllDogs() at the
     * configured interval.
     */
    void start();

    /**
     * @brief Stop the background worker
     *
     * Signals the worker thread to stop and waits for it to finish.
     * Safe to call multiple times.
     */
    void stop();

    /**
     * @brief Check if worker is currently running
     *
     * @return bool True if worker thread is active
     */
    bool isRunning() const;

    // ========================================================================
    // MANUAL TRIGGERS
    // ========================================================================

    /**
     * @brief Run urgency check immediately
     *
     * Triggers an immediate run without waiting for the next scheduled time.
     * Blocks until the run completes.
     *
     * @return WorkerRunResult Results of the run
     */
    WorkerRunResult runNow();

    /**
     * @brief Run urgency check for a specific shelter
     *
     * @param shelter_id UUID of the shelter
     * @return WorkerRunResult Results of the run
     */
    WorkerRunResult runForShelter(const std::string& shelter_id);

    /**
     * @brief Queue a run to happen soon (non-blocking)
     *
     * Wakes up the worker thread to run earlier than scheduled.
     */
    void triggerRun();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Set the run interval
     *
     * @param minutes Interval between runs in minutes
     */
    void setInterval(int minutes);

    /**
     * @brief Get the current run interval
     *
     * @return std::chrono::minutes Current interval
     */
    std::chrono::minutes getInterval() const;

    /**
     * @brief Set whether to auto-trigger alerts
     *
     * @param enable True to auto-trigger alerts for critical dogs
     */
    void setAutoAlert(bool enable);

    // ========================================================================
    // STATUS AND STATISTICS
    // ========================================================================

    /**
     * @brief Get last run result
     *
     * @return std::optional<WorkerRunResult> Last run if available
     */
    std::optional<WorkerRunResult> getLastRunResult() const;

    /**
     * @brief Get time until next scheduled run
     *
     * @return std::chrono::seconds Seconds until next run
     */
    std::chrono::seconds getTimeUntilNextRun() const;

    /**
     * @brief Get worker statistics
     *
     * @return Json::Value Statistics including run history, errors, etc.
     */
    Json::Value getStatistics() const;

    /**
     * @brief Get recent run history
     *
     * @param limit Maximum number of runs to return
     * @return std::vector<WorkerRunResult> Recent run results
     */
    std::vector<WorkerRunResult> getRunHistory(int limit = 10) const;

private:
    // Private constructor for singleton
    UrgencyWorker();
    ~UrgencyWorker();

    // ========================================================================
    // WORKER THREAD METHODS
    // ========================================================================

    /**
     * @brief Main worker loop (runs in background thread)
     */
    void workerLoop();

    /**
     * @brief Process all dogs and update urgency levels
     *
     * @return WorkerRunResult Results of the processing
     */
    WorkerRunResult processAllDogs();

    /**
     * @brief Process dogs at a specific shelter
     *
     * @param shelter_id UUID of the shelter
     * @return WorkerRunResult Results of the processing
     */
    WorkerRunResult processShelter(const std::string& shelter_id);

    /**
     * @brief Check for newly critical dogs and trigger alerts
     *
     * @param previous_critical Dog IDs that were critical before this run
     * @return int Number of new critical dogs detected
     */
    int checkForNewCritical(const std::vector<std::string>& previous_critical);

    /**
     * @brief Update all shelter status caches
     */
    void updateShelterStatuses();

    /**
     * @brief Record a run result
     */
    void recordRunResult(const WorkerRunResult& result);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    /// Worker thread
    std::thread worker_thread_;

    /// Running flag (atomic for thread safety)
    std::atomic<bool> running_{false};

    /// Stop requested flag
    std::atomic<bool> stop_requested_{false};

    /// Mutex for condition variable
    mutable std::mutex mutex_;

    /// Condition variable for sleeping/waking
    std::condition_variable cv_;

    /// Run interval
    std::chrono::minutes interval_{15};

    /// Next scheduled run time
    std::chrono::system_clock::time_point next_run_time_;

    /// Auto-alert enabled
    bool auto_alert_enabled_ = true;

    /// Last run result
    std::optional<WorkerRunResult> last_run_result_;

    /// Run history (most recent first)
    std::vector<WorkerRunResult> run_history_;
    static constexpr int MAX_HISTORY_SIZE = 100;

    /// Statistics
    int total_runs_ = 0;
    int total_errors_ = 0;
    int total_dogs_processed_ = 0;
    int total_urgency_changes_ = 0;
    int total_alerts_triggered_ = 0;
};

} // namespace wtl::core::services
