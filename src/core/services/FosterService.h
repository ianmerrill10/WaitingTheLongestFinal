/**
 * @file FosterService.h
 * @brief Service for foster home and placement management
 *
 * PURPOSE:
 * Provides business logic for the Foster-First Pathway feature, managing
 * foster homes, placements, and applications. This is a primary feature
 * because research shows fostering makes dogs 14x more likely to be adopted.
 *
 * USAGE:
 * auto& service = FosterService::getInstance();
 * auto home = service.findById("home-uuid");
 * auto placements = service.getActivePlacements();
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - FosterHome, FosterPlacement, FosterApplication models
 *
 * MODIFICATION GUIDE:
 * - To add new query methods, follow the pattern in findById()
 * - All database access must use the connection pool
 * - All errors must be captured with WTL_CAPTURE_ERROR macro
 * - Emit events for significant state changes
 *
 * @author Agent 9 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "core/models/FosterHome.h"
#include "core/models/FosterPlacement.h"

namespace wtl::core::services {

/**
 * @class FosterService
 * @brief Singleton service for foster home and placement operations
 *
 * Thread-safe singleton that manages all foster-related business logic.
 * Uses connection pooling for database access and emits events for
 * significant state changes to enable real-time updates.
 */
class FosterService {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the FosterService singleton
     */
    static FosterService& getInstance();

    // Delete copy/move constructors and assignment operators
    FosterService(const FosterService&) = delete;
    FosterService& operator=(const FosterService&) = delete;
    FosterService(FosterService&&) = delete;
    FosterService& operator=(FosterService&&) = delete;

    // ========================================================================
    // FOSTER HOME CRUD
    // ========================================================================

    /**
     * @brief Find a foster home by ID
     * @param id The UUID of the foster home
     * @return The foster home if found, std::nullopt otherwise
     */
    std::optional<models::FosterHome> findById(const std::string& id);

    /**
     * @brief Find a foster home by user ID
     * @param user_id The UUID of the user
     * @return The foster home if found, std::nullopt otherwise
     */
    std::optional<models::FosterHome> findByUserId(const std::string& user_id);

    /**
     * @brief Find all foster homes with pagination
     * @param limit Maximum number to return
     * @param offset Number to skip
     * @return Vector of foster homes
     */
    std::vector<models::FosterHome> findAll(int limit = 100, int offset = 0);

    /**
     * @brief Find all active foster homes
     * @return Vector of active foster homes
     */
    std::vector<models::FosterHome> findActive();

    /**
     * @brief Find foster homes by ZIP code within radius
     * @param zip The ZIP code to search from
     * @param radius_miles Search radius in miles
     * @return Vector of foster homes within radius
     */
    std::vector<models::FosterHome> findByZipCode(const std::string& zip, int radius_miles = 50);

    /**
     * @brief Save a new foster home
     * @param home The foster home to save
     * @return The saved foster home with generated ID
     */
    models::FosterHome save(const models::FosterHome& home);

    /**
     * @brief Update an existing foster home
     * @param home The foster home to update
     * @return The updated foster home
     */
    models::FosterHome update(const models::FosterHome& home);

    /**
     * @brief Delete a foster home by ID
     * @param id The UUID of the foster home
     * @return true if deleted, false if not found
     */
    bool deleteById(const std::string& id);

    // ========================================================================
    // FOSTER HOME OPERATIONS
    // ========================================================================

    /**
     * @brief Register a user as a foster home
     * @param user_id The user's UUID
     * @param home The foster home data
     * @return true if registration successful
     */
    bool registerAsFoster(const std::string& user_id, const models::FosterHome& home);

    /**
     * @brief Deactivate a foster home
     * @param home_id The foster home UUID
     * @return true if deactivated successfully
     */
    bool deactivate(const std::string& home_id);

    /**
     * @brief Reactivate a foster home
     * @param home_id The foster home UUID
     * @return true if reactivated successfully
     */
    bool reactivate(const std::string& home_id);

    /**
     * @brief Update current dog count for a foster home
     * @param home_id The foster home UUID
     * @param current_dogs New count of dogs
     * @return true if updated successfully
     */
    bool updateCapacity(const std::string& home_id, int current_dogs);

    // ========================================================================
    // PLACEMENT OPERATIONS
    // ========================================================================

    /**
     * @brief Create a new foster placement
     * @param foster_home_id The foster home UUID
     * @param dog_id The dog UUID
     * @param shelter_id The originating shelter UUID
     * @return The created placement
     * @emits FOSTER_PLACEMENT_STARTED
     */
    models::FosterPlacement createPlacement(const std::string& foster_home_id,
                                            const std::string& dog_id,
                                            const std::string& shelter_id);

    /**
     * @brief End a foster placement
     * @param placement_id The placement UUID
     * @param outcome The outcome (adopted_by_foster, adopted_other, returned_to_shelter)
     * @param notes Optional notes about the outcome
     * @return The updated placement
     * @emits FOSTER_PLACEMENT_ENDED
     */
    models::FosterPlacement endPlacement(const std::string& placement_id,
                                         const std::string& outcome,
                                         const std::string& notes = "");

    /**
     * @brief Get all active placements
     * @return Vector of active placements
     */
    std::vector<models::FosterPlacement> getActivePlacements();

    /**
     * @brief Get placements by foster home
     * @param home_id The foster home UUID
     * @return Vector of placements for this home
     */
    std::vector<models::FosterPlacement> getPlacementsByHome(const std::string& home_id);

    /**
     * @brief Get placements by dog
     * @param dog_id The dog UUID
     * @return Vector of placements for this dog
     */
    std::vector<models::FosterPlacement> getPlacementsByDog(const std::string& dog_id);

    /**
     * @brief Find a placement by ID
     * @param placement_id The placement UUID
     * @return The placement if found, std::nullopt otherwise
     */
    std::optional<models::FosterPlacement> findPlacementById(const std::string& placement_id);

    // ========================================================================
    // APPLICATION OPERATIONS
    // ========================================================================

    /**
     * @brief Submit a foster application
     * @param foster_home_id The foster home UUID
     * @param dog_id The dog UUID
     * @param message Message from applicant
     * @return The created application
     * @emits FOSTER_APPLICATION_SUBMITTED
     */
    models::FosterApplication submitApplication(const std::string& foster_home_id,
                                                const std::string& dog_id,
                                                const std::string& message);

    /**
     * @brief Approve a foster application
     * @param app_id The application UUID
     * @param response Response message to applicant
     * @return The updated application
     * @emits FOSTER_APPROVED
     */
    models::FosterApplication approveApplication(const std::string& app_id,
                                                 const std::string& response);

    /**
     * @brief Reject a foster application
     * @param app_id The application UUID
     * @param response Response message to applicant
     * @return The updated application
     */
    models::FosterApplication rejectApplication(const std::string& app_id,
                                                const std::string& response);

    /**
     * @brief Withdraw a foster application
     * @param app_id The application UUID
     * @return true if withdrawn successfully
     */
    bool withdrawApplication(const std::string& app_id);

    /**
     * @brief Get all pending applications
     * @return Vector of pending applications
     */
    std::vector<models::FosterApplication> getPendingApplications();

    /**
     * @brief Get applications by foster home
     * @param home_id The foster home UUID
     * @return Vector of applications for this home
     */
    std::vector<models::FosterApplication> getApplicationsByHome(const std::string& home_id);

    /**
     * @brief Get applications by dog
     * @param dog_id The dog UUID
     * @return Vector of applications for this dog
     */
    std::vector<models::FosterApplication> getApplicationsByDog(const std::string& dog_id);

    /**
     * @brief Find an application by ID
     * @param app_id The application UUID
     * @return The application if found, std::nullopt otherwise
     */
    std::optional<models::FosterApplication> findApplicationById(const std::string& app_id);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Count total active foster homes
     * @return Number of active foster homes
     */
    int countActiveFosters();

    /**
     * @brief Count total active placements
     * @return Number of active placements
     */
    int countActivePlacements();

    /**
     * @brief Get comprehensive foster statistics
     * @return JSON with various statistics
     */
    Json::Value getStatistics();

private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR
    // ========================================================================

    FosterService() = default;
    ~FosterService() = default;

    // ========================================================================
    // PRIVATE HELPERS
    // ========================================================================

    /**
     * @brief Generate a new UUID
     * @return UUID string
     */
    std::string generateUuid();

    /**
     * @brief Get current timestamp
     * @return Current system time
     */
    std::chrono::system_clock::time_point now();
};

} // namespace wtl::core::services
