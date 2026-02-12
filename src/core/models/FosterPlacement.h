/**
 * @file FosterPlacement.h
 * @brief Data models for foster placements and applications
 *
 * PURPOSE:
 * Defines FosterPlacement and FosterApplication models for tracking the
 * lifecycle of dogs in foster care - from application through placement
 * to final outcome. Supports the "Foster-First Pathway" which makes dogs
 * 14x more likely to be adopted.
 *
 * USAGE:
 * FosterPlacement placement;
 * placement.foster_home_id = "home-uuid";
 * placement.dog_id = "dog-uuid";
 * placement.status = "active";
 *
 * FosterApplication app;
 * app.foster_home_id = "home-uuid";
 * app.dog_id = "dog-uuid";
 * app.message = "I'd love to foster Max!";
 *
 * DEPENDENCIES:
 * - Json::Value (jsoncpp)
 * - pqxx::row (libpqxx)
 * - chrono for timestamps
 *
 * MODIFICATION GUIDE:
 * - Add new fields to structs, then update all conversion methods
 * - Update database migrations for schema changes
 * - Ensure FosterService handles new fields properly
 *
 * @author Agent 9 - WaitingTheLongest Build System
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
 * @struct FosterPlacement
 * @brief Represents an active or completed foster placement
 *
 * Tracks the placement of a dog in a foster home, including timeline,
 * status, and outcome. Used to manage ongoing fosters and collect
 * statistics on foster program effectiveness.
 */
struct FosterPlacement {
    // ========================================================================
    // IDENTIFICATION
    // ========================================================================

    std::string id;                              ///< UUID primary key
    std::string foster_home_id;                  ///< FK to foster_homes table
    std::string dog_id;                          ///< FK to dogs table
    std::string shelter_id;                      ///< FK to shelters table (originating shelter)

    // ========================================================================
    // TIMELINE
    // ========================================================================

    std::chrono::system_clock::time_point start_date;                         ///< When foster began
    std::optional<std::chrono::system_clock::time_point> expected_end_date;   ///< Estimated end date
    std::optional<std::chrono::system_clock::time_point> actual_end_date;     ///< When foster actually ended

    // ========================================================================
    // STATUS
    // ========================================================================

    /**
     * Placement status values:
     * - "active": Dog currently in foster care
     * - "completed": Foster ended normally
     * - "returned_early": Dog returned before expected date
     */
    std::string status;

    // ========================================================================
    // OUTCOME
    // ========================================================================

    /**
     * Outcome values:
     * - "ongoing": Placement still active
     * - "adopted_by_foster": Foster family adopted the dog ("foster fail")
     * - "adopted_other": Dog adopted by someone else
     * - "returned_to_shelter": Dog returned to shelter
     */
    std::string outcome;
    std::optional<std::string> outcome_notes;    ///< Additional notes about outcome

    // ========================================================================
    // SUPPORT PROVIDED
    // ========================================================================

    bool supplies_provided = false;              ///< Whether foster received supplies (food, crate, etc.)
    int vet_visits = 0;                          ///< Number of vet visits during foster period

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;   ///< Record creation timestamp
    std::chrono::system_clock::time_point updated_at;   ///< Last update timestamp

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create a FosterPlacement from JSON data
     * @param json JSON object containing placement data
     * @return FosterPlacement populated from JSON
     */
    static FosterPlacement fromJson(const Json::Value& json);

    /**
     * @brief Create a FosterPlacement from a database row
     * @param row pqxx database row
     * @return FosterPlacement populated from database
     */
    static FosterPlacement fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert FosterPlacement to JSON representation
     * @return Json::Value containing all placement data
     */
    Json::Value toJson() const;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Check if placement is currently active
     * @return true if status is "active"
     */
    bool isActive() const;

    /**
     * @brief Calculate duration of foster in days
     * @return Number of days (uses actual_end_date if ended, otherwise now)
     */
    int getDurationDays() const;
};

/**
 * @struct FosterApplication
 * @brief Represents an application to foster a specific dog
 *
 * Tracks the application process when a foster home applies to
 * foster a particular dog. Includes application status and
 * communication between applicant and shelter.
 */
struct FosterApplication {
    // ========================================================================
    // IDENTIFICATION
    // ========================================================================

    std::string id;                              ///< UUID primary key
    std::string foster_home_id;                  ///< FK to foster_homes table
    std::string dog_id;                          ///< FK to dogs table

    // ========================================================================
    // STATUS
    // ========================================================================

    /**
     * Application status values:
     * - "pending": Awaiting shelter review
     * - "approved": Application approved, ready for placement
     * - "rejected": Application denied
     * - "withdrawn": Applicant withdrew application
     */
    std::string status;

    // ========================================================================
    // COMMUNICATION
    // ========================================================================

    std::string message;                         ///< Message from applicant to shelter
    std::optional<std::string> response;         ///< Response from shelter to applicant

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;   ///< Application submission time
    std::chrono::system_clock::time_point updated_at;   ///< Last update timestamp

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create a FosterApplication from JSON data
     * @param json JSON object containing application data
     * @return FosterApplication populated from JSON
     */
    static FosterApplication fromJson(const Json::Value& json);

    /**
     * @brief Create a FosterApplication from a database row
     * @param row pqxx database row
     * @return FosterApplication populated from database
     */
    static FosterApplication fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert FosterApplication to JSON representation
     * @return Json::Value containing all application data
     */
    Json::Value toJson() const;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Check if application is pending review
     * @return true if status is "pending"
     */
    bool isPending() const;

    /**
     * @brief Check if application was approved
     * @return true if status is "approved"
     */
    bool isApproved() const;
};

} // namespace wtl::core::models
