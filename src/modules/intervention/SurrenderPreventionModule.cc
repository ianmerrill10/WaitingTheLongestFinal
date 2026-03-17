/**
 * @file SurrenderPreventionModule.cc
 * @brief Implementation of the SurrenderPreventionModule
 * @see SurrenderPreventionModule.h for documentation
 */

#include "SurrenderPreventionModule.h"
#include "modules/ModuleManager.h"
#include "core/EventBus.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>

namespace wtl::modules::intervention {

// ============================================================================
// LIFECYCLE
// ============================================================================

bool SurrenderPreventionModule::initialize() {
    std::cout << "[SurrenderPreventionModule] Initializing..." << std::endl;

    // Load surrender reasons and resources
    loadSurrenderReasons();
    loadResources();

    // Subscribe to events
    auto& event_bus = core::EventBus::getInstance();

    auto inquiry_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "surrender_inquiry") {
                handleSurrenderInquiry(event);
            }
        },
        "surrender_prevention_inquiry_handler"
    );
    subscription_ids_.push_back(inquiry_id);

    auto approved_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "assistance_approved") {
                handleAssistanceApproved(event);
            }
        },
        "surrender_prevention_approved_handler"
    );
    subscription_ids_.push_back(approved_id);

    enabled_ = true;
    std::cout << "[SurrenderPreventionModule] Initialization complete." << std::endl;
    std::cout << "[SurrenderPreventionModule] Loaded " << surrender_reasons_.size()
              << " surrender reasons and " << resources_.size() << " resources." << std::endl;

    return true;
}

void SurrenderPreventionModule::shutdown() {
    std::cout << "[SurrenderPreventionModule] Shutting down..." << std::endl;

    // Unsubscribe from events
    auto& event_bus = core::EventBus::getInstance();
    for (const auto& id : subscription_ids_) {
        event_bus.unsubscribe(id);
    }
    subscription_ids_.clear();

    enabled_ = false;
    std::cout << "[SurrenderPreventionModule] Shutdown complete." << std::endl;
}

void SurrenderPreventionModule::onConfigUpdate(const Json::Value& config) {
    configure(config);
}

// ============================================================================
// STATUS
// ============================================================================

bool SurrenderPreventionModule::isHealthy() const {
    return enabled_;
}

Json::Value SurrenderPreventionModule::getStatus() const {
    Json::Value status;
    status["name"] = getName();
    status["version"] = getVersion();
    status["enabled"] = enabled_;
    status["healthy"] = isHealthy();
    status["hotline_enabled"] = hotline_enabled_;
    status["financial_assistance_enabled"] = financial_assistance_enabled_;
    status["max_assistance_amount"] = max_assistance_amount_;

    if (hotline_enabled_) {
        status["hotline_number"] = hotline_number_;
        status["hotline_hours"] = hotline_hours_;
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    status["pending_requests"] = static_cast<int>(pending_requests_.size());
    status["loaded_reasons"] = static_cast<int>(surrender_reasons_.size());
    status["loaded_resources"] = static_cast<int>(resources_.size());

    return status;
}

Json::Value SurrenderPreventionModule::getMetrics() const {
    Json::Value metrics;
    metrics["total_inquiries"] = static_cast<Json::Int64>(total_inquiries_.load());
    metrics["assistance_requests"] = static_cast<Json::Int64>(assistance_requests_.load());
    metrics["approved_requests"] = static_cast<Json::Int64>(approved_requests_.load());
    metrics["successful_preventions"] = static_cast<Json::Int64>(successful_preventions_.load());
    metrics["total_assistance_disbursed"] = total_assistance_disbursed_.load();

    // Calculate success rate
    int64_t requests = assistance_requests_.load();
    int64_t successes = successful_preventions_.load();
    if (requests > 0) {
        metrics["prevention_success_rate"] = static_cast<double>(successes) / requests * 100.0;
    } else {
        metrics["prevention_success_rate"] = 0.0;
    }

    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void SurrenderPreventionModule::configure(const Json::Value& config) {
    if (config.isMember("hotline_enabled")) {
        hotline_enabled_ = config["hotline_enabled"].asBool();
    }
    if (config.isMember("financial_assistance_enabled")) {
        financial_assistance_enabled_ = config["financial_assistance_enabled"].asBool();
    }
    if (config.isMember("max_assistance_amount")) {
        max_assistance_amount_ = config["max_assistance_amount"].asDouble();
    }
    if (config.isMember("hotline_number")) {
        hotline_number_ = config["hotline_number"].asString();
    }
    if (config.isMember("hotline_hours")) {
        hotline_hours_ = config["hotline_hours"].asString();
    }
}

Json::Value SurrenderPreventionModule::getDefaultConfig() const {
    Json::Value config;
    config["enabled"] = true;
    config["hotline_enabled"] = true;
    config["financial_assistance_enabled"] = true;
    config["max_assistance_amount"] = 500.0;
    config["hotline_number"] = "1-800-KEEP-PET";
    config["hotline_hours"] = "24/7";
    return config;
}

// ============================================================================
// SURRENDER PREVENTION API
// ============================================================================

AssistanceRequest SurrenderPreventionModule::submitAssistanceRequest(
    const AssistanceRequest& request) {

    AssistanceRequest new_request = request;
    new_request.id = generateRequestId();
    new_request.status = "pending";

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    new_request.created_at = ss.str();
    new_request.updated_at = new_request.created_at;

    // Calculate recommended assistance amount
    if (financial_assistance_enabled_ && new_request.requested_amount > 0) {
        new_request.requested_amount = std::min(
            new_request.requested_amount,
            calculateAssistanceAmount(new_request)
        );
    }

    // Store the request
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        pending_requests_[new_request.id] = new_request;
    }

    // Update metrics
    assistance_requests_++;

    // Notify partner organizations
    notifyPartnerOrganizations(new_request);

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "assistance_request_submitted";
    event.data["request_id"] = new_request.id;
    event.data["reason_code"] = new_request.reason_code;
    event.data["zip_code"] = new_request.zip_code;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[SurrenderPreventionModule] Assistance request " << new_request.id
              << " submitted for reason: " << new_request.reason_code << std::endl;

    return new_request;
}

std::optional<AssistanceRequest> SurrenderPreventionModule::getAssistanceRequest(
    const std::string& request_id) {

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = pending_requests_.find(request_id);
    if (it != pending_requests_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool SurrenderPreventionModule::updateAssistanceStatus(
    const std::string& request_id,
    const std::string& status,
    const std::string& notes) {

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = pending_requests_.find(request_id);
    if (it == pending_requests_.end()) {
        return false;
    }

    it->second.status = status;

    // Update timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    it->second.updated_at = ss.str();

    if (!notes.empty()) {
        it->second.metadata["status_notes"] = notes;
    }

    // Update metrics based on status
    if (status == "approved") {
        approved_requests_++;
    }

    std::cout << "[SurrenderPreventionModule] Request " << request_id
              << " status updated to: " << status << std::endl;

    return true;
}

std::vector<PreventionResource> SurrenderPreventionModule::getResourcesForReason(
    const std::string& reason_code,
    const std::string& zip_code) {

    std::vector<PreventionResource> matching;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, resource] : resources_) {
        if (!resource.active) continue;

        // Check if resource applies to this reason
        bool reason_match = resource.applicable_reasons.empty(); // Empty means all reasons
        for (const auto& reason : resource.applicable_reasons) {
            if (reason == reason_code || reason == "all") {
                reason_match = true;
                break;
            }
        }

        if (!reason_match) continue;

        // Check location if specified
        if (!zip_code.empty() && !resource.service_areas.empty()) {
            bool location_match = false;
            for (const auto& area : resource.service_areas) {
                if (area == "national" || area == zip_code ||
                    zip_code.substr(0, 3) == area.substr(0, 3)) {
                    location_match = true;
                    break;
                }
            }
            if (!location_match) continue;
        }

        matching.push_back(resource);
    }

    return matching;
}

std::vector<SurrenderReason> SurrenderPreventionModule::getSurrenderReasons() const {
    std::vector<SurrenderReason> reasons;
    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [code, reason] : surrender_reasons_) {
        reasons.push_back(reason);
    }
    return reasons;
}

void SurrenderPreventionModule::recordPreventionSuccess(
    const std::string& request_id,
    const Json::Value& outcome_details) {

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = pending_requests_.find(request_id);
        if (it != pending_requests_.end()) {
            it->second.status = "success";
            it->second.metadata["outcome"] = outcome_details;

            // Track disbursement
            if (it->second.requested_amount > 0) {
                auto current = total_assistance_disbursed_.load();
                while (!total_assistance_disbursed_.compare_exchange_weak(current, current + it->second.requested_amount));
            }
        }
    }

    successful_preventions_++;

    // Publish success event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "surrender_prevented";
    event.data["request_id"] = request_id;
    event.data["outcome"] = outcome_details;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[SurrenderPreventionModule] Prevention success recorded for request: "
              << request_id << std::endl;
}

Json::Value SurrenderPreventionModule::getPreventionStats(
    const std::string& start_date,
    const std::string& end_date) {

    Json::Value stats;
    stats["period_start"] = start_date;
    stats["period_end"] = end_date;
    stats["total_inquiries"] = static_cast<Json::Int64>(total_inquiries_.load());
    stats["total_requests"] = static_cast<Json::Int64>(assistance_requests_.load());
    stats["approved_requests"] = static_cast<Json::Int64>(approved_requests_.load());
    stats["successful_preventions"] = static_cast<Json::Int64>(successful_preventions_.load());
    stats["total_disbursed"] = total_assistance_disbursed_.load();

    // Breakdown by reason
    Json::Value by_reason(Json::objectValue);
    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, request] : pending_requests_) {
        std::string reason = request.reason_code;
        if (!by_reason.isMember(reason)) {
            by_reason[reason] = 0;
        }
        by_reason[reason] = by_reason[reason].asInt() + 1;
    }
    stats["requests_by_reason"] = by_reason;

    return stats;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void SurrenderPreventionModule::handleSurrenderInquiry(const core::Event& event) {
    total_inquiries_++;

    std::string reason = event.data.get("reason", "unknown").asString();
    std::string zip_code = event.data.get("zip_code", "").asString();

    std::cout << "[SurrenderPreventionModule] Surrender inquiry received. "
              << "Reason: " << reason << std::endl;

    // Get matching resources
    auto resources = getResourcesForReason(reason, zip_code);

    // Publish resources found event
    core::Event response;
    response.type = core::EventType::CUSTOM;
    response.data["type"] = "surrender_resources_found";
    response.data["reason"] = reason;
    response.data["resource_count"] = static_cast<int>(resources.size());

    Json::Value resource_list(Json::arrayValue);
    for (const auto& resource : resources) {
        Json::Value r;
        r["id"] = resource.id;
        r["name"] = resource.name;
        r["type"] = resource.type;
        r["contact"] = resource.contact_info;
        resource_list.append(r);
    }
    response.data["resources"] = resource_list;

    core::EventBus::getInstance().publishAsync(response);
}

void SurrenderPreventionModule::handleAssistanceApproved(const core::Event& event) {
    std::string request_id = event.data.get("request_id", "").asString();
    if (!request_id.empty()) {
        updateAssistanceStatus(request_id, "approved", "Automated approval");
    }
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void SurrenderPreventionModule::loadSurrenderReasons() {
    // Load default surrender reasons
    std::vector<SurrenderReason> reasons = {
        {"financial", "Financial Hardship",
         "Unable to afford food, vet care, or other pet expenses",
         {"pet_food_banks", "low_cost_vet_clinics", "financial_assistance"},
         true, 90},
        {"housing", "Housing Issues",
         "Moving to pet-restricted housing, homelessness, eviction",
         {"pet_friendly_housing", "temporary_foster", "housing_assistance"},
         true, 95},
        {"behavioral", "Behavioral Problems",
         "Aggression, destruction, excessive barking, not house trained",
         {"behavior_training", "trainer_referrals", "behavior_hotline"},
         false, 70},
        {"medical", "Medical Issues",
         "Owner health problems, allergies, or pet medical costs",
         {"medical_assistance", "low_cost_vet", "owner_support"},
         true, 85},
        {"time", "Lifestyle Changes",
         "New job, new baby, too busy, travel requirements",
         {"dog_walking", "daycare", "lifestyle_tips"},
         false, 50},
        {"family", "Family Changes",
         "Divorce, death, family conflict about pet",
         {"counseling", "mediation", "temporary_care"},
         false, 60},
        {"landlord", "Landlord Issues",
         "New landlord rules, lease violations, deposit issues",
         {"tenant_rights", "negotiation_help", "pet_deposits"},
         true, 80},
        {"allergies", "Allergies",
         "Family member developed allergies to pet",
         {"allergy_management", "hypoallergenic_tips", "air_purifiers"},
         false, 40},
        {"size", "Dog Too Big/Active",
         "Underestimated size or energy level of dog",
         {"exercise_help", "training", "activity_groups"},
         false, 45},
        {"other", "Other Reasons",
         "Reasons not listed above",
         {"general_support", "counseling", "resources"},
         true, 30}
    };

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& reason : reasons) {
        surrender_reasons_[reason.code] = reason;
    }
}

void SurrenderPreventionModule::loadResources() {
    // Load default prevention resources
    std::vector<PreventionResource> default_resources = {
        {"res_001", "National Pet Food Bank Network", "financial",
         "Free and low-cost pet food distribution",
         "https://petfoodbank.org", "1-800-PET-FOOD",
         {"financial", "all"}, {"national"}, true},
        {"res_002", "Low Cost Vet Clinics Directory", "medical",
         "Directory of affordable veterinary services",
         "https://lowcostvet.org", "Use website directory",
         {"financial", "medical"}, {"national"}, true},
        {"res_003", "Pet Behavior Hotline", "behavioral",
         "Free phone consultations with certified behaviorists",
         "https://behaviorhotline.org", "1-800-DOG-HELP",
         {"behavioral"}, {"national"}, true},
        {"res_004", "Pet-Friendly Housing Database", "housing",
         "Searchable database of pet-friendly rentals",
         "https://petfriendlyhousing.org", "",
         {"housing", "landlord"}, {"national"}, true},
        {"res_005", "Emergency Pet Foster Network", "other",
         "Temporary foster care during owner emergencies",
         "https://emergencyfoster.org", "1-800-FOSTER-1",
         {"housing", "medical", "family", "all"}, {"national"}, true},
        {"res_006", "Red Rover Relief", "financial",
         "Grants for pet owners in crisis",
         "https://redrover.org/relief", "916-429-2457",
         {"financial", "medical", "housing"}, {"national"}, true},
        {"res_007", "Pets of the Homeless", "other",
         "Resources for homeless pet owners",
         "https://petsofthehomeless.org", "775-841-7463",
         {"housing", "financial"}, {"national"}, true},
        {"res_008", "The Pet Fund", "medical",
         "Financial assistance for non-basic veterinary care",
         "https://thepetfund.com", "916-443-6007",
         {"medical", "financial"}, {"national"}, true}
    };

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& resource : default_resources) {
        resources_[resource.id] = resource;
    }
}

std::string SurrenderPreventionModule::generateRequestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "SPR-" << std::put_time(std::gmtime(&time_t), "%Y%m%d")
       << "-" << dis(gen);
    return ss.str();
}

void SurrenderPreventionModule::notifyPartnerOrganizations(
    const AssistanceRequest& request) {

    // Publish event for partner notification
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "partner_notification_needed";
    event.data["request_id"] = request.id;
    event.data["reason_code"] = request.reason_code;
    event.data["zip_code"] = request.zip_code;
    event.data["urgency"] = "normal";

    // Mark as urgent if financial and high amount
    if (request.reason_code == "housing" || request.reason_code == "financial") {
        event.data["urgency"] = "high";
    }

    core::EventBus::getInstance().publishAsync(event);
}

double SurrenderPreventionModule::calculateAssistanceAmount(
    const AssistanceRequest& request) {

    double base_amount = request.requested_amount;

    // Cap at max
    base_amount = std::min(base_amount, max_assistance_amount_);

    // Adjust based on reason priority
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = surrender_reasons_.find(request.reason_code);
    if (it != surrender_reasons_.end()) {
        // Higher priority reasons get higher assistance
        double multiplier = it->second.priority_score / 100.0;
        base_amount *= multiplier;
    }

    return std::min(base_amount, max_assistance_amount_);
}

} // namespace wtl::modules::intervention
