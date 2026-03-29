/**
 * @file StateService.h
 * @brief Service for state-related business logic
 *
 * PURPOSE:
 * Handles all state CRUD operations and count updates.
 * States are used for geographic organization and aggregated statistics.
 *
 * USAGE:
 * auto& service = StateService::getInstance();
 * auto state = service.findByCode("TX");
 * auto active = service.findActive();
 *
 * DEPENDENCIES:
 * - ConnectionPool (Agent 1) - Database connections
 * - ErrorCapture (Agent 1) - Error logging
 * - SelfHealing (Agent 1) - Retry logic
 * - State model - State data structure
 *
 * MODIFICATION GUIDE:
 * - To add new query methods, follow the pattern in findByCode()
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
#include "core/models/State.h"
#include "core/debug/SelfHealing.h"

namespace wtl::core::services {

/**
 * @class StateService
 * @brief Singleton service for state operations
 *
 * Thread-safe singleton that manages all state-related business logic.
 * Uses connection pooling for database access.
 */
class StateService {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the StateService instance
     */
    static StateService& getInstance();

    // Prevent copying and moving
    StateService(const StateService&) = delete;
    StateService& operator=(const StateService&) = delete;
    StateService(StateService&&) = delete;
    StateService& operator=(StateService&&) = delete;

    // ========================================================================
    // QUERY OPERATIONS
    // ========================================================================

    /**
     * @brief Find a state by its code
     *
     * @param code Two-letter state code (e.g., "TX", "CA")
     * @return std::optional<State> The state if found, std::nullopt otherwise
     */
    std::optional<models::State> findByCode(const std::string& code);

    /**
     * @brief Find all states
     *
     * @return std::vector<State> List of all states
     */
    std::vector<models::State> findAll();

    /**
     * @brief Find all active states
     *
     * Returns states where is_active = true. These are states
     * where the platform is currently operating.
     *
     * @return std::vector<State> List of active states
     */
    std::vector<models::State> findActive();

    // ========================================================================
    // SAVE OPERATIONS
    // ========================================================================

    /**
     * @brief Save a new state to the database
     *
     * @param state The state to save
     * @return State The saved state with timestamps
     */
    models::State save(const models::State& state);

    /**
     * @brief Update an existing state
     *
     * @param state The state with updated fields
     * @return State The updated state
     */
    models::State update(const models::State& state);

    // ========================================================================
    // COUNT MANAGEMENT
    // ========================================================================

    /**
     * @brief Update the cached counts for a state
     *
     * Recalculates dog_count and shelter_count for the specified state
     * by querying the dogs and shelters tables.
     *
     * @param code Two-letter state code
     */
    void updateCounts(const std::string& code);

    /**
     * @brief Update counts for all states
     *
     * Iterates through all states and updates their cached counts.
     * This is typically run as a scheduled background task.
     */
    void updateAllCounts();

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    /**
     * @brief Private constructor for singleton pattern
     */
    StateService();

    /**
     * @brief Destructor
     */
    ~StateService();

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
