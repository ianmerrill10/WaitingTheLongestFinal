/**
 * @file FosterHome.cc
 * @brief Implementation of FosterHome model
 * @see FosterHome.h for documentation
 */

#include "core/models/FosterHome.h"

#include <iomanip>
#include <sstream>

namespace wtl::core::models {

// ============================================================================
// JSON CONVERSION
// ============================================================================

FosterHome FosterHome::fromJson(const Json::Value& json) {
    FosterHome home;

    // Identification
    home.id = json.get("id", "").asString();
    home.user_id = json.get("user_id", "").asString();

    // Capacity
    home.max_dogs = json.get("max_dogs", 1).asInt();
    home.current_dogs = json.get("current_dogs", 0).asInt();

    // Home environment
    home.has_yard = json.get("has_yard", false).asBool();
    home.has_other_dogs = json.get("has_other_dogs", false).asBool();
    home.has_cats = json.get("has_cats", false).asBool();
    home.has_children = json.get("has_children", false).asBool();

    // Parse children_ages array
    if (json.isMember("children_ages") && json["children_ages"].isArray()) {
        for (const auto& age : json["children_ages"]) {
            home.children_ages.push_back(age.asString());
        }
    }

    // Preferences
    home.ok_with_puppies = json.get("ok_with_puppies", true).asBool();
    home.ok_with_seniors = json.get("ok_with_seniors", true).asBool();
    home.ok_with_medical = json.get("ok_with_medical", false).asBool();
    home.ok_with_behavioral = json.get("ok_with_behavioral", false).asBool();

    // Parse preferred_sizes array
    if (json.isMember("preferred_sizes") && json["preferred_sizes"].isArray()) {
        for (const auto& size : json["preferred_sizes"]) {
            home.preferred_sizes.push_back(size.asString());
        }
    }

    // Parse preferred_breeds array
    if (json.isMember("preferred_breeds") && json["preferred_breeds"].isArray()) {
        for (const auto& breed : json["preferred_breeds"]) {
            home.preferred_breeds.push_back(breed.asString());
        }
    }

    // Location
    home.zip_code = json.get("zip_code", "").asString();
    home.max_transport_miles = json.get("max_transport_miles", 50).asInt();

    if (json.isMember("latitude") && !json["latitude"].isNull()) {
        home.latitude = json["latitude"].asDouble();
    }
    if (json.isMember("longitude") && !json["longitude"].isNull()) {
        home.longitude = json["longitude"].asDouble();
    }

    // Status
    home.is_active = json.get("is_active", true).asBool();
    home.is_verified = json.get("is_verified", false).asBool();

    // Background check date parsing
    if (json.isMember("background_check_date") && !json["background_check_date"].isNull()) {
        std::string date_str = json["background_check_date"].asString();
        if (!date_str.empty()) {
            std::istringstream ss(date_str);
            std::tm tm = {};
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            if (!ss.fail()) {
                home.background_check_date = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            }
        }
    }

    // Statistics
    home.fosters_completed = json.get("fosters_completed", 0).asInt();
    home.fosters_converted_to_adoption = json.get("fosters_converted_to_adoption", 0).asInt();

    // Timestamps (parse ISO 8601 format)
    auto parseTimestamp = [](const std::string& str) -> std::chrono::system_clock::time_point {
        if (str.empty()) {
            return std::chrono::system_clock::now();
        }
        std::istringstream ss(str);
        std::tm tm = {};
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        if (ss.fail()) {
            return std::chrono::system_clock::now();
        }
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    };

    home.created_at = parseTimestamp(json.get("created_at", "").asString());
    home.updated_at = parseTimestamp(json.get("updated_at", "").asString());

    return home;
}

FosterHome FosterHome::fromDbRow(const pqxx::row& row) {
    FosterHome home;

    // Identification
    home.id = row["id"].as<std::string>();
    home.user_id = row["user_id"].as<std::string>();

    // Capacity
    home.max_dogs = row["max_dogs"].as<int>(1);
    home.current_dogs = row["current_dogs"].as<int>(0);

    // Home environment
    home.has_yard = row["has_yard"].as<bool>(false);
    home.has_other_dogs = row["has_other_dogs"].as<bool>(false);
    home.has_cats = row["has_cats"].as<bool>(false);
    home.has_children = row["has_children"].as<bool>(false);

    // Parse children_ages from PostgreSQL array
    if (!row["children_ages"].is_null()) {
        std::string arr_str = row["children_ages"].as<std::string>();
        // Parse PostgreSQL array format: {value1,value2,value3}
        if (arr_str.length() > 2) {
            arr_str = arr_str.substr(1, arr_str.length() - 2); // Remove { }
            std::istringstream ss(arr_str);
            std::string item;
            while (std::getline(ss, item, ',')) {
                // Remove quotes if present
                if (item.front() == '"') item = item.substr(1, item.length() - 2);
                home.children_ages.push_back(item);
            }
        }
    }

    // Preferences
    home.ok_with_puppies = row["ok_with_puppies"].as<bool>(true);
    home.ok_with_seniors = row["ok_with_seniors"].as<bool>(true);
    home.ok_with_medical = row["ok_with_medical"].as<bool>(false);
    home.ok_with_behavioral = row["ok_with_behavioral"].as<bool>(false);

    // Parse preferred_sizes from PostgreSQL array
    if (!row["preferred_sizes"].is_null()) {
        std::string arr_str = row["preferred_sizes"].as<std::string>();
        if (arr_str.length() > 2) {
            arr_str = arr_str.substr(1, arr_str.length() - 2);
            std::istringstream ss(arr_str);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (item.front() == '"') item = item.substr(1, item.length() - 2);
                home.preferred_sizes.push_back(item);
            }
        }
    }

    // Parse preferred_breeds from PostgreSQL array
    if (!row["preferred_breeds"].is_null()) {
        std::string arr_str = row["preferred_breeds"].as<std::string>();
        if (arr_str.length() > 2) {
            arr_str = arr_str.substr(1, arr_str.length() - 2);
            std::istringstream ss(arr_str);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (item.front() == '"') item = item.substr(1, item.length() - 2);
                home.preferred_breeds.push_back(item);
            }
        }
    }

    // Location
    home.zip_code = row["zip_code"].as<std::string>("");
    home.max_transport_miles = row["max_transport_miles"].as<int>(50);

    if (!row["latitude"].is_null()) {
        home.latitude = row["latitude"].as<double>();
    }
    if (!row["longitude"].is_null()) {
        home.longitude = row["longitude"].as<double>();
    }

    // Status
    home.is_active = row["is_active"].as<bool>(true);
    home.is_verified = row["is_verified"].as<bool>(false);

    if (!row["background_check_date"].is_null()) {
        std::string date_str = row["background_check_date"].as<std::string>();
        std::istringstream ss(date_str);
        std::tm tm = {};
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) {
            home.background_check_date = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }
    }

    // Statistics
    home.fosters_completed = row["fosters_completed"].as<int>(0);
    home.fosters_converted_to_adoption = row["fosters_converted_to_adoption"].as<int>(0);

    // Timestamps
    auto parseDbTimestamp = [](const std::string& str) -> std::chrono::system_clock::time_point {
        if (str.empty()) {
            return std::chrono::system_clock::now();
        }
        std::istringstream ss(str);
        std::tm tm = {};
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            return std::chrono::system_clock::now();
        }
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    };

    home.created_at = parseDbTimestamp(row["created_at"].as<std::string>(""));
    home.updated_at = parseDbTimestamp(row["updated_at"].as<std::string>(""));

    return home;
}

Json::Value FosterHome::toJson() const {
    Json::Value json;

    // Identification
    json["id"] = id;
    json["user_id"] = user_id;

    // Capacity
    json["max_dogs"] = max_dogs;
    json["current_dogs"] = current_dogs;
    json["has_capacity"] = hasCapacity();

    // Home environment
    json["has_yard"] = has_yard;
    json["has_other_dogs"] = has_other_dogs;
    json["has_cats"] = has_cats;
    json["has_children"] = has_children;

    Json::Value children_ages_json(Json::arrayValue);
    for (const auto& age : children_ages) {
        children_ages_json.append(age);
    }
    json["children_ages"] = children_ages_json;

    // Preferences
    json["ok_with_puppies"] = ok_with_puppies;
    json["ok_with_seniors"] = ok_with_seniors;
    json["ok_with_medical"] = ok_with_medical;
    json["ok_with_behavioral"] = ok_with_behavioral;

    Json::Value sizes_json(Json::arrayValue);
    for (const auto& size : preferred_sizes) {
        sizes_json.append(size);
    }
    json["preferred_sizes"] = sizes_json;

    Json::Value breeds_json(Json::arrayValue);
    for (const auto& breed : preferred_breeds) {
        breeds_json.append(breed);
    }
    json["preferred_breeds"] = breeds_json;

    // Location
    json["zip_code"] = zip_code;
    json["max_transport_miles"] = max_transport_miles;

    if (latitude.has_value()) {
        json["latitude"] = latitude.value();
    } else {
        json["latitude"] = Json::nullValue;
    }

    if (longitude.has_value()) {
        json["longitude"] = longitude.value();
    } else {
        json["longitude"] = Json::nullValue;
    }

    // Status
    json["is_active"] = is_active;
    json["is_verified"] = is_verified;

    // Format background check date
    if (background_check_date.has_value()) {
        auto time_t_val = std::chrono::system_clock::to_time_t(background_check_date.value());
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
        json["background_check_date"] = ss.str();
    } else {
        json["background_check_date"] = Json::nullValue;
    }

    // Statistics
    json["fosters_completed"] = fosters_completed;
    json["fosters_converted_to_adoption"] = fosters_converted_to_adoption;
    json["success_rate"] = getSuccessRate();

    // Timestamps
    auto formatTimestamp = [](const std::chrono::system_clock::time_point& tp) -> std::string {
        auto time_t_val = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    };

    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    return json;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool FosterHome::hasCapacity() const {
    return is_active && current_dogs < max_dogs;
}

double FosterHome::getSuccessRate() const {
    if (fosters_completed == 0) {
        return 0.0;
    }
    return (static_cast<double>(fosters_converted_to_adoption) / fosters_completed) * 100.0;
}

} // namespace wtl::core::models
