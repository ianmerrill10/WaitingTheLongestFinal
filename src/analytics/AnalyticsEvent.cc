/**
 * @file AnalyticsEvent.cc
 * @brief Implementation of AnalyticsEvent struct
 * @see AnalyticsEvent.h for documentation
 */

#include "analytics/AnalyticsEvent.h"

#include <iomanip>
#include <random>
#include <sstream>

#include "core/debug/ErrorCapture.h"

namespace wtl::analytics {

// ============================================================================
// CONSTRUCTORS
// ============================================================================

AnalyticsEvent::AnalyticsEvent(AnalyticsEventType type,
                               const std::string& entity_type_param,
                               const std::string& entity_id_param)
    : event_type(type)
    , entity_type(entity_type_param)
    , entity_id(entity_id_param)
    , source("web")
    , timestamp(std::chrono::system_clock::now())
{
    generateId();
}

AnalyticsEvent::AnalyticsEvent(AnalyticsEventType type,
                               const std::string& entity_type_param,
                               const std::string& entity_id_param,
                               const std::optional<std::string>& user_id_param,
                               const std::string& session_id_param)
    : event_type(type)
    , entity_type(entity_type_param)
    , entity_id(entity_id_param)
    , user_id(user_id_param)
    , session_id(session_id_param)
    , source("web")
    , timestamp(std::chrono::system_clock::now())
{
    generateId();
}

// ============================================================================
// SERIALIZATION
// ============================================================================

Json::Value AnalyticsEvent::toJson() const {
    Json::Value json;

    // Identification
    json["id"] = id;
    json["event_type"] = analyticsEventTypeToString(event_type);
    json["entity_type"] = entity_type;
    json["entity_id"] = entity_id;

    // User context
    if (user_id) {
        json["user_id"] = *user_id;
    }
    json["session_id"] = session_id;
    if (visitor_id) {
        json["visitor_id"] = *visitor_id;
    }

    // Source tracking
    json["source"] = source;
    if (referrer) {
        json["referrer"] = *referrer;
    }
    if (utm_campaign) {
        json["utm_campaign"] = *utm_campaign;
    }
    if (utm_source) {
        json["utm_source"] = *utm_source;
    }
    if (utm_medium) {
        json["utm_medium"] = *utm_medium;
    }
    if (page_url) {
        json["page_url"] = *page_url;
    }

    // Device context
    if (device_type) {
        json["device_type"] = *device_type;
    }
    if (browser) {
        json["browser"] = *browser;
    }
    if (os) {
        json["os"] = *os;
    }
    if (ip_hash) {
        json["ip_hash"] = *ip_hash;
    }
    if (country_code) {
        json["country_code"] = *country_code;
    }
    if (region_code) {
        json["region_code"] = *region_code;
    }
    if (city) {
        json["city"] = *city;
    }

    // Event data
    json["data"] = data;
    json["timestamp"] = getTimestampISO();
    json["timestamp_epoch"] = static_cast<Json::Int64>(getTimestampEpoch());

    if (received_at) {
        json["received_at"] = formatTimestampISO(*received_at);
    }

    json["is_processed"] = is_processed;
    json["category"] = getCategory();

    return json;
}

AnalyticsEvent AnalyticsEvent::fromJson(const Json::Value& json) {
    AnalyticsEvent event;

    // Required fields
    if (!json.isMember("event_type")) {
        throw std::runtime_error("Missing required field: event_type");
    }

    auto type_opt = analyticsEventTypeFromString(json["event_type"].asString());
    if (!type_opt) {
        throw std::runtime_error("Invalid event_type: " + json["event_type"].asString());
    }
    event.event_type = *type_opt;

    // ID - generate if not provided
    if (json.isMember("id") && !json["id"].asString().empty()) {
        event.id = json["id"].asString();
    } else {
        event.generateId();
    }

    // Entity info
    event.entity_type = json.get("entity_type", "").asString();
    event.entity_id = json.get("entity_id", "").asString();

    // User context
    if (json.isMember("user_id") && !json["user_id"].isNull()) {
        event.user_id = json["user_id"].asString();
    }
    event.session_id = json.get("session_id", "").asString();
    if (json.isMember("visitor_id") && !json["visitor_id"].isNull()) {
        event.visitor_id = json["visitor_id"].asString();
    }

    // Source tracking
    event.source = json.get("source", "web").asString();
    if (json.isMember("referrer") && !json["referrer"].isNull()) {
        event.referrer = json["referrer"].asString();
    }
    if (json.isMember("utm_campaign") && !json["utm_campaign"].isNull()) {
        event.utm_campaign = json["utm_campaign"].asString();
    }
    if (json.isMember("utm_source") && !json["utm_source"].isNull()) {
        event.utm_source = json["utm_source"].asString();
    }
    if (json.isMember("utm_medium") && !json["utm_medium"].isNull()) {
        event.utm_medium = json["utm_medium"].asString();
    }
    if (json.isMember("page_url") && !json["page_url"].isNull()) {
        event.page_url = json["page_url"].asString();
    }

    // Device context
    if (json.isMember("device_type") && !json["device_type"].isNull()) {
        event.device_type = json["device_type"].asString();
    }
    if (json.isMember("browser") && !json["browser"].isNull()) {
        event.browser = json["browser"].asString();
    }
    if (json.isMember("os") && !json["os"].isNull()) {
        event.os = json["os"].asString();
    }
    if (json.isMember("ip_hash") && !json["ip_hash"].isNull()) {
        event.ip_hash = json["ip_hash"].asString();
    }
    if (json.isMember("country_code") && !json["country_code"].isNull()) {
        event.country_code = json["country_code"].asString();
    }
    if (json.isMember("region_code") && !json["region_code"].isNull()) {
        event.region_code = json["region_code"].asString();
    }
    if (json.isMember("city") && !json["city"].isNull()) {
        event.city = json["city"].asString();
    }

    // Event data
    if (json.isMember("data")) {
        event.data = json["data"];
    }

    // Timestamp
    if (json.isMember("timestamp")) {
        event.timestamp = parseTimestamp(json["timestamp"].asString());
    } else if (json.isMember("timestamp_epoch")) {
        auto epoch = json["timestamp_epoch"].asInt64();
        event.timestamp = std::chrono::system_clock::from_time_t(
            static_cast<std::time_t>(epoch));
    } else {
        event.setTimestampNow();
    }

    if (json.isMember("received_at") && !json["received_at"].isNull()) {
        event.received_at = parseTimestamp(json["received_at"].asString());
    }

    event.is_processed = json.get("is_processed", false).asBool();

    return event;
}

AnalyticsEvent AnalyticsEvent::fromDbRow(const pqxx::row& row) {
    AnalyticsEvent event;

    try {
        event.id = row["id"].as<std::string>();

        auto type_opt = analyticsEventTypeFromString(row["event_type"].as<std::string>());
        if (type_opt) {
            event.event_type = *type_opt;
        } else {
            event.event_type = AnalyticsEventType::PAGE_VIEW;
        }

        event.entity_type = row["entity_type"].as<std::string>("");
        event.entity_id = row["entity_id"].as<std::string>("");

        if (!row["user_id"].is_null()) {
            event.user_id = row["user_id"].as<std::string>();
        }

        event.session_id = row["session_id"].as<std::string>("");

        if (!row["visitor_id"].is_null()) {
            event.visitor_id = row["visitor_id"].as<std::string>();
        }

        event.source = row["source"].as<std::string>("web");

        if (!row["referrer"].is_null()) {
            event.referrer = row["referrer"].as<std::string>();
        }

        if (!row["utm_campaign"].is_null()) {
            event.utm_campaign = row["utm_campaign"].as<std::string>();
        }

        if (!row["utm_source"].is_null()) {
            event.utm_source = row["utm_source"].as<std::string>();
        }

        if (!row["utm_medium"].is_null()) {
            event.utm_medium = row["utm_medium"].as<std::string>();
        }

        if (!row["page_url"].is_null()) {
            event.page_url = row["page_url"].as<std::string>();
        }

        if (!row["device_type"].is_null()) {
            event.device_type = row["device_type"].as<std::string>();
        }

        if (!row["browser"].is_null()) {
            event.browser = row["browser"].as<std::string>();
        }

        if (!row["os"].is_null()) {
            event.os = row["os"].as<std::string>();
        }

        if (!row["ip_hash"].is_null()) {
            event.ip_hash = row["ip_hash"].as<std::string>();
        }

        if (!row["country_code"].is_null()) {
            event.country_code = row["country_code"].as<std::string>();
        }

        if (!row["region_code"].is_null()) {
            event.region_code = row["region_code"].as<std::string>();
        }

        if (!row["city"].is_null()) {
            event.city = row["city"].as<std::string>();
        }

        // Parse JSON data field
        if (!row["data"].is_null()) {
            std::string data_str = row["data"].as<std::string>();
            Json::CharReaderBuilder builder;
            std::istringstream stream(data_str);
            std::string errors;
            if (!Json::parseFromStream(builder, stream, &event.data, &errors)) {
                event.data = Json::Value(Json::objectValue);
            }
        }

        // Parse timestamp
        std::string ts_str = row["timestamp"].as<std::string>();
        event.timestamp = parseTimestamp(ts_str);

        if (!row["received_at"].is_null()) {
            std::string recv_str = row["received_at"].as<std::string>();
            event.received_at = parseTimestamp(recv_str);
        }

        event.is_processed = row["is_processed"].as<bool>(false);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to parse AnalyticsEvent from DB row: " + std::string(e.what()),
            {{"error", e.what()}}
        );
        throw;
    }

    return event;
}

std::string AnalyticsEvent::toJsonString() const {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, toJson());
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

std::string AnalyticsEvent::getCategory() const {
    return getEventTypeCategory(event_type);
}

bool AnalyticsEvent::isConversion() const {
    return wtl::analytics::isConversionEvent(event_type);
}

bool AnalyticsEvent::isSocialEvent() const {
    return wtl::analytics::isSocialEvent(event_type);
}

std::string AnalyticsEvent::getSocialPlatform() const {
    return wtl::analytics::getSocialPlatform(event_type);
}

std::string AnalyticsEvent::getTimestampISO() const {
    return formatTimestampISO(timestamp);
}

int64_t AnalyticsEvent::getTimestampEpoch() const {
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    return static_cast<int64_t>(epoch);
}

void AnalyticsEvent::setTimestampNow() {
    timestamp = std::chrono::system_clock::now();
}

void AnalyticsEvent::generateId() {
    id = generateUUID();
}

bool AnalyticsEvent::isValid() const {
    return getValidationErrors().empty();
}

std::vector<std::string> AnalyticsEvent::getValidationErrors() const {
    std::vector<std::string> errors;

    if (id.empty()) {
        errors.push_back("Event ID is required");
    }

    if (session_id.empty() && !user_id && !visitor_id) {
        errors.push_back("At least one of session_id, user_id, or visitor_id is required");
    }

    if (source.empty()) {
        errors.push_back("Source is required");
    }

    return errors;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::chrono::system_clock::time_point parseTimestamp(const std::string& str) {
    // Try parsing as ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ or YYYY-MM-DD HH:MM:SS
    std::tm tm = {};
    std::istringstream ss(str);

    // Try ISO 8601 with T separator
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (!ss.fail()) {
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    // Try with space separator
    ss.clear();
    ss.str(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (!ss.fail()) {
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    // Try parsing as Unix epoch
    try {
        int64_t epoch = std::stoll(str);
        return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(epoch));
    } catch (...) {
        // Fall back to current time
        return std::chrono::system_clock::now();
    }
}

std::string formatTimestampISO(const std::chrono::system_clock::time_point& tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_val;

#ifdef _WIN32
    gmtime_s(&tm_val, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm_val);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string generateUUID() {
    // Generate a UUID v4 using random numbers
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t high = dis(gen);
    uint64_t low = dis(gen);

    // Set version to 4 (random)
    high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;

    // Set variant to DCE 1.1
    low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    // Format as UUID string
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << ((high >> 32) & 0xFFFFFFFF) << "-";
    ss << std::setw(4) << ((high >> 16) & 0xFFFF) << "-";
    ss << std::setw(4) << (high & 0xFFFF) << "-";
    ss << std::setw(4) << ((low >> 48) & 0xFFFF) << "-";
    ss << std::setw(12) << (low & 0xFFFFFFFFFFFFULL);

    return ss.str();
}

} // namespace wtl::analytics
