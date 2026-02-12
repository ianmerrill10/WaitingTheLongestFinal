/**
 * @file RescueCoordinationModule.cc
 * @brief Implementation of the RescueCoordinationModule
 * @see RescueCoordinationModule.h for documentation
 */

#include "RescueCoordinationModule.h"
#include "modules/ModuleManager.h"
#include "core/EventBus.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <algorithm>

namespace wtl::modules::intervention {

// ============================================================================
// LIFECYCLE
// ============================================================================

bool RescueCoordinationModule::initialize() {
    std::cout << "[RescueCoordinationModule] Initializing..." << std::endl;

    // Subscribe to events
    auto& event_bus = core::EventBus::getInstance();

    auto critical_id = event_bus.subscribe(
        core::EventType::DOG_BECAME_CRITICAL,
        [this](const core::Event& event) {
            handleDogBecameCritical(event);
        },
        "rescue_coordination_critical_handler"
    );
    subscription_ids_.push_back(critical_id);

    auto request_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "rescue_request") {
                handleRescueRequestReceived(event);
            }
        },
        "rescue_coordination_request_handler"
    );
    subscription_ids_.push_back(request_id);

    auto capacity_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "rescue_capacity_update") {
                handleRescueCapacityUpdate(event);
            }
        },
        "rescue_coordination_capacity_handler"
    );
    subscription_ids_.push_back(capacity_id);

    enabled_ = true;
    std::cout << "[RescueCoordinationModule] Initialization complete." << std::endl;
    std::cout << "[RescueCoordinationModule] Auto-alert rescues: "
              << (auto_alert_rescues_ ? "enabled" : "disabled") << std::endl;
    std::cout << "[RescueCoordinationModule] Critical threshold: "
              << critical_hours_threshold_ << " hours" << std::endl;

    return true;
}

void RescueCoordinationModule::shutdown() {
    std::cout << "[RescueCoordinationModule] Shutting down..." << std::endl;

    auto& event_bus = core::EventBus::getInstance();
    for (const auto& id : subscription_ids_) {
        event_bus.unsubscribe(id);
    }
    subscription_ids_.clear();

    enabled_ = false;
    std::cout << "[RescueCoordinationModule] Shutdown complete." << std::endl;
}

void RescueCoordinationModule::onConfigUpdate(const Json::Value& config) {
    configure(config);
}

// ============================================================================
// STATUS
// ============================================================================

bool RescueCoordinationModule::isHealthy() const {
    return enabled_;
}

Json::Value RescueCoordinationModule::getStatus() const {
    Json::Value status;
    status["name"] = getName();
    status["version"] = getVersion();
    status["enabled"] = enabled_;
    status["healthy"] = isHealthy();
    status["auto_alert_rescues"] = auto_alert_rescues_;
    status["critical_hours_threshold"] = critical_hours_threshold_;
    status["urgent_hours_threshold"] = urgent_hours_threshold_;

    std::lock_guard<std::mutex> lock(data_mutex_);
    status["registered_rescues"] = static_cast<int>(rescues_.size());
    status["pending_requests"] = 0;
    status["active_commitments"] = 0;

    int pending = 0, active_commitments = 0;
    for (const auto& [id, req] : rescue_requests_) {
        if (req.status == "pending" || req.status == "in_progress") pending++;
    }
    for (const auto& [id, commit] : commitments_) {
        if (commit.status == "pending_pickup" || commit.status == "picked_up") {
            active_commitments++;
        }
    }
    status["pending_requests"] = pending;
    status["active_commitments"] = active_commitments;

    return status;
}

Json::Value RescueCoordinationModule::getMetrics() const {
    Json::Value metrics;
    metrics["rescues_registered"] = static_cast<Json::Int64>(rescues_registered_.load());
    metrics["requests_created"] = static_cast<Json::Int64>(requests_created_.load());
    metrics["commitments_made"] = static_cast<Json::Int64>(commitments_made_.load());
    metrics["dogs_saved"] = static_cast<Json::Int64>(dogs_saved_.load());
    metrics["alerts_sent"] = static_cast<Json::Int64>(alerts_sent_.load());
    metrics["requests_expired"] = static_cast<Json::Int64>(requests_expired_.load());

    // Calculate save rate
    int64_t requests = requests_created_.load();
    int64_t saved = dogs_saved_.load();
    if (requests > 0) {
        metrics["save_rate"] = static_cast<double>(saved) / requests * 100.0;
    } else {
        metrics["save_rate"] = 0.0;
    }

    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void RescueCoordinationModule::configure(const Json::Value& config) {
    if (config.isMember("auto_alert_rescues")) {
        auto_alert_rescues_ = config["auto_alert_rescues"].asBool();
    }
    if (config.isMember("critical_hours_threshold")) {
        critical_hours_threshold_ = config["critical_hours_threshold"].asInt();
    }
    if (config.isMember("urgent_hours_threshold")) {
        urgent_hours_threshold_ = config["urgent_hours_threshold"].asInt();
    }
}

Json::Value RescueCoordinationModule::getDefaultConfig() const {
    Json::Value config;
    config["enabled"] = true;
    config["auto_alert_rescues"] = true;
    config["critical_hours_threshold"] = 24;
    config["urgent_hours_threshold"] = 72;
    return config;
}

// ============================================================================
// RESCUE ORGANIZATION MANAGEMENT
// ============================================================================

RescueOrganization RescueCoordinationModule::registerRescue(const RescueOrganization& rescue) {
    RescueOrganization new_rescue = rescue;
    new_rescue.id = generateRescueId();
    new_rescue.active = true;
    new_rescue.response_rate = 0.0;
    new_rescue.avg_response_hours = 0.0;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
    new_rescue.last_active_date = ss.str();

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        rescues_[new_rescue.id] = new_rescue;
    }

    rescues_registered_++;

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "rescue_registered";
    event.data["rescue_id"] = new_rescue.id;
    event.data["rescue_name"] = new_rescue.name;
    event.data["state"] = new_rescue.state;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[RescueCoordinationModule] Rescue registered: " << new_rescue.name
              << " (" << new_rescue.id << ")" << std::endl;

    return new_rescue;
}

bool RescueCoordinationModule::updateRescue(const RescueOrganization& rescue) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = rescues_.find(rescue.id);
    if (it == rescues_.end()) {
        return false;
    }

    it->second = rescue;
    return true;
}

std::optional<RescueOrganization> RescueCoordinationModule::getRescue(
    const std::string& rescue_id) {

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = rescues_.find(rescue_id);
    if (it != rescues_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<RescueOrganization> RescueCoordinationModule::findMatchingRescues(
    const std::string& breed,
    const std::string& size,
    const std::string& special_needs,
    const std::string& state) {

    std::vector<RescueOrganization> matching;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, rescue] : rescues_) {
        if (!rescue.active) continue;
        if (rescue.current_capacity >= rescue.max_capacity) continue;

        // Check state
        if (!state.empty() && rescue.state != state) continue;

        // Check breed acceptance
        if (!rescue.accepted_breeds.empty()) {
            bool breed_match = false;
            for (const auto& accepted : rescue.accepted_breeds) {
                if (accepted == "all" || accepted == breed ||
                    breed.find(accepted) != std::string::npos) {
                    breed_match = true;
                    break;
                }
            }
            if (!breed_match) continue;
        }

        // Check special needs capability
        if (!special_needs.empty()) {
            bool can_handle = rescue.specializations.empty(); // Empty = general
            for (const auto& spec : rescue.specializations) {
                if (special_needs.find(spec) != std::string::npos) {
                    can_handle = true;
                    break;
                }
            }
            if (!can_handle) continue;
        }

        matching.push_back(rescue);
    }

    // Sort by response rate and available capacity
    std::sort(matching.begin(), matching.end(),
        [](const RescueOrganization& a, const RescueOrganization& b) {
            // Prioritize higher response rate
            if (std::abs(a.response_rate - b.response_rate) > 5.0) {
                return a.response_rate > b.response_rate;
            }
            // Then by available capacity
            return (a.max_capacity - a.current_capacity) >
                   (b.max_capacity - b.current_capacity);
        });

    return matching;
}

bool RescueCoordinationModule::updateCapacity(const std::string& rescue_id,
                                                int current_capacity) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = rescues_.find(rescue_id);
    if (it == rescues_.end()) {
        return false;
    }

    it->second.current_capacity = current_capacity;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
    it->second.last_active_date = ss.str();

    return true;
}

std::vector<RescueOrganization> RescueCoordinationModule::getRescuesWithCapacity(int min_spaces) {
    std::vector<RescueOrganization> with_capacity;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, rescue] : rescues_) {
        if (!rescue.active) continue;

        int available = rescue.max_capacity - rescue.current_capacity;
        if (available >= min_spaces) {
            with_capacity.push_back(rescue);
        }
    }

    return with_capacity;
}

// ============================================================================
// RESCUE REQUESTS
// ============================================================================

RescueRequest RescueCoordinationModule::createRescueRequest(const RescueRequest& request) {
    RescueRequest new_request = request;
    new_request.id = generateRequestId();
    new_request.status = "pending";

    // Determine urgency
    if (new_request.hours_until_deadline <= critical_hours_threshold_) {
        new_request.urgency = "critical";
    } else if (new_request.hours_until_deadline <= urgent_hours_threshold_) {
        new_request.urgency = "urgent";
    } else if (new_request.urgency.empty()) {
        new_request.urgency = "normal";
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    new_request.created_at = ss.str();
    new_request.updated_at = new_request.created_at;

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        rescue_requests_[new_request.id] = new_request;
    }

    requests_created_++;

    // Auto-alert if enabled
    if (auto_alert_rescues_) {
        alertRescues(new_request.id);
    }

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "rescue_request_created";
    event.data["request_id"] = new_request.id;
    event.data["dog_id"] = new_request.dog_id;
    event.data["urgency"] = new_request.urgency;
    event.data["hours_until_deadline"] = new_request.hours_until_deadline;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[RescueCoordinationModule] Rescue request created: " << new_request.id
              << " for " << new_request.dog_name << " (" << new_request.urgency << ")" << std::endl;

    return new_request;
}

std::optional<RescueRequest> RescueCoordinationModule::getRescueRequest(
    const std::string& request_id) {

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = rescue_requests_.find(request_id);
    if (it != rescue_requests_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<RescueRequest> RescueCoordinationModule::getPendingRequests() {
    std::vector<RescueRequest> pending;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, request] : rescue_requests_) {
        if (request.status == "pending" || request.status == "in_progress") {
            pending.push_back(request);
        }
    }

    // Sort by urgency (critical first) then by hours remaining
    std::sort(pending.begin(), pending.end(),
        [](const RescueRequest& a, const RescueRequest& b) {
            auto urgency_score = [](const std::string& u) {
                if (u == "critical" || u == "emergency") return 0;
                if (u == "urgent") return 1;
                return 2;
            };
            int score_a = urgency_score(a.urgency);
            int score_b = urgency_score(b.urgency);
            if (score_a != score_b) return score_a < score_b;
            return a.hours_until_deadline < b.hours_until_deadline;
        });

    return pending;
}

std::vector<RescueRequest> RescueCoordinationModule::getCriticalRequests() {
    std::vector<RescueRequest> critical;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, request] : rescue_requests_) {
        if ((request.status == "pending" || request.status == "in_progress") &&
            (request.urgency == "critical" || request.urgency == "emergency" ||
             request.hours_until_deadline <= critical_hours_threshold_)) {
            critical.push_back(request);
        }
    }

    // Sort by hours remaining
    std::sort(critical.begin(), critical.end(),
        [](const RescueRequest& a, const RescueRequest& b) {
            return a.hours_until_deadline < b.hours_until_deadline;
        });

    return critical;
}

int RescueCoordinationModule::alertRescues(const std::string& request_id) {
    RescueRequest request;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = rescue_requests_.find(request_id);
        if (it == rescue_requests_.end()) {
            return 0;
        }
        request = it->second;
    }

    // Find matching rescues
    auto matching = findMatchingRescues(request.breed, request.size,
                                         request.special_needs, request.shelter_state);

    int alerted = 0;
    std::vector<std::string> contacted;

    for (const auto& rescue : matching) {
        // Skip if already contacted or declined
        if (std::find(request.contacted_rescues.begin(), request.contacted_rescues.end(),
                      rescue.id) != request.contacted_rescues.end()) {
            continue;
        }
        if (std::find(request.declined_rescues.begin(), request.declined_rescues.end(),
                      rescue.id) != request.declined_rescues.end()) {
            continue;
        }

        // Send alert
        core::Event alert;
        alert.type = core::EventType::CUSTOM;
        alert.data["type"] = "rescue_alert";
        alert.data["rescue_id"] = rescue.id;
        alert.data["rescue_email"] = rescue.contact_email;
        alert.data["request_id"] = request.id;
        alert.data["dog_name"] = request.dog_name;
        alert.data["breed"] = request.breed;
        alert.data["urgency"] = request.urgency;
        alert.data["hours_remaining"] = request.hours_until_deadline;
        alert.data["shelter"] = request.shelter_name;
        alert.data["location"] = request.shelter_city + ", " + request.shelter_state;
        core::EventBus::getInstance().publishAsync(alert);

        contacted.push_back(rescue.id);
        alerted++;
        alerts_sent_++;
    }

    // Update request with contacted rescues
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = rescue_requests_.find(request_id);
        if (it != rescue_requests_.end()) {
            for (const auto& id : contacted) {
                it->second.contacted_rescues.push_back(id);
            }
            it->second.status = "in_progress";
        }
    }

    std::cout << "[RescueCoordinationModule] Alerted " << alerted
              << " rescues for request " << request_id << std::endl;

    return alerted;
}

void RescueCoordinationModule::recordDecline(const std::string& request_id,
                                               const std::string& rescue_id,
                                               const std::string& reason) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = rescue_requests_.find(request_id);
    if (it != rescue_requests_.end()) {
        it->second.declined_rescues.push_back(rescue_id);
    }

    std::cout << "[RescueCoordinationModule] Rescue " << rescue_id
              << " declined request " << request_id << ": " << reason << std::endl;
}

// ============================================================================
// COMMITMENTS
// ============================================================================

std::optional<RescueCommitment> RescueCoordinationModule::commitToRescue(
    const std::string& request_id,
    const std::string& rescue_id,
    const std::string& pickup_date) {

    RescueCommitment commitment;
    commitment.id = generateCommitmentId();
    commitment.request_id = request_id;
    commitment.rescue_id = rescue_id;
    commitment.planned_pickup_date = pickup_date;
    commitment.status = "pending_pickup";
    commitment.transport_arranged = "self";

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    commitment.committed_at = ss.str();

    // Get rescue name
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto rescue_it = rescues_.find(rescue_id);
        if (rescue_it != rescues_.end()) {
            commitment.rescue_name = rescue_it->second.name;
        }

        // Update request
        auto request_it = rescue_requests_.find(request_id);
        if (request_it == rescue_requests_.end()) {
            return std::nullopt;
        }

        request_it->second.status = "committed";
        request_it->second.committed_rescue_id = rescue_id;
        request_it->second.commitment_date = commitment.committed_at;
        request_it->second.pickup_date = pickup_date;

        // Store commitment
        commitments_[commitment.id] = commitment;
    }

    commitments_made_++;

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "rescue_committed";
    event.data["commitment_id"] = commitment.id;
    event.data["request_id"] = request_id;
    event.data["rescue_id"] = rescue_id;
    event.data["rescue_name"] = commitment.rescue_name;
    event.data["pickup_date"] = pickup_date;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[RescueCoordinationModule] Commitment made: " << commitment.rescue_name
              << " will rescue dog from request " << request_id << std::endl;

    return commitment;
}

bool RescueCoordinationModule::updateCommitment(const std::string& commitment_id,
                                                  const std::string& status,
                                                  const std::string& notes) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = commitments_.find(commitment_id);
    if (it == commitments_.end()) {
        return false;
    }

    std::string old_status = it->second.status;
    it->second.status = status;
    if (!notes.empty()) {
        it->second.notes = notes;
    }

    // Update rescue request if completed
    if (status == "in_foster" || status == "adopted") {
        auto request_it = rescue_requests_.find(it->second.request_id);
        if (request_it != rescue_requests_.end()) {
            request_it->second.status = "completed";
        }
        dogs_saved_++;
    }

    std::cout << "[RescueCoordinationModule] Commitment " << commitment_id
              << " status changed: " << old_status << " -> " << status << std::endl;

    return true;
}

bool RescueCoordinationModule::cancelCommitment(const std::string& commitment_id,
                                                  const std::string& reason) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = commitments_.find(commitment_id);
    if (it == commitments_.end()) {
        return false;
    }

    it->second.status = "cancelled";
    it->second.notes = "Cancelled: " + reason;

    // Reopen the rescue request
    auto request_it = rescue_requests_.find(it->second.request_id);
    if (request_it != rescue_requests_.end()) {
        request_it->second.status = "pending";
        request_it->second.committed_rescue_id = "";
    }

    std::cout << "[RescueCoordinationModule] Commitment " << commitment_id
              << " cancelled: " << reason << std::endl;

    return true;
}

std::vector<RescueCommitment> RescueCoordinationModule::getCommitmentsForRescue(
    const std::string& rescue_id) {

    std::vector<RescueCommitment> result;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, commitment] : commitments_) {
        if (commitment.rescue_id == rescue_id) {
            result.push_back(commitment);
        }
    }

    return result;
}

// ============================================================================
// NETWORK STATISTICS
// ============================================================================

NetworkStats RescueCoordinationModule::getNetworkStats() {
    NetworkStats stats;
    stats.total_rescues = 0;
    stats.active_rescues = 0;
    stats.total_capacity = 0;
    stats.available_capacity = 0;
    stats.foster_homes = 0;
    stats.pending_requests = 0;
    stats.dogs_saved_this_month = static_cast<int>(dogs_saved_.load());
    stats.avg_response_time_hours = 0.0;

    double total_response_hours = 0.0;
    int response_count = 0;

    std::lock_guard<std::mutex> lock(data_mutex_);

    for (const auto& [id, rescue] : rescues_) {
        stats.total_rescues++;
        if (rescue.active) {
            stats.active_rescues++;
            stats.total_capacity += rescue.max_capacity;
            stats.available_capacity += (rescue.max_capacity - rescue.current_capacity);
            stats.foster_homes += rescue.foster_network_size;

            if (rescue.avg_response_hours > 0) {
                total_response_hours += rescue.avg_response_hours;
                response_count++;
            }
        }
    }

    for (const auto& [id, request] : rescue_requests_) {
        if (request.status == "pending" || request.status == "in_progress") {
            stats.pending_requests++;
        }
    }

    if (response_count > 0) {
        stats.avg_response_time_hours = total_response_hours / response_count;
    }

    return stats;
}

Json::Value RescueCoordinationModule::getRescuePerformance(const std::string& rescue_id) {
    Json::Value perf;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = rescues_.find(rescue_id);
    if (it == rescues_.end()) {
        perf["error"] = "Rescue not found";
        return perf;
    }

    const auto& rescue = it->second;
    perf["rescue_id"] = rescue.id;
    perf["rescue_name"] = rescue.name;
    perf["response_rate"] = rescue.response_rate;
    perf["avg_response_hours"] = rescue.avg_response_hours;
    perf["current_capacity"] = rescue.current_capacity;
    perf["max_capacity"] = rescue.max_capacity;
    perf["last_active"] = rescue.last_active_date;

    // Count commitments
    int total_commits = 0, completed = 0, cancelled = 0;
    for (const auto& [id, commit] : commitments_) {
        if (commit.rescue_id == rescue_id) {
            total_commits++;
            if (commit.status == "in_foster" || commit.status == "adopted") {
                completed++;
            } else if (commit.status == "cancelled") {
                cancelled++;
            }
        }
    }

    perf["total_commitments"] = total_commits;
    perf["completed_rescues"] = completed;
    perf["cancelled_commitments"] = cancelled;

    return perf;
}

Json::Value RescueCoordinationModule::getCoordinationStats(const std::string& start_date,
                                                            const std::string& end_date) {
    Json::Value stats;
    stats["period_start"] = start_date;
    stats["period_end"] = end_date;
    stats["rescues_registered"] = static_cast<Json::Int64>(rescues_registered_.load());
    stats["requests_created"] = static_cast<Json::Int64>(requests_created_.load());
    stats["commitments_made"] = static_cast<Json::Int64>(commitments_made_.load());
    stats["dogs_saved"] = static_cast<Json::Int64>(dogs_saved_.load());
    stats["alerts_sent"] = static_cast<Json::Int64>(alerts_sent_.load());
    stats["requests_expired"] = static_cast<Json::Int64>(requests_expired_.load());

    auto network = getNetworkStats();
    stats["network_capacity"] = network.total_capacity;
    stats["available_capacity"] = network.available_capacity;
    stats["active_rescues"] = network.active_rescues;

    return stats;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void RescueCoordinationModule::handleDogBecameCritical(const core::Event& event) {
    RescueRequest request;
    request.dog_id = event.data.get("dog_id", "").asString();
    request.dog_name = event.data.get("name", "Unknown").asString();
    request.breed = event.data.get("breed", "Mixed").asString();
    request.size = event.data.get("size", "medium").asString();
    request.shelter_id = event.data.get("shelter_id", "").asString();
    request.shelter_name = event.data.get("shelter_name", "").asString();
    request.shelter_city = event.data.get("shelter_city", "").asString();
    request.shelter_state = event.data.get("shelter_state", "").asString();
    request.hours_until_deadline = event.data.get("hours_until_euthanasia", 24).asInt();
    request.urgency = "critical";

    if (!request.dog_id.empty()) {
        createRescueRequest(request);
    }
}

void RescueCoordinationModule::handleRescueRequestReceived(const core::Event& event) {
    RescueRequest request;
    request.dog_id = event.data.get("dog_id", "").asString();
    request.dog_name = event.data.get("dog_name", "").asString();
    request.breed = event.data.get("breed", "").asString();
    request.size = event.data.get("size", "").asString();
    request.shelter_id = event.data.get("shelter_id", "").asString();
    request.shelter_name = event.data.get("shelter_name", "").asString();
    request.hours_until_deadline = event.data.get("hours_remaining", 168).asInt();

    if (!request.dog_id.empty()) {
        createRescueRequest(request);
    }
}

void RescueCoordinationModule::handleRescueCapacityUpdate(const core::Event& event) {
    std::string rescue_id = event.data.get("rescue_id", "").asString();
    int capacity = event.data.get("current_capacity", -1).asInt();

    if (!rescue_id.empty() && capacity >= 0) {
        updateCapacity(rescue_id, capacity);
    }
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

std::string RescueCoordinationModule::generateRescueId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    std::stringstream ss;
    ss << "RSC-" << dis(gen);
    return ss.str();
}

std::string RescueCoordinationModule::generateRequestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "RRQ-" << std::put_time(std::gmtime(&time_t), "%Y%m%d")
       << "-" << dis(gen);
    return ss.str();
}

std::string RescueCoordinationModule::generateCommitmentId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    std::stringstream ss;
    ss << "CMT-" << dis(gen);
    return ss.str();
}

} // namespace wtl::modules::intervention
