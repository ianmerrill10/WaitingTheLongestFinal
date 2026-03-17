/**
 * @file UrgentAlertService.h
 * @brief Emergency broadcast system for critical dog alerts
 *
 * PURPOSE:
 * Specialized service for urgent/emergency notifications when dogs are
 * running out of time. Coordinates mass notifications to:
 * - All users within radius of the dog
 * - Users who have favorited the dog or shelter
 * - Foster homes with matching preferences
 * - Rescue organizations in the area
 *
 * This is the LIFE-SAVING component of the notification system.
 *
 * USAGE:
 * auto& urgent = UrgentAlertService::getInstance();
 * urgent.blastNotification(dog, 100); // 100 mile radius
 * urgent.triggerCriticalAlert(dog);
 *
 * DEPENDENCIES:
 * - NotificationService (delivery)
 * - UrgencyCalculator (urgency levels)
 * - AlertService (alert tracking)
 * - ConnectionPool (database)
 * - EventBus (event publishing)
 *
 * MODIFICATION GUIDE:
 * - Adjust radius defaults in constants
 * - Add new user eligibility criteria in findEligibleUsers()
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "notifications/Notification.h"

// Forward declarations
namespace wtl::core::models {
    struct Dog;
    struct Shelter;
}

namespace wtl::notifications {

/**
 * @struct BlastResult
 * @brief Result of an urgent blast notification
 */
struct BlastResult {
    std::string alert_id;         ///< ID of the created alert
    int users_notified;           ///< Number of users successfully notified
    int users_attempted;          ///< Total users attempted
    int users_within_radius;      ///< Users found within radius
    int rescues_notified;         ///< Rescue organizations notified
    int fosters_notified;         ///< Foster homes notified
    std::vector<std::string> channels_used; ///< Channels used for delivery
    std::chrono::milliseconds duration; ///< Time taken to send

    Json::Value toJson() const {
        Json::Value json;
        json["alert_id"] = alert_id;
        json["users_notified"] = users_notified;
        json["users_attempted"] = users_attempted;
        json["users_within_radius"] = users_within_radius;
        json["rescues_notified"] = rescues_notified;
        json["fosters_notified"] = fosters_notified;
        json["duration_ms"] = static_cast<Json::Int64>(duration.count());

        Json::Value channels(Json::arrayValue);
        for (const auto& ch : channels_used) {
            channels.append(ch);
        }
        json["channels_used"] = channels;

        return json;
    }
};

/**
 * @struct UrgentAlertConfig
 * @brief Configuration for urgent alerts
 */
struct UrgentAlertConfig {
    double default_radius_miles = 100.0;   ///< Default alert radius
    double critical_radius_miles = 200.0;  ///< Radius for critical (<24h) alerts
    double max_radius_miles = 500.0;       ///< Maximum allowed radius
    int max_notifications_per_alert = 10000; ///< Prevent runaway blasts
    bool notify_rescues = true;            ///< Include rescue organizations
    bool notify_fosters = true;            ///< Include foster homes
    bool bypass_preferences = false;       ///< Bypass user preferences for critical
    int cooldown_minutes = 60;             ///< Cooldown between blasts for same dog
};

/**
 * @class UrgentAlertService
 * @brief Singleton service for emergency dog alert broadcasts
 *
 * Thread-safe singleton that coordinates urgent notifications when
 * dogs are running out of time. Maximizes reach while respecting
 * reasonable limits.
 */
class UrgentAlertService {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to UrgentAlertService instance
     */
    static UrgentAlertService& getInstance();

    // Prevent copying
    UrgentAlertService(const UrgentAlertService&) = delete;
    UrgentAlertService& operator=(const UrgentAlertService&) = delete;

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Set alert configuration
     *
     * @param config Configuration settings
     */
    void setConfig(const UrgentAlertConfig& config);

    /**
     * @brief Get current configuration
     *
     * @return UrgentAlertConfig Current configuration
     */
    UrgentAlertConfig getConfig() const;

    // =========================================================================
    // URGENT BROADCASTS
    // =========================================================================

    /**
     * @brief Blast notification to all nearby users
     *
     * Sends urgent notification to all eligible users within radius.
     * This is the main method for emergency broadcasts.
     *
     * @param dog The dog needing urgent help
     * @param radius_miles Radius in miles (0 = use default based on urgency)
     * @return BlastResult Results of the blast
     */
    BlastResult blastNotification(const core::models::Dog& dog, double radius_miles = 0);

    /**
     * @brief Trigger critical alert for dog with <24 hours
     *
     * Highest priority alert - uses maximum radius and bypasses
     * some user preferences.
     *
     * @param dog The critical dog
     * @return BlastResult Results of the alert
     */
    BlastResult triggerCriticalAlert(const core::models::Dog& dog);

    /**
     * @brief Trigger high urgency alert for dog with <72 hours
     *
     * @param dog The dog
     * @return BlastResult Results of the alert
     */
    BlastResult triggerHighUrgencyAlert(const core::models::Dog& dog);

    /**
     * @brief Send urgent foster needed alert
     *
     * Targets foster homes with matching preferences.
     *
     * @param dog The dog needing foster
     * @param radius_miles Search radius
     * @return BlastResult Results
     */
    BlastResult sendFosterNeededAlert(const core::models::Dog& dog, double radius_miles = 0);

    /**
     * @brief Notify rescue organizations
     *
     * Targets rescue organizations that could potentially pull the dog.
     *
     * @param dog The dog needing rescue
     * @param radius_miles Search radius
     * @return int Number of rescues notified
     */
    int notifyRescueOrganizations(const core::models::Dog& dog, double radius_miles = 0);

    // =========================================================================
    // TARGETING
    // =========================================================================

    /**
     * @brief Find all users eligible for urgent notification
     *
     * @param dog The dog
     * @param radius_miles Search radius
     * @param include_fosters Include foster homes
     * @param include_rescues Include rescue organizations
     * @return std::vector<std::string> List of user IDs
     */
    std::vector<std::string> findEligibleUsers(const core::models::Dog& dog,
                                                double radius_miles,
                                                bool include_fosters = true,
                                                bool include_rescues = true);

    /**
     * @brief Find foster homes matching dog criteria
     *
     * @param dog The dog
     * @param radius_miles Search radius
     * @return std::vector<std::string> List of foster user IDs
     */
    std::vector<std::string> findMatchingFosters(const core::models::Dog& dog,
                                                  double radius_miles);

    /**
     * @brief Find rescue organizations in area
     *
     * @param state_code State code
     * @param radius_miles Search radius from dog's shelter
     * @param shelter_lat Shelter latitude
     * @param shelter_lon Shelter longitude
     * @return std::vector<std::string> List of rescue organization IDs
     */
    std::vector<std::string> findNearbyRescues(const std::string& state_code,
                                                double radius_miles,
                                                double shelter_lat,
                                                double shelter_lon);

    // =========================================================================
    // COOLDOWN MANAGEMENT
    // =========================================================================

    /**
     * @brief Check if blast is on cooldown for dog
     *
     * Prevents alert fatigue from repeated blasts for same dog.
     *
     * @param dog_id Dog UUID
     * @return true if on cooldown
     */
    bool isOnCooldown(const std::string& dog_id);

    /**
     * @brief Set cooldown for a dog
     *
     * @param dog_id Dog UUID
     */
    void setCooldown(const std::string& dog_id);

    /**
     * @brief Clear cooldown for a dog
     *
     * @param dog_id Dog UUID
     */
    void clearCooldown(const std::string& dog_id);

    // =========================================================================
    // INTEGRATION
    // =========================================================================

    /**
     * @brief Integrate with UrgencyWorker
     *
     * Called by UrgencyWorker when dog urgency changes.
     *
     * @param dog_id Dog UUID
     * @param new_urgency_level New urgency level string
     * @param old_urgency_level Previous urgency level string
     */
    void onUrgencyChanged(const std::string& dog_id,
                          const std::string& new_urgency_level,
                          const std::string& old_urgency_level);

    /**
     * @brief Subscribe to urgency events
     *
     * Registers with EventBus to receive urgency change events.
     */
    void subscribeToEvents();

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get service statistics
     * @return Json::Value Statistics JSON
     */
    Json::Value getStats() const;

    /**
     * @brief Get recent blast history
     *
     * @param limit Maximum entries to return
     * @return std::vector<BlastResult> Recent blasts
     */
    std::vector<BlastResult> getRecentBlasts(int limit = 10) const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    UrgentAlertService() = default;
    ~UrgentAlertService() = default;

    /**
     * @brief Get shelter info for a dog
     */
    std::optional<core::models::Shelter> getShelterForDog(const std::string& dog_id);

    /**
     * @brief Calculate appropriate radius based on urgency
     */
    double calculateRadius(const core::models::Dog& dog) const;

    /**
     * @brief Build urgent notification for dog
     */
    Notification buildUrgentNotification(const core::models::Dog& dog,
                                         const core::models::Shelter& shelter,
                                         NotificationType type);

    /**
     * @brief Log blast to database
     */
    void logBlast(const BlastResult& result, const std::string& dog_id);

    /**
     * @brief Emit blast event
     */
    void emitBlastEvent(const BlastResult& result, const std::string& dog_id);

    // Configuration
    UrgentAlertConfig config_;

    // Cooldown tracking
    std::mutex cooldown_mutex_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> cooldowns_;

    // Statistics
    mutable std::mutex stats_mutex_;
    uint64_t total_blasts_ = 0;
    uint64_t total_users_notified_ = 0;
    uint64_t total_critical_alerts_ = 0;
    std::vector<BlastResult> recent_blasts_;
};

} // namespace wtl::notifications
