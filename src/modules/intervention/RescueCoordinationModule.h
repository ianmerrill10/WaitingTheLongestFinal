/**
 * @file RescueCoordinationModule.h
 * @brief Module for coordinating rescue operations between shelters and rescues
 *
 * PURPOSE:
 * Facilitates communication and coordination between kill shelters, rescue
 * organizations, and foster networks to maximize the number of dogs saved.
 * Handles rescue requests, capacity tracking, and emergency coordination.
 *
 * USAGE:
 * Enable in config.json:
 * {
 *     "modules": {
 *         "rescue_coordination": {
 *             "enabled": true,
 *             "auto_alert_rescues": true,
 *             "critical_hours_threshold": 24
 *         }
 *     }
 * }
 *
 * DEPENDENCIES:
 * - IModule.h (module interface)
 * - EventBus.h (event subscription)
 *
 * @author WaitingTheLongest Integration Agent
 * @date 2024-01-28
 */

#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <queue>

#include <json/json.h>

#include "modules/IModule.h"
#include "core/EventBus.h"

namespace wtl::modules::intervention {

/**
 * @struct RescueOrganization
 * @brief A rescue organization in the network
 */
struct RescueOrganization {
    std::string id;
    std::string name;
    std::string type; // breed_specific, all_breed, senior, special_needs
    std::string city;
    std::string state;
    std::string contact_email;
    std::string contact_phone;
    std::vector<std::string> accepted_breeds;
    std::vector<std::string> specializations; // medical, behavioral, hospice
    int current_capacity;
    int max_capacity;
    int foster_network_size;
    bool accepts_urgent;
    bool active;
    double response_rate; // 0-100%
    double avg_response_hours;
    std::string last_active_date;
};

/**
 * @struct RescueRequest
 * @brief A request for rescue assistance
 */
struct RescueRequest {
    std::string id;
    std::string dog_id;
    std::string dog_name;
    std::string breed;
    std::string size;
    std::string shelter_id;
    std::string shelter_name;
    std::string shelter_city;
    std::string shelter_state;
    std::string urgency; // normal, urgent, critical, emergency
    int hours_until_deadline;
    std::string special_needs;
    std::string medical_notes;
    std::string behavioral_notes;
    std::string status; // pending, in_progress, committed, completed, expired
    std::string created_at;
    std::string updated_at;
    std::vector<std::string> contacted_rescues;
    std::vector<std::string> declined_rescues;
    std::string committed_rescue_id;
    std::string commitment_date;
    std::string pickup_date;
};

/**
 * @struct RescueCommitment
 * @brief A commitment from a rescue to take a dog
 */
struct RescueCommitment {
    std::string id;
    std::string request_id;
    std::string rescue_id;
    std::string rescue_name;
    std::string committed_at;
    std::string planned_pickup_date;
    std::string transport_arranged; // self, volunteer_network, commercial
    std::string status; // pending_pickup, picked_up, in_foster, adopted, cancelled
    std::string notes;
};

/**
 * @struct NetworkStats
 * @brief Statistics about the rescue network
 */
struct NetworkStats {
    int total_rescues;
    int active_rescues;
    int total_capacity;
    int available_capacity;
    int foster_homes;
    int pending_requests;
    int dogs_saved_this_month;
    double avg_response_time_hours;
};

/**
 * @class RescueCoordinationModule
 * @brief Module for coordinating rescue operations
 *
 * Features:
 * - Rescue organization registry
 * - Automated rescue alerts for urgent dogs
 * - Capacity tracking across the network
 * - Commitment and pickup coordination
 * - Performance tracking for rescues
 * - Emergency coordination for high-kill situations
 */
class RescueCoordinationModule : public IModule {
public:
    RescueCoordinationModule() = default;
    ~RescueCoordinationModule() override = default;

    // ========================================================================
    // IModule INTERFACE - LIFECYCLE
    // ========================================================================

    bool initialize() override;
    void shutdown() override;
    void onConfigUpdate(const Json::Value& config) override;

    // ========================================================================
    // IModule INTERFACE - IDENTITY
    // ========================================================================

    std::string getName() const override { return "rescue_coordination"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "Coordinates rescue operations between shelters and rescue organizations";
    }
    std::vector<std::string> getDependencies() const override {
        return {}; // No dependencies
    }

    // ========================================================================
    // IModule INTERFACE - STATUS
    // ========================================================================

    bool isEnabled() const override { return enabled_; }
    bool isHealthy() const override;
    Json::Value getStatus() const override;
    Json::Value getMetrics() const override;

    // ========================================================================
    // IModule INTERFACE - CONFIGURATION
    // ========================================================================

    void configure(const Json::Value& config) override;
    Json::Value getDefaultConfig() const override;

    // ========================================================================
    // RESCUE ORGANIZATION MANAGEMENT
    // ========================================================================

    /**
     * @brief Register a rescue organization
     * @param rescue The rescue organization details
     * @return The registered rescue with ID
     */
    RescueOrganization registerRescue(const RescueOrganization& rescue);

    /**
     * @brief Update rescue organization details
     * @param rescue Updated rescue data
     * @return true if updated
     */
    bool updateRescue(const RescueOrganization& rescue);

    /**
     * @brief Get rescue organization by ID
     * @param rescue_id The rescue ID
     * @return The rescue if found
     */
    std::optional<RescueOrganization> getRescue(const std::string& rescue_id);

    /**
     * @brief Find rescues that can help with a specific dog
     * @param breed The dog's breed
     * @param size The dog's size
     * @param special_needs Any special needs
     * @param state State to search in (or nearby)
     * @return List of matching rescues
     */
    std::vector<RescueOrganization> findMatchingRescues(
        const std::string& breed,
        const std::string& size,
        const std::string& special_needs,
        const std::string& state);

    /**
     * @brief Update rescue capacity
     * @param rescue_id The rescue ID
     * @param current_capacity Current number of dogs
     * @return true if updated
     */
    bool updateCapacity(const std::string& rescue_id, int current_capacity);

    /**
     * @brief Get rescues with available capacity
     * @param min_spaces Minimum spaces needed
     * @return List of rescues with capacity
     */
    std::vector<RescueOrganization> getRescuesWithCapacity(int min_spaces = 1);

    // ========================================================================
    // RESCUE REQUESTS
    // ========================================================================

    /**
     * @brief Create a rescue request for a dog
     * @param request The rescue request details
     * @return The created request with ID
     */
    RescueRequest createRescueRequest(const RescueRequest& request);

    /**
     * @brief Get rescue request by ID
     * @param request_id The request ID
     * @return The request if found
     */
    std::optional<RescueRequest> getRescueRequest(const std::string& request_id);

    /**
     * @brief Get all pending rescue requests
     * @return List of pending requests
     */
    std::vector<RescueRequest> getPendingRequests();

    /**
     * @brief Get critical requests (under 24 hours)
     * @return List of critical requests
     */
    std::vector<RescueRequest> getCriticalRequests();

    /**
     * @brief Alert rescues about a request
     * @param request_id The request ID
     * @return Number of rescues alerted
     */
    int alertRescues(const std::string& request_id);

    /**
     * @brief Record a rescue declining a request
     * @param request_id The request ID
     * @param rescue_id The declining rescue ID
     * @param reason Reason for decline
     */
    void recordDecline(const std::string& request_id,
                       const std::string& rescue_id,
                       const std::string& reason);

    // ========================================================================
    // COMMITMENTS
    // ========================================================================

    /**
     * @brief Commit a rescue to take a dog
     * @param request_id The rescue request ID
     * @param rescue_id The committing rescue ID
     * @param pickup_date Planned pickup date
     * @return The commitment if successful
     */
    std::optional<RescueCommitment> commitToRescue(
        const std::string& request_id,
        const std::string& rescue_id,
        const std::string& pickup_date);

    /**
     * @brief Update commitment status
     * @param commitment_id The commitment ID
     * @param status New status
     * @param notes Additional notes
     * @return true if updated
     */
    bool updateCommitment(const std::string& commitment_id,
                          const std::string& status,
                          const std::string& notes = "");

    /**
     * @brief Cancel a commitment
     * @param commitment_id The commitment ID
     * @param reason Cancellation reason
     * @return true if cancelled
     */
    bool cancelCommitment(const std::string& commitment_id, const std::string& reason);

    /**
     * @brief Get active commitments for a rescue
     * @param rescue_id The rescue ID
     * @return List of active commitments
     */
    std::vector<RescueCommitment> getCommitmentsForRescue(const std::string& rescue_id);

    // ========================================================================
    // NETWORK STATISTICS
    // ========================================================================

    /**
     * @brief Get network statistics
     * @return Current network stats
     */
    NetworkStats getNetworkStats();

    /**
     * @brief Get rescue performance stats
     * @param rescue_id The rescue ID
     * @return Performance statistics
     */
    Json::Value getRescuePerformance(const std::string& rescue_id);

    /**
     * @brief Get coordination statistics
     * @param start_date Start of period
     * @param end_date End of period
     * @return Statistics for the period
     */
    Json::Value getCoordinationStats(const std::string& start_date,
                                      const std::string& end_date);

private:
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    void handleDogBecameCritical(const core::Event& event);
    void handleRescueRequestReceived(const core::Event& event);
    void handleRescueCapacityUpdate(const core::Event& event);

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    std::string generateRescueId();
    std::string generateRequestId();
    std::string generateCommitmentId();
    void checkForExpiringRequests();
    void updateRescueResponseStats(const std::string& rescue_id, bool responded, double hours);
    std::vector<RescueOrganization> filterRescuesByBreed(
        const std::vector<RescueOrganization>& rescues,
        const std::string& breed);

    // ========================================================================
    // STATE
    // ========================================================================

    bool enabled_ = false;
    bool auto_alert_rescues_ = true;
    int critical_hours_threshold_ = 24;
    int urgent_hours_threshold_ = 72;

    // Subscription IDs for cleanup
    std::vector<std::string> subscription_ids_;

    // Data storage
    std::map<std::string, RescueOrganization> rescues_;
    std::map<std::string, RescueRequest> rescue_requests_;
    std::map<std::string, RescueCommitment> commitments_;
    mutable std::mutex data_mutex_;

    // Metrics
    std::atomic<int64_t> rescues_registered_{0};
    std::atomic<int64_t> requests_created_{0};
    std::atomic<int64_t> commitments_made_{0};
    std::atomic<int64_t> dogs_saved_{0};
    std::atomic<int64_t> alerts_sent_{0};
    std::atomic<int64_t> requests_expired_{0};
};

} // namespace wtl::modules::intervention
