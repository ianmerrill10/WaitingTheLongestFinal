/**
 * @file ReturnPreventionModule.cc
 * @brief Implementation of the ReturnPreventionModule
 * @see ReturnPreventionModule.h for documentation
 */

#include "ReturnPreventionModule.h"
#include "modules/ModuleManager.h"
#include "core/EventBus.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <ctime>

namespace wtl::modules::intervention {

// ============================================================================
// LIFECYCLE
// ============================================================================

bool ReturnPreventionModule::initialize() {
    std::cout << "[ReturnPreventionModule] Initializing..." << std::endl;

    // Load support resources
    loadSupportResources();

    // Subscribe to events
    auto& event_bus = core::EventBus::getInstance();

    auto adopted_id = event_bus.subscribe(
        core::EventType::DOG_ADOPTED,
        [this](const core::Event& event) {
            handleDogAdopted(event);
        },
        "return_prevention_adopted_handler"
    );
    subscription_ids_.push_back(adopted_id);

    auto followup_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "followup_response") {
                handleFollowupResponse(event);
            }
        },
        "return_prevention_followup_handler"
    );
    subscription_ids_.push_back(followup_id);

    auto return_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "return_request") {
                handleReturnRequest(event);
            }
        },
        "return_prevention_return_handler"
    );
    subscription_ids_.push_back(return_id);

    enabled_ = true;
    std::cout << "[ReturnPreventionModule] Initialization complete." << std::endl;
    std::cout << "[ReturnPreventionModule] Loaded " << support_resources_.size()
              << " support resources." << std::endl;
    std::cout << "[ReturnPreventionModule] Follow-up schedule: ";
    for (size_t i = 0; i < followup_days_.size(); ++i) {
        std::cout << followup_days_[i];
        if (i < followup_days_.size() - 1) std::cout << ", ";
    }
    std::cout << " days" << std::endl;

    return true;
}

void ReturnPreventionModule::shutdown() {
    std::cout << "[ReturnPreventionModule] Shutting down..." << std::endl;

    auto& event_bus = core::EventBus::getInstance();
    for (const auto& id : subscription_ids_) {
        event_bus.unsubscribe(id);
    }
    subscription_ids_.clear();

    enabled_ = false;
    std::cout << "[ReturnPreventionModule] Shutdown complete." << std::endl;
}

void ReturnPreventionModule::onConfigUpdate(const Json::Value& config) {
    configure(config);
}

// ============================================================================
// STATUS
// ============================================================================

bool ReturnPreventionModule::isHealthy() const {
    return enabled_;
}

Json::Value ReturnPreventionModule::getStatus() const {
    Json::Value status;
    status["name"] = getName();
    status["version"] = getVersion();
    status["enabled"] = enabled_;
    status["healthy"] = isHealthy();
    status["support_hotline_enabled"] = support_hotline_enabled_;
    if (support_hotline_enabled_) {
        status["support_hotline"] = support_hotline_number_;
    }

    Json::Value days_array(Json::arrayValue);
    for (int day : followup_days_) {
        days_array.append(day);
    }
    status["followup_days"] = days_array;

    std::lock_guard<std::mutex> lock(data_mutex_);
    status["active_adoptions"] = static_cast<int>(adoptions_.size());
    status["pending_followups"] = static_cast<int>(followup_tasks_.size());
    status["support_resources"] = static_cast<int>(support_resources_.size());

    return status;
}

Json::Value ReturnPreventionModule::getMetrics() const {
    Json::Value metrics;
    metrics["total_adoptions_tracked"] = static_cast<Json::Int64>(total_adoptions_tracked_.load());
    metrics["followups_completed"] = static_cast<Json::Int64>(followups_completed_.load());
    metrics["issues_reported"] = static_cast<Json::Int64>(issues_reported_.load());
    metrics["at_risk_interventions"] = static_cast<Json::Int64>(at_risk_interventions_.load());
    metrics["successful_completions"] = static_cast<Json::Int64>(successful_completions_.load());
    metrics["returns_recorded"] = static_cast<Json::Int64>(returns_recorded_.load());

    // Calculate success rate
    int64_t total = total_adoptions_tracked_.load();
    int64_t returns = returns_recorded_.load();
    if (total > 0) {
        double retention_rate = (1.0 - static_cast<double>(returns) / total) * 100.0;
        metrics["retention_rate"] = retention_rate;
    } else {
        metrics["retention_rate"] = 0.0;
    }

    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void ReturnPreventionModule::configure(const Json::Value& config) {
    if (config.isMember("support_hotline_enabled")) {
        support_hotline_enabled_ = config["support_hotline_enabled"].asBool();
    }
    if (config.isMember("support_hotline_number")) {
        support_hotline_number_ = config["support_hotline_number"].asString();
    }
    if (config.isMember("followup_days") && config["followup_days"].isArray()) {
        followup_days_.clear();
        for (const auto& day : config["followup_days"]) {
            followup_days_.push_back(day.asInt());
        }
    }
}

Json::Value ReturnPreventionModule::getDefaultConfig() const {
    Json::Value config;
    config["enabled"] = true;
    config["support_hotline_enabled"] = true;
    config["support_hotline_number"] = "1-800-DOG-HELP";

    Json::Value days(Json::arrayValue);
    days.append(3);
    days.append(7);
    days.append(14);
    days.append(30);
    days.append(60);
    days.append(90);
    config["followup_days"] = days;

    return config;
}

// ============================================================================
// RETURN PREVENTION API
// ============================================================================

AdoptionRecord ReturnPreventionModule::registerAdoption(const AdoptionRecord& adoption) {
    AdoptionRecord new_adoption = adoption;
    new_adoption.id = generateAdoptionId();
    new_adoption.status = "active";
    new_adoption.risk_score = 0;

    // Get current date if not provided
    if (new_adoption.adoption_date.empty()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
        new_adoption.adoption_date = ss.str();
    }

    // Store the adoption
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        adoptions_[new_adoption.id] = new_adoption;
    }

    // Schedule follow-ups
    scheduleFollowups(new_adoption.id, new_adoption.adoption_date);

    // Update metrics
    total_adoptions_tracked_++;

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "adoption_registered_for_followup";
    event.data["adoption_id"] = new_adoption.id;
    event.data["dog_id"] = new_adoption.dog_id;
    event.data["adopter_id"] = new_adoption.adopter_id;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[ReturnPreventionModule] Adoption " << new_adoption.id
              << " registered for follow-up. Dog: " << new_adoption.dog_name << std::endl;

    return new_adoption;
}

std::optional<AdoptionRecord> ReturnPreventionModule::getAdoption(const std::string& adoption_id) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = adoptions_.find(adoption_id);
    if (it != adoptions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<FollowupTask> ReturnPreventionModule::getPendingFollowups() {
    std::vector<FollowupTask> pending;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
    std::string today = ss.str();

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, task] : followup_tasks_) {
        if (task.status == "pending" && task.scheduled_date <= today) {
            pending.push_back(task);
        }
    }

    return pending;
}

bool ReturnPreventionModule::completeFollowup(const std::string& task_id,
                                               const Json::Value& response) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = followup_tasks_.find(task_id);
    if (it == followup_tasks_.end()) {
        return false;
    }

    it->second.status = "completed";
    it->second.response = response;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    it->second.completed_date = ss.str();

    // Update adoption record
    auto adoption_it = adoptions_.find(it->second.adoption_id);
    if (adoption_it != adoptions_.end()) {
        adoption_it->second.completed_followups.push_back(it->second.day_number);
    }

    followups_completed_++;

    std::cout << "[ReturnPreventionModule] Follow-up " << task_id
              << " completed (Day " << it->second.day_number << ")" << std::endl;

    return true;
}

ReturnRisk ReturnPreventionModule::assessReturnRisk(const std::string& adoption_id) {
    ReturnRisk risk;
    risk.adoption_id = adoption_id;
    risk.risk_score = 0;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = adoptions_.find(adoption_id);
    if (it == adoptions_.end()) {
        return risk;
    }

    risk.risk_score = calculateRiskScore(it->second);

    // Determine risk factors
    if (it->second.completed_followups.empty()) {
        risk.risk_factors.push_back("No follow-ups completed");
        risk.recommended_interventions.push_back("Schedule immediate outreach call");
    }

    // Check for missed follow-ups
    // (simplified check - would be more complex in production)
    if (it->second.completed_followups.size() < 2 && followup_days_.size() > 2) {
        risk.risk_factors.push_back("Missing scheduled follow-ups");
        risk.recommended_interventions.push_back("Send follow-up reminder email");
    }

    // Check notes for issues
    if (it->second.notes.isMember("issues")) {
        risk.risk_factors.push_back("Previous issues reported");
        risk.recommended_interventions.push_back("Schedule behavioral consultation");
    }

    if (risk.risk_score >= 70) {
        risk.recommended_interventions.push_back("Assign dedicated support coordinator");
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    risk.last_assessment_date = ss.str();

    return risk;
}

bool ReturnPreventionModule::reportIssue(const std::string& adoption_id,
                                          const std::string& issue_type,
                                          const std::string& description) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = adoptions_.find(adoption_id);
    if (it == adoptions_.end()) {
        return false;
    }

    // Add issue to notes
    if (!it->second.notes.isMember("issues")) {
        it->second.notes["issues"] = Json::Value(Json::arrayValue);
    }

    Json::Value issue;
    issue["type"] = issue_type;
    issue["description"] = description;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    issue["reported_at"] = ss.str();

    it->second.notes["issues"].append(issue);

    // Recalculate risk
    it->second.risk_score = calculateRiskScore(it->second);

    issues_reported_++;

    std::cout << "[ReturnPreventionModule] Issue reported for adoption " << adoption_id
              << ": " << issue_type << std::endl;

    // Publish event for notification
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "adoption_issue_reported";
    event.data["adoption_id"] = adoption_id;
    event.data["issue_type"] = issue_type;
    event.data["risk_score"] = it->second.risk_score;
    core::EventBus::getInstance().publishAsync(event);

    return true;
}

std::vector<SupportResource> ReturnPreventionModule::getResources(const std::string& issue_type) {
    std::vector<SupportResource> matching;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& resource : support_resources_) {
        if (resource.category == issue_type || resource.category == "general" ||
            issue_type == "all") {
            matching.push_back(resource);
        }
    }

    return matching;
}

void ReturnPreventionModule::markAtRisk(const std::string& adoption_id,
                                         const std::string& reason) {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = adoptions_.find(adoption_id);
        if (it != adoptions_.end()) {
            it->second.status = "at_risk";
            it->second.notes["at_risk_reason"] = reason;
        }
    }

    at_risk_interventions_++;

    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "adoption_marked_at_risk";
    event.data["adoption_id"] = adoption_id;
    event.data["reason"] = reason;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[ReturnPreventionModule] Adoption " << adoption_id
              << " marked at-risk: " << reason << std::endl;
}

void ReturnPreventionModule::recordSuccess(const std::string& adoption_id) {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = adoptions_.find(adoption_id);
        if (it != adoptions_.end()) {
            it->second.status = "completed";
        }
    }

    successful_completions_++;

    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "adoption_success";
    event.data["adoption_id"] = adoption_id;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[ReturnPreventionModule] Adoption " << adoption_id
              << " marked as successful!" << std::endl;
}

void ReturnPreventionModule::recordReturn(const std::string& adoption_id,
                                           const std::string& return_reason,
                                           const std::string& return_date) {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = adoptions_.find(adoption_id);
        if (it != adoptions_.end()) {
            it->second.status = "returned";
            it->second.notes["return_reason"] = return_reason;
            it->second.notes["return_date"] = return_date;
        }
    }

    returns_recorded_++;

    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "adoption_returned";
    event.data["adoption_id"] = adoption_id;
    event.data["return_reason"] = return_reason;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[ReturnPreventionModule] Return recorded for adoption " << adoption_id
              << ". Reason: " << return_reason << std::endl;
}

Json::Value ReturnPreventionModule::getStats(const std::string& start_date,
                                              const std::string& end_date) {
    Json::Value stats;
    stats["period_start"] = start_date;
    stats["period_end"] = end_date;
    stats["total_tracked"] = static_cast<Json::Int64>(total_adoptions_tracked_.load());
    stats["followups_completed"] = static_cast<Json::Int64>(followups_completed_.load());
    stats["issues_reported"] = static_cast<Json::Int64>(issues_reported_.load());
    stats["at_risk_interventions"] = static_cast<Json::Int64>(at_risk_interventions_.load());
    stats["successful_completions"] = static_cast<Json::Int64>(successful_completions_.load());
    stats["returns"] = static_cast<Json::Int64>(returns_recorded_.load());

    // Calculate rates
    int64_t total = total_adoptions_tracked_.load();
    if (total > 0) {
        stats["retention_rate"] = (1.0 - static_cast<double>(returns_recorded_.load()) / total) * 100.0;
        stats["completion_rate"] = static_cast<double>(successful_completions_.load()) / total * 100.0;
    }

    return stats;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void ReturnPreventionModule::handleDogAdopted(const core::Event& event) {
    AdoptionRecord adoption;
    adoption.dog_id = event.data.get("dog_id", "").asString();
    adoption.dog_name = event.data.get("dog_name", "").asString();
    adoption.adopter_id = event.data.get("adopter_id", "").asString();
    adoption.adopter_name = event.data.get("adopter_name", "").asString();
    adoption.adopter_email = event.data.get("adopter_email", "").asString();
    adoption.shelter_id = event.data.get("shelter_id", "").asString();

    if (!adoption.dog_id.empty() && !adoption.adopter_id.empty()) {
        registerAdoption(adoption);
    }
}

void ReturnPreventionModule::handleFollowupResponse(const core::Event& event) {
    std::string task_id = event.data.get("task_id", "").asString();
    if (!task_id.empty()) {
        Json::Value response;
        response["satisfaction"] = event.data.get("satisfaction", 5).asInt();
        response["issues"] = event.data.get("issues", "").asString();
        response["comments"] = event.data.get("comments", "").asString();
        completeFollowup(task_id, response);
    }
}

void ReturnPreventionModule::handleReturnRequest(const core::Event& event) {
    std::string adoption_id = event.data.get("adoption_id", "").asString();
    if (!adoption_id.empty()) {
        // Mark as at-risk and trigger intervention
        markAtRisk(adoption_id, "Return requested by adopter");

        // Publish intervention needed event
        core::Event intervention;
        intervention.type = core::EventType::CUSTOM;
        intervention.data["type"] = "urgent_intervention_needed";
        intervention.data["adoption_id"] = adoption_id;
        intervention.data["reason"] = "Return request received";
        core::EventBus::getInstance().publishAsync(intervention);
    }
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void ReturnPreventionModule::loadSupportResources() {
    support_resources_ = {
        {"sr_001", "Positive Reinforcement Training Guide", "training",
         "Free online guide to positive training methods",
         "https://trainingguide.org", "", true},
        {"sr_002", "Certified Dog Trainer Directory", "training",
         "Find certified positive trainers in your area",
         "https://cpdt.org/find-trainer", "", false},
        {"sr_003", "Behavioral Consultation Hotline", "behavioral",
         "Free phone consultations with behaviorists",
         "", "1-800-DOG-BEHAVIOR", true},
        {"sr_004", "Veterinary Behavior Specialist Referral", "behavioral",
         "Referrals to veterinary behaviorists",
         "https://dacvb.org", "", false},
        {"sr_005", "Low-Cost Veterinary Care Finder", "medical",
         "Directory of affordable vet services",
         "https://lowcostvet.org", "", true},
        {"sr_006", "Pet Insurance Comparison", "medical",
         "Compare pet insurance options",
         "https://petinsurancecompare.org", "", true},
        {"sr_007", "New Dog Integration Guide", "general",
         "Tips for helping your new dog adjust",
         "https://newdogguide.org", "", true},
        {"sr_008", "Adopter Support Community", "general",
         "Online community for new adopters",
         "https://adopterscommunity.org", "", true}
    };
}

std::string ReturnPreventionModule::generateAdoptionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "ADP-" << std::put_time(std::gmtime(&time_t), "%Y%m%d")
       << "-" << dis(gen);
    return ss.str();
}

std::string ReturnPreventionModule::generateTaskId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    std::stringstream ss;
    ss << "FUT-" << dis(gen);
    return ss.str();
}

void ReturnPreventionModule::scheduleFollowups(const std::string& adoption_id,
                                                const std::string& adoption_date) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    std::vector<std::string> task_ids;

    for (int day : followup_days_) {
        FollowupTask task;
        task.id = generateTaskId();
        task.adoption_id = adoption_id;
        task.day_number = day;
        task.scheduled_date = calculateScheduledDate(adoption_date, day);
        task.type = (day <= 7) ? "phone" : "email";
        task.status = "pending";

        followup_tasks_[task.id] = task;
        task_ids.push_back(task.id);
    }

    adoption_tasks_[adoption_id] = task_ids;

    std::cout << "[ReturnPreventionModule] Scheduled " << task_ids.size()
              << " follow-ups for adoption " << adoption_id << std::endl;
}

std::string ReturnPreventionModule::calculateScheduledDate(const std::string& adoption_date,
                                                            int days) {
    // Parse adoption date
    std::tm tm = {};
    std::istringstream ss(adoption_date);
    ss >> std::get_time(&tm, "%Y-%m-%d");

    // Add days
    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    time_point += std::chrono::hours(24 * days);

    auto new_time = std::chrono::system_clock::to_time_t(time_point);
    std::stringstream result;
    result << std::put_time(std::gmtime(&new_time), "%Y-%m-%d");

    return result.str();
}

int ReturnPreventionModule::calculateRiskScore(const AdoptionRecord& adoption) {
    int score = 0;

    // Base score starts at 20
    score = 20;

    // Incomplete follow-ups increase risk
    size_t expected_followups = 0;
    // Count how many follow-ups should have happened by now
    // (simplified - would use actual date comparison in production)
    expected_followups = followup_days_.size() / 2;

    if (adoption.completed_followups.size() < expected_followups) {
        score += 20;
    }

    // Issues reported increase risk
    if (adoption.notes.isMember("issues")) {
        int issue_count = adoption.notes["issues"].size();
        score += issue_count * 15;
    }

    // At-risk status
    if (adoption.status == "at_risk") {
        score += 30;
    }

    // Cap at 100
    return std::min(score, 100);
}

} // namespace wtl::modules::intervention
