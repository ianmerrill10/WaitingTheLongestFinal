/**
 * @file ReturnPreventionModule.h
 * @brief Module for preventing adopted dogs from being returned to shelters
 *
 * PURPOSE:
 * Provides post-adoption support, resources, and intervention to help
 * adopters successfully integrate their new dogs and prevent returns.
 * Focuses on the critical first 30-90 days after adoption.
 *
 * USAGE:
 * Enable in config.json:
 * {
 *     "modules": {
 *         "return_prevention": {
 *             "enabled": true,
 *             "followup_days": [3, 7, 14, 30, 60, 90],
 *             "support_hotline_enabled": true
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
#include <queue>
#include <memory>
#include <mutex>
#include <chrono>

#include <json/json.h>

#include "modules/IModule.h"
#include "core/EventBus.h"

namespace wtl::modules::intervention {

/**
 * @struct AdoptionRecord
 * @brief Tracks an adoption for follow-up purposes
 */
struct AdoptionRecord {
    std::string id;
    std::string dog_id;
    std::string dog_name;
    std::string adopter_id;
    std::string adopter_name;
    std::string adopter_email;
    std::string adopter_phone;
    std::string shelter_id;
    std::string adoption_date;
    std::vector<int> completed_followups; // Days completed
    std::string status; // active, completed, returned, at_risk
    int risk_score;
    Json::Value notes;
};

/**
 * @struct FollowupTask
 * @brief A scheduled follow-up task
 */
struct FollowupTask {
    std::string id;
    std::string adoption_id;
    int day_number;
    std::string scheduled_date;
    std::string type; // email, phone, sms
    std::string status; // pending, completed, skipped, failed
    std::string completed_date;
    Json::Value response;
};

/**
 * @struct ReturnRisk
 * @brief Risk assessment for potential return
 */
struct ReturnRisk {
    std::string adoption_id;
    int risk_score; // 0-100
    std::vector<std::string> risk_factors;
    std::vector<std::string> recommended_interventions;
    std::string last_assessment_date;
};

/**
 * @struct SupportResource
 * @brief A support resource for adopters
 */
struct SupportResource {
    std::string id;
    std::string name;
    std::string category; // training, behavioral, medical, general
    std::string description;
    std::string url;
    std::string contact;
    bool free;
};

/**
 * @class ReturnPreventionModule
 * @brief Module for preventing post-adoption returns
 *
 * Features:
 * - Automated follow-up scheduling at key intervals
 * - Risk assessment and early warning system
 * - Support resource matching
 * - Behavioral consultation referrals
 * - Integration success tracking
 * - Adopter support hotline
 */
class ReturnPreventionModule : public IModule {
public:
    ReturnPreventionModule() = default;
    ~ReturnPreventionModule() override = default;

    // ========================================================================
    // IModule INTERFACE - LIFECYCLE
    // ========================================================================

    bool initialize() override;
    void shutdown() override;
    void onConfigUpdate(const Json::Value& config) override;

    // ========================================================================
    // IModule INTERFACE - IDENTITY
    // ========================================================================

    std::string getName() const override { return "return_prevention"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "Prevents post-adoption returns through follow-up support and early intervention";
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
    // RETURN PREVENTION API
    // ========================================================================

    /**
     * @brief Register a new adoption for follow-up
     * @param adoption The adoption details
     * @return The registered adoption with scheduled follow-ups
     */
    AdoptionRecord registerAdoption(const AdoptionRecord& adoption);

    /**
     * @brief Get adoption record by ID
     * @param adoption_id The adoption ID
     * @return The adoption record if found
     */
    std::optional<AdoptionRecord> getAdoption(const std::string& adoption_id);

    /**
     * @brief Get adoptions requiring follow-up today
     * @return List of pending follow-up tasks
     */
    std::vector<FollowupTask> getPendingFollowups();

    /**
     * @brief Complete a follow-up task
     * @param task_id The task ID
     * @param response Response/notes from the follow-up
     * @return true if completed successfully
     */
    bool completeFollowup(const std::string& task_id, const Json::Value& response);

    /**
     * @brief Assess return risk for an adoption
     * @param adoption_id The adoption ID
     * @return Risk assessment results
     */
    ReturnRisk assessReturnRisk(const std::string& adoption_id);

    /**
     * @brief Report an issue with an adoption
     * @param adoption_id The adoption ID
     * @param issue_type Type of issue (behavioral, medical, household, other)
     * @param description Description of the issue
     * @return true if reported successfully
     */
    bool reportIssue(const std::string& adoption_id,
                     const std::string& issue_type,
                     const std::string& description);

    /**
     * @brief Get support resources for an issue
     * @param issue_type The type of issue
     * @return Matching support resources
     */
    std::vector<SupportResource> getResources(const std::string& issue_type);

    /**
     * @brief Mark an adoption as at-risk
     * @param adoption_id The adoption ID
     * @param reason Reason for risk status
     */
    void markAtRisk(const std::string& adoption_id, const std::string& reason);

    /**
     * @brief Record a successful adoption completion
     * @param adoption_id The adoption ID
     */
    void recordSuccess(const std::string& adoption_id);

    /**
     * @brief Record a return (for tracking purposes)
     * @param adoption_id The adoption ID
     * @param return_reason Reason for return
     * @param return_date Date of return
     */
    void recordReturn(const std::string& adoption_id,
                      const std::string& return_reason,
                      const std::string& return_date);

    /**
     * @brief Get return prevention statistics
     * @param start_date Start of period
     * @param end_date End of period
     * @return Statistics for the period
     */
    Json::Value getStats(const std::string& start_date,
                         const std::string& end_date);

private:
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    void handleDogAdopted(const core::Event& event);
    void handleFollowupResponse(const core::Event& event);
    void handleReturnRequest(const core::Event& event);

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    void loadSupportResources();
    std::string generateAdoptionId();
    std::string generateTaskId();
    void scheduleFollowups(const std::string& adoption_id, const std::string& adoption_date);
    std::string calculateScheduledDate(const std::string& adoption_date, int days);
    void processFollowupQueue();
    int calculateRiskScore(const AdoptionRecord& adoption);

    // ========================================================================
    // STATE
    // ========================================================================

    bool enabled_ = false;
    bool support_hotline_enabled_ = false;
    std::string support_hotline_number_;
    std::vector<int> followup_days_ = {3, 7, 14, 30, 60, 90};

    // Subscription IDs for cleanup
    std::vector<std::string> subscription_ids_;

    // Data storage
    std::map<std::string, AdoptionRecord> adoptions_;
    std::map<std::string, FollowupTask> followup_tasks_;
    std::map<std::string, std::vector<std::string>> adoption_tasks_; // adoption_id -> task_ids
    std::vector<SupportResource> support_resources_;
    mutable std::mutex data_mutex_;

    // Metrics
    std::atomic<int64_t> total_adoptions_tracked_{0};
    std::atomic<int64_t> followups_completed_{0};
    std::atomic<int64_t> issues_reported_{0};
    std::atomic<int64_t> at_risk_interventions_{0};
    std::atomic<int64_t> successful_completions_{0};
    std::atomic<int64_t> returns_recorded_{0};
};

} // namespace wtl::modules::intervention
