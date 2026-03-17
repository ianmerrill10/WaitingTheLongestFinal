/**
 * @file State.h
 * @brief State model representing a US state for geographic organization
 *
 * PURPOSE:
 * Defines the State data model for geographic organization of shelters
 * and dogs. States are used for regional filtering and aggregated counts.
 *
 * USAGE:
 * State state = State::fromJson(jsonData);
 * State state = State::fromDbRow(dbRow);
 * Json::Value json = state.toJson();
 *
 * DEPENDENCIES:
 * - jsoncpp (Json::Value)
 * - pqxx (database row conversion)
 * - chrono (time handling)
 *
 * MODIFICATION GUIDE:
 * - Add new fields to struct and update all conversion methods
 * - State code is the primary key (2-letter abbreviation)
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <string>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

namespace wtl::core::models {

/**
 * @struct State
 * @brief Represents a US state for geographic organization
 *
 * Contains state information including counts of dogs and shelters
 * for quick aggregation without expensive queries.
 */
struct State {
    // ========================================================================
    // IDENTITY
    // ========================================================================

    std::string code;                        ///< Two-letter state code (PK) e.g., "TX", "CA"
    std::string name;                        ///< Full state name e.g., "Texas", "California"
    std::string region;                      ///< Geographic region e.g., "Southwest", "West"

    // ========================================================================
    // STATUS
    // ========================================================================

    bool is_active;                          ///< Whether platform is active in this state

    // ========================================================================
    // CACHED COUNTS
    // ========================================================================

    int dog_count;                           ///< Total dogs in state (cached)
    int shelter_count;                       ///< Total shelters in state (cached)

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;  ///< Record creation time
    std::chrono::system_clock::time_point updated_at;  ///< Last update time

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create a State from JSON data
     *
     * @param json The JSON object containing state data
     * @return State The constructed State object
     */
    static State fromJson(const Json::Value& json);

    /**
     * @brief Create a State from a database row
     *
     * @param row The pqxx database row
     * @return State The constructed State object
     */
    static State fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert State to JSON for API responses
     *
     * @return Json::Value The JSON representation
     */
    Json::Value toJson() const;

private:
    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Parse a timestamp string to time_point
     * @param timestamp ISO 8601 formatted timestamp string
     * @return time_point The parsed time
     */
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);

    /**
     * @brief Format a time_point to ISO 8601 string
     * @param tp The time point to format
     * @return std::string ISO 8601 formatted string
     */
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
};

} // namespace wtl::core::models
