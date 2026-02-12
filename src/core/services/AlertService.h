/**
 * @file AlertService.h
 * @brief Service for triggering and managing urgency alerts
 *
 * PURPOSE:
 * Manages the alert system for urgent dogs. When a dog becomes critical
 * (< 24 hours) or high urgency, this service triggers appropriate alerts
 * to maximize the chance of rescue. Alerts are the call-to-action that
 * mobilizes the rescue community.
 *
 * ALERT TYPES:
 * - Critical Alert: < 24 hours - Emergency broadcast to all channels
 * - High Urgency Alert: 1-3 days - Targeted notifications
 * - New Euthanasia List: Batch alert when shelter updates list
 *
 * INTEGRATION:
 * - EventBus: Emits events for other systems to react
 * - BroadcastService: WebSocket alerts to connected clients
 * - NotificationService: Push notifications (future)
 *
 * USAGE:
 * auto& alerts = AlertService::getInstance();
 * alerts.triggerCriticalAlert(dog);
 * auto active = alerts.getActiveAlerts();
 *
 * DEPENDENCIES:
 * - Dog, Shelter models (Agent 3)
 * - UrgencyLevel (this agent)
 * - EventBus (Agent 10)
 * - BroadcastService (Agent 5)
 * - ConnectionPool (Agent 1)
 * - ErrorCapture (Agent 1)
 *
 * MODIFICATION GUIDE:
 * - To add new alert types: Add to AlertType enum, update trigger methods
 * - To add new channels: Update the broadcast methods
 * - All alerts must be logged for audit purposes
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <functional>
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
 * @enum AlertType
 * @brief Types of urgency alerts
 */
enum class AlertType {
    CRITICAL,          ///< Dog has < 24 hours
    HIGH_URGENCY,      ///< Dog has 1-3 days
    EUTHANASIA_LIST,   ///< New euthanasia list from shelter
    SHELTER_CRITICAL,  ///< Shelter has multiple critical dogs
    DATE_CHANGED,      ///< Euthanasia date changed (sooner)
    RESCUED            ///< Good news - dog was rescued (for morale)
};

/**
 * @brief Convert AlertType to string
 */
inline std::string alertTypeToString(AlertType type) {
    switch (type) {
        case AlertType::CRITICAL:
            return "critical";
        case AlertType::HIGH_URGENCY:
            return "high_urgency";
        case AlertType::EUTHANASIA_LIST:
            return "euthanasia_list";
        case AlertType::SHELTER_CRITICAL:
            return "shelter_critical";
        case AlertType::DATE_CHANGED:
            return "date_changed";
        case AlertType::RESCUED:
            return "rescued";
        default:
            return "unknown";
    }
}

/**
 * @struct UrgencyAlert
 * @brief Represents a single urgency alert
 */
struct UrgencyAlert {
    std::string id;             ///< Alert UUID
    std::string dog_id;         ///< Dog this alert is for
    std::string dog_name;       ///< Dog name (for display)
    std::string shelter_id;     ///< Shelter where dog is located
    std::string shelter_name;   ///< Shelter name (for display)
    std::string state_code;     ///< State code for geographic filtering

    AlertType alert_type;       ///< Type of alert
    UrgencyLevel urgency_level; ///< Current urgency level

    std::string message;        ///< Alert message text
    std::string action_url;     ///< URL to take action (dog profile)

    /// Time remaining (if applicable)
    std::optional<int> hours_remaining;
    std::optional<int> days_remaining;

    /// Timestamps
    std::chrono::system_clock::time_point created_at;
    std::optional<std::chrono::system_clock::time_point> acknowledged_at;
    std::optional<std::chrono::system_clock::time_point> resolved_at;

    /// Status
    bool acknowledged;
    bool resolved;
    std::string resolution;     ///< "adopted", "fostered", "rescued", "expired"

    /// Who acknowledged/resolved
    std::optional<std::string> acknowledged_by;
    std::optional<std::string> resolved_by;

    /**
     * @brief Check if alert is still active
     */
    bool isActive() const {
        return !resolved;
    }

    /**
     * @brief Convert to JSON for API responses
     */
    Json::Value toJson() const {
        Json::Value json;
        json["id"] = id;
        json["dog_id"] = dog_id;
        json["dog_name"] = dog_name;
        json["shelter_id"] = shelter_id;
        json["shelter_name"] = shelter_name;
        json["state_code"] = state_code;

        json["alert_type"] = alertTypeToString(alert_type);
        json["urgency_level"] = urgencyToString(urgency_level);
        json["urgency_color"] = getUrgencyColor(urgency_level);

        json["message"] = message;
        json["action_url"] = action_url;

        if (hours_remaining.has_value()) {
            json["hours_remaining"] = hours_remaining.value();
        }
        if (days_remaining.has_value()) {
            json["days_remaining"] = days_remaining.value();
        }

        auto created_time = std::chrono::system_clock::to_time_t(created_at);
        json["created_at"] = static_cast<Json::Int64>(created_time);

        json["acknowledged"] = acknowledged;
        json["resolved"] = resolved;

        if (acknowledged && acknowledged_at.has_value()) {
            auto ack_time = std::chrono::system_clock::to_time_t(acknowledged_at.value());
            json["acknowledged_at"] = static_cast<Json::Int64>(ack_time);
        }

        if (resolved && resolved_at.has_value()) {
            auto res_time = std::chrono::system_clock::to_time_t(resolved_at.value());
            json["resolved_at"] = static_cast<Json::Int64>(res_time);
            json["resolution"] = resolution;
        }

        return json;
    }
};

/**
 * @brief Callback type for alert handlers
 */
using AlertHandler = std::function<void(const UrgencyAlert&)>;

/**
 * @class AlertService
 * @brief Singleton service for managing urgency alerts
 *
 * Thread-safe singleton that handles alert creation, distribution,
 * and lifecycle management.
 */
class AlertService {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to the AlertService instance
     */
    static AlertService& getInstance();

    // Prevent copying and assignment
    AlertService(const AlertService&) = delete;
    AlertService& operator=(const AlertService&) = delete;

    // ========================================================================
    // TRIGGER ALERTS
    // ========================================================================

    /**
     * @brief Trigger critical alert for a dog (< 24 hours)
     *
     * This is the highest priority alert. Triggers:
     * - Emergency broadcast to all connected clients
     * - Event emission to EventBus
     * - Database logging
     * - (Future) Push notifications, SMS alerts
     *
     * @param dog The dog in critical condition
     * @param shelter The shelter where the dog is located
     * @return The created alert
     */
    UrgencyAlert triggerCriticalAlert(const models::Dog& dog,
                                       const models::Shelter& shelter);

    /**
     * @brief Trigger high urgency alert for a dog (1-3 days)
     *
     * @param dog The dog with high urgency
     * @param shelter The shelter where the dog is located
     * @return The created alert
     */
    UrgencyAlert triggerHighUrgencyAlert(const models::Dog& dog,
                                          const models::Shelter& shelter);

    /**
     * @brief Trigger alert for new euthanasia list
     *
     * Called when a shelter updates their euthanasia list.
     *
     * @param shelter The shelter that updated the list
     * @param dogs Dogs on the new euthanasia list
     * @return Vector of created alerts (one per critical dog)
     */
    std::vector<UrgencyAlert> triggerNewEuthanasiaListAlert(
        const models::Shelter& shelter,
        const std::vector<models::Dog>& dogs);

    /**
     * @brief Trigger alert when euthanasia date moves sooner
     *
     * @param dog The affected dog
     * @param shelter The shelter
     * @param old_date Previous euthanasia date
     * @param new_date New (sooner) euthanasia date
     * @return The created alert
     */
    UrgencyAlert triggerDateChangedAlert(
        const models::Dog& dog,
        const models::Shelter& shelter,
        const std::chrono::system_clock::time_point& old_date,
        const std::chrono::system_clock::time_point& new_date);

    /**
     * @brief Trigger positive alert when dog is rescued
     *
     * Good news alerts help maintain morale and show impact.
     *
     * @param dog The rescued dog
     * @param shelter The shelter
     * @param outcome "adopted", "fostered", "rescue_pulled"
     * @return The created alert
     */
    UrgencyAlert triggerRescuedAlert(const models::Dog& dog,
                                      const models::Shelter& shelter,
                                      const std::string& outcome);

    // ========================================================================
    // QUERY ALERTS
    // ========================================================================

    /**
     * @brief Get all active (unresolved) alerts
     *
     * @return std::vector<UrgencyAlert> Active alerts, sorted by urgency
     */
    std::vector<UrgencyAlert> getActiveAlerts();

    /**
     * @brief Get active alerts for a specific shelter
     *
     * @param shelter_id UUID of the shelter
     * @return std::vector<UrgencyAlert> Active alerts for this shelter
     */
    std::vector<UrgencyAlert> getAlertsForShelter(const std::string& shelter_id);

    /**
     * @brief Get active alerts for a specific state
     *
     * @param state_code Two-letter state code
     * @return std::vector<UrgencyAlert> Active alerts in this state
     */
    std::vector<UrgencyAlert> getAlertsForState(const std::string& state_code);

    /**
     * @brief Get alert by ID
     *
     * @param alert_id UUID of the alert
     * @return std::optional<UrgencyAlert> The alert if found
     */
    std::optional<UrgencyAlert> getAlertById(const std::string& alert_id);

    /**
     * @brief Get alerts for a specific dog
     *
     * @param dog_id UUID of the dog
     * @return std::vector<UrgencyAlert> All alerts for this dog
     */
    std::vector<UrgencyAlert> getAlertsForDog(const std::string& dog_id);

    /**
     * @brief Get critical alerts only
     *
     * @return std::vector<UrgencyAlert> Only critical (< 24 hours) alerts
     */
    std::vector<UrgencyAlert> getCriticalAlerts();

    // ========================================================================
    // ALERT MANAGEMENT
    // ========================================================================

    /**
     * @brief Acknowledge an alert
     *
     * Marks that someone has seen and is addressing the alert.
     *
     * @param alert_id UUID of the alert
     * @param user_id ID of user acknowledging (optional)
     */
    void acknowledgeAlert(const std::string& alert_id,
                          const std::string& user_id = "");

    /**
     * @brief Resolve an alert
     *
     * Marks the alert as resolved with an outcome.
     *
     * @param alert_id UUID of the alert
     * @param resolution "adopted", "fostered", "rescued", "expired"
     * @param user_id ID of user resolving (optional)
     */
    void resolveAlert(const std::string& alert_id,
                      const std::string& resolution,
                      const std::string& user_id = "");

    /**
     * @brief Auto-resolve alerts for a dog when adopted/fostered
     *
     * @param dog_id UUID of the dog
     * @param resolution The outcome
     */
    void resolveAlertsForDog(const std::string& dog_id,
                              const std::string& resolution);

    // ========================================================================
    // EVENT INTEGRATION
    // ========================================================================

    /**
     * @brief Emit urgency event to EventBus
     *
     * @param dog The dog whose urgency changed
     * @param new_level The new urgency level
     */
    void emitUrgencyEvent(const models::Dog& dog, UrgencyLevel new_level);

    // ========================================================================
    // ALERT HANDLERS
    // ========================================================================

    /**
     * @brief Register a handler for critical alerts
     *
     * @param handler Function to call when critical alert triggers
     * @return Handler ID for unregistration
     */
    std::string registerCriticalHandler(AlertHandler handler);

    /**
     * @brief Register a handler for all alerts
     *
     * @param handler Function to call for any alert
     * @return Handler ID for unregistration
     */
    std::string registerAlertHandler(AlertHandler handler);

    /**
     * @brief Unregister a handler
     *
     * @param handler_id ID returned from registration
     */
    void unregisterHandler(const std::string& handler_id);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get alert statistics
     */
    Json::Value getStatistics();

    /**
     * @brief Get statistics for a specific shelter
     */
    Json::Value getShelterStatistics(const std::string& shelter_id);

private:
    // Private constructor for singleton
    AlertService();
    ~AlertService() = default;

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================

    /**
     * @brief Generate a UUID for alerts
     */
    std::string generateAlertId();

    /**
     * @brief Create alert message based on type and dog info
     */
    std::string createAlertMessage(AlertType type,
                                    const models::Dog& dog,
                                    const models::Shelter& shelter,
                                    std::optional<int> hours_remaining = std::nullopt);

    /**
     * @brief Save alert to database
     */
    void saveAlert(const UrgencyAlert& alert);

    /**
     * @brief Broadcast alert to connected clients
     */
    void broadcastAlert(const UrgencyAlert& alert);

    /**
     * @brief Emit event to EventBus
     */
    void emitAlertEvent(const UrgencyAlert& alert);

    /**
     * @brief Invoke registered handlers
     */
    void invokeHandlers(const UrgencyAlert& alert);

    /**
     * @brief Calculate time remaining for display
     */
    std::pair<std::optional<int>, std::optional<int>> calculateTimeRemaining(
        const std::optional<std::chrono::system_clock::time_point>& euthanasia_date);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    /// Mutex for thread-safe operations
    mutable std::mutex mutex_;

    /// In-memory cache of active alerts (for quick access)
    std::vector<UrgencyAlert> active_alerts_cache_;
    bool cache_valid_ = false;

    /// Registered handlers
    std::vector<std::pair<std::string, AlertHandler>> critical_handlers_;
    std::vector<std::pair<std::string, AlertHandler>> all_handlers_;

    /// Statistics counters
    int total_alerts_created_ = 0;
    int total_critical_alerts_ = 0;
    int total_resolved_ = 0;
    int total_adopted_ = 0;
    int total_fostered_ = 0;
    int total_rescued_ = 0;
};

} // namespace wtl::core::services
