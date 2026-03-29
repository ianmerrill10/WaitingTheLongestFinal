/**
 * @file FosterPlacement.cc
 * @brief Implementation of FosterPlacement and FosterApplication models
 * @see FosterPlacement.h for documentation
 */

#include "core/models/FosterPlacement.h"

#include <iomanip>
#include <sstream>

namespace wtl::core::models {

// ============================================================================
// FOSTER PLACEMENT - JSON CONVERSION
// ============================================================================

FosterPlacement FosterPlacement::fromJson(const Json::Value& json) {
    FosterPlacement placement;

    // Helper to parse ISO 8601 timestamps
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

    // Helper to parse optional timestamps
    auto parseOptionalTimestamp = [&parseTimestamp](const Json::Value& val)
        -> std::optional<std::chrono::system_clock::time_point> {
        if (val.isNull() || val.asString().empty()) {
            return std::nullopt;
        }
        return parseTimestamp(val.asString());
    };

    // Identification
    placement.id = json.get("id", "").asString();
    placement.foster_home_id = json.get("foster_home_id", "").asString();
    placement.dog_id = json.get("dog_id", "").asString();
    placement.shelter_id = json.get("shelter_id", "").asString();

    // Timeline
    placement.start_date = parseTimestamp(json.get("start_date", "").asString());
    placement.expected_end_date = parseOptionalTimestamp(json["expected_end_date"]);
    placement.actual_end_date = parseOptionalTimestamp(json["actual_end_date"]);

    // Status
    placement.status = json.get("status", "active").asString();

    // Outcome
    placement.outcome = json.get("outcome", "ongoing").asString();
    if (json.isMember("outcome_notes") && !json["outcome_notes"].isNull()) {
        placement.outcome_notes = json["outcome_notes"].asString();
    }

    // Support provided
    placement.supplies_provided = json.get("supplies_provided", false).asBool();
    placement.vet_visits = json.get("vet_visits", 0).asInt();

    // Timestamps
    placement.created_at = parseTimestamp(json.get("created_at", "").asString());
    placement.updated_at = parseTimestamp(json.get("updated_at", "").asString());

    return placement;
}

FosterPlacement FosterPlacement::fromDbRow(const pqxx::row& row) {
    FosterPlacement placement;

    // Helper to parse database timestamps
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

    // Identification
    placement.id = row["id"].as<std::string>();
    placement.foster_home_id = row["foster_home_id"].as<std::string>();
    placement.dog_id = row["dog_id"].as<std::string>();
    placement.shelter_id = row["shelter_id"].as<std::string>();

    // Timeline
    placement.start_date = parseDbTimestamp(row["start_date"].as<std::string>(""));

    if (!row["expected_end_date"].is_null()) {
        placement.expected_end_date = parseDbTimestamp(row["expected_end_date"].as<std::string>());
    }

    if (!row["actual_end_date"].is_null()) {
        placement.actual_end_date = parseDbTimestamp(row["actual_end_date"].as<std::string>());
    }

    // Status
    placement.status = row["status"].as<std::string>("active");

    // Outcome
    placement.outcome = row["outcome"].as<std::string>("ongoing");
    if (!row["outcome_notes"].is_null()) {
        placement.outcome_notes = row["outcome_notes"].as<std::string>();
    }

    // Support provided
    placement.supplies_provided = row["supplies_provided"].as<bool>(false);
    placement.vet_visits = row["vet_visits"].as<int>(0);

    // Timestamps
    placement.created_at = parseDbTimestamp(row["created_at"].as<std::string>(""));
    placement.updated_at = parseDbTimestamp(row["updated_at"].as<std::string>(""));

    return placement;
}

Json::Value FosterPlacement::toJson() const {
    Json::Value json;

    // Helper to format timestamps
    auto formatTimestamp = [](const std::chrono::system_clock::time_point& tp) -> std::string {
        auto time_t_val = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    };

    // Helper to format optional timestamps
    auto formatOptionalTimestamp = [&formatTimestamp](
        const std::optional<std::chrono::system_clock::time_point>& opt) -> Json::Value {
        if (opt.has_value()) {
            return formatTimestamp(opt.value());
        }
        return Json::nullValue;
    };

    // Identification
    json["id"] = id;
    json["foster_home_id"] = foster_home_id;
    json["dog_id"] = dog_id;
    json["shelter_id"] = shelter_id;

    // Timeline
    json["start_date"] = formatTimestamp(start_date);
    json["expected_end_date"] = formatOptionalTimestamp(expected_end_date);
    json["actual_end_date"] = formatOptionalTimestamp(actual_end_date);

    // Status
    json["status"] = status;
    json["is_active"] = isActive();

    // Outcome
    json["outcome"] = outcome;
    if (outcome_notes.has_value()) {
        json["outcome_notes"] = outcome_notes.value();
    } else {
        json["outcome_notes"] = Json::nullValue;
    }

    // Support provided
    json["supplies_provided"] = supplies_provided;
    json["vet_visits"] = vet_visits;

    // Calculated fields
    json["duration_days"] = getDurationDays();

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    return json;
}

// ============================================================================
// FOSTER PLACEMENT - HELPER METHODS
// ============================================================================

bool FosterPlacement::isActive() const {
    return status == "active";
}

int FosterPlacement::getDurationDays() const {
    auto end = actual_end_date.value_or(std::chrono::system_clock::now());
    auto duration = std::chrono::duration_cast<std::chrono::hours>(end - start_date);
    return static_cast<int>(duration.count() / 24);
}

// ============================================================================
// FOSTER APPLICATION - JSON CONVERSION
// ============================================================================

FosterApplication FosterApplication::fromJson(const Json::Value& json) {
    FosterApplication app;

    // Helper to parse ISO 8601 timestamps
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

    // Identification
    app.id = json.get("id", "").asString();
    app.foster_home_id = json.get("foster_home_id", "").asString();
    app.dog_id = json.get("dog_id", "").asString();

    // Status
    app.status = json.get("status", "pending").asString();

    // Communication
    app.message = json.get("message", "").asString();
    if (json.isMember("response") && !json["response"].isNull()) {
        app.response = json["response"].asString();
    }

    // Timestamps
    app.created_at = parseTimestamp(json.get("created_at", "").asString());
    app.updated_at = parseTimestamp(json.get("updated_at", "").asString());

    return app;
}

FosterApplication FosterApplication::fromDbRow(const pqxx::row& row) {
    FosterApplication app;

    // Helper to parse database timestamps
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

    // Identification
    app.id = row["id"].as<std::string>();
    app.foster_home_id = row["foster_home_id"].as<std::string>();
    app.dog_id = row["dog_id"].as<std::string>();

    // Status
    app.status = row["status"].as<std::string>("pending");

    // Communication
    app.message = row["message"].as<std::string>("");
    if (!row["response"].is_null()) {
        app.response = row["response"].as<std::string>();
    }

    // Timestamps
    app.created_at = parseDbTimestamp(row["created_at"].as<std::string>(""));
    app.updated_at = parseDbTimestamp(row["updated_at"].as<std::string>(""));

    return app;
}

Json::Value FosterApplication::toJson() const {
    Json::Value json;

    // Helper to format timestamps
    auto formatTimestamp = [](const std::chrono::system_clock::time_point& tp) -> std::string {
        auto time_t_val = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    };

    // Identification
    json["id"] = id;
    json["foster_home_id"] = foster_home_id;
    json["dog_id"] = dog_id;

    // Status
    json["status"] = status;
    json["is_pending"] = isPending();
    json["is_approved"] = isApproved();

    // Communication
    json["message"] = message;
    if (response.has_value()) {
        json["response"] = response.value();
    } else {
        json["response"] = Json::nullValue;
    }

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    return json;
}

// ============================================================================
// FOSTER APPLICATION - HELPER METHODS
// ============================================================================

bool FosterApplication::isPending() const {
    return status == "pending";
}

bool FosterApplication::isApproved() const {
    return status == "approved";
}

} // namespace wtl::core::models
