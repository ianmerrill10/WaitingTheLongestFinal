/**
 * @file SurrenderPreventionModule.h
 * @brief Module for preventing dog surrenders through proactive intervention
 *
 * PURPOSE:
 * Provides resources, support, and intervention services to help pet owners
 * keep their dogs rather than surrendering them to shelters. This includes
 * financial assistance, behavioral resources, and crisis support.
 *
 * USAGE:
 * This module is automatically registered by ModuleLoader.
 * Enable it in config.json:
 * {
 *     "modules": {
 *         "surrender_prevention": {
 *             "enabled": true,
 *             "hotline_enabled": true,
 *             "financial_assistance_enabled": true,
 *             "max_assistance_amount": 500
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
 * @struct SurrenderReason
 * @brief Categories of surrender reasons with associated resources
 */
struct SurrenderReason {
    std::string code;
    std::string name;
    std::string description;
    std::vector<std::string> resources;
    bool financial_help_available;
    int priority_score;
};

/**
 * @struct AssistanceRequest
 * @brief A request for surrender prevention assistance
 */
struct AssistanceRequest {
    std::string id;
    std::string user_id;
    std::string reason_code;
    std::string description;
    std::string contact_email;
    std::string contact_phone;
    std::string zip_code;
    double requested_amount;
    std::string status; // pending, approved, denied, completed
    std::string created_at;
    std::string updated_at;
    Json::Value metadata;
};

/**
 * @struct PreventionResource
 * @brief A resource available to help prevent surrender
 */
struct PreventionResource {
    std::string id;
    std::string name;
    std::string type; // financial, behavioral, medical, housing, other
    std::string description;
    std::string url;
    std::string contact_info;
    std::vector<std::string> applicable_reasons;
    std::vector<std::string> service_areas; // zip codes or state codes
    bool active;
};

/**
 * @class SurrenderPreventionModule
 * @brief Module for preventing dog surrenders through intervention
 *
 * Features:
 * - Surrender prevention hotline integration
 * - Financial assistance program management
 * - Resource matching based on surrender reason
 * - Crisis intervention workflows
 * - Partner organization coordination
 * - Success tracking and metrics
 */
class SurrenderPreventionModule : public IModule {
public:
    SurrenderPreventionModule() = default;
    ~SurrenderPreventionModule() override = default;

    // ========================================================================
    // IModule INTERFACE - LIFECYCLE
    // ========================================================================

    bool initialize() override;
    void shutdown() override;
    void onConfigUpdate(const Json::Value& config) override;

    // ========================================================================
    // IModule INTERFACE - IDENTITY
    // ========================================================================

    std::string getName() const override { return "surrender_prevention"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "Prevents dog surrenders through proactive intervention, financial assistance, and resource matching";
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
    // SURRENDER PREVENTION API
    // ========================================================================

    /**
     * @brief Submit a request for surrender prevention assistance
     * @param request The assistance request details
     * @return The created request with assigned ID
     */
    AssistanceRequest submitAssistanceRequest(const AssistanceRequest& request);

    /**
     * @brief Get assistance request by ID
     * @param request_id The request ID
     * @return The request if found, nullopt otherwise
     */
    std::optional<AssistanceRequest> getAssistanceRequest(const std::string& request_id);

    /**
     * @brief Update assistance request status
     * @param request_id The request ID
     * @param status New status
     * @param notes Optional notes about the status change
     * @return true if updated successfully
     */
    bool updateAssistanceStatus(const std::string& request_id,
                                const std::string& status,
                                const std::string& notes = "");

    /**
     * @brief Get resources matching a surrender reason
     * @param reason_code The surrender reason code
     * @param zip_code Optional zip code for location-based filtering
     * @return List of matching resources
     */
    std::vector<PreventionResource> getResourcesForReason(
        const std::string& reason_code,
        const std::string& zip_code = "");

    /**
     * @brief Get all available surrender reasons
     * @return List of surrender reasons with resources
     */
    std::vector<SurrenderReason> getSurrenderReasons() const;

    /**
     * @brief Record a successful surrender prevention
     * @param request_id The assistance request that led to success
     * @param outcome_details Details about the outcome
     */
    void recordPreventionSuccess(const std::string& request_id,
                                 const Json::Value& outcome_details);

    /**
     * @brief Get prevention statistics
     * @param start_date Start of period (ISO format)
     * @param end_date End of period (ISO format)
     * @return Statistics for the period
     */
    Json::Value getPreventionStats(const std::string& start_date,
                                   const std::string& end_date);

private:
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    void handleSurrenderInquiry(const core::Event& event);
    void handleAssistanceApproved(const core::Event& event);

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    void loadSurrenderReasons();
    void loadResources();
    std::string generateRequestId();
    void notifyPartnerOrganizations(const AssistanceRequest& request);
    double calculateAssistanceAmount(const AssistanceRequest& request);

    // ========================================================================
    // STATE
    // ========================================================================

    bool enabled_ = false;
    bool hotline_enabled_ = false;
    bool financial_assistance_enabled_ = false;
    double max_assistance_amount_ = 500.0;
    std::string hotline_number_;
    std::string hotline_hours_;

    // Subscription IDs for cleanup
    std::vector<std::string> subscription_ids_;

    // Data storage
    std::map<std::string, SurrenderReason> surrender_reasons_;
    std::map<std::string, PreventionResource> resources_;
    std::map<std::string, AssistanceRequest> pending_requests_;
    mutable std::mutex data_mutex_;

    // Metrics
    std::atomic<int64_t> total_inquiries_{0};
    std::atomic<int64_t> assistance_requests_{0};
    std::atomic<int64_t> approved_requests_{0};
    std::atomic<int64_t> successful_preventions_{0};
    std::atomic<double> total_assistance_disbursed_{0.0};
};

} // namespace wtl::modules::intervention
