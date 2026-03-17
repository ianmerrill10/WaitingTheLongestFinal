/**
 * @file EuthanasiaTracker.h
 * @brief Service for tracking euthanasia schedules and list management
 *
 * PURPOSE:
 * Manages euthanasia dates, list additions/removals, and provides countdown
 * functionality. This is the most critical data tracking component - every
 * date tracked here represents a life that can potentially be saved.
 *
 * KEY RESPONSIBILITIES:
 * - Set and clear euthanasia dates for dogs
 * - Add/remove dogs from euthanasia lists
 * - Provide countdown calculations for urgent dogs
 * - Maintain history of all euthanasia-related events for audit
 *
 * USAGE:
 * auto& tracker = EuthanasiaTracker::getInstance();
 * tracker.setEuthanasiaDate(dog_id, date);
 * auto countdown = tracker.getCountdown(dog_id);
 *
 * DEPENDENCIES:
 * - Dog model (Agent 3)
 * - DogService (Agent 3)
 * - EventBus (Agent 10) - for emitting events
 * - ConnectionPool (Agent 1)
 * - ErrorCapture (Agent 1)
 *
 * MODIFICATION GUIDE:
 * - All date changes must be logged in the history
 * - Events must be emitted for all state changes
 * - Thread safety must be maintained for all operations
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "core/services/UrgencyLevel.h"
#include "core/utils/WaitTimeCalculator.h"

// Forward declarations
namespace wtl::core::models {
    struct Dog;
}

namespace wtl::core::services {

/**
 * @struct EuthanasiaCountdown
 * @brief Extended countdown with urgency level and JSON serialization
 *
 * Extends the base CountdownComponents with urgency-specific fields
 * needed by the euthanasia tracking system.
 */
struct EuthanasiaCountdown {
    int days = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;

    /// Formatted string: "DD:HH:MM:SS"
    std::string formatted;

    /// True if less than 24 hours remaining
    bool is_critical = false;

    /// Associated urgency level
    UrgencyLevel urgency_level = UrgencyLevel::NORMAL;

    /// Total seconds remaining (for calculations)
    int64_t total_seconds = 0;

    /**
     * @brief Convert to JSON for API/WebSocket responses
     */
    Json::Value toJson() const {
        Json::Value json;
        json["days"] = days;
        json["hours"] = hours;
        json["minutes"] = minutes;
        json["seconds"] = seconds;
        json["formatted"] = formatted;
        json["is_critical"] = is_critical;
        json["urgency_level"] = urgencyToString(urgency_level);
        json["total_seconds"] = static_cast<Json::Int64>(total_seconds);
        return json;
    }
};

/**
 * @struct EuthanasiaEvent
 * @brief Record of a euthanasia-related event for audit trail
 *
 * Every change to euthanasia status is logged for accountability
 * and to enable analysis of shelter practices.
 */
struct EuthanasiaEvent {
    std::string id;           ///< Event UUID
    std::string dog_id;       ///< Dog this event relates to
    std::string shelter_id;   ///< Shelter where event occurred

    /// Action type: "added", "removed", "date_set", "date_changed", "date_cleared"
    std::string action;

    /// Timestamp of the event
    std::chrono::system_clock::time_point timestamp;

    /// Reason for the action (especially important for removals)
    std::string reason;

    /// Old euthanasia date (if changed)
    std::optional<std::chrono::system_clock::time_point> old_date;

    /// New euthanasia date (if set or changed)
    std::optional<std::chrono::system_clock::time_point> new_date;

    /// User/system that made the change
    std::string actor;

    /**
     * @brief Convert to JSON for API responses
     */
    Json::Value toJson() const {
        Json::Value json;
        json["id"] = id;
        json["dog_id"] = dog_id;
        json["shelter_id"] = shelter_id;
        json["action"] = action;

        auto time_t_val = std::chrono::system_clock::to_time_t(timestamp);
        json["timestamp"] = static_cast<Json::Int64>(time_t_val);

        json["reason"] = reason;

        if (old_date.has_value()) {
            auto old_time = std::chrono::system_clock::to_time_t(old_date.value());
            json["old_date"] = static_cast<Json::Int64>(old_time);
        }

        if (new_date.has_value()) {
            auto new_time = std::chrono::system_clock::to_time_t(new_date.value());
            json["new_date"] = static_cast<Json::Int64>(new_time);
        }

        json["actor"] = actor;

        return json;
    }
};

/**
 * @class EuthanasiaTracker
 * @brief Singleton service for tracking euthanasia schedules
 *
 * Thread-safe singleton that manages all euthanasia date tracking,
 * list management, and countdown calculations.
 */
class EuthanasiaTracker {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to the EuthanasiaTracker instance
     */
    static EuthanasiaTracker& getInstance();

    // Prevent copying and assignment
    EuthanasiaTracker(const EuthanasiaTracker&) = delete;
    EuthanasiaTracker& operator=(const EuthanasiaTracker&) = delete;

    // ========================================================================
    // DATE MANAGEMENT
    // ========================================================================

    /**
     * @brief Set euthanasia date for a dog
     *
     * Sets the scheduled euthanasia date and updates urgency level.
     * Emits DOG_URGENCY_CHANGED event.
     *
     * @param dog_id UUID of the dog
     * @param date Scheduled euthanasia date/time
     * @param reason Optional reason for setting date
     * @param actor User/system making the change (default: "system")
     *
     * @example
     * tracker.setEuthanasiaDate(dog_id, scheduled_time, "Shelter capacity", "shelter_admin");
     */
    void setEuthanasiaDate(const std::string& dog_id,
                           std::chrono::system_clock::time_point date,
                           const std::string& reason = "",
                           const std::string& actor = "system");

    /**
     * @brief Update/change an existing euthanasia date
     *
     * @param dog_id UUID of the dog
     * @param new_date New scheduled euthanasia date
     * @param reason Reason for the change
     * @param actor User/system making the change
     */
    void updateEuthanasiaDate(const std::string& dog_id,
                               std::chrono::system_clock::time_point new_date,
                               const std::string& reason,
                               const std::string& actor = "system");

    /**
     * @brief Clear euthanasia date for a dog
     *
     * Removes the euthanasia date and updates urgency level.
     * This is called when a dog is rescued, adopted, or pulled by rescue.
     *
     * @param dog_id UUID of the dog
     * @param reason Why the date was cleared (adoption, rescue, etc.)
     * @param actor User/system making the change
     */
    void clearEuthanasiaDate(const std::string& dog_id,
                              const std::string& reason = "Unknown",
                              const std::string& actor = "system");

    // ========================================================================
    // EUTHANASIA LIST MANAGEMENT
    // ========================================================================

    /**
     * @brief Add a dog to the euthanasia list
     *
     * @param dog_id UUID of the dog
     * @param scheduled Optional scheduled date
     * @param reason Why the dog was added
     * @param actor User/system making the change
     */
    void addToEuthanasiaList(const std::string& dog_id,
                              std::optional<std::chrono::system_clock::time_point> scheduled = std::nullopt,
                              const std::string& reason = "",
                              const std::string& actor = "system");

    /**
     * @brief Remove a dog from the euthanasia list
     *
     * @param dog_id UUID of the dog
     * @param reason Why the dog was removed (CRITICAL - must document!)
     * @param actor User/system making the change
     */
    void removeFromEuthanasiaList(const std::string& dog_id,
                                   const std::string& reason,
                                   const std::string& actor = "system");

    /**
     * @brief Process a full euthanasia list update from a shelter
     *
     * Reconciles the provided list with current data, adding new dogs
     * and potentially removing dogs no longer on the list.
     *
     * @param shelter_id UUID of the shelter
     * @param dog_ids List of dog IDs on the euthanasia list
     * @param scheduled_date Optional common scheduled date
     * @param actor Source of the list update
     *
     * @return Number of changes made (additions + removals)
     */
    int processEuthanasiaListUpdate(const std::string& shelter_id,
                                     const std::vector<std::string>& dog_ids,
                                     std::optional<std::chrono::system_clock::time_point> scheduled_date = std::nullopt,
                                     const std::string& actor = "shelter_sync");

    // ========================================================================
    // QUERY METHODS
    // ========================================================================

    /**
     * @brief Get euthanasia list for a shelter
     *
     * @param shelter_id UUID of the shelter
     * @return std::vector<Dog> Dogs on the euthanasia list
     */
    std::vector<models::Dog> getEuthanasiaList(const std::string& shelter_id);

    /**
     * @brief Get euthanasia date for a dog
     *
     * @param dog_id UUID of the dog
     * @return std::optional<time_point> The scheduled date if set
     */
    std::optional<std::chrono::system_clock::time_point>
        getEuthanasiaDate(const std::string& dog_id);

    /**
     * @brief Check if dog is on euthanasia list
     *
     * @param dog_id UUID of the dog
     * @return bool True if on euthanasia list
     */
    bool isOnEuthanasiaList(const std::string& dog_id);

    // ========================================================================
    // COUNTDOWN
    // ========================================================================

    /**
     * @brief Get countdown components for a dog
     *
     * Calculates the time remaining until scheduled euthanasia,
     * broken down into days, hours, minutes, seconds.
     *
     * @param dog_id UUID of the dog
     * @return std::optional<EuthanasiaCountdown> Countdown if date is set
     */
    std::optional<EuthanasiaCountdown> getCountdown(const std::string& dog_id);

    /**
     * @brief Calculate countdown from a specific date
     *
     * @param euthanasia_date The scheduled euthanasia date
     * @return EuthanasiaCountdown The calculated countdown
     */
    EuthanasiaCountdown calculateCountdown(
        const std::chrono::system_clock::time_point& euthanasia_date);

    /**
     * @brief Get all dogs with countdowns (for WebSocket updates)
     *
     * @return std::vector<pair<dog_id, countdown>> Dogs with active countdowns
     */
    std::vector<std::pair<std::string, EuthanasiaCountdown>> getAllCountdowns();

    // ========================================================================
    // HISTORY
    // ========================================================================

    /**
     * @brief Get euthanasia event history for a dog
     *
     * @param dog_id UUID of the dog
     * @return std::vector<EuthanasiaEvent> All events for this dog
     */
    std::vector<EuthanasiaEvent> getHistory(const std::string& dog_id);

    /**
     * @brief Get recent euthanasia events for a shelter
     *
     * @param shelter_id UUID of the shelter
     * @param limit Maximum number of events to return
     * @return std::vector<EuthanasiaEvent> Recent events
     */
    std::vector<EuthanasiaEvent> getShelterHistory(const std::string& shelter_id,
                                                    int limit = 100);

    /**
     * @brief Get system-wide recent euthanasia events
     *
     * @param limit Maximum number of events to return
     * @return std::vector<EuthanasiaEvent> Recent events
     */
    std::vector<EuthanasiaEvent> getRecentEvents(int limit = 100);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get euthanasia tracking statistics
     */
    Json::Value getStatistics();

    /**
     * @brief Get statistics for a specific shelter
     */
    Json::Value getShelterStatistics(const std::string& shelter_id);

private:
    // Private constructor for singleton
    EuthanasiaTracker();
    ~EuthanasiaTracker() = default;

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================

    /**
     * @brief Log an event to the database
     */
    void logEvent(const EuthanasiaEvent& event);

    /**
     * @brief Generate a UUID for events
     */
    std::string generateEventId();

    /**
     * @brief Update dog record with new euthanasia status
     */
    void updateDogRecord(const std::string& dog_id,
                          std::optional<std::chrono::system_clock::time_point> euthanasia_date,
                          bool is_on_list);

    /**
     * @brief Emit event to EventBus
     */
    void emitUrgencyEvent(const std::string& dog_id,
                           UrgencyLevel new_level,
                           const std::string& action);

    /**
     * @brief Format countdown components to string
     */
    std::string formatCountdown(int days, int hours, int minutes, int seconds);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    /// Mutex for thread-safe operations
    mutable std::mutex mutex_;

    /// Statistics counters
    int total_dates_set_ = 0;
    int total_dates_cleared_ = 0;
    int total_list_additions_ = 0;
    int total_list_removals_ = 0;
};

} // namespace wtl::core::services
