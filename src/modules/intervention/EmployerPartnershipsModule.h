/**
 * @file EmployerPartnershipsModule.h
 * @brief Module for managing employer-sponsored pet adoption programs
 *
 * PURPOSE:
 * Partners with employers to promote pet adoption as an employee benefit,
 * including "pawternity" leave, adoption fee reimbursement, and workplace
 * pet-friendly policies to increase adoption rates.
 *
 * USAGE:
 * Enable in config.json:
 * {
 *     "modules": {
 *         "employer_partnerships": {
 *             "enabled": true,
 *             "max_adoption_benefit": 500,
 *             "pawternity_days": 3
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

#include <json/json.h>

#include "modules/IModule.h"
#include "core/EventBus.h"

namespace wtl::modules::intervention {

/**
 * @struct EmployerPartner
 * @brief An employer partner in the program
 */
struct EmployerPartner {
    std::string id;
    std::string company_name;
    std::string industry;
    std::string contact_name;
    std::string contact_email;
    std::string contact_phone;
    std::string city;
    std::string state;
    int employee_count;
    std::string partnership_tier; // bronze, silver, gold, platinum
    bool offers_adoption_benefit;
    double adoption_benefit_amount;
    bool offers_pawternity_leave;
    int pawternity_days;
    bool pet_friendly_office;
    bool offers_pet_insurance;
    std::string join_date;
    bool active;
    int total_adoptions;
    double total_benefits_paid;
};

/**
 * @struct AdoptionBenefitClaim
 * @brief A claim for adoption benefits from an employee
 */
struct AdoptionBenefitClaim {
    std::string id;
    std::string employer_id;
    std::string employee_email;
    std::string employee_name;
    std::string dog_id;
    std::string dog_name;
    std::string shelter_name;
    double adoption_fee;
    double requested_amount;
    double approved_amount;
    std::string status; // pending, approved, denied, paid
    std::string submitted_at;
    std::string processed_at;
    std::string notes;
    std::string receipt_url;
};

/**
 * @struct PawtternityRequest
 * @brief A request for pawternity leave
 */
struct PawtternityRequest {
    std::string id;
    std::string employer_id;
    std::string employee_email;
    std::string employee_name;
    std::string dog_id;
    std::string dog_name;
    std::string adoption_date;
    int days_requested;
    int days_approved;
    std::string start_date;
    std::string end_date;
    std::string status; // pending, approved, denied
    std::string submitted_at;
    std::string processed_at;
};

/**
 * @struct PartnershipMetrics
 * @brief Metrics for an employer partnership
 */
struct PartnershipMetrics {
    std::string employer_id;
    int total_adoptions;
    int adoptions_this_year;
    int adoptions_this_month;
    double total_benefits_paid;
    double benefits_paid_this_year;
    int pawternity_days_used;
    double employee_participation_rate;
};

/**
 * @class EmployerPartnershipsModule
 * @brief Module for employer-sponsored adoption programs
 *
 * Features:
 * - Employer partner registry
 * - Adoption benefit claims processing
 * - Pawternity leave management
 * - Employee adoption tracking
 * - Partnership ROI reporting
 * - Marketing materials for partners
 */
class EmployerPartnershipsModule : public IModule {
public:
    EmployerPartnershipsModule() = default;
    ~EmployerPartnershipsModule() override = default;

    // ========================================================================
    // IModule INTERFACE - LIFECYCLE
    // ========================================================================

    bool initialize() override;
    void shutdown() override;
    void onConfigUpdate(const Json::Value& config) override;

    // ========================================================================
    // IModule INTERFACE - IDENTITY
    // ========================================================================

    std::string getName() const override { return "employer_partnerships"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "Employer-sponsored pet adoption programs with benefits and pawternity leave";
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
    // PARTNER MANAGEMENT
    // ========================================================================

    /**
     * @brief Register a new employer partner
     * @param partner The partner details
     * @return The registered partner with ID
     */
    EmployerPartner registerPartner(const EmployerPartner& partner);

    /**
     * @brief Update partner information
     * @param partner Updated partner data
     * @return true if updated
     */
    bool updatePartner(const EmployerPartner& partner);

    /**
     * @brief Get partner by ID
     * @param partner_id The partner ID
     * @return The partner if found
     */
    std::optional<EmployerPartner> getPartner(const std::string& partner_id);

    /**
     * @brief Get partner by employee email domain
     * @param email The employee email
     * @return The partner if found by domain match
     */
    std::optional<EmployerPartner> getPartnerByEmail(const std::string& email);

    /**
     * @brief Get all active partners
     * @return List of active partners
     */
    std::vector<EmployerPartner> getActivePartners();

    /**
     * @brief Get partners by tier
     * @param tier The partnership tier
     * @return List of partners in that tier
     */
    std::vector<EmployerPartner> getPartnersByTier(const std::string& tier);

    // ========================================================================
    // ADOPTION BENEFITS
    // ========================================================================

    /**
     * @brief Submit an adoption benefit claim
     * @param claim The claim details
     * @return The submitted claim with ID
     */
    AdoptionBenefitClaim submitBenefitClaim(const AdoptionBenefitClaim& claim);

    /**
     * @brief Get benefit claim by ID
     * @param claim_id The claim ID
     * @return The claim if found
     */
    std::optional<AdoptionBenefitClaim> getBenefitClaim(const std::string& claim_id);

    /**
     * @brief Process a benefit claim (approve/deny)
     * @param claim_id The claim ID
     * @param approved Whether to approve
     * @param approved_amount Amount to approve (if different)
     * @param notes Processing notes
     * @return true if processed
     */
    bool processBenefitClaim(const std::string& claim_id,
                              bool approved,
                              double approved_amount = -1,
                              const std::string& notes = "");

    /**
     * @brief Get pending benefit claims for a partner
     * @param partner_id The partner ID
     * @return List of pending claims
     */
    std::vector<AdoptionBenefitClaim> getPendingClaims(const std::string& partner_id);

    /**
     * @brief Check if employee is eligible for benefits
     * @param employee_email The employee email
     * @return Eligibility info as JSON
     */
    Json::Value checkEmployeeEligibility(const std::string& employee_email);

    // ========================================================================
    // PAWTERNITY LEAVE
    // ========================================================================

    /**
     * @brief Submit a pawternity leave request
     * @param request The request details
     * @return The submitted request with ID
     */
    PawtternityRequest submitPawtternityRequest(const PawtternityRequest& request);

    /**
     * @brief Get pawternity request by ID
     * @param request_id The request ID
     * @return The request if found
     */
    std::optional<PawtternityRequest> getPawtternityRequest(const std::string& request_id);

    /**
     * @brief Process a pawternity request
     * @param request_id The request ID
     * @param approved Whether to approve
     * @param days_approved Days approved (can be different from requested)
     * @return true if processed
     */
    bool processPawtternityRequest(const std::string& request_id,
                                     bool approved,
                                     int days_approved = -1);

    /**
     * @brief Get pending pawternity requests for a partner
     * @param partner_id The partner ID
     * @return List of pending requests
     */
    std::vector<PawtternityRequest> getPendingPawtternityRequests(const std::string& partner_id);

    // ========================================================================
    // REPORTING
    // ========================================================================

    /**
     * @brief Get partnership metrics for a partner
     * @param partner_id The partner ID
     * @return Partnership metrics
     */
    PartnershipMetrics getPartnershipMetrics(const std::string& partner_id);

    /**
     * @brief Get program-wide statistics
     * @return Program statistics as JSON
     */
    Json::Value getProgramStats();

    /**
     * @brief Get partner leaderboard
     * @param metric Metric to rank by (adoptions, benefits, participation)
     * @param limit Number of partners to return
     * @return Leaderboard as JSON
     */
    Json::Value getPartnerLeaderboard(const std::string& metric = "adoptions",
                                        int limit = 10);

    /**
     * @brief Generate partner report
     * @param partner_id The partner ID
     * @param start_date Start of period
     * @param end_date End of period
     * @return Report as JSON
     */
    Json::Value generatePartnerReport(const std::string& partner_id,
                                        const std::string& start_date,
                                        const std::string& end_date);

private:
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    void handleDogAdopted(const core::Event& event);
    void handleBenefitClaimSubmitted(const core::Event& event);
    void handlePartnerInquiry(const core::Event& event);

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    std::string generatePartnerId();
    std::string generateClaimId();
    std::string generateRequestId();
    std::string extractEmailDomain(const std::string& email);
    void updatePartnerStats(const std::string& partner_id, double amount);
    std::string calculatePartnerTier(int adoptions, double benefits_paid, int employee_count);

    // ========================================================================
    // STATE
    // ========================================================================

    bool enabled_ = false;
    double max_adoption_benefit_ = 500.0;
    int default_pawternity_days_ = 3;

    // Subscription IDs for cleanup
    std::vector<std::string> subscription_ids_;

    // Data storage
    std::map<std::string, EmployerPartner> partners_;
    std::map<std::string, std::string> email_domain_to_partner_; // domain -> partner_id
    std::map<std::string, AdoptionBenefitClaim> benefit_claims_;
    std::map<std::string, PawtternityRequest> pawternity_requests_;
    mutable std::mutex data_mutex_;

    // Metrics
    std::atomic<int64_t> partners_registered_{0};
    std::atomic<int64_t> claims_submitted_{0};
    std::atomic<int64_t> claims_approved_{0};
    std::atomic<int64_t> pawternity_requests_count_{0};
    std::atomic<int64_t> pawternity_days_granted_{0};
    std::atomic<double> total_benefits_paid_{0.0};
    std::atomic<int64_t> adoptions_through_partners_{0};
};

} // namespace wtl::modules::intervention
