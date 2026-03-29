/**
 * @file KillShelterMonitor.h
 * @brief Service for monitoring kill shelters and tracking dogs at risk
 *
 * PURPOSE:
 * Monitors kill shelters for changes, tracks dogs at risk of euthanasia,
 * and provides status reports on shelter urgency levels. This is the
 * surveillance system that keeps watch on shelters where dogs' lives
 * hang in the balance.
 *
 * KEY RESPONSIBILITIES:
 * - Track all kill shelters in the system
 * - Identify dogs approaching euthanasia deadlines
 * - Provide real-time urgency statistics by shelter
 * - Monitor euthanasia lists and schedule changes
 *
 * USAGE:
 * auto& monitor = KillShelterMonitor::getInstance();
 * auto critical_dogs = monitor.getCriticalDogs();  // < 24 hours
 * auto at_risk = monitor.getDogsAtRisk(7);  // Within 7 days
 * auto statuses = monitor.getShelterStatuses();
 *
 * DEPENDENCIES:
 * - Dog, Shelter models (Agent 3)
 * - DogService, ShelterService (Agent 3)
 * - UrgencyCalculator (this agent)
 * - ConnectionPool (Agent 1)
 * - ErrorCapture (Agent 1)
 *
 * MODIFICATION GUIDE:
 * - To add new monitoring criteria: Update relevant getter methods
 * - To change risk thresholds: Update the constants and documentation
 * - All queries should use the connection pool for thread safety
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
#include "core/models/Dog.h"
#include "core/models/Shelter.h"

namespace wtl::core::services {

/**
 * @struct ShelterUrgencyStatus
 * @brief Summary of urgency status for a single shelter
 *
 * Provides a quick overview of how many dogs at a shelter need urgent help,
 * enabling rescue coordinators to prioritize their efforts.
 */
struct ShelterUrgencyStatus {
    /// Shelter identification
    std::string shelter_id;
    std::string shelter_name;
    std::string state_code;
    std::string city;

    /// Shelter classification
    bool is_kill_shelter;
    int avg_hold_days;

    /// Dog counts by status
    int total_dogs;
    int available_dogs;

    /// Urgency breakdown
    int critical_count;    ///< < 24 hours
    int high_count;        ///< 1-3 days
    int medium_count;      ///< 4-7 days
    int normal_count;      ///< > 7 days or no date

    /// Dogs on euthanasia list
    int on_euthanasia_list_count;

    /// Next scheduled euthanasia (if known)
    std::optional<std::chrono::system_clock::time_point> next_euthanasia;
    std::optional<std::string> next_euthanasia_dog_id;
    std::optional<std::string> next_euthanasia_dog_name;

    /// Last update timestamp
    std::chrono::system_clock::time_point last_updated;

    /**
     * @brief Calculate overall urgency score for this shelter
     *
     * Higher score = more urgent attention needed
     * Formula: (critical * 4) + (high * 2) + medium
     */
    int getUrgencyScore() const {
        return (critical_count * 4) + (high_count * 2) + medium_count;
    }

    /**
     * @brief Check if shelter has any critical dogs
     */
    bool hasCriticalDogs() const {
        return critical_count > 0;
    }

    /**
     * @brief Check if shelter has any urgent dogs (high or critical)
     */
    bool hasUrgentDogs() const {
        return critical_count > 0 || high_count > 0;
    }

    /**
     * @brief Convert to JSON for API responses
     */
    Json::Value toJson() const {
        Json::Value json;
        json["shelter_id"] = shelter_id;
        json["shelter_name"] = shelter_name;
        json["state_code"] = state_code;
        json["city"] = city;
        json["is_kill_shelter"] = is_kill_shelter;
        json["avg_hold_days"] = avg_hold_days;

        json["total_dogs"] = total_dogs;
        json["available_dogs"] = available_dogs;

        Json::Value urgency;
        urgency["critical"] = critical_count;
        urgency["high"] = high_count;
        urgency["medium"] = medium_count;
        urgency["normal"] = normal_count;
        urgency["score"] = getUrgencyScore();
        json["urgency"] = urgency;

        json["on_euthanasia_list_count"] = on_euthanasia_list_count;

        if (next_euthanasia.has_value()) {
            auto time_t_val = std::chrono::system_clock::to_time_t(next_euthanasia.value());
            json["next_euthanasia_timestamp"] = static_cast<Json::Int64>(time_t_val);
            if (next_euthanasia_dog_id.has_value()) {
                json["next_euthanasia_dog_id"] = next_euthanasia_dog_id.value();
            }
            if (next_euthanasia_dog_name.has_value()) {
                json["next_euthanasia_dog_name"] = next_euthanasia_dog_name.value();
            }
        }

        auto updated_time = std::chrono::system_clock::to_time_t(last_updated);
        json["last_updated"] = static_cast<Json::Int64>(updated_time);

        return json;
    }
};

/**
 * @class KillShelterMonitor
 * @brief Singleton service for monitoring kill shelters
 *
 * Thread-safe singleton that provides comprehensive monitoring of kill shelters,
 * tracking dogs at risk and providing urgency statistics.
 */
class KillShelterMonitor {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to the KillShelterMonitor instance
     */
    static KillShelterMonitor& getInstance();

    // Prevent copying and assignment
    KillShelterMonitor(const KillShelterMonitor&) = delete;
    KillShelterMonitor& operator=(const KillShelterMonitor&) = delete;

    // ========================================================================
    // SHELTER QUERIES
    // ========================================================================

    /**
     * @brief Get all kill shelters in the system
     *
     * @return std::vector<Shelter> All shelters marked as kill shelters
     */
    std::vector<models::Shelter> getKillShelters();

    /**
     * @brief Get kill shelters in a specific state
     *
     * @param state_code Two-letter state code (e.g., "TX", "CA")
     * @return std::vector<Shelter> Kill shelters in the specified state
     */
    std::vector<models::Shelter> getKillSheltersByState(const std::string& state_code);

    /**
     * @brief Get shelters with critical dogs (< 24 hours)
     *
     * @return std::vector<Shelter> Shelters that have dogs facing imminent euthanasia
     */
    std::vector<models::Shelter> getSheltersWithCriticalDogs();

    // ========================================================================
    // DOG QUERIES - AT RISK
    // ========================================================================

    /**
     * @brief Get dogs at risk within specified days threshold
     *
     * Returns dogs with known euthanasia dates within the threshold,
     * plus dogs on euthanasia lists without specific dates.
     *
     * @param days_threshold Maximum days until euthanasia (default 7)
     * @return std::vector<Dog> Dogs at risk, sorted by urgency (most urgent first)
     */
    std::vector<models::Dog> getDogsAtRisk(int days_threshold = 7);

    /**
     * @brief Get critical dogs (< 24 hours until euthanasia)
     *
     * EMERGENCY level - these dogs need immediate intervention.
     *
     * @return std::vector<Dog> Critical dogs, sorted by time remaining (least first)
     */
    std::vector<models::Dog> getCriticalDogs();

    /**
     * @brief Get high urgency dogs (1-3 days until euthanasia)
     *
     * @return std::vector<Dog> High urgency dogs
     */
    std::vector<models::Dog> getHighUrgencyDogs();

    /**
     * @brief Get all dogs currently on euthanasia lists
     *
     * @return std::vector<Dog> Dogs marked as on euthanasia list
     */
    std::vector<models::Dog> getDogsOnEuthanasiaList();

    /**
     * @brief Get dogs at risk at a specific shelter
     *
     * @param shelter_id UUID of the shelter
     * @param days_threshold Maximum days until euthanasia
     * @return std::vector<Dog> Dogs at risk at this shelter
     */
    std::vector<models::Dog> getDogsAtRiskByShelter(const std::string& shelter_id,
                                                     int days_threshold = 7);

    /**
     * @brief Get dogs at risk in a specific state
     *
     * @param state_code Two-letter state code
     * @param days_threshold Maximum days until euthanasia
     * @return std::vector<Dog> Dogs at risk in this state
     */
    std::vector<models::Dog> getDogsAtRiskByState(const std::string& state_code,
                                                   int days_threshold = 7);

    // ========================================================================
    // SHELTER STATUS
    // ========================================================================

    /**
     * @brief Get urgency status for all kill shelters
     *
     * Returns a summary of urgency levels at each kill shelter,
     * sorted by urgency score (highest first).
     *
     * @return std::vector<ShelterUrgencyStatus> Shelter statuses
     */
    std::vector<ShelterUrgencyStatus> getShelterStatuses();

    /**
     * @brief Get urgency status for a specific shelter
     *
     * @param shelter_id UUID of the shelter
     * @return std::optional<ShelterUrgencyStatus> Status if shelter exists
     */
    std::optional<ShelterUrgencyStatus> getShelterStatus(const std::string& shelter_id);

    /**
     * @brief Get shelters sorted by urgency score
     *
     * @param limit Maximum number of shelters to return
     * @return std::vector<ShelterUrgencyStatus> Top shelters by urgency
     */
    std::vector<ShelterUrgencyStatus> getMostUrgentShelters(int limit = 10);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get comprehensive urgency statistics
     *
     * @return Json::Value Statistics including totals, breakdowns, trends
     */
    Json::Value getUrgencyStatistics();

    /**
     * @brief Get urgency statistics for a specific state
     *
     * @param state_code Two-letter state code
     * @return Json::Value State-specific statistics
     */
    Json::Value getStateStatistics(const std::string& state_code);

    /**
     * @brief Get summary counts
     *
     * Quick access to total counts without full queries.
     */
    int getTotalKillShelters();
    int getTotalDogsAtRisk(int days_threshold = 7);
    int getTotalCriticalDogs();
    int getTotalOnEuthanasiaList();

    // ========================================================================
    // REFRESH / UPDATE
    // ========================================================================

    /**
     * @brief Refresh all shelter status caches
     *
     * Called periodically by UrgencyWorker to ensure data is current.
     */
    void refreshStatuses();

    /**
     * @brief Refresh status for a specific shelter
     *
     * @param shelter_id UUID of the shelter
     */
    void refreshShelterStatus(const std::string& shelter_id);

    /**
     * @brief Mark data as stale, requiring refresh
     */
    void invalidateCache();

private:
    // Private constructor for singleton
    KillShelterMonitor();
    ~KillShelterMonitor() = default;

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================

    /**
     * @brief Calculate urgency status for a shelter
     */
    ShelterUrgencyStatus calculateShelterStatus(const models::Shelter& shelter);

    /**
     * @brief Sort dogs by urgency (most urgent first)
     */
    void sortByUrgency(std::vector<models::Dog>& dogs);

    /**
     * @brief Check if cache is valid
     */
    bool isCacheValid() const;

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    /// Mutex for thread-safe operations
    mutable std::mutex mutex_;

    /// Cached shelter statuses
    std::vector<ShelterUrgencyStatus> cached_statuses_;

    /// Cache validity
    std::chrono::system_clock::time_point last_cache_update_;
    std::chrono::minutes cache_ttl_{5};  // 5 minute cache TTL
    bool cache_valid_ = false;

    /// Statistics counters (updated on refresh)
    int total_kill_shelters_ = 0;
    int total_critical_ = 0;
    int total_high_ = 0;
    int total_medium_ = 0;
    int total_on_list_ = 0;
};

} // namespace wtl::core::services
