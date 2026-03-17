/**
 * @file User.h
 * @brief User model representing a platform user
 *
 * PURPOSE:
 * Defines the User data model for authentication and user management.
 * Supports different roles including regular users, foster parents,
 * shelter admins, and platform admins.
 *
 * USAGE:
 * User user = User::fromJson(jsonData);
 * User user = User::fromDbRow(dbRow);
 * Json::Value json = user.toJson();        // Excludes password_hash
 * Json::Value json = user.toJsonPrivate(); // Includes all for admin use
 *
 * DEPENDENCIES:
 * - jsoncpp (Json::Value)
 * - pqxx (database row conversion)
 * - chrono (time handling)
 *
 * MODIFICATION GUIDE:
 * - Add new fields to struct and update all conversion methods
 * - toJson() must NEVER include password_hash
 * - toJsonPrivate() is for admin dashboard only
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
 * @struct User
 * @brief Represents a platform user with authentication and profile data
 *
 * Contains all user information including credentials, profile,
 * role, and preferences.
 */
struct User {
    // ========================================================================
    // IDENTITY
    // ========================================================================

    std::string id;                          ///< UUID primary key
    std::string email;                       ///< Email address (unique, for login)
    std::string password_hash;               ///< Bcrypt hashed password
    std::string display_name;                ///< Display name shown on site

    // ========================================================================
    // PROFILE
    // ========================================================================

    std::optional<std::string> phone;        ///< Phone number
    std::optional<std::string> zip_code;     ///< ZIP code for location-based features

    // ========================================================================
    // ROLES
    // ========================================================================

    std::string role;                        ///< user, foster, shelter_admin, admin
    std::optional<std::string> shelter_id;   ///< If shelter_admin, linked shelter

    // ========================================================================
    // PREFERENCES (stored as JSON)
    // ========================================================================

    Json::Value notification_preferences;    ///< Email, push notification settings
    Json::Value search_preferences;          ///< Saved search filters

    // ========================================================================
    // STATUS
    // ========================================================================

    bool is_active;                          ///< Account active
    bool email_verified;                     ///< Email verified
    std::optional<std::chrono::system_clock::time_point> last_login; ///< Last login time

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;  ///< Account creation time
    std::chrono::system_clock::time_point updated_at;  ///< Last update time

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create a User from JSON data
     *
     * @param json The JSON object containing user data
     * @return User The constructed User object
     */
    static User fromJson(const Json::Value& json);

    /**
     * @brief Create a User from a database row
     *
     * @param row The pqxx database row
     * @return User The constructed User object
     */
    static User fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert User to JSON for API responses (EXCLUDES password_hash)
     *
     * This is the standard conversion for API responses. It deliberately
     * excludes the password_hash field for security.
     *
     * @return Json::Value The JSON representation (no password)
     */
    Json::Value toJson() const;

    /**
     * @brief Convert User to JSON for admin use (INCLUDES all fields)
     *
     * WARNING: This includes sensitive data. Use only for admin dashboards
     * where full user data is required for management purposes.
     *
     * @return Json::Value The complete JSON representation
     */
    Json::Value toJsonPrivate() const;

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
     * @brief Parse JSON string from database column
     * @param json_str The JSON string from database
     * @return Json::Value The parsed JSON value
     */
    static Json::Value parseJsonString(const std::string& json_str);
};

} // namespace wtl::core::models
