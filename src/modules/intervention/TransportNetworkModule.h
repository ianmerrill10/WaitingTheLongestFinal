/**
 * @file TransportNetworkModule.h
 * @brief Module for coordinating transport of dogs between shelters and adopters
 *
 * PURPOSE:
 * Manages a network of volunteer transporters to move dogs from high-kill
 * shelters to rescue organizations, foster homes, or adopters in different
 * geographic areas. Enables cross-country rescue coordination.
 *
 * USAGE:
 * Enable in config.json:
 * {
 *     "modules": {
 *         "transport_network": {
 *             "enabled": true,
 *             "max_transport_distance_miles": 2000,
 *             "max_leg_distance_miles": 300
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
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include <json/json.h>

#include "modules/IModule.h"
#include "core/EventBus.h"

namespace wtl::modules::intervention {

/**
 * @struct GeoLocation
 * @brief Geographic coordinates
 */
struct GeoLocation {
    double latitude;
    double longitude;
    std::string city;
    std::string state;
    std::string zip_code;
};

/**
 * @struct Transporter
 * @brief A volunteer transporter
 */
struct Transporter {
    std::string id;
    std::string name;
    std::string email;
    std::string phone;
    GeoLocation home_location;
    int max_distance_miles;
    std::vector<std::string> available_days; // "monday", "tuesday", etc.
    std::string vehicle_type; // suv, sedan, van, truck
    int max_dogs;
    bool has_crate;
    bool active;
    std::string verified_date;
    int completed_transports;
    double rating;
};

/**
 * @struct TransportLeg
 * @brief A single leg of a transport journey
 */
struct TransportLeg {
    std::string id;
    int leg_number;
    std::string transporter_id;
    std::string transporter_name;
    GeoLocation start_location;
    GeoLocation end_location;
    double distance_miles;
    std::string scheduled_date;
    std::string scheduled_time;
    std::string status; // pending, confirmed, in_progress, completed, cancelled
    std::string notes;
};

/**
 * @struct TransportRequest
 * @brief A request to transport a dog
 */
struct TransportRequest {
    std::string id;
    std::string dog_id;
    std::string dog_name;
    std::string dog_size; // small, medium, large, xlarge
    bool special_needs;
    std::string special_needs_notes;
    GeoLocation origin;
    GeoLocation destination;
    std::string origin_contact;
    std::string destination_contact;
    std::string preferred_date;
    std::string urgency; // normal, urgent, critical
    std::string status; // pending, planning, confirmed, in_progress, completed, cancelled
    std::vector<TransportLeg> legs;
    double total_distance;
    std::string created_at;
    std::string updated_at;
};

/**
 * @struct TransportRoute
 * @brief A calculated route with legs
 */
struct TransportRoute {
    std::string request_id;
    std::vector<TransportLeg> legs;
    double total_distance;
    int estimated_days;
    bool feasible;
    std::string notes;
};

/**
 * @class TransportNetworkModule
 * @brief Module for coordinating dog transport logistics
 *
 * Features:
 * - Volunteer transporter management
 * - Multi-leg route planning
 * - Automatic transporter matching
 * - Real-time transport tracking
 * - Integration with shelter systems
 * - Transport history and statistics
 */
class TransportNetworkModule : public IModule {
public:
    TransportNetworkModule() = default;
    ~TransportNetworkModule() override = default;

    // ========================================================================
    // IModule INTERFACE - LIFECYCLE
    // ========================================================================

    bool initialize() override;
    void shutdown() override;
    void onConfigUpdate(const Json::Value& config) override;

    // ========================================================================
    // IModule INTERFACE - IDENTITY
    // ========================================================================

    std::string getName() const override { return "transport_network"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "Coordinates volunteer transport network for moving dogs between locations";
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
    // TRANSPORTER MANAGEMENT
    // ========================================================================

    /**
     * @brief Register a new volunteer transporter
     * @param transporter Transporter details
     * @return The registered transporter with ID
     */
    Transporter registerTransporter(const Transporter& transporter);

    /**
     * @brief Update transporter information
     * @param transporter Updated transporter data
     * @return true if updated successfully
     */
    bool updateTransporter(const Transporter& transporter);

    /**
     * @brief Get transporter by ID
     * @param transporter_id The transporter ID
     * @return The transporter if found
     */
    std::optional<Transporter> getTransporter(const std::string& transporter_id);

    /**
     * @brief Find available transporters for a route segment
     * @param start Start location
     * @param end End location
     * @param date Preferred date
     * @return List of available transporters
     */
    std::vector<Transporter> findAvailableTransporters(
        const GeoLocation& start,
        const GeoLocation& end,
        const std::string& date);

    /**
     * @brief Set transporter availability
     * @param transporter_id The transporter ID
     * @param active Whether transporter is active
     * @return true if updated
     */
    bool setTransporterAvailability(const std::string& transporter_id, bool active);

    // ========================================================================
    // TRANSPORT REQUESTS
    // ========================================================================

    /**
     * @brief Create a transport request
     * @param request The transport request details
     * @return The created request with ID
     */
    TransportRequest createTransportRequest(const TransportRequest& request);

    /**
     * @brief Get transport request by ID
     * @param request_id The request ID
     * @return The request if found
     */
    std::optional<TransportRequest> getTransportRequest(const std::string& request_id);

    /**
     * @brief Calculate a route for a transport request
     * @param request_id The request ID
     * @return The calculated route
     */
    TransportRoute calculateRoute(const std::string& request_id);

    /**
     * @brief Assign transporters to a route
     * @param request_id The request ID
     * @param route The route with transporter assignments
     * @return true if assigned successfully
     */
    bool assignTransporters(const std::string& request_id, const TransportRoute& route);

    /**
     * @brief Confirm a transport leg
     * @param request_id The request ID
     * @param leg_id The leg ID
     * @return true if confirmed
     */
    bool confirmLeg(const std::string& request_id, const std::string& leg_id);

    /**
     * @brief Complete a transport leg
     * @param request_id The request ID
     * @param leg_id The leg ID
     * @param notes Completion notes
     * @return true if completed
     */
    bool completeLeg(const std::string& request_id, const std::string& leg_id,
                     const std::string& notes);

    /**
     * @brief Cancel a transport request
     * @param request_id The request ID
     * @param reason Cancellation reason
     * @return true if cancelled
     */
    bool cancelTransport(const std::string& request_id, const std::string& reason);

    /**
     * @brief Get pending transport requests
     * @return List of pending requests
     */
    std::vector<TransportRequest> getPendingRequests();

    /**
     * @brief Get active transports
     * @return List of in-progress transports
     */
    std::vector<TransportRequest> getActiveTransports();

    // ========================================================================
    // UTILITIES
    // ========================================================================

    /**
     * @brief Calculate distance between two locations
     * @param loc1 First location
     * @param loc2 Second location
     * @return Distance in miles
     */
    double calculateDistance(const GeoLocation& loc1, const GeoLocation& loc2);

    /**
     * @brief Get transport statistics
     * @param start_date Start of period
     * @param end_date End of period
     * @return Statistics for the period
     */
    Json::Value getTransportStats(const std::string& start_date,
                                   const std::string& end_date);

private:
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    void handleTransportNeeded(const core::Event& event);
    void handleTransporterSignup(const core::Event& event);

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    std::string generateTransporterId();
    std::string generateRequestId();
    std::string generateLegId();
    std::vector<GeoLocation> calculateWaypoints(const GeoLocation& origin,
                                                  const GeoLocation& destination);
    bool isTransporterAvailable(const Transporter& transporter,
                                const GeoLocation& start,
                                const GeoLocation& end,
                                const std::string& date);
    void notifyTransporters(const TransportRequest& request);
    void updateTransporterStats(const std::string& transporter_id, bool success);

    // ========================================================================
    // STATE
    // ========================================================================

    bool enabled_ = false;
    int max_transport_distance_miles_ = 2000;
    int max_leg_distance_miles_ = 300;
    int min_leg_distance_miles_ = 50;

    // Subscription IDs for cleanup
    std::vector<std::string> subscription_ids_;

    // Data storage
    std::map<std::string, Transporter> transporters_;
    std::map<std::string, TransportRequest> transport_requests_;
    mutable std::mutex data_mutex_;

    // Metrics
    std::atomic<int64_t> total_transporters_{0};
    std::atomic<int64_t> active_transporters_{0};
    std::atomic<int64_t> transport_requests_created_{0};
    std::atomic<int64_t> transports_completed_{0};
    std::atomic<int64_t> legs_completed_{0};
    std::atomic<double> total_miles_transported_{0.0};
    std::atomic<int64_t> dogs_transported_{0};
};

} // namespace wtl::modules::intervention
