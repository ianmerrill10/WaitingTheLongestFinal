/**
 * @file State.cc
 * @brief Implementation of State model
 * @see State.h for documentation
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/models/State.h"

#include <iomanip>
#include <sstream>

namespace wtl::core::models {

// ============================================================================
// STATIC CONVERSION METHODS
// ============================================================================

State State::fromJson(const Json::Value& json) {
    State state;

    // Identity fields
    state.code = json.get("code", "").asString();
    state.name = json.get("name", "").asString();
    state.region = json.get("region", "").asString();

    // Status
    state.is_active = json.get("is_active", true).asBool();

    // Cached counts
    state.dog_count = json.get("dog_count", 0).asInt();
    state.shelter_count = json.get("shelter_count", 0).asInt();

    // Timestamps
    if (json.isMember("created_at") && !json["created_at"].isNull()) {
        state.created_at = parseTimestamp(json["created_at"].asString());
    } else {
        state.created_at = std::chrono::system_clock::now();
    }
    if (json.isMember("updated_at") && !json["updated_at"].isNull()) {
        state.updated_at = parseTimestamp(json["updated_at"].asString());
    } else {
        state.updated_at = std::chrono::system_clock::now();
    }

    return state;
}

State State::fromDbRow(const pqxx::row& row) {
    State state;

    // Identity fields
    state.code = row["code"].as<std::string>();
    state.name = row["name"].as<std::string>();
    state.region = row["region"].as<std::string>("");

    // Status
    state.is_active = row["is_active"].as<bool>(true);

    // Cached counts
    state.dog_count = row["dog_count"].as<int>(0);
    state.shelter_count = row["shelter_count"].as<int>(0);

    // Timestamps
    state.created_at = parseTimestamp(row["created_at"].as<std::string>());
    state.updated_at = parseTimestamp(row["updated_at"].as<std::string>());

    return state;
}

Json::Value State::toJson() const {
    Json::Value json;

    // Identity
    json["code"] = code;
    json["name"] = name;
    json["region"] = region;

    // Status
    json["is_active"] = is_active;

    // Cached counts
    json["dog_count"] = dog_count;
    json["shelter_count"] = shelter_count;

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    return json;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

std::chrono::system_clock::time_point State::parseTimestamp(const std::string& timestamp) {
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

std::string State::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::gmtime(&time);

    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

} // namespace wtl::core::models
