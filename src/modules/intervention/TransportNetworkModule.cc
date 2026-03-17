/**
 * @file TransportNetworkModule.cc
 * @brief Implementation of the TransportNetworkModule
 * @see TransportNetworkModule.h for documentation
 */

#include "TransportNetworkModule.h"
#include "modules/ModuleManager.h"
#include "core/EventBus.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>

namespace wtl::modules::intervention {

// Constants for distance calculation
constexpr double EARTH_RADIUS_MILES = 3959.0;
constexpr double DEG_TO_RAD = 3.14159265358979323846 / 180.0;

// ============================================================================
// LIFECYCLE
// ============================================================================

bool TransportNetworkModule::initialize() {
    std::cout << "[TransportNetworkModule] Initializing..." << std::endl;

    // Subscribe to events
    auto& event_bus = core::EventBus::getInstance();

    auto needed_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "transport_needed") {
                handleTransportNeeded(event);
            }
        },
        "transport_network_needed_handler"
    );
    subscription_ids_.push_back(needed_id);

    auto signup_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "transporter_signup") {
                handleTransporterSignup(event);
            }
        },
        "transport_network_signup_handler"
    );
    subscription_ids_.push_back(signup_id);

    enabled_ = true;
    std::cout << "[TransportNetworkModule] Initialization complete." << std::endl;
    std::cout << "[TransportNetworkModule] Max transport distance: "
              << max_transport_distance_miles_ << " miles" << std::endl;
    std::cout << "[TransportNetworkModule] Max leg distance: "
              << max_leg_distance_miles_ << " miles" << std::endl;

    return true;
}

void TransportNetworkModule::shutdown() {
    std::cout << "[TransportNetworkModule] Shutting down..." << std::endl;

    auto& event_bus = core::EventBus::getInstance();
    for (const auto& id : subscription_ids_) {
        event_bus.unsubscribe(id);
    }
    subscription_ids_.clear();

    enabled_ = false;
    std::cout << "[TransportNetworkModule] Shutdown complete." << std::endl;
}

void TransportNetworkModule::onConfigUpdate(const Json::Value& config) {
    configure(config);
}

// ============================================================================
// STATUS
// ============================================================================

bool TransportNetworkModule::isHealthy() const {
    return enabled_;
}

Json::Value TransportNetworkModule::getStatus() const {
    Json::Value status;
    status["name"] = getName();
    status["version"] = getVersion();
    status["enabled"] = enabled_;
    status["healthy"] = isHealthy();
    status["max_transport_distance_miles"] = max_transport_distance_miles_;
    status["max_leg_distance_miles"] = max_leg_distance_miles_;

    std::lock_guard<std::mutex> lock(data_mutex_);
    status["total_transporters"] = static_cast<int>(transporters_.size());
    status["active_transporters"] = static_cast<int>(active_transporters_.load());
    status["pending_requests"] = 0;
    status["active_transports"] = 0;

    int pending = 0, active = 0;
    for (const auto& [id, req] : transport_requests_) {
        if (req.status == "pending" || req.status == "planning") pending++;
        else if (req.status == "confirmed" || req.status == "in_progress") active++;
    }
    status["pending_requests"] = pending;
    status["active_transports"] = active;

    return status;
}

Json::Value TransportNetworkModule::getMetrics() const {
    Json::Value metrics;
    metrics["total_transporters"] = static_cast<Json::Int64>(total_transporters_.load());
    metrics["active_transporters"] = static_cast<Json::Int64>(active_transporters_.load());
    metrics["transport_requests"] = static_cast<Json::Int64>(transport_requests_created_.load());
    metrics["transports_completed"] = static_cast<Json::Int64>(transports_completed_.load());
    metrics["legs_completed"] = static_cast<Json::Int64>(legs_completed_.load());
    metrics["total_miles"] = total_miles_transported_.load();
    metrics["dogs_transported"] = static_cast<Json::Int64>(dogs_transported_.load());

    // Calculate averages
    int64_t completed = transports_completed_.load();
    if (completed > 0) {
        metrics["avg_miles_per_transport"] = total_miles_transported_.load() / completed;
        metrics["avg_legs_per_transport"] = static_cast<double>(legs_completed_.load()) / completed;
    }

    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void TransportNetworkModule::configure(const Json::Value& config) {
    if (config.isMember("max_transport_distance_miles")) {
        max_transport_distance_miles_ = config["max_transport_distance_miles"].asInt();
    }
    if (config.isMember("max_leg_distance_miles")) {
        max_leg_distance_miles_ = config["max_leg_distance_miles"].asInt();
    }
    if (config.isMember("min_leg_distance_miles")) {
        min_leg_distance_miles_ = config["min_leg_distance_miles"].asInt();
    }
}

Json::Value TransportNetworkModule::getDefaultConfig() const {
    Json::Value config;
    config["enabled"] = true;
    config["max_transport_distance_miles"] = 2000;
    config["max_leg_distance_miles"] = 300;
    config["min_leg_distance_miles"] = 50;
    return config;
}

// ============================================================================
// TRANSPORTER MANAGEMENT
// ============================================================================

Transporter TransportNetworkModule::registerTransporter(const Transporter& transporter) {
    Transporter new_transporter = transporter;
    new_transporter.id = generateTransporterId();
    new_transporter.active = true;
    new_transporter.completed_transports = 0;
    new_transporter.rating = 5.0;

    // Set verified date
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    new_transporter.verified_date = ss.str();

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        transporters_[new_transporter.id] = new_transporter;
    }

    total_transporters_++;
    active_transporters_++;

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "transporter_registered";
    event.data["transporter_id"] = new_transporter.id;
    event.data["location"] = new_transporter.home_location.city + ", " +
                             new_transporter.home_location.state;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[TransportNetworkModule] Transporter " << new_transporter.id
              << " registered: " << new_transporter.name << std::endl;

    return new_transporter;
}

bool TransportNetworkModule::updateTransporter(const Transporter& transporter) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = transporters_.find(transporter.id);
    if (it == transporters_.end()) {
        return false;
    }

    bool was_active = it->second.active;
    it->second = transporter;

    // Update active count if status changed
    if (was_active && !transporter.active) {
        active_transporters_--;
    } else if (!was_active && transporter.active) {
        active_transporters_++;
    }

    return true;
}

std::optional<Transporter> TransportNetworkModule::getTransporter(
    const std::string& transporter_id) {

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = transporters_.find(transporter_id);
    if (it != transporters_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<Transporter> TransportNetworkModule::findAvailableTransporters(
    const GeoLocation& start,
    const GeoLocation& end,
    const std::string& date) {

    std::vector<Transporter> available;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, transporter] : transporters_) {
        if (isTransporterAvailable(transporter, start, end, date)) {
            available.push_back(transporter);
        }
    }

    // Sort by rating and distance from start
    std::sort(available.begin(), available.end(),
        [this, &start](const Transporter& a, const Transporter& b) {
            double dist_a = calculateDistance(a.home_location, start);
            double dist_b = calculateDistance(b.home_location, start);
            // Prefer higher rating, then closer distance
            if (std::abs(a.rating - b.rating) > 0.1) {
                return a.rating > b.rating;
            }
            return dist_a < dist_b;
        });

    return available;
}

bool TransportNetworkModule::setTransporterAvailability(const std::string& transporter_id,
                                                         bool active) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = transporters_.find(transporter_id);
    if (it == transporters_.end()) {
        return false;
    }

    bool was_active = it->second.active;
    it->second.active = active;

    if (was_active && !active) {
        active_transporters_--;
    } else if (!was_active && active) {
        active_transporters_++;
    }

    return true;
}

// ============================================================================
// TRANSPORT REQUESTS
// ============================================================================

TransportRequest TransportNetworkModule::createTransportRequest(
    const TransportRequest& request) {

    TransportRequest new_request = request;
    new_request.id = generateRequestId();
    new_request.status = "pending";

    // Calculate total distance
    new_request.total_distance = calculateDistance(request.origin, request.destination);

    // Set timestamps
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    new_request.created_at = ss.str();
    new_request.updated_at = new_request.created_at;

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        transport_requests_[new_request.id] = new_request;
    }

    transport_requests_created_++;

    // Notify potential transporters
    notifyTransporters(new_request);

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "transport_request_created";
    event.data["request_id"] = new_request.id;
    event.data["dog_id"] = new_request.dog_id;
    event.data["distance"] = new_request.total_distance;
    event.data["urgency"] = new_request.urgency;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[TransportNetworkModule] Transport request " << new_request.id
              << " created for " << new_request.dog_name
              << " (" << new_request.total_distance << " miles)" << std::endl;

    return new_request;
}

std::optional<TransportRequest> TransportNetworkModule::getTransportRequest(
    const std::string& request_id) {

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = transport_requests_.find(request_id);
    if (it != transport_requests_.end()) {
        return it->second;
    }
    return std::nullopt;
}

TransportRoute TransportNetworkModule::calculateRoute(const std::string& request_id) {
    TransportRoute route;
    route.request_id = request_id;
    route.feasible = false;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = transport_requests_.find(request_id);
    if (it == transport_requests_.end()) {
        route.notes = "Request not found";
        return route;
    }

    const auto& request = it->second;

    // Check if distance is within limits
    if (request.total_distance > max_transport_distance_miles_) {
        route.notes = "Distance exceeds maximum transport distance";
        return route;
    }

    // Calculate waypoints for multi-leg transport
    std::vector<GeoLocation> waypoints = calculateWaypoints(request.origin, request.destination);

    // Create legs
    GeoLocation current_location = request.origin;
    int leg_number = 1;

    for (const auto& waypoint : waypoints) {
        TransportLeg leg;
        leg.id = generateLegId();
        leg.leg_number = leg_number++;
        leg.start_location = current_location;
        leg.end_location = waypoint;
        leg.distance_miles = calculateDistance(current_location, waypoint);
        leg.status = "pending";

        route.legs.push_back(leg);
        current_location = waypoint;
    }

    // Add final leg to destination
    if (current_location.city != request.destination.city) {
        TransportLeg final_leg;
        final_leg.id = generateLegId();
        final_leg.leg_number = leg_number;
        final_leg.start_location = current_location;
        final_leg.end_location = request.destination;
        final_leg.distance_miles = calculateDistance(current_location, request.destination);
        final_leg.status = "pending";
        route.legs.push_back(final_leg);
    }

    route.total_distance = request.total_distance;
    route.estimated_days = static_cast<int>(route.legs.size());
    route.feasible = !route.legs.empty();
    route.notes = route.feasible ? "Route calculated successfully" : "Unable to calculate route";

    std::cout << "[TransportNetworkModule] Route calculated for " << request_id
              << ": " << route.legs.size() << " legs, "
              << route.total_distance << " miles" << std::endl;

    return route;
}

bool TransportNetworkModule::assignTransporters(const std::string& request_id,
                                                  const TransportRoute& route) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = transport_requests_.find(request_id);
    if (it == transport_requests_.end()) {
        return false;
    }

    it->second.legs = route.legs;
    it->second.status = "confirmed";

    // Update timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    it->second.updated_at = ss.str();

    std::cout << "[TransportNetworkModule] Transporters assigned to request "
              << request_id << std::endl;

    return true;
}

bool TransportNetworkModule::confirmLeg(const std::string& request_id,
                                          const std::string& leg_id) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = transport_requests_.find(request_id);
    if (it == transport_requests_.end()) {
        return false;
    }

    for (auto& leg : it->second.legs) {
        if (leg.id == leg_id) {
            leg.status = "confirmed";
            return true;
        }
    }

    return false;
}

bool TransportNetworkModule::completeLeg(const std::string& request_id,
                                           const std::string& leg_id,
                                           const std::string& notes) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = transport_requests_.find(request_id);
    if (it == transport_requests_.end()) {
        return false;
    }

    bool all_completed = true;
    double completed_miles = 0;

    for (auto& leg : it->second.legs) {
        if (leg.id == leg_id) {
            leg.status = "completed";
            leg.notes = notes;
            completed_miles = leg.distance_miles;

            // Update transporter stats
            if (!leg.transporter_id.empty()) {
                // Note: This is outside the lock in production
                // updateTransporterStats(leg.transporter_id, true);
            }
        }
        if (leg.status != "completed") {
            all_completed = false;
        }
    }

    legs_completed_++;
    auto current = total_miles_transported_.load();
    while (!total_miles_transported_.compare_exchange_weak(current, current + completed_miles));

    // Check if all legs completed
    if (all_completed) {
        it->second.status = "completed";
        transports_completed_++;
        dogs_transported_++;

        // Publish completion event
        core::Event event;
        event.type = core::EventType::CUSTOM;
        event.data["type"] = "transport_completed";
        event.data["request_id"] = request_id;
        event.data["dog_id"] = it->second.dog_id;
        event.data["total_miles"] = it->second.total_distance;
        core::EventBus::getInstance().publishAsync(event);

        std::cout << "[TransportNetworkModule] Transport " << request_id
                  << " completed! Dog " << it->second.dog_name
                  << " has arrived at destination." << std::endl;
    }

    return true;
}

bool TransportNetworkModule::cancelTransport(const std::string& request_id,
                                               const std::string& reason) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = transport_requests_.find(request_id);
    if (it == transport_requests_.end()) {
        return false;
    }

    it->second.status = "cancelled";

    // Update timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    it->second.updated_at = ss.str();

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "transport_cancelled";
    event.data["request_id"] = request_id;
    event.data["reason"] = reason;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[TransportNetworkModule] Transport " << request_id
              << " cancelled: " << reason << std::endl;

    return true;
}

std::vector<TransportRequest> TransportNetworkModule::getPendingRequests() {
    std::vector<TransportRequest> pending;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, request] : transport_requests_) {
        if (request.status == "pending" || request.status == "planning") {
            pending.push_back(request);
        }
    }

    return pending;
}

std::vector<TransportRequest> TransportNetworkModule::getActiveTransports() {
    std::vector<TransportRequest> active;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, request] : transport_requests_) {
        if (request.status == "confirmed" || request.status == "in_progress") {
            active.push_back(request);
        }
    }

    return active;
}

// ============================================================================
// UTILITIES
// ============================================================================

double TransportNetworkModule::calculateDistance(const GeoLocation& loc1,
                                                   const GeoLocation& loc2) {
    // Haversine formula
    double lat1 = loc1.latitude * DEG_TO_RAD;
    double lat2 = loc2.latitude * DEG_TO_RAD;
    double dLat = (loc2.latitude - loc1.latitude) * DEG_TO_RAD;
    double dLon = (loc2.longitude - loc1.longitude) * DEG_TO_RAD;

    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1) * std::cos(lat2) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return EARTH_RADIUS_MILES * c;
}

Json::Value TransportNetworkModule::getTransportStats(const std::string& start_date,
                                                        const std::string& end_date) {
    Json::Value stats;
    stats["period_start"] = start_date;
    stats["period_end"] = end_date;
    stats["total_transporters"] = static_cast<Json::Int64>(total_transporters_.load());
    stats["active_transporters"] = static_cast<Json::Int64>(active_transporters_.load());
    stats["requests_created"] = static_cast<Json::Int64>(transport_requests_created_.load());
    stats["transports_completed"] = static_cast<Json::Int64>(transports_completed_.load());
    stats["legs_completed"] = static_cast<Json::Int64>(legs_completed_.load());
    stats["total_miles"] = total_miles_transported_.load();
    stats["dogs_transported"] = static_cast<Json::Int64>(dogs_transported_.load());

    return stats;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void TransportNetworkModule::handleTransportNeeded(const core::Event& event) {
    TransportRequest request;
    request.dog_id = event.data.get("dog_id", "").asString();
    request.dog_name = event.data.get("dog_name", "").asString();
    request.dog_size = event.data.get("dog_size", "medium").asString();
    request.urgency = event.data.get("urgency", "normal").asString();

    if (event.data.isMember("origin")) {
        request.origin.city = event.data["origin"].get("city", "").asString();
        request.origin.state = event.data["origin"].get("state", "").asString();
        request.origin.zip_code = event.data["origin"].get("zip_code", "").asString();
        request.origin.latitude = event.data["origin"].get("latitude", 0.0).asDouble();
        request.origin.longitude = event.data["origin"].get("longitude", 0.0).asDouble();
    }

    if (event.data.isMember("destination")) {
        request.destination.city = event.data["destination"].get("city", "").asString();
        request.destination.state = event.data["destination"].get("state", "").asString();
        request.destination.zip_code = event.data["destination"].get("zip_code", "").asString();
        request.destination.latitude = event.data["destination"].get("latitude", 0.0).asDouble();
        request.destination.longitude = event.data["destination"].get("longitude", 0.0).asDouble();
    }

    if (!request.dog_id.empty()) {
        createTransportRequest(request);
    }
}

void TransportNetworkModule::handleTransporterSignup(const core::Event& event) {
    Transporter transporter;
    transporter.name = event.data.get("name", "").asString();
    transporter.email = event.data.get("email", "").asString();
    transporter.phone = event.data.get("phone", "").asString();
    transporter.max_distance_miles = event.data.get("max_distance", 100).asInt();
    transporter.vehicle_type = event.data.get("vehicle_type", "sedan").asString();
    transporter.max_dogs = event.data.get("max_dogs", 1).asInt();
    transporter.has_crate = event.data.get("has_crate", true).asBool();

    if (event.data.isMember("location")) {
        transporter.home_location.city = event.data["location"].get("city", "").asString();
        transporter.home_location.state = event.data["location"].get("state", "").asString();
        transporter.home_location.zip_code = event.data["location"].get("zip_code", "").asString();
        transporter.home_location.latitude = event.data["location"].get("latitude", 0.0).asDouble();
        transporter.home_location.longitude = event.data["location"].get("longitude", 0.0).asDouble();
    }

    if (!transporter.name.empty() && !transporter.email.empty()) {
        registerTransporter(transporter);
    }
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

std::string TransportNetworkModule::generateTransporterId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    std::stringstream ss;
    ss << "TRP-" << dis(gen);
    return ss.str();
}

std::string TransportNetworkModule::generateRequestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "TRQ-" << std::put_time(std::gmtime(&time_t), "%Y%m%d")
       << "-" << dis(gen);
    return ss.str();
}

std::string TransportNetworkModule::generateLegId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    std::stringstream ss;
    ss << "LEG-" << dis(gen);
    return ss.str();
}

std::vector<GeoLocation> TransportNetworkModule::calculateWaypoints(
    const GeoLocation& origin,
    const GeoLocation& destination) {

    std::vector<GeoLocation> waypoints;

    double total_distance = calculateDistance(origin, destination);

    if (total_distance <= max_leg_distance_miles_) {
        // Single leg transport
        return waypoints;
    }

    // Calculate number of legs needed
    int num_legs = static_cast<int>(std::ceil(total_distance / max_leg_distance_miles_));

    // Interpolate waypoints
    for (int i = 1; i < num_legs; ++i) {
        double fraction = static_cast<double>(i) / num_legs;

        GeoLocation waypoint;
        waypoint.latitude = origin.latitude + fraction * (destination.latitude - origin.latitude);
        waypoint.longitude = origin.longitude + fraction * (destination.longitude - origin.longitude);
        waypoint.city = "Waypoint " + std::to_string(i);
        waypoint.state = "";
        waypoint.zip_code = "";

        waypoints.push_back(waypoint);
    }

    return waypoints;
}

bool TransportNetworkModule::isTransporterAvailable(const Transporter& transporter,
                                                      const GeoLocation& start,
                                                      const GeoLocation& end,
                                                      const std::string& date) {
    if (!transporter.active) {
        return false;
    }

    // Check distance from transporter's home to start
    double distance_to_start = calculateDistance(transporter.home_location, start);
    if (distance_to_start > transporter.max_distance_miles) {
        return false;
    }

    // Check leg distance
    double leg_distance = calculateDistance(start, end);
    if (leg_distance > transporter.max_distance_miles) {
        return false;
    }

    // Check day availability (simplified - would parse date in production)
    // For now, assume all active transporters are available

    return true;
}

void TransportNetworkModule::notifyTransporters(const TransportRequest& request) {
    // Find transporters near the origin
    std::vector<Transporter> nearby;

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        for (const auto& [id, transporter] : transporters_) {
            if (!transporter.active) continue;

            double distance = calculateDistance(transporter.home_location, request.origin);
            if (distance <= transporter.max_distance_miles) {
                nearby.push_back(transporter);
            }
        }
    }

    // Publish notification event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "transport_volunteers_notified";
    event.data["request_id"] = request.id;
    event.data["transporter_count"] = static_cast<int>(nearby.size());
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[TransportNetworkModule] Notified " << nearby.size()
              << " transporters about request " << request.id << std::endl;
}

void TransportNetworkModule::updateTransporterStats(const std::string& transporter_id,
                                                      bool success) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = transporters_.find(transporter_id);
    if (it != transporters_.end()) {
        it->second.completed_transports++;
        // Simple rating update
        if (success) {
            it->second.rating = std::min(5.0, it->second.rating + 0.1);
        } else {
            it->second.rating = std::max(1.0, it->second.rating - 0.2);
        }
    }
}

} // namespace wtl::modules::intervention
