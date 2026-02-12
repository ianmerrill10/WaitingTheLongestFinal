/**
 * @file Shelter.h
 * @brief Shelter model representing an animal shelter or rescue organization
 *
 * PURPOSE:
 * Defines the Shelter data model with all fields required for the WaitingTheLongest
 * platform. Shelters are where dogs are housed and is critical for geographic
 * search and kill shelter tracking.
 *
 * USAGE:
 * Shelter shelter = Shelter::fromJson(jsonData);
 * Shelter shelter = Shelter::fromDbRow(dbRow);
 * Json::Value json = shelter.toJson();
 *
 * DEPENDENCIES:
 * - jsoncpp (Json::Value)
 * - pqxx (database row conversion)
 * - chrono (time handling)
 *
 * MODIFICATION GUIDE:
 * - Add new fields to struct and update all conversion methods
 * - Maintain backward compatibility with JSON format
 * - Ensure database column names match in fromDbRow()
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

namespace wtl::core::models {

/**
 * @struct Shelter
 * @brief Represents an animal shelter or rescue organization
 *
 * Contains all information about a shelter including location,
 * contact info, classification, and kill shelter status.
 */
struct Shelter {
    // ========================================================================
    // IDENTITY
    // ========================================================================

    std::string id;                          ///< UUID primary key
    std::string name;                        ///< Shelter name
    std::string state_code;                  ///< FK to states table (2-letter code)

    // ========================================================================
    // LOCATION
    // ========================================================================

    std::string city;                        ///< City name
    std::string address;                     ///< Street address
    std::string zip_code;                    ///< ZIP/Postal code
    std::optional<double> latitude;          ///< GPS latitude
    std::optional<double> longitude;         ///< GPS longitude

    // ========================================================================
    // CONTACT
    // ========================================================================

    std::optional<std::string> phone;        ///< Phone number
    std::optional<std::string> email;        ///< Email address
    std::optional<std::string> website;      ///< Website URL

    // ========================================================================
    // CLASSIFICATION
    // ========================================================================

    std::string shelter_type;                ///< municipal, rescue, private
    bool is_kill_shelter;                    ///< Whether this is a kill shelter
    int avg_hold_days;                       ///< Average days before euthanasia risk
    std::optional<std::string> euthanasia_schedule; ///< Description of euthanasia schedule
    bool accepts_rescue_pulls;               ///< Whether rescues can pull dogs

    // ========================================================================
    // STATS (cached for performance)
    // ========================================================================

    int dog_count;                           ///< Total dogs at shelter
    int available_count;                     ///< Available dogs count

    // ========================================================================
    // EXTERNAL INTEGRATION
    // ========================================================================

    std::string external_id;                 ///< ID from source system (e.g., RescueGroups)
    std::string source;                      ///< Data source: rescuegroups, shelter_direct (NO Petfinder API exists)

    // ========================================================================
    // STATUS
    // ========================================================================

    bool is_verified;                        ///< Whether shelter info is verified
    bool is_active;                          ///< Whether shelter is active on platform

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;  ///< Record creation time
    std::chrono::system_clock::time_point updated_at;  ///< Last update time

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create a Shelter from JSON data
     *
     * @param json The JSON object containing shelter data
     * @return Shelter The constructed Shelter object
     */
    static Shelter fromJson(const Json::Value& json);

    /**
     * @brief Create a Shelter from a database row
     *
     * @param row The pqxx database row
     * @return Shelter The constructed Shelter object
     */
    static Shelter fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert Shelter to JSON for API responses
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
