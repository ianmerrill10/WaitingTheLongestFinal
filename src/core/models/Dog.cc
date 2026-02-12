/**
 * @file Dog.cc
 * @brief Implementation of Dog model
 * @see Dog.h for documentation
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/models/Dog.h"

#include <iomanip>
#include <sstream>
#include <regex>

namespace wtl::core::models {

// ============================================================================
// STATIC CONVERSION METHODS
// ============================================================================

Dog Dog::fromJson(const Json::Value& json) {
    Dog dog;

    // Identity fields
    dog.id = json.get("id", "").asString();
    dog.name = json.get("name", "").asString();
    dog.shelter_id = json.get("shelter_id", "").asString();

    // Basic info
    dog.breed_primary = json.get("breed_primary", "Unknown").asString();
    if (json.isMember("breed_secondary") && !json["breed_secondary"].isNull()) {
        dog.breed_secondary = json["breed_secondary"].asString();
    }
    dog.size = json.get("size", "medium").asString();
    dog.age_category = json.get("age_category", "adult").asString();
    dog.age_months = json.get("age_months", 0).asInt();
    dog.gender = json.get("gender", "unknown").asString();
    dog.color_primary = json.get("color_primary", "Unknown").asString();
    if (json.isMember("color_secondary") && !json["color_secondary"].isNull()) {
        dog.color_secondary = json["color_secondary"].asString();
    }
    if (json.isMember("weight_lbs") && !json["weight_lbs"].isNull()) {
        dog.weight_lbs = json["weight_lbs"].asDouble();
    }

    // Status
    dog.status = json.get("status", "adoptable").asString();
    dog.is_available = json.get("is_available", true).asBool();

    // Dates
    if (json.isMember("intake_date") && !json["intake_date"].isNull()) {
        dog.intake_date = parseTimestamp(json["intake_date"].asString());
    } else {
        dog.intake_date = std::chrono::system_clock::now();
    }
    if (json.isMember("available_date") && !json["available_date"].isNull()) {
        dog.available_date = parseTimestamp(json["available_date"].asString());
    }
    if (json.isMember("euthanasia_date") && !json["euthanasia_date"].isNull()) {
        dog.euthanasia_date = parseTimestamp(json["euthanasia_date"].asString());
    }

    // Urgency
    dog.urgency_level = json.get("urgency_level", "normal").asString();
    dog.is_on_euthanasia_list = json.get("is_on_euthanasia_list", false).asBool();

    // Media - photo_urls as array
    if (json.isMember("photo_urls") && json["photo_urls"].isArray()) {
        for (const auto& url : json["photo_urls"]) {
            dog.photo_urls.push_back(url.asString());
        }
    }
    if (json.isMember("video_url") && !json["video_url"].isNull()) {
        dog.video_url = json["video_url"].asString();
    }

    // Description
    dog.description = json.get("description", "").asString();

    // Tags as array
    if (json.isMember("tags") && json["tags"].isArray()) {
        for (const auto& tag : json["tags"]) {
            dog.tags.push_back(tag.asString());
        }
    }

    // External
    if (json.isMember("external_id") && !json["external_id"].isNull()) {
        dog.external_id = json["external_id"].asString();
    }
    dog.source = json.get("source", "unknown").asString();

    // Timestamps
    if (json.isMember("created_at") && !json["created_at"].isNull()) {
        dog.created_at = parseTimestamp(json["created_at"].asString());
    } else {
        dog.created_at = std::chrono::system_clock::now();
    }
    if (json.isMember("updated_at") && !json["updated_at"].isNull()) {
        dog.updated_at = parseTimestamp(json["updated_at"].asString());
    } else {
        dog.updated_at = std::chrono::system_clock::now();
    }

    return dog;
}

Dog Dog::fromDbRow(const pqxx::row& row) {
    Dog dog;

    // Identity fields
    dog.id = row["id"].as<std::string>();
    dog.name = row["name"].as<std::string>();
    dog.shelter_id = row["shelter_id"].as<std::string>();

    // Basic info
    dog.breed_primary = row["breed_primary"].as<std::string>("Unknown");
    if (!row["breed_secondary"].is_null()) {
        dog.breed_secondary = row["breed_secondary"].as<std::string>();
    }
    dog.size = row["size"].as<std::string>("medium");
    dog.age_category = row["age_category"].as<std::string>("adult");
    dog.age_months = row["age_months"].as<int>(0);
    dog.gender = row["gender"].as<std::string>("unknown");
    dog.color_primary = row["color_primary"].as<std::string>("Unknown");
    if (!row["color_secondary"].is_null()) {
        dog.color_secondary = row["color_secondary"].as<std::string>();
    }
    if (!row["weight_lbs"].is_null()) {
        dog.weight_lbs = row["weight_lbs"].as<double>();
    }

    // Status
    dog.status = row["status"].as<std::string>("adoptable");
    dog.is_available = row["is_available"].as<bool>(true);

    // Dates - parse from PostgreSQL timestamp format
    dog.intake_date = parseTimestamp(row["intake_date"].as<std::string>());
    if (!row["available_date"].is_null()) {
        dog.available_date = parseTimestamp(row["available_date"].as<std::string>());
    }
    if (!row["euthanasia_date"].is_null()) {
        dog.euthanasia_date = parseTimestamp(row["euthanasia_date"].as<std::string>());
    }

    // Urgency
    dog.urgency_level = row["urgency_level"].as<std::string>("normal");
    dog.is_on_euthanasia_list = row["is_on_euthanasia_list"].as<bool>(false);

    // Media - PostgreSQL arrays
    if (!row["photo_urls"].is_null()) {
        dog.photo_urls = parsePostgresArray(row["photo_urls"].as<std::string>());
    }
    if (!row["video_url"].is_null()) {
        dog.video_url = row["video_url"].as<std::string>();
    }

    // Description
    dog.description = row["description"].as<std::string>("");

    // Tags - PostgreSQL array
    if (!row["tags"].is_null()) {
        dog.tags = parsePostgresArray(row["tags"].as<std::string>());
    }

    // External
    if (!row["external_id"].is_null()) {
        dog.external_id = row["external_id"].as<std::string>();
    }
    dog.source = row["source"].as<std::string>("unknown");

    // Timestamps
    dog.created_at = parseTimestamp(row["created_at"].as<std::string>());
    dog.updated_at = parseTimestamp(row["updated_at"].as<std::string>());

    return dog;
}

Json::Value Dog::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    json["name"] = name;
    json["shelter_id"] = shelter_id;

    // Basic info
    json["breed_primary"] = breed_primary;
    if (breed_secondary) {
        json["breed_secondary"] = *breed_secondary;
    } else {
        json["breed_secondary"] = Json::nullValue;
    }
    json["size"] = size;
    json["age_category"] = age_category;
    json["age_months"] = age_months;
    json["gender"] = gender;
    json["color_primary"] = color_primary;
    if (color_secondary) {
        json["color_secondary"] = *color_secondary;
    } else {
        json["color_secondary"] = Json::nullValue;
    }
    if (weight_lbs) {
        json["weight_lbs"] = *weight_lbs;
    } else {
        json["weight_lbs"] = Json::nullValue;
    }

    // Status
    json["status"] = status;
    json["is_available"] = is_available;

    // Dates
    json["intake_date"] = formatTimestamp(intake_date);
    if (available_date) {
        json["available_date"] = formatTimestamp(*available_date);
    } else {
        json["available_date"] = Json::nullValue;
    }
    if (euthanasia_date) {
        json["euthanasia_date"] = formatTimestamp(*euthanasia_date);
    } else {
        json["euthanasia_date"] = Json::nullValue;
    }

    // Urgency
    json["urgency_level"] = urgency_level;
    json["is_on_euthanasia_list"] = is_on_euthanasia_list;

    // Media
    Json::Value photos_array(Json::arrayValue);
    for (const auto& url : photo_urls) {
        photos_array.append(url);
    }
    json["photo_urls"] = photos_array;
    if (video_url) {
        json["video_url"] = *video_url;
    } else {
        json["video_url"] = Json::nullValue;
    }

    // Description
    json["description"] = description;

    // Tags
    Json::Value tags_array(Json::arrayValue);
    for (const auto& tag : tags) {
        tags_array.append(tag);
    }
    json["tags"] = tags_array;

    // External
    if (external_id) {
        json["external_id"] = *external_id;
    } else {
        json["external_id"] = Json::nullValue;
    }
    json["source"] = source;

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    return json;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

std::chrono::system_clock::time_point Dog::parseTimestamp(const std::string& timestamp) {
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

std::string Dog::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::gmtime(&time);

    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::vector<std::string> Dog::parsePostgresArray(const std::string& array_str) {
    std::vector<std::string> result;

    // PostgreSQL array format: {value1,value2,"quoted value",etc}
    if (array_str.empty() || array_str == "{}") {
        return result;
    }

    // Remove the outer braces
    std::string content = array_str;
    if (content.front() == '{') {
        content = content.substr(1);
    }
    if (content.back() == '}') {
        content = content.substr(0, content.length() - 1);
    }

    // Parse comma-separated values, respecting quotes
    std::string current;
    bool in_quotes = false;
    bool escaped = false;

    for (size_t i = 0; i < content.length(); ++i) {
        char c = content[i];

        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }

        if (c == ',' && !in_quotes) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    // Don't forget the last value
    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

std::string Dog::formatPostgresArray(const std::vector<std::string>& vec) {
    if (vec.empty()) {
        return "{}";
    }

    std::ostringstream ss;
    ss << "{";

    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) {
            ss << ",";
        }

        // Quote strings that contain special characters
        const std::string& val = vec[i];
        bool needs_quotes = val.find(',') != std::string::npos ||
                           val.find('"') != std::string::npos ||
                           val.find('{') != std::string::npos ||
                           val.find('}') != std::string::npos ||
                           val.find(' ') != std::string::npos;

        if (needs_quotes) {
            ss << "\"";
            // Escape internal quotes
            for (char c : val) {
                if (c == '"') {
                    ss << "\\\"";
                } else {
                    ss << c;
                }
            }
            ss << "\"";
        } else {
            ss << val;
        }
    }

    ss << "}";
    return ss.str();
}

} // namespace wtl::core::models
