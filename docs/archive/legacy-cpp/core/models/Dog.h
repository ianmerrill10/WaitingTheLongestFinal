/**
 * @file Dog.h
 * @brief Dog model representing a dog available for adoption
 *
 * PURPOSE:
 * Defines the Dog data model with all fields required for the WaitingTheLongest
 * platform. Dogs are the primary entity - this model represents dogs waiting
 * in shelters for adoption.
 *
 * USAGE:
 * Dog dog = Dog::fromJson(jsonData);
 * Dog dog = Dog::fromDbRow(dbRow);
 * Json::Value json = dog.toJson();
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
#include <utility>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

namespace wtl::core::models {

/**
 * @struct Dog
 * @brief Represents a dog in the shelter system
 *
 * Contains all information about a dog including basic info,
 * status, urgency level, media, and timestamps.
 */
struct Dog {
    // ========================================================================
    // SPECIAL MEMBER FUNCTIONS
    // ========================================================================

    Dog() = default;
    ~Dog() = default;
    Dog(const Dog&) = default;
    Dog& operator=(const Dog&) = default;
    Dog(Dog&&) noexcept = default;
    Dog& operator=(Dog&&) noexcept = default;

    /**
     * @brief ADL swap for use with std::sort and other algorithms
     */
    friend void swap(Dog& a, Dog& b) noexcept {
        using std::swap;
        swap(a.id, b.id);
        swap(a.name, b.name);
        swap(a.shelter_id, b.shelter_id);
        swap(a.breed_primary, b.breed_primary);
        swap(a.breed_secondary, b.breed_secondary);
        swap(a.size, b.size);
        swap(a.age_category, b.age_category);
        swap(a.age_months, b.age_months);
        swap(a.gender, b.gender);
        swap(a.color_primary, b.color_primary);
        swap(a.color_secondary, b.color_secondary);
        swap(a.weight_lbs, b.weight_lbs);
        swap(a.status, b.status);
        swap(a.is_available, b.is_available);
        swap(a.intake_date, b.intake_date);
        swap(a.available_date, b.available_date);
        swap(a.euthanasia_date, b.euthanasia_date);
        swap(a.urgency_level, b.urgency_level);
        swap(a.is_on_euthanasia_list, b.is_on_euthanasia_list);
        swap(a.photo_urls, b.photo_urls);
        swap(a.video_url, b.video_url);
        swap(a.description, b.description);
        swap(a.tags, b.tags);
        swap(a.good_with_kids, b.good_with_kids);
        swap(a.good_with_dogs, b.good_with_dogs);
        swap(a.good_with_cats, b.good_with_cats);
        swap(a.house_trained, b.house_trained);
        swap(a.crate_trained, b.crate_trained);
        swap(a.leash_trained, b.leash_trained);
        swap(a.has_special_needs, b.has_special_needs);
        swap(a.special_needs_description, b.special_needs_description);
        swap(a.has_medical_conditions, b.has_medical_conditions);
        swap(a.medical_conditions, b.medical_conditions);
        swap(a.is_heartworm_positive, b.is_heartworm_positive);
        swap(a.has_behavioral_notes, b.has_behavioral_notes);
        swap(a.behavioral_notes, b.behavioral_notes);
        swap(a.energy_level, b.energy_level);
        swap(a.adoption_fee, b.adoption_fee);
        swap(a.fee_includes, b.fee_includes);
        swap(a.is_fee_waived, b.is_fee_waived);
        swap(a.external_id, b.external_id);
        swap(a.source, b.source);
        swap(a.created_at, b.created_at);
        swap(a.updated_at, b.updated_at);
    }

    // ========================================================================
    // IDENTITY
    // ========================================================================

    std::string id;                          ///< UUID primary key
    std::string name;                        ///< Dog's name
    std::string shelter_id;                  ///< FK to shelters table

    // ========================================================================
    // BASIC INFO
    // ========================================================================

    std::string breed_primary;               ///< Primary breed
    std::optional<std::string> breed_secondary; ///< Secondary breed if mixed
    std::string size;                        ///< small, medium, large, xlarge
    std::string age_category;                ///< puppy, young, adult, senior
    int age_months;                          ///< Age in months
    std::string gender;                      ///< male, female
    std::string color_primary;               ///< Primary coat color
    std::optional<std::string> color_secondary; ///< Secondary coat color
    std::optional<double> weight_lbs;        ///< Weight in pounds

    // ========================================================================
    // STATUS
    // ========================================================================

    std::string status;                      ///< adoptable, pending, adopted, etc.
    bool is_available;                       ///< Quick flag for availability

    // ========================================================================
    // DATES
    // ========================================================================

    std::chrono::system_clock::time_point intake_date;     ///< When dog entered shelter
    std::optional<std::chrono::system_clock::time_point> available_date; ///< When became available
    std::optional<std::chrono::system_clock::time_point> euthanasia_date; ///< Scheduled euthanasia

    // ========================================================================
    // URGENCY
    // ========================================================================

    std::string urgency_level;               ///< normal, medium, high, critical
    bool is_on_euthanasia_list;              ///< Whether on kill list

    // ========================================================================
    // MEDIA
    // ========================================================================

    std::vector<std::string> photo_urls;     ///< List of photo URLs
    std::optional<std::string> video_url;    ///< Optional video URL

    // ========================================================================
    // DESCRIPTION
    // ========================================================================

    std::string description;                 ///< Full description text
    std::vector<std::string> tags;           ///< Tags like good_with_kids, house_trained

    // ========================================================================
    // COMPATIBILITY FLAGS
    // ========================================================================

    std::optional<bool> good_with_kids;      ///< Good with children
    std::optional<bool> good_with_dogs;      ///< Good with other dogs
    std::optional<bool> good_with_cats;      ///< Good with cats
    std::optional<bool> house_trained;       ///< House trained
    std::optional<bool> crate_trained;       ///< Crate trained
    std::optional<bool> leash_trained;       ///< Leash trained

    // ========================================================================
    // SPECIAL NEEDS & BEHAVIORAL
    // ========================================================================

    bool has_special_needs = false;          ///< Whether dog has special needs
    std::string special_needs_description;   ///< Description of special needs
    bool has_medical_conditions = false;     ///< Whether dog has medical conditions
    std::string medical_conditions;          ///< Description of medical conditions
    bool is_heartworm_positive = false;      ///< Heartworm status
    bool has_behavioral_notes = false;       ///< Whether behavioral notes exist
    std::string behavioral_notes;            ///< Behavioral assessment notes
    std::string energy_level;                ///< low, medium, high

    // ========================================================================
    // ADOPTION INFORMATION
    // ========================================================================

    std::optional<double> adoption_fee;      ///< Adoption fee in dollars
    std::string fee_includes;                ///< What the fee includes
    bool is_fee_waived = false;              ///< Fee waived for urgent dogs

    // ========================================================================
    // EXTERNAL INTEGRATION
    // ========================================================================

    std::optional<std::string> external_id;  ///< ID from source system (e.g., PetFinder)
    std::string source;                      ///< rescuegroups, shelter_direct (NOTE: NO Petfinder API exists)

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;  ///< Record creation time
    std::chrono::system_clock::time_point updated_at;  ///< Last update time

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create a Dog from JSON data
     *
     * @param json The JSON object containing dog data
     * @return Dog The constructed Dog object
     *
     * @example
     * Json::Value json;
     * json["id"] = "uuid-here";
     * json["name"] = "Buddy";
     * Dog dog = Dog::fromJson(json);
     */
    static Dog fromJson(const Json::Value& json);

    /**
     * @brief Create a Dog from a database row
     *
     * @param row The pqxx database row
     * @return Dog The constructed Dog object
     *
     * @example
     * pqxx::result result = txn.exec("SELECT * FROM dogs WHERE id = $1", id);
     * if (!result.empty()) {
     *     Dog dog = Dog::fromDbRow(result[0]);
     * }
     */
    static Dog fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert Dog to JSON for API responses
     *
     * @return Json::Value The JSON representation
     *
     * @example
     * Dog dog;
     * Json::Value json = dog.toJson();
     * std::cout << json.toStyledString();
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

    /**
     * @brief Parse a PostgreSQL array string to vector
     * @param array_str The PostgreSQL array string format
     * @return std::vector<std::string> The parsed vector
     */
    static std::vector<std::string> parsePostgresArray(const std::string& array_str);

    /**
     * @brief Format a vector to PostgreSQL array string
     * @param vec The vector to format
     * @return std::string PostgreSQL array format
     */
    static std::string formatPostgresArray(const std::vector<std::string>& vec);
};

} // namespace wtl::core::models
