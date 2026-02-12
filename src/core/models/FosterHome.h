/**
 * @file FosterHome.h
 * @brief Data model for foster home registration and management
 *
 * PURPOSE:
 * Represents a foster home in the system - individuals/families who have
 * registered to temporarily care for dogs awaiting adoption. Research shows
 * fostering makes dogs 14x more likely to be adopted, making this a primary
 * feature of the platform.
 *
 * USAGE:
 * FosterHome home;
 * home.user_id = "user-uuid";
 * home.max_dogs = 2;
 * home.has_yard = true;
 * auto json = home.toJson();
 *
 * DEPENDENCIES:
 * - Json::Value (jsoncpp)
 * - pqxx::row (libpqxx)
 * - chrono for timestamps
 *
 * MODIFICATION GUIDE:
 * - Add new fields to struct, then update fromJson, fromDbRow, and toJson
 * - Ensure database migration is created for new fields
 * - Update FosterService to handle new fields
 *
 * @author Agent 9 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

namespace wtl::core::models {

/**
 * @struct FosterHome
 * @brief Represents a registered foster home in the system
 *
 * Foster homes are users who have registered to temporarily care for dogs.
 * This model tracks their home environment, preferences, capacity, and
 * verification status to enable matching with compatible dogs.
 */
struct FosterHome {
    // ========================================================================
    // IDENTIFICATION
    // ========================================================================

    std::string id;                              ///< UUID primary key
    std::string user_id;                         ///< FK to users table

    // ========================================================================
    // CAPACITY
    // ========================================================================

    int max_dogs = 1;                            ///< Maximum dogs they can foster simultaneously
    int current_dogs = 0;                        ///< Current number of dogs being fostered

    // ========================================================================
    // HOME ENVIRONMENT
    // ========================================================================

    bool has_yard = false;                       ///< Has a fenced yard
    bool has_other_dogs = false;                 ///< Has other dogs in the home
    bool has_cats = false;                       ///< Has cats in the home
    bool has_children = false;                   ///< Has children in the home
    std::vector<std::string> children_ages;      ///< Age ranges of children (e.g., "0-2", "3-5", "6-10")

    // ========================================================================
    // PREFERENCES
    // ========================================================================

    bool ok_with_puppies = true;                 ///< Willing to foster puppies
    bool ok_with_seniors = true;                 ///< Willing to foster senior dogs
    bool ok_with_medical = false;                ///< Willing to foster dogs with medical needs
    bool ok_with_behavioral = false;             ///< Willing to foster dogs with behavioral issues
    std::vector<std::string> preferred_sizes;    ///< Preferred sizes: small, medium, large, xlarge
    std::vector<std::string> preferred_breeds;   ///< Preferred breeds (empty = all breeds)

    // ========================================================================
    // LOCATION
    // ========================================================================

    std::string zip_code;                        ///< Foster home ZIP code
    int max_transport_miles = 50;                ///< Maximum miles willing to travel for pickup
    std::optional<double> latitude;              ///< Geographic latitude (for distance calc)
    std::optional<double> longitude;             ///< Geographic longitude (for distance calc)

    // ========================================================================
    // STATUS
    // ========================================================================

    bool is_active = true;                       ///< Currently accepting foster placements
    bool is_verified = false;                    ///< Background check completed
    std::optional<std::chrono::system_clock::time_point> background_check_date;  ///< When background check was done

    // ========================================================================
    // STATISTICS
    // ========================================================================

    int fosters_completed = 0;                   ///< Total fosters completed
    int fosters_converted_to_adoption = 0;       ///< "Foster fails" - adopted their foster dogs

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;   ///< Record creation timestamp
    std::chrono::system_clock::time_point updated_at;   ///< Last update timestamp

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create a FosterHome from JSON data
     * @param json JSON object containing foster home data
     * @return FosterHome populated from JSON
     */
    static FosterHome fromJson(const Json::Value& json);

    /**
     * @brief Create a FosterHome from a database row
     * @param row pqxx database row
     * @return FosterHome populated from database
     */
    static FosterHome fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert FosterHome to JSON representation
     * @return Json::Value containing all foster home data
     */
    Json::Value toJson() const;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Check if foster home has available capacity
     * @return true if current_dogs < max_dogs and is_active
     */
    bool hasCapacity() const;

    /**
     * @brief Calculate foster success rate
     * @return Percentage of fosters that led to adoption (0-100)
     */
    double getSuccessRate() const;
};

} // namespace wtl::core::models
