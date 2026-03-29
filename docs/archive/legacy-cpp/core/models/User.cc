/**
 * @file User.cc
 * @brief Implementation of User model
 * @see User.h for documentation
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/models/User.h"

#include <iomanip>
#include <sstream>

namespace wtl::core::models {

// ============================================================================
// STATIC CONVERSION METHODS
// ============================================================================

User User::fromJson(const Json::Value& json) {
    User user;

    // Identity fields
    user.id = json.get("id", "").asString();
    user.email = json.get("email", "").asString();
    user.password_hash = json.get("password_hash", "").asString();
    user.display_name = json.get("display_name", "").asString();

    // Profile
    if (json.isMember("phone") && !json["phone"].isNull()) {
        user.phone = json["phone"].asString();
    }
    if (json.isMember("zip_code") && !json["zip_code"].isNull()) {
        user.zip_code = json["zip_code"].asString();
    }

    // Roles
    user.role = json.get("role", "user").asString();
    if (json.isMember("shelter_id") && !json["shelter_id"].isNull()) {
        user.shelter_id = json["shelter_id"].asString();
    }

    // Preferences - can be objects directly from JSON
    if (json.isMember("notification_preferences")) {
        user.notification_preferences = json["notification_preferences"];
    } else {
        user.notification_preferences = Json::objectValue;
    }
    if (json.isMember("search_preferences")) {
        user.search_preferences = json["search_preferences"];
    } else {
        user.search_preferences = Json::objectValue;
    }

    // Status
    user.is_active = json.get("is_active", true).asBool();
    user.email_verified = json.get("email_verified", false).asBool();
    if (json.isMember("last_login") && !json["last_login"].isNull()) {
        user.last_login = parseTimestamp(json["last_login"].asString());
    }

    // Timestamps
    if (json.isMember("created_at") && !json["created_at"].isNull()) {
        user.created_at = parseTimestamp(json["created_at"].asString());
    } else {
        user.created_at = std::chrono::system_clock::now();
    }
    if (json.isMember("updated_at") && !json["updated_at"].isNull()) {
        user.updated_at = parseTimestamp(json["updated_at"].asString());
    } else {
        user.updated_at = std::chrono::system_clock::now();
    }

    return user;
}

User User::fromDbRow(const pqxx::row& row) {
    User user;

    // Identity fields
    user.id = row["id"].as<std::string>();
    user.email = row["email"].as<std::string>();
    user.password_hash = row["password_hash"].as<std::string>();
    user.display_name = row["display_name"].as<std::string>("");

    // Profile
    if (!row["phone"].is_null()) {
        user.phone = row["phone"].as<std::string>();
    }
    if (!row["zip_code"].is_null()) {
        user.zip_code = row["zip_code"].as<std::string>();
    }

    // Roles
    user.role = row["role"].as<std::string>("user");
    if (!row["shelter_id"].is_null()) {
        user.shelter_id = row["shelter_id"].as<std::string>();
    }

    // Preferences - stored as JSONB in PostgreSQL
    if (!row["notification_preferences"].is_null()) {
        user.notification_preferences = parseJsonString(row["notification_preferences"].as<std::string>());
    } else {
        user.notification_preferences = Json::objectValue;
    }
    if (!row["search_preferences"].is_null()) {
        user.search_preferences = parseJsonString(row["search_preferences"].as<std::string>());
    } else {
        user.search_preferences = Json::objectValue;
    }

    // Status
    user.is_active = row["is_active"].as<bool>(true);
    user.email_verified = row["email_verified"].as<bool>(false);
    if (!row["last_login"].is_null()) {
        user.last_login = parseTimestamp(row["last_login"].as<std::string>());
    }

    // Timestamps
    user.created_at = parseTimestamp(row["created_at"].as<std::string>());
    user.updated_at = parseTimestamp(row["updated_at"].as<std::string>());

    return user;
}

Json::Value User::toJson() const {
    Json::Value json;

    // Identity (EXCLUDE password_hash for security)
    json["id"] = id;
    json["email"] = email;
    // password_hash deliberately NOT included
    json["display_name"] = display_name;

    // Profile
    if (phone) {
        json["phone"] = *phone;
    } else {
        json["phone"] = Json::nullValue;
    }
    if (zip_code) {
        json["zip_code"] = *zip_code;
    } else {
        json["zip_code"] = Json::nullValue;
    }

    // Roles
    json["role"] = role;
    if (shelter_id) {
        json["shelter_id"] = *shelter_id;
    } else {
        json["shelter_id"] = Json::nullValue;
    }

    // Preferences
    json["notification_preferences"] = notification_preferences;
    json["search_preferences"] = search_preferences;

    // Status
    json["is_active"] = is_active;
    json["email_verified"] = email_verified;
    if (last_login) {
        json["last_login"] = formatTimestamp(*last_login);
    } else {
        json["last_login"] = Json::nullValue;
    }

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    return json;
}

Json::Value User::toJsonPrivate() const {
    // Start with the public JSON
    Json::Value json = toJson();

    // Add the password_hash for admin use
    // This is needed for admin tools that may need to audit or migrate data
    json["password_hash"] = password_hash;

    return json;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

std::chrono::system_clock::time_point User::parseTimestamp(const std::string& timestamp) {
    // Parse ISO 8601 format: "2024-01-28T12:00:00Z" or PostgreSQL format
    std::tm tm = {};
    std::istringstream ss(timestamp);

    // Try ISO 8601 format first
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) {
        // Try PostgreSQL format: "2024-01-28 12:00:00"
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    }

    if (ss.fail()) {
        // Return current time if parsing fails
        return std::chrono::system_clock::now();
    }

    // Convert to time_point
    std::time_t time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

std::string User::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::gmtime(&time);

    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

Json::Value User::parseJsonString(const std::string& json_str) {
    Json::Value result;

    if (json_str.empty()) {
        return Json::objectValue;
    }

    Json::CharReaderBuilder builder;
    std::istringstream ss(json_str);
    std::string errors;

    if (!Json::parseFromStream(builder, ss, &result, &errors)) {
        // Return empty object on parse failure
        return Json::objectValue;
    }

    return result;
}

} // namespace wtl::core::models
