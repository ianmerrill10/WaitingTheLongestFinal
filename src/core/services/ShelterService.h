/**
 * @file ShelterService.h
 * @brief Service for shelter-related business logic
 *
 * PURPOSE:
 * Handles all shelter CRUD operations and queries. Critical for
 * managing kill shelter tracking and dog count statistics.
 *
 * USAGE:
 * auto& service = ShelterService::getInstance();
 * auto shelter = service.findById(id);
 * auto kill_shelters = service.findKillShelters();
 *
 * DEPENDENCIES:
 * - ConnectionPool (Agent 1) - Database connections
 * - ErrorCapture (Agent 1) - Error logging
 * - SelfHealing (Agent 1) - Retry logic
 * - Shelter model - Shelter data structure
 *
 * MODIFICATION GUIDE:
 * - To add new query methods, follow the pattern in findById()
 * - All database access must use the connection pool
 * - All errors must be captured with WTL_CAPTURE_ERROR macro
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

// Project includes
#include "core/models/Shelter.h"
#include "core/debug/SelfHealing.h"

namespace wtl::core::services {

/**
 * @class ShelterService
 * @brief Singleton service for shelter operations
 *
 * Thread-safe singleton that manages all shelter-related business logic.
 * Uses connection pooling for database access.
 */
class ShelterService {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the ShelterService instance
     */
    static ShelterService& getInstance();

    // Prevent copying and moving
    ShelterService(const ShelterService&) = delete;
    ShelterService& operator=(const ShelterService&) = delete;
    ShelterService(ShelterService&&) = delete;
    ShelterService& operator=(ShelterService&&) = delete;

    // ========================================================================
    // CRUD OPERATIONS
    // ========================================================================

    /**
     * @brief Find a shelter by its unique ID
     *
     * @param id The UUID of the shelter to find
     * @return std::optional<Shelter> The shelter if found, std::nullopt otherwise
     */
    std::optional<models::Shelter> findById(const std::string& id);

    /**
     * @brief Find all shelters with pagination
     *
     * @param limit Maximum number of shelters to return (default 100)
     * @param offset Number of shelters to skip (default 0)
     * @return std::vector<Shelter> List of shelters
     */
    std::vector<models::Shelter> findAll(int limit = 100, int offset = 0);

    /**
     * @brief Find all shelters in a specific state
     *
     * @param state_code Two-letter state code (e.g., "TX", "CA")
     * @return std::vector<Shelter> List of shelters in the state
     */
    std::vector<models::Shelter> findByStateCode(const std::string& state_code);

    /**
     * @brief Find all kill shelters
     *
     * Returns shelters where is_kill_shelter = true.
     * These are priority shelters for urgent dog monitoring.
     *
     * @return std::vector<Shelter> List of kill shelters
     */
    std::vector<models::Shelter> findKillShelters();

    /**
     * @brief Save a new shelter to the database
     *
     * @param shelter The shelter to save
     * @return Shelter The saved shelter with generated ID and timestamps
     */
    models::Shelter save(const models::Shelter& shelter);

    /**
     * @brief Update an existing shelter
     *
     * @param shelter The shelter with updated fields
     * @return Shelter The updated shelter
     */
    models::Shelter update(const models::Shelter& shelter);

    /**
     * @brief Delete a shelter by ID
     *
     * @param id The UUID of the shelter to delete
     * @return bool True if deleted, false if not found
     */
    bool deleteById(const std::string& id);

    // ========================================================================
    // COUNT QUERIES
    // ========================================================================

    /**
     * @brief Count all shelters in the system
     * @return int Total shelter count
     */
    int countAll();

    /**
     * @brief Count shelters in a specific state
     * @param state_code Two-letter state code
     * @return int Shelter count in state
     */
    int countByStateCode(const std::string& state_code);

    // ========================================================================
    // DOG COUNT MANAGEMENT
    // ========================================================================

    /**
     * @brief Update the cached dog count for a shelter
     *
     * This recalculates the dog_count and available_count fields
     * by querying the dogs table. Should be called when dogs are
     * added or removed from a shelter.
     *
     * @param shelter_id The UUID of the shelter
     */
    void updateDogCount(const std::string& shelter_id);

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    /**
     * @brief Private constructor for singleton pattern
     */
    ShelterService();

    /**
     * @brief Destructor
     */
    ~ShelterService();

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================

    /**
     * @brief Execute a database query with self-healing
     */
    template<typename T>
    T executeWithHealing(const std::string& operation_name,
                         std::function<T()> operation);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    std::mutex mutex_;  ///< Mutex for thread-safe operations
};

} // namespace wtl::core::services
