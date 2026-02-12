/**
 * @file BreedInterventionModule.h
 * @brief Module for intervening on behalf of overlooked and discriminated breeds
 *
 * PURPOSE:
 * Addresses breed-specific challenges in adoption, including breed-specific
 * legislation (BSL), apartment restrictions, insurance discrimination, and
 * general bias against certain breeds like pit bulls, Rottweilers, etc.
 *
 * USAGE:
 * Enable in config.json:
 * {
 *     "modules": {
 *         "breed_intervention": {
 *             "enabled": true,
 *             "featured_breeds": ["pit_bull", "rottweiler", "german_shepherd"],
 *             "bsl_tracking_enabled": true
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
#include <set>
#include <memory>
#include <mutex>

#include <json/json.h>

#include "modules/IModule.h"
#include "core/EventBus.h"

namespace wtl::modules::intervention {

/**
 * @struct BreedProfile
 * @brief Profile of a breed including challenges and resources
 */
struct BreedProfile {
    std::string breed_code;
    std::string breed_name;
    std::vector<std::string> common_names; // aliases
    std::string description;
    std::vector<std::string> positive_traits;
    std::vector<std::string> challenges;
    double avg_shelter_wait_days;
    double adoption_rate_percent;
    double return_rate_percent;
    bool subject_to_bsl;
    bool commonly_restricted;
    std::vector<std::string> education_resources;
};

/**
 * @struct BSLLocation
 * @brief Breed-specific legislation by location
 */
struct BSLLocation {
    std::string location_code;
    std::string city;
    std::string county;
    std::string state;
    std::string country;
    std::vector<std::string> banned_breeds;
    std::vector<std::string> restricted_breeds;
    std::string restriction_type; // ban, restriction, mandatory_insurance, mandatory_neuter
    std::string legislation_name;
    std::string effective_date;
    std::string notes;
    bool verified;
};

/**
 * @struct PetFriendlyResource
 * @brief A pet-friendly housing, insurance, or other resource
 */
struct PetFriendlyResource {
    std::string id;
    std::string name;
    std::string type; // housing, insurance, airline, rental_car, hotel
    std::string description;
    std::vector<std::string> accepted_breeds; // Empty means all
    std::vector<std::string> service_areas;
    std::string website;
    std::string contact;
    bool verified;
    std::string last_verified_date;
};

/**
 * @struct BreedAdvocacyResource
 * @brief Educational and advocacy resources for breeds
 */
struct BreedAdvocacyResource {
    std::string id;
    std::string title;
    std::string type; // article, video, infographic, study
    std::string url;
    std::vector<std::string> relevant_breeds;
    std::string description;
    std::vector<std::string> tags;
};

/**
 * @class BreedInterventionModule
 * @brief Module for breed-specific intervention and advocacy
 *
 * Features:
 * - BSL tracking and alerts
 * - Breed-friendly housing/insurance database
 * - Educational content for misunderstood breeds
 * - Breed-specific adoption promotion
 * - Advocacy resource coordination
 * - Breed statistics tracking
 */
class BreedInterventionModule : public IModule {
public:
    BreedInterventionModule() = default;
    ~BreedInterventionModule() override = default;

    // ========================================================================
    // IModule INTERFACE - LIFECYCLE
    // ========================================================================

    bool initialize() override;
    void shutdown() override;
    void onConfigUpdate(const Json::Value& config) override;

    // ========================================================================
    // IModule INTERFACE - IDENTITY
    // ========================================================================

    std::string getName() const override { return "breed_intervention"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "Intervention for overlooked and discriminated breeds including BSL tracking";
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
    // BREED PROFILES
    // ========================================================================

    /**
     * @brief Get breed profile
     * @param breed_code The breed code or name
     * @return The breed profile if found
     */
    std::optional<BreedProfile> getBreedProfile(const std::string& breed_code);

    /**
     * @brief Get all breeds subject to BSL
     * @return List of BSL-affected breed codes
     */
    std::vector<std::string> getBSLBreeds();

    /**
     * @brief Get commonly restricted breeds
     * @return List of commonly restricted breed codes
     */
    std::vector<std::string> getRestrictedBreeds();

    /**
     * @brief Get overlooked breeds (high wait times)
     * @return List of overlooked breed codes
     */
    std::vector<std::string> getOverlookedBreeds();

    // ========================================================================
    // BSL TRACKING
    // ========================================================================

    /**
     * @brief Check if a breed is banned in a location
     * @param breed_code The breed code
     * @param city City name
     * @param state State code
     * @return true if banned
     */
    bool isBreedBannedInLocation(const std::string& breed_code,
                                  const std::string& city,
                                  const std::string& state);

    /**
     * @brief Check if a breed is restricted in a location
     * @param breed_code The breed code
     * @param city City name
     * @param state State code
     * @return Restriction details if restricted
     */
    std::optional<BSLLocation> getBreedRestriction(const std::string& breed_code,
                                                     const std::string& city,
                                                     const std::string& state);

    /**
     * @brief Get all BSL locations for a breed
     * @param breed_code The breed code
     * @return List of BSL locations
     */
    std::vector<BSLLocation> getBSLLocationsForBreed(const std::string& breed_code);

    /**
     * @brief Get all BSL locations in a state
     * @param state State code
     * @return List of BSL locations
     */
    std::vector<BSLLocation> getBSLLocationsInState(const std::string& state);

    // ========================================================================
    // PET-FRIENDLY RESOURCES
    // ========================================================================

    /**
     * @brief Find pet-friendly housing for a breed
     * @param breed_code The breed code
     * @param city City name
     * @param state State code
     * @return List of pet-friendly housing options
     */
    std::vector<PetFriendlyResource> findPetFriendlyHousing(
        const std::string& breed_code,
        const std::string& city,
        const std::string& state);

    /**
     * @brief Find insurance that covers a breed
     * @param breed_code The breed code
     * @param state State code
     * @return List of insurance providers
     */
    std::vector<PetFriendlyResource> findBreedFriendlyInsurance(
        const std::string& breed_code,
        const std::string& state);

    /**
     * @brief Get all resources for a breed
     * @param breed_code The breed code
     * @param resource_type Type filter (housing, insurance, airline, etc.)
     * @return List of matching resources
     */
    std::vector<PetFriendlyResource> getResourcesForBreed(
        const std::string& breed_code,
        const std::string& resource_type = "");

    // ========================================================================
    // EDUCATION & ADVOCACY
    // ========================================================================

    /**
     * @brief Get educational resources for a breed
     * @param breed_code The breed code
     * @return List of educational resources
     */
    std::vector<BreedAdvocacyResource> getEducationalResources(const std::string& breed_code);

    /**
     * @brief Get breed advocacy organizations
     * @param breed_code The breed code
     * @return List of advocacy organizations
     */
    std::vector<BreedAdvocacyResource> getAdvocacyOrganizations(const std::string& breed_code);

    /**
     * @brief Generate breed myth-busting content
     * @param breed_code The breed code
     * @return Myth-busting content as JSON
     */
    Json::Value generateMythBustingContent(const std::string& breed_code);

    // ========================================================================
    // STATISTICS & REPORTING
    // ========================================================================

    /**
     * @brief Get breed adoption statistics
     * @param breed_code The breed code
     * @return Statistics JSON
     */
    Json::Value getBreedStats(const std::string& breed_code);

    /**
     * @brief Get comparative breed statistics
     * @return Comparison of all tracked breeds
     */
    Json::Value getBreedComparison();

    /**
     * @brief Report a BSL alert for adopters
     * @param adopter_id Adopter ID
     * @param breed_code Breed code
     * @param location Location being considered
     */
    void alertAdopterAboutBSL(const std::string& adopter_id,
                               const std::string& breed_code,
                               const std::string& location);

private:
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    void handleDogCreated(const core::Event& event);
    void handleDogAdopted(const core::Event& event);
    void handleAdopterLocationSearch(const core::Event& event);

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    void loadBreedProfiles();
    void loadBSLData();
    void loadResources();
    std::string normalizeBreedCode(const std::string& breed);
    bool breedMatchesCategory(const std::string& breed_code, const std::string& category);

    // ========================================================================
    // STATE
    // ========================================================================

    bool enabled_ = false;
    bool bsl_tracking_enabled_ = true;
    std::vector<std::string> featured_breeds_;

    // Subscription IDs for cleanup
    std::vector<std::string> subscription_ids_;

    // Data storage
    std::map<std::string, BreedProfile> breed_profiles_;
    std::vector<BSLLocation> bsl_locations_;
    std::vector<PetFriendlyResource> pet_friendly_resources_;
    std::vector<BreedAdvocacyResource> advocacy_resources_;
    std::set<std::string> bsl_breeds_;
    std::set<std::string> restricted_breeds_;
    mutable std::mutex data_mutex_;

    // Metrics
    std::atomic<int64_t> bsl_alerts_sent_{0};
    std::atomic<int64_t> resource_lookups_{0};
    std::atomic<int64_t> education_views_{0};
    std::atomic<int64_t> restricted_breed_adoptions_{0};
};

} // namespace wtl::modules::intervention
