/**
 * @file EmployerPartnershipsModule.cc
 * @brief Implementation of the EmployerPartnershipsModule
 * @see EmployerPartnershipsModule.h for documentation
 */

#include "EmployerPartnershipsModule.h"
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

bool EmployerPartnershipsModule::initialize() {
    std::cout << "[EmployerPartnershipsModule] Initializing..." << std::endl;

    // Subscribe to events
    auto& event_bus = core::EventBus::getInstance();

    auto adopted_id = event_bus.subscribe(
        core::EventType::DOG_ADOPTED,
        [this](const core::Event& event) {
            handleDogAdopted(event);
        },
        "employer_partnerships_adopted_handler"
    );
    subscription_ids_.push_back(adopted_id);

    auto claim_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "benefit_claim_submitted") {
                handleBenefitClaimSubmitted(event);
            }
        },
        "employer_partnerships_claim_handler"
    );
    subscription_ids_.push_back(claim_id);

    auto inquiry_id = event_bus.subscribe(
        core::EventType::CUSTOM,
        [this](const core::Event& event) {
            if (event.data.isMember("type") &&
                event.data["type"].asString() == "partner_inquiry") {
                handlePartnerInquiry(event);
            }
        },
        "employer_partnerships_inquiry_handler"
    );
    subscription_ids_.push_back(inquiry_id);

    enabled_ = true;
    std::cout << "[EmployerPartnershipsModule] Initialization complete." << std::endl;
    std::cout << "[EmployerPartnershipsModule] Max adoption benefit: $"
              << max_adoption_benefit_ << std::endl;
    std::cout << "[EmployerPartnershipsModule] Default pawternity days: "
              << default_pawternity_days_ << std::endl;

    return true;
}

void EmployerPartnershipsModule::shutdown() {
    std::cout << "[EmployerPartnershipsModule] Shutting down..." << std::endl;

    auto& event_bus = core::EventBus::getInstance();
    for (const auto& id : subscription_ids_) {
        event_bus.unsubscribe(id);
    }
    subscription_ids_.clear();

    enabled_ = false;
    std::cout << "[EmployerPartnershipsModule] Shutdown complete." << std::endl;
}

void EmployerPartnershipsModule::onConfigUpdate(const Json::Value& config) {
    configure(config);
}

// ============================================================================
// STATUS
// ============================================================================

bool EmployerPartnershipsModule::isHealthy() const {
    return enabled_;
}

Json::Value EmployerPartnershipsModule::getStatus() const {
    Json::Value status;
    status["name"] = getName();
    status["version"] = getVersion();
    status["enabled"] = enabled_;
    status["healthy"] = isHealthy();
    status["max_adoption_benefit"] = max_adoption_benefit_;
    status["default_pawternity_days"] = default_pawternity_days_;

    std::lock_guard<std::mutex> lock(data_mutex_);
    status["total_partners"] = static_cast<int>(partners_.size());
    status["pending_claims"] = 0;
    status["pending_pawternity"] = 0;

    int pending_claims = 0, pending_pawt = 0;
    for (const auto& [id, claim] : benefit_claims_) {
        if (claim.status == "pending") pending_claims++;
    }
    for (const auto& [id, req] : pawternity_requests_) {
        if (req.status == "pending") pending_pawt++;
    }
    status["pending_claims"] = pending_claims;
    status["pending_pawternity"] = pending_pawt;

    return status;
}

Json::Value EmployerPartnershipsModule::getMetrics() const {
    Json::Value metrics;
    metrics["partners_registered"] = static_cast<Json::Int64>(partners_registered_.load());
    metrics["claims_submitted"] = static_cast<Json::Int64>(claims_submitted_.load());
    metrics["claims_approved"] = static_cast<Json::Int64>(claims_approved_.load());
    metrics["pawternity_requests"] = static_cast<Json::Int64>(pawternity_requests_count_.load());
    metrics["pawternity_days_granted"] = static_cast<Json::Int64>(pawternity_days_granted_.load());
    metrics["total_benefits_paid"] = total_benefits_paid_.load();
    metrics["adoptions_through_partners"] = static_cast<Json::Int64>(adoptions_through_partners_.load());

    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void EmployerPartnershipsModule::configure(const Json::Value& config) {
    if (config.isMember("max_adoption_benefit")) {
        max_adoption_benefit_ = config["max_adoption_benefit"].asDouble();
    }
    if (config.isMember("pawternity_days")) {
        default_pawternity_days_ = config["pawternity_days"].asInt();
    }
}

Json::Value EmployerPartnershipsModule::getDefaultConfig() const {
    Json::Value config;
    config["enabled"] = true;
    config["max_adoption_benefit"] = 500.0;
    config["pawternity_days"] = 3;
    return config;
}

// ============================================================================
// PARTNER MANAGEMENT
// ============================================================================

EmployerPartner EmployerPartnershipsModule::registerPartner(const EmployerPartner& partner) {
    EmployerPartner new_partner = partner;
    new_partner.id = generatePartnerId();
    new_partner.active = true;
    new_partner.total_adoptions = 0;
    new_partner.total_benefits_paid = 0.0;

    // Set default benefits if not specified
    if (new_partner.adoption_benefit_amount <= 0) {
        new_partner.adoption_benefit_amount = max_adoption_benefit_;
    }
    if (new_partner.pawternity_days <= 0) {
        new_partner.pawternity_days = default_pawternity_days_;
    }

    // Calculate initial tier
    new_partner.partnership_tier = "bronze";

    // Set join date
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
    new_partner.join_date = ss.str();

    // Extract email domain for lookup
    std::string domain = "";
    if (!new_partner.contact_email.empty()) {
        domain = extractEmailDomain(new_partner.contact_email);
    }

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        partners_[new_partner.id] = new_partner;
        if (!domain.empty()) {
            email_domain_to_partner_[domain] = new_partner.id;
        }
    }

    partners_registered_++;

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "employer_partner_registered";
    event.data["partner_id"] = new_partner.id;
    event.data["company_name"] = new_partner.company_name;
    event.data["employee_count"] = new_partner.employee_count;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[EmployerPartnershipsModule] Partner registered: " << new_partner.company_name
              << " (" << new_partner.employee_count << " employees)" << std::endl;

    return new_partner;
}

bool EmployerPartnershipsModule::updatePartner(const EmployerPartner& partner) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = partners_.find(partner.id);
    if (it == partners_.end()) {
        return false;
    }

    it->second = partner;
    return true;
}

std::optional<EmployerPartner> EmployerPartnershipsModule::getPartner(const std::string& partner_id) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = partners_.find(partner_id);
    if (it != partners_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<EmployerPartner> EmployerPartnershipsModule::getPartnerByEmail(const std::string& email) {
    std::string domain = extractEmailDomain(email);
    if (domain.empty()) return std::nullopt;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = email_domain_to_partner_.find(domain);
    if (it != email_domain_to_partner_.end()) {
        auto partner_it = partners_.find(it->second);
        if (partner_it != partners_.end()) {
            return partner_it->second;
        }
    }

    return std::nullopt;
}

std::vector<EmployerPartner> EmployerPartnershipsModule::getActivePartners() {
    std::vector<EmployerPartner> active;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, partner] : partners_) {
        if (partner.active) {
            active.push_back(partner);
        }
    }

    return active;
}

std::vector<EmployerPartner> EmployerPartnershipsModule::getPartnersByTier(const std::string& tier) {
    std::vector<EmployerPartner> result;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, partner] : partners_) {
        if (partner.active && partner.partnership_tier == tier) {
            result.push_back(partner);
        }
    }

    return result;
}

// ============================================================================
// ADOPTION BENEFITS
// ============================================================================

AdoptionBenefitClaim EmployerPartnershipsModule::submitBenefitClaim(const AdoptionBenefitClaim& claim) {
    AdoptionBenefitClaim new_claim = claim;
    new_claim.id = generateClaimId();
    new_claim.status = "pending";
    new_claim.approved_amount = 0.0;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    new_claim.submitted_at = ss.str();

    // Cap requested amount at max benefit
    auto partner = getPartner(new_claim.employer_id);
    if (partner.has_value()) {
        new_claim.requested_amount = std::min(new_claim.requested_amount,
                                               partner->adoption_benefit_amount);
    }

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        benefit_claims_[new_claim.id] = new_claim;
    }

    claims_submitted_++;

    // Publish event
    core::Event event;
    event.type = core::EventType::CUSTOM;
    event.data["type"] = "benefit_claim_created";
    event.data["claim_id"] = new_claim.id;
    event.data["employer_id"] = new_claim.employer_id;
    event.data["amount"] = new_claim.requested_amount;
    core::EventBus::getInstance().publishAsync(event);

    std::cout << "[EmployerPartnershipsModule] Benefit claim submitted: " << new_claim.id
              << " for $" << new_claim.requested_amount << std::endl;

    return new_claim;
}

std::optional<AdoptionBenefitClaim> EmployerPartnershipsModule::getBenefitClaim(
    const std::string& claim_id) {

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = benefit_claims_.find(claim_id);
    if (it != benefit_claims_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool EmployerPartnershipsModule::processBenefitClaim(const std::string& claim_id,
                                                       bool approved,
                                                       double approved_amount,
                                                       const std::string& notes) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = benefit_claims_.find(claim_id);
    if (it == benefit_claims_.end()) {
        return false;
    }

    it->second.status = approved ? "approved" : "denied";
    if (approved) {
        it->second.approved_amount = (approved_amount >= 0) ?
            approved_amount : it->second.requested_amount;
    }
    if (!notes.empty()) {
        it->second.notes = notes;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    it->second.processed_at = ss.str();

    if (approved) {
        claims_approved_++;
        auto current = total_benefits_paid_.load();
        while (!total_benefits_paid_.compare_exchange_weak(current, current + it->second.approved_amount));

        // Update partner stats
        auto partner_it = partners_.find(it->second.employer_id);
        if (partner_it != partners_.end()) {
            partner_it->second.total_adoptions++;
            partner_it->second.total_benefits_paid += it->second.approved_amount;

            // Recalculate tier
            partner_it->second.partnership_tier = calculatePartnerTier(
                partner_it->second.total_adoptions,
                partner_it->second.total_benefits_paid,
                partner_it->second.employee_count);
        }
    }

    std::cout << "[EmployerPartnershipsModule] Claim " << claim_id
              << " " << (approved ? "approved" : "denied") << std::endl;

    return true;
}

std::vector<AdoptionBenefitClaim> EmployerPartnershipsModule::getPendingClaims(
    const std::string& partner_id) {

    std::vector<AdoptionBenefitClaim> pending;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, claim] : benefit_claims_) {
        if (claim.status == "pending" &&
            (partner_id.empty() || claim.employer_id == partner_id)) {
            pending.push_back(claim);
        }
    }

    return pending;
}

Json::Value EmployerPartnershipsModule::checkEmployeeEligibility(const std::string& employee_email) {
    Json::Value result;
    result["email"] = employee_email;
    result["eligible"] = false;

    auto partner = getPartnerByEmail(employee_email);
    if (!partner.has_value()) {
        result["reason"] = "No partnership found for email domain";
        return result;
    }

    if (!partner->active) {
        result["reason"] = "Partnership is not active";
        return result;
    }

    result["eligible"] = true;
    result["employer_id"] = partner->id;
    result["company_name"] = partner->company_name;
    result["benefits"]["adoption_benefit"] = partner->offers_adoption_benefit;
    result["benefits"]["adoption_benefit_amount"] = partner->adoption_benefit_amount;
    result["benefits"]["pawternity_leave"] = partner->offers_pawternity_leave;
    result["benefits"]["pawternity_days"] = partner->pawternity_days;
    result["benefits"]["pet_insurance"] = partner->offers_pet_insurance;

    return result;
}

// ============================================================================
// PAWTERNITY LEAVE
// ============================================================================

PawtternityRequest EmployerPartnershipsModule::submitPawtternityRequest(
    const PawtternityRequest& request) {

    PawtternityRequest new_request = request;
    new_request.id = generateRequestId();
    new_request.status = "pending";
    new_request.days_approved = 0;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    new_request.submitted_at = ss.str();

    // Cap days at partner's max
    auto partner = getPartner(new_request.employer_id);
    if (partner.has_value()) {
        new_request.days_requested = std::min(new_request.days_requested,
                                               partner->pawternity_days);
    }

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        pawternity_requests_[new_request.id] = new_request;
    }

    pawternity_requests_count_++;

    std::cout << "[EmployerPartnershipsModule] Pawternity request submitted: " << new_request.id
              << " for " << new_request.days_requested << " days" << std::endl;

    return new_request;
}

std::optional<PawtternityRequest> EmployerPartnershipsModule::getPawtternityRequest(
    const std::string& request_id) {

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = pawternity_requests_.find(request_id);
    if (it != pawternity_requests_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool EmployerPartnershipsModule::processPawtternityRequest(const std::string& request_id,
                                                             bool approved,
                                                             int days_approved) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = pawternity_requests_.find(request_id);
    if (it == pawternity_requests_.end()) {
        return false;
    }

    it->second.status = approved ? "approved" : "denied";
    if (approved) {
        it->second.days_approved = (days_approved >= 0) ?
            days_approved : it->second.days_requested;
        pawternity_days_granted_ += it->second.days_approved;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    it->second.processed_at = ss.str();

    std::cout << "[EmployerPartnershipsModule] Pawternity request " << request_id
              << " " << (approved ? "approved" : "denied") << std::endl;

    return true;
}

std::vector<PawtternityRequest> EmployerPartnershipsModule::getPendingPawtternityRequests(
    const std::string& partner_id) {

    std::vector<PawtternityRequest> pending;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, req] : pawternity_requests_) {
        if (req.status == "pending" &&
            (partner_id.empty() || req.employer_id == partner_id)) {
            pending.push_back(req);
        }
    }

    return pending;
}

// ============================================================================
// REPORTING
// ============================================================================

PartnershipMetrics EmployerPartnershipsModule::getPartnershipMetrics(const std::string& partner_id) {
    PartnershipMetrics metrics;
    metrics.employer_id = partner_id;
    metrics.total_adoptions = 0;
    metrics.adoptions_this_year = 0;
    metrics.adoptions_this_month = 0;
    metrics.total_benefits_paid = 0.0;
    metrics.benefits_paid_this_year = 0.0;
    metrics.pawternity_days_used = 0;
    metrics.employee_participation_rate = 0.0;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = partners_.find(partner_id);
    if (it != partners_.end()) {
        metrics.total_adoptions = it->second.total_adoptions;
        metrics.total_benefits_paid = it->second.total_benefits_paid;

        if (it->second.employee_count > 0) {
            metrics.employee_participation_rate =
                static_cast<double>(it->second.total_adoptions) /
                it->second.employee_count * 100.0;
        }
    }

    // Count pawternity days
    for (const auto& [id, req] : pawternity_requests_) {
        if (req.employer_id == partner_id && req.status == "approved") {
            metrics.pawternity_days_used += req.days_approved;
        }
    }

    return metrics;
}

Json::Value EmployerPartnershipsModule::getProgramStats() {
    Json::Value stats;
    stats["total_partners"] = static_cast<Json::Int64>(partners_registered_.load());
    stats["claims_submitted"] = static_cast<Json::Int64>(claims_submitted_.load());
    stats["claims_approved"] = static_cast<Json::Int64>(claims_approved_.load());
    stats["total_benefits_paid"] = total_benefits_paid_.load();
    stats["pawternity_requests"] = static_cast<Json::Int64>(pawternity_requests_count_.load());
    stats["pawternity_days_granted"] = static_cast<Json::Int64>(pawternity_days_granted_.load());
    stats["adoptions_through_partners"] = static_cast<Json::Int64>(adoptions_through_partners_.load());

    // Count by tier
    std::map<std::string, int> tier_counts;
    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, partner] : partners_) {
        if (partner.active) {
            tier_counts[partner.partnership_tier]++;
        }
    }

    Json::Value tiers(Json::objectValue);
    for (const auto& [tier, count] : tier_counts) {
        tiers[tier] = count;
    }
    stats["partners_by_tier"] = tiers;

    return stats;
}

Json::Value EmployerPartnershipsModule::getPartnerLeaderboard(const std::string& metric, int limit) {
    Json::Value leaderboard(Json::arrayValue);

    std::vector<std::pair<std::string, double>> rankings;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [id, partner] : partners_) {
        if (!partner.active) continue;

        double score = 0;
        if (metric == "adoptions") {
            score = partner.total_adoptions;
        } else if (metric == "benefits") {
            score = partner.total_benefits_paid;
        } else if (metric == "participation") {
            if (partner.employee_count > 0) {
                score = static_cast<double>(partner.total_adoptions) / partner.employee_count * 100;
            }
        }

        rankings.emplace_back(id, score);
    }

    std::sort(rankings.begin(), rankings.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    int count = 0;
    for (const auto& [id, score] : rankings) {
        if (count >= limit) break;

        auto it = partners_.find(id);
        if (it != partners_.end()) {
            Json::Value entry;
            entry["rank"] = count + 1;
            entry["partner_id"] = id;
            entry["company_name"] = it->second.company_name;
            entry["tier"] = it->second.partnership_tier;
            entry["score"] = score;
            leaderboard.append(entry);
            count++;
        }
    }

    return leaderboard;
}

Json::Value EmployerPartnershipsModule::generatePartnerReport(const std::string& partner_id,
                                                                const std::string& start_date,
                                                                const std::string& end_date) {
    Json::Value report;
    report["partner_id"] = partner_id;
    report["period_start"] = start_date;
    report["period_end"] = end_date;

    auto partner = getPartner(partner_id);
    if (!partner.has_value()) {
        report["error"] = "Partner not found";
        return report;
    }

    report["company_name"] = partner->company_name;
    report["tier"] = partner->partnership_tier;
    report["employee_count"] = partner->employee_count;

    auto metrics = getPartnershipMetrics(partner_id);
    report["total_adoptions"] = metrics.total_adoptions;
    report["total_benefits_paid"] = metrics.total_benefits_paid;
    report["pawternity_days_used"] = metrics.pawternity_days_used;
    report["participation_rate"] = metrics.employee_participation_rate;

    // List of benefits
    Json::Value benefits;
    benefits["adoption_benefit"] = partner->offers_adoption_benefit;
    benefits["adoption_benefit_amount"] = partner->adoption_benefit_amount;
    benefits["pawternity_leave"] = partner->offers_pawternity_leave;
    benefits["pawternity_days"] = partner->pawternity_days;
    benefits["pet_friendly_office"] = partner->pet_friendly_office;
    benefits["pet_insurance"] = partner->offers_pet_insurance;
    report["benefits_offered"] = benefits;

    return report;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void EmployerPartnershipsModule::handleDogAdopted(const core::Event& event) {
    std::string adopter_email = event.data.get("adopter_email", "").asString();
    if (adopter_email.empty()) return;

    auto partner = getPartnerByEmail(adopter_email);
    if (partner.has_value()) {
        adoptions_through_partners_++;

        // Publish event about partner adoption
        core::Event partner_event;
        partner_event.type = core::EventType::CUSTOM;
        partner_event.data["type"] = "partner_employee_adopted";
        partner_event.data["partner_id"] = partner->id;
        partner_event.data["company_name"] = partner->company_name;
        partner_event.data["dog_id"] = event.data.get("dog_id", "").asString();
        partner_event.data["eligible_for_benefit"] = partner->offers_adoption_benefit;
        partner_event.data["eligible_for_pawternity"] = partner->offers_pawternity_leave;
        core::EventBus::getInstance().publishAsync(partner_event);

        std::cout << "[EmployerPartnershipsModule] Adoption by " << partner->company_name
                  << " employee detected" << std::endl;
    }
}

void EmployerPartnershipsModule::handleBenefitClaimSubmitted(const core::Event& event) {
    AdoptionBenefitClaim claim;
    claim.employer_id = event.data.get("employer_id", "").asString();
    claim.employee_email = event.data.get("employee_email", "").asString();
    claim.employee_name = event.data.get("employee_name", "").asString();
    claim.dog_id = event.data.get("dog_id", "").asString();
    claim.dog_name = event.data.get("dog_name", "").asString();
    claim.adoption_fee = event.data.get("adoption_fee", 0.0).asDouble();
    claim.requested_amount = event.data.get("requested_amount", 0.0).asDouble();

    if (!claim.employer_id.empty() && !claim.employee_email.empty()) {
        submitBenefitClaim(claim);
    }
}

void EmployerPartnershipsModule::handlePartnerInquiry(const core::Event& event) {
    std::string company_name = event.data.get("company_name", "").asString();
    std::string contact_email = event.data.get("contact_email", "").asString();

    std::cout << "[EmployerPartnershipsModule] Partnership inquiry from: "
              << company_name << " (" << contact_email << ")" << std::endl;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

std::string EmployerPartnershipsModule::generatePartnerId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    std::stringstream ss;
    ss << "EMP-" << dis(gen);
    return ss.str();
}

std::string EmployerPartnershipsModule::generateClaimId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "CLM-" << std::put_time(std::gmtime(&time_t), "%Y%m%d")
       << "-" << dis(gen);
    return ss.str();
}

std::string EmployerPartnershipsModule::generateRequestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    std::stringstream ss;
    ss << "PAW-" << dis(gen);
    return ss.str();
}

std::string EmployerPartnershipsModule::extractEmailDomain(const std::string& email) {
    size_t at_pos = email.find('@');
    if (at_pos == std::string::npos) {
        return "";
    }
    return email.substr(at_pos + 1);
}

std::string EmployerPartnershipsModule::calculatePartnerTier(int adoptions,
                                                               double benefits_paid,
                                                               int employee_count) {
    // Calculate participation rate
    double participation = (employee_count > 0) ?
        static_cast<double>(adoptions) / employee_count * 100 : 0;

    // Platinum: 50+ adoptions or 5%+ participation
    if (adoptions >= 50 || participation >= 5.0) {
        return "platinum";
    }
    // Gold: 25+ adoptions or 2%+ participation
    if (adoptions >= 25 || participation >= 2.0) {
        return "gold";
    }
    // Silver: 10+ adoptions or 1%+ participation
    if (adoptions >= 10 || participation >= 1.0) {
        return "silver";
    }
    // Bronze: default
    return "bronze";
}

} // namespace wtl::modules::intervention
