/**
 * @file Shelter.cc
 * @brief Implementation of Shelter model
 * @see Shelter.h for documentation
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/models/Shelter.h"

#include <iomanip>
#include <sstream>

namespace wtl::core::models {

// ============================================================================
// STATIC CONVERSION METHODS
// ============================================================================

Shelter Shelter::fromJson(const Json::Value& json) {
    Shelter shelter;

    // Identity fields
    shelter.id = json.get("id", "").asString();
    shelter.name = json.get("name", "").asString();
    shelter.state_code = json.get("state_code", "").asString();

    // Location
    shelter.city = json.get("city", "").asString();
    shelter.address = json.get("address", "").asString();
    shelter.zip_code = json.get("zip_code", "").asString();
    if (json.isMember("latitude") && !json["latitude"].isNull()) {
        shelter.latitude = json["latitude"].asDouble();
    }
    if (json.isMember("longitude") && !json["longitude"].isNull()) {
        shelter.longitude = json["longitude"].asDouble();
    }

    // Contact
    if (json.isMember("phone") && !json["phone"].isNull()) {
        shelter.phone = json["phone"].asString();
    }
    if (json.isMember("email") && !json["email"].isNull()) {
        shelter.email = json["email"].asString();
    }
    if (json.isMember("website") && !json["website"].isNull()) {
        shelter.website = json["website"].asString();
    }

    // Classification
    shelter.shelter_type = json.get("shelter_type", "rescue").asString();
    shelter.is_kill_shelter = json.get("is_kill_shelter", false).asBool();
    shelter.avg_hold_days = json.get("avg_hold_days", 7).asInt();
    if (json.isMember("euthanasia_schedule") && !json["euthanasia_schedule"].isNull()) {
        shelter.euthanasia_schedule = json["euthanasia_schedule"].asString();
    }
    shelter.accepts_rescue_pulls = json.get("accepts_rescue_pulls", true).asBool();

    // Stats
    shelter.dog_count = json.get("dog_count", 0).asInt();
    shelter.available_count = json.get("available_count", 0).asInt();

    // Status
    shelter.is_verified = json.get("is_verified", false).asBool();
    shelter.is_active = json.get("is_active", true).asBool();

    // Timestamps
    if (json.isMember("created_at") && !json["created_at"].isNull()) {
        shelter.created_at = parseTimestamp(json["created_at"].asString());
    } else {
        shelter.created_at = std::chrono::system_clock::now();
    }
    if (json.isMember("updated_at") && !json["updated_at"].isNull()) {
        shelter.updated_at = parseTimestamp(json["updated_at"].asString());
    } else {
        shelter.updated_at = std::chrono::system_clock::now();
    }

    return shelter;
}

Shelter Shelter::fromDbRow(const pqxx::row& row) {
    Shelter shelter;

    // Identity fields
    shelter.id = row["id"].as<std::string>();
    shelter.name = row["name"].as<std::string>();
    shelter.state_code = row["state_code"].as<std::string>();

    // Location
    shelter.city = row["city"].as<std::string>("");
    shelter.address = row["address"].as<std::string>("");
    shelter.zip_code = row["zip_code"].as<std::string>("");
    if (!row["latitude"].is_null()) {
        shelter.latitude = row["latitude"].as<double>();
    }
    if (!row["longitude"].is_null()) {
        shelter.longitude = row["longitude"].as<double>();
    }

    // Contact
    if (!row["phone"].is_null()) {
        shelter.phone = row["phone"].as<std::string>();
    }
    if (!row["email"].is_null()) {
        shelter.email = row["email"].as<std::string>();
    }
    if (!row["website"].is_null()) {
        shelter.website = row["website"].as<std::string>();
    }

    // Classification
    shelter.shelter_type = row["shelter_type"].as<std::string>("rescue");
    shelter.is_kill_shelter = row["is_kill_shelter"].as<bool>(false);
    shelter.avg_hold_days = row["avg_hold_days"].as<int>(7);
    if (!row["euthanasia_schedule"].is_null()) {
        shelter.euthanasia_schedule = row["euthanasia_schedule"].as<std::string>();
    }
    shelter.accepts_rescue_pulls = row["accepts_rescue_pulls"].as<bool>(true);

    // Stats
    shelter.dog_count = row["dog_count"].as<int>(0);
    shelter.available_count = row["available_count"].as<int>(0);

    // Status
    shelter.is_verified = row["is_verified"].as<bool>(false);
    shelter.is_active = row["is_active"].as<bool>(true);

    // Timestamps
    shelter.created_at = parseTimestamp(row["created_at"].as<std::string>());
    shelter.updated_at = parseTimestamp(row["updated_at"].as<std::string>());

    return shelter;
}

Json::Value Shelter::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    json["name"] = name;
    json["state_code"] = state_code;

    // Location
    json["city"] = city;
    json["address"] = address;
    json["zip_code"] = zip_code;
    if (latitude) {
        json["latitude"] = *latitude;
    } else {
        json["latitude"] = Json::nullValue;
    }
    if (longitude) {
        json["longitude"] = *longitude;
    } else {
        json["longitude"] = Json::nullValue;
    }

    // Contact
    if (phone) {
        json["phone"] = *phone;
    } else {
        json["phone"] = Json::nullValue;
    }
    if (email) {
        json["email"] = *email;
    } else {
        json["email"] = Json::nullValue;
    }
    if (website) {
        json["website"] = *website;
    } else {
        json["website"] = Json::nullValue;
    }

    // Classification
    json["shelter_type"] = shelter_type;
    json["is_kill_shelter"] = is_kill_shelter;
    json["avg_hold_days"] = avg_hold_days;
    if (euthanasia_schedule) {
        json["euthanasia_schedule"] = *euthanasia_schedule;
    } else {
        json["euthanasia_schedule"] = Json::nullValue;
    }
    json["accepts_rescue_pulls"] = accepts_rescue_pulls;

    // Stats
    json["dog_count"] = dog_count;
    json["available_count"] = available_count;

    // Status
    json["is_verified"] = is_verified;
    json["is_active"] = is_active;

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    return json;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

std::chrono::system_clock::time_point Shelter::parseTimestamp(const std::string& timestamp) {
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

std::string Shelter::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::gmtime(&time);

    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

} // namespace wtl::core::models
