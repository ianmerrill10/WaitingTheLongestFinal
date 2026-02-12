/**
 * @file DogService.h
 * @brief Service for dog-related business logic
 *
 * PURPOSE:
 * Handles all dog CRUD operations, search, and urgency calculations.
 * This is the primary interface for dog data access throughout the platform.
 *
 * USAGE:
 * auto& service = DogService::getInstance();
 * auto dog = service.findById(id);
 * auto wait_time = service.calculateWaitTime(dog);
 *
 * DEPENDENCIES:
 * - ConnectionPool (Agent 1) - Database connections
 * - ErrorCapture (Agent 1) - Error logging
 * - SelfHealing (Agent 1) - Retry logic
 * - Dog model - Dog data structure
 * - WaitTimeCalculator - Time calculations
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
#include "core/models/Dog.h"
#include "core/utils/WaitTimeCalculator.h"
#include "core/debug/SelfHealing.h"

namespace wtl::core::services {

// Forward declarations
using WaitTimeComponents = ::wtl::core::utils::WaitTimeComponents;
using CountdownComponents = ::wtl::core::utils::CountdownComponents;

/**
 * @class DogService
 * @brief Singleton service for dog operations
 *
 * Thread-safe singleton that manages all dog-related business logic.
 * Uses connection pooling for database access and provides self-healing
 * capabilities for transient failures.
 */
class DogService {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the DogService instance
     *
     * Thread-safe singleton implementation using Meyer's singleton pattern.
     */
    static DogService& getInstance();

    // Prevent copying and moving
    DogService(const DogService&) = delete;
    DogService& operator=(const DogService&) = delete;
    DogService(DogService&&) = delete;
    DogService& operator=(DogService&&) = delete;

    // ========================================================================
    // CRUD OPERATIONS
    // ========================================================================

    /**
     * @brief Find a dog by its unique ID
     *
     * @param id The UUID of the dog to find
     * @return std::optional<Dog> The dog if found, std::nullopt otherwise
     *
     * @example
     * auto dog = service.findById("550e8400-e29b-41d4-a716-446655440000");
     * if (dog) {
     *     std::cout << dog->name << std::endl;
     * }
     */
    std::optional<models::Dog> findById(const std::string& id);

    /**
     * @brief Find all dogs with pagination
     *
     * @param limit Maximum number of dogs to return (default 100)
     * @param offset Number of dogs to skip (default 0)
     * @return std::vector<Dog> List of dogs
     */
    std::vector<models::Dog> findAll(int limit = 100, int offset = 0);

    /**
     * @brief Find all dogs in a specific shelter
     *
     * @param shelter_id The UUID of the shelter
     * @return std::vector<Dog> List of dogs in the shelter
     */
    std::vector<models::Dog> findByShelterId(const std::string& shelter_id);

    /**
     * @brief Find all dogs in a specific state
     *
     * @param state_code Two-letter state code (e.g., "TX", "CA")
     * @return std::vector<Dog> List of dogs in the state
     */
    std::vector<models::Dog> findByStateCode(const std::string& state_code);

    /**
     * @brief Save a new dog to the database
     *
     * @param dog The dog to save
     * @return Dog The saved dog with generated ID and timestamps
     */
    models::Dog save(const models::Dog& dog);

    /**
     * @brief Update an existing dog
     *
     * @param dog The dog with updated fields
     * @return Dog The updated dog
     */
    models::Dog update(const models::Dog& dog);

    /**
     * @brief Delete a dog by ID
     *
     * @param id The UUID of the dog to delete
     * @return bool True if deleted, false if not found
     */
    bool deleteById(const std::string& id);

    // ========================================================================
    // COUNT QUERIES
    // ========================================================================

    /**
     * @brief Count all dogs in the system
     * @return int Total dog count
     */
    int countAll();

    /**
     * @brief Count dogs in a specific shelter
     * @param shelter_id The UUID of the shelter
     * @return int Dog count in shelter
     */
    int countByShelterId(const std::string& shelter_id);

    /**
     * @brief Count dogs in a specific state
     * @param state_code Two-letter state code
     * @return int Dog count in state
     */
    int countByStateCode(const std::string& state_code);

    // ========================================================================
    // WAIT TIME CALCULATIONS
    // ========================================================================

    /**
     * @brief Calculate wait time for a dog
     *
     * @param dog The dog to calculate wait time for
     * @return WaitTimeComponents The wait time breakdown
     *
     * @example
     * auto wait = service.calculateWaitTime(dog);
     * std::cout << wait.formatted << std::endl; // "02:03:15:08:45:32"
     */
    WaitTimeComponents calculateWaitTime(const models::Dog& dog);

    /**
     * @brief Calculate countdown to euthanasia
     *
     * @param dog The dog to calculate countdown for
     * @return std::optional<CountdownComponents> Countdown if applicable
     *
     * Returns nullopt if dog doesn't have a euthanasia date or if the
     * date has already passed.
     */
    std::optional<CountdownComponents> calculateCountdown(const models::Dog& dog);

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    /**
     * @brief Private constructor for singleton pattern
     */
    DogService();

    /**
     * @brief Destructor
     */
    ~DogService();

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================

    /**
     * @brief Execute a database query with self-healing
     *
     * Wraps database operations with retry logic and fallback handling.
     * Uses the SelfHealing system for automatic recovery.
     *
     * @tparam T The return type of the operation
     * @param operation_name Name for logging/tracking
     * @param operation The database operation to execute
     * @return T The result of the operation
     */
    template<typename T>
    T executeWithHealing(const std::string& operation_name,
                         std::function<T()> operation);

    /**
     * @brief Format dog for PostgreSQL array insertion
     * @param vec Vector of strings to format
     * @return std::string PostgreSQL array literal
     */
    std::string formatPostgresArray(const std::vector<std::string>& vec);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    std::mutex mutex_;  ///< Mutex for thread-safe operations
};

} // namespace wtl::core::services
