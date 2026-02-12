/**
 * @file RescueGroupsAggregator.h
 * @brief RescueGroups.org API client for fetching shelter and dog data
 *
 * PURPOSE:
 * Connects to the RescueGroups.org API to fetch organizations and animals.
 * Maps external data to our internal Dog and Shelter models, handles
 * pagination, authentication, and incremental sync.
 *
 * USAGE:
 * RescueGroupsAggregator aggregator;
 * aggregator.initialize(config);
 * auto stats = aggregator.sync();
 *
 * DEPENDENCIES:
 * - BaseAggregator for common functionality
 * - HttpClient for API requests
 * - DataMapper for data transformation
 * - DuplicateDetector for duplicate handling
 *
 * MODIFICATION GUIDE:
 * - Update field mappings as RescueGroups API changes
 * - Adjust rate limiting based on API limits
 * - Add new animal types if expanding beyond dogs
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <optional>

// Project includes
#include "aggregators/BaseAggregator.h"

// Forward declarations
namespace wtl::core::models {
    struct Dog;
    struct Shelter;
}

namespace wtl::aggregators {

/**
 * @class RescueGroupsAggregator
 * @brief Fetches dog and shelter data from RescueGroups.org
 *
 * RescueGroups uses a JSON-based API with API key authentication.
 * Supports searching/filtering animals and organizations.
 */
class RescueGroupsAggregator : public BaseAggregator {
public:
    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Default constructor
     */
    RescueGroupsAggregator();

    /**
     * @brief Construct with configuration
     * @param config Aggregator configuration
     */
    explicit RescueGroupsAggregator(const AggregatorConfig& config);

    /**
     * @brief Destructor
     */
    ~RescueGroupsAggregator() override = default;

    // =========================================================================
    // SPECIFIC METHODS
    // =========================================================================

    /**
     * @brief Fetch organizations from specified states
     *
     * @param state_codes List of state codes to fetch (e.g., {"TX", "CA"})
     * @return Number of organizations fetched
     */
    size_t fetchOrganizations(const std::vector<std::string>& state_codes);

    /**
     * @brief Fetch animals from a specific organization
     *
     * @param org_id Organization ID
     * @param species Species filter (default "dogs")
     * @return Number of animals fetched
     */
    size_t fetchAnimalsFromOrg(const std::string& org_id,
                                const std::string& species = "dogs");

    /**
     * @brief Search animals with filters
     *
     * @param filters Search filters
     * @return Number of animals found
     */
    size_t searchAnimals(const Json::Value& filters);

    /**
     * @brief Get organization details
     *
     * @param org_id Organization ID
     * @return Organization data as JSON
     */
    Json::Value getOrganization(const std::string& org_id);

    /**
     * @brief Get animal details
     *
     * @param animal_id Animal ID
     * @return Animal data as JSON
     */
    Json::Value getAnimal(const std::string& animal_id);

protected:
    // =========================================================================
    // BaseAggregator OVERRIDES
    // =========================================================================

    /**
     * @brief Perform full sync
     * @return Sync statistics
     */
    SyncStats doSync() override;

    /**
     * @brief Perform incremental sync
     * @param since Only fetch records modified after this time
     * @return Sync statistics
     */
    SyncStats doSyncSince(std::chrono::system_clock::time_point since) override;

    /**
     * @brief Test API connection
     * @return true if connection successful
     */
    bool doTestConnection() override;

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    /**
     * @brief Build RescueGroups API request body
     *
     * @param object_type Type of object (animals, orgs)
     * @param object_action Action to perform (search, publicSearch, view)
     * @param search_params Search parameters
     * @return JSON request body
     */
    Json::Value buildApiRequest(const std::string& object_type,
                                 const std::string& object_action,
                                 const Json::Value& search_params = Json::Value());

    /**
     * @brief Make RescueGroups API call
     *
     * @param request Request body
     * @return Response JSON
     */
    Json::Value makeApiCall(const Json::Value& request);

    /**
     * @brief Process animal response
     *
     * @param response API response
     * @param stats Sync stats to update
     * @return Number of animals processed
     */
    size_t processAnimalResponse(const Json::Value& response, SyncStats& stats);

    /**
     * @brief Process organization response
     *
     * @param response API response
     * @param stats Sync stats to update
     * @return Number of organizations processed
     */
    size_t processOrganizationResponse(const Json::Value& response, SyncStats& stats);

    /**
     * @brief Save dog to database
     *
     * @param dog Dog to save
     * @param stats Stats to update
     * @return true if saved successfully
     */
    bool saveDog(const wtl::core::models::Dog& dog, SyncStats& stats);

    /**
     * @brief Save shelter to database
     *
     * @param shelter Shelter to save
     * @param stats Stats to update
     * @return true if saved successfully
     */
    bool saveShelter(const wtl::core::models::Shelter& shelter, SyncStats& stats);

    /**
     * @brief Build search filters for animals
     *
     * @param species Species filter
     * @param status Status filter
     * @param since Only include animals updated after this time
     * @return Search parameters JSON
     */
    Json::Value buildAnimalSearchParams(
        const std::string& species = "dogs",
        const std::string& status = "available",
        std::optional<std::chrono::system_clock::time_point> since = std::nullopt);

    /**
     * @brief Build search filters for organizations
     *
     * @param state_codes State codes to filter by
     * @return Search parameters JSON
     */
    Json::Value buildOrgSearchParams(const std::vector<std::string>& state_codes);

    /**
     * @brief Format timestamp for RescueGroups API
     *
     * @param tp Time point
     * @return Formatted string
     */
    std::string formatTimestamp(std::chrono::system_clock::time_point tp) const;

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    std::vector<std::string> target_states_;  ///< States to sync
    std::string species_filter_;               ///< Species to sync (default: dogs)
    int animals_per_request_{100};             ///< Animals per API request
};

} // namespace wtl::aggregators
