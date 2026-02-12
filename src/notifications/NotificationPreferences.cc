/**
 * @file NotificationPreferences.cc
 * @brief Implementation of UserNotificationPreferences
 * @see NotificationPreferences.h for documentation
 */

#include "notifications/NotificationPreferences.h"

// Standard library includes
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

// Project includes
#include "core/debug/ErrorCapture.h"

namespace wtl::notifications {

// ============================================================================
// CONSTANTS
// ============================================================================

// Earth's radius in miles for distance calculation
constexpr double EARTH_RADIUS_MILES = 3959.0;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UserNotificationPreferences::UserNotificationPreferences()
    : receive_critical_alerts(true)
    , critical_alert_radius_miles(50.0)
    , critical_alerts_any_dog(true)
    , receive_urgency_alerts(true)
    , urgency_alert_radius_miles(100.0)
    , receive_match_alerts(true)
    , min_match_score(70)
    , receive_perfect_match_only(false)
    , receive_foster_alerts(true)
    , receive_foster_urgent_alerts(true)
    , receive_foster_followup(true)
    , receive_dog_updates(true)
    , receive_success_stories(true)
    , receive_blog_posts(true)
    , receive_tips(false) // Disabled by default - users can opt in
    , enable_push(true)
    , enable_email(true)
    , enable_sms(false)   // Disabled by default - users must opt in
    , enable_websocket(true)
    , quiet_hours_enabled(true)
    , quiet_hours_start(22) // 10 PM
    , quiet_hours_end(7)    // 7 AM
    , timezone("America/Chicago")
    , max_notifications_per_day(20)
    , max_emails_per_day(5)
    , max_sms_per_week(3)
    , created_at(std::chrono::system_clock::now())
    , updated_at(std::chrono::system_clock::now()) {
}

// ============================================================================
// JSON CONVERSION
// ============================================================================

Json::Value UserNotificationPreferences::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    json["user_id"] = user_id;

    // Critical alerts
    json["receive_critical_alerts"] = receive_critical_alerts;
    json["critical_alert_radius_miles"] = critical_alert_radius_miles;
    json["critical_alerts_any_dog"] = critical_alerts_any_dog;

    // Urgency alerts
    json["receive_urgency_alerts"] = receive_urgency_alerts;
    json["urgency_alert_radius_miles"] = urgency_alert_radius_miles;

    // Match alerts
    json["receive_match_alerts"] = receive_match_alerts;
    json["min_match_score"] = min_match_score;
    json["receive_perfect_match_only"] = receive_perfect_match_only;

    // Foster alerts
    json["receive_foster_alerts"] = receive_foster_alerts;
    json["receive_foster_urgent_alerts"] = receive_foster_urgent_alerts;
    json["receive_foster_followup"] = receive_foster_followup;

    // Dog updates
    json["receive_dog_updates"] = receive_dog_updates;
    json["receive_success_stories"] = receive_success_stories;

    // Content
    json["receive_blog_posts"] = receive_blog_posts;
    json["receive_tips"] = receive_tips;

    // Channels
    json["enable_push"] = enable_push;
    json["enable_email"] = enable_email;
    json["enable_sms"] = enable_sms;
    json["enable_websocket"] = enable_websocket;

    // Quiet hours
    json["quiet_hours_enabled"] = quiet_hours_enabled;
    json["quiet_hours_start"] = quiet_hours_start;
    json["quiet_hours_end"] = quiet_hours_end;
    json["timezone"] = timezone;

    // Frequency limits
    json["max_notifications_per_day"] = max_notifications_per_day;
    json["max_emails_per_day"] = max_emails_per_day;
    json["max_sms_per_week"] = max_sms_per_week;

    // Location
    if (home_latitude.has_value()) {
        json["home_latitude"] = home_latitude.value();
    }
    if (home_longitude.has_value()) {
        json["home_longitude"] = home_longitude.value();
    }
    if (home_zip.has_value()) {
        json["home_zip"] = home_zip.value();
    }

    Json::Value states_json(Json::arrayValue);
    for (const auto& state : preferred_states) {
        states_json.append(state);
    }
    json["preferred_states"] = states_json;

    // Timestamps
    auto created_time = std::chrono::system_clock::to_time_t(created_at);
    json["created_at"] = static_cast<Json::Int64>(created_time);
    auto updated_time = std::chrono::system_clock::to_time_t(updated_at);
    json["updated_at"] = static_cast<Json::Int64>(updated_time);

    return json;
}

UserNotificationPreferences UserNotificationPreferences::fromJson(const Json::Value& json) {
    UserNotificationPreferences prefs;

    // Identity
    prefs.id = json.get("id", "").asString();
    prefs.user_id = json.get("user_id", "").asString();

    // Critical alerts
    prefs.receive_critical_alerts = json.get("receive_critical_alerts", true).asBool();
    prefs.critical_alert_radius_miles = json.get("critical_alert_radius_miles", 50.0).asDouble();
    prefs.critical_alerts_any_dog = json.get("critical_alerts_any_dog", true).asBool();

    // Urgency alerts
    prefs.receive_urgency_alerts = json.get("receive_urgency_alerts", true).asBool();
    prefs.urgency_alert_radius_miles = json.get("urgency_alert_radius_miles", 100.0).asDouble();

    // Match alerts
    prefs.receive_match_alerts = json.get("receive_match_alerts", true).asBool();
    prefs.min_match_score = json.get("min_match_score", 70).asInt();
    prefs.receive_perfect_match_only = json.get("receive_perfect_match_only", false).asBool();

    // Foster alerts
    prefs.receive_foster_alerts = json.get("receive_foster_alerts", true).asBool();
    prefs.receive_foster_urgent_alerts = json.get("receive_foster_urgent_alerts", true).asBool();
    prefs.receive_foster_followup = json.get("receive_foster_followup", true).asBool();

    // Dog updates
    prefs.receive_dog_updates = json.get("receive_dog_updates", true).asBool();
    prefs.receive_success_stories = json.get("receive_success_stories", true).asBool();

    // Content
    prefs.receive_blog_posts = json.get("receive_blog_posts", true).asBool();
    prefs.receive_tips = json.get("receive_tips", false).asBool();

    // Channels
    prefs.enable_push = json.get("enable_push", true).asBool();
    prefs.enable_email = json.get("enable_email", true).asBool();
    prefs.enable_sms = json.get("enable_sms", false).asBool();
    prefs.enable_websocket = json.get("enable_websocket", true).asBool();

    // Quiet hours
    prefs.quiet_hours_enabled = json.get("quiet_hours_enabled", true).asBool();
    prefs.quiet_hours_start = json.get("quiet_hours_start", 22).asInt();
    prefs.quiet_hours_end = json.get("quiet_hours_end", 7).asInt();
    prefs.timezone = json.get("timezone", "America/Chicago").asString();

    // Frequency limits
    prefs.max_notifications_per_day = json.get("max_notifications_per_day", 20).asInt();
    prefs.max_emails_per_day = json.get("max_emails_per_day", 5).asInt();
    prefs.max_sms_per_week = json.get("max_sms_per_week", 3).asInt();

    // Location
    if (json.isMember("home_latitude") && !json["home_latitude"].isNull()) {
        prefs.home_latitude = json["home_latitude"].asDouble();
    }
    if (json.isMember("home_longitude") && !json["home_longitude"].isNull()) {
        prefs.home_longitude = json["home_longitude"].asDouble();
    }
    if (json.isMember("home_zip") && !json["home_zip"].isNull()) {
        prefs.home_zip = json["home_zip"].asString();
    }

    if (json.isMember("preferred_states") && json["preferred_states"].isArray()) {
        for (const auto& state : json["preferred_states"]) {
            prefs.preferred_states.push_back(state.asString());
        }
    }

    // Timestamps
    if (json.isMember("created_at") && !json["created_at"].isNull()) {
        auto created_time = static_cast<std::time_t>(json["created_at"].asInt64());
        prefs.created_at = std::chrono::system_clock::from_time_t(created_time);
    }
    if (json.isMember("updated_at") && !json["updated_at"].isNull()) {
        auto updated_time = static_cast<std::time_t>(json["updated_at"].asInt64());
        prefs.updated_at = std::chrono::system_clock::from_time_t(updated_time);
    }

    return prefs;
}

// ============================================================================
// DATABASE CONVERSION
// ============================================================================

UserNotificationPreferences UserNotificationPreferences::fromDbRow(const pqxx::row& row) {
    UserNotificationPreferences prefs;

    try {
        // Identity
        prefs.id = row["id"].as<std::string>();
        prefs.user_id = row["user_id"].as<std::string>();

        // Critical alerts
        prefs.receive_critical_alerts = row["receive_critical_alerts"].as<bool>();
        prefs.critical_alert_radius_miles = row["critical_alert_radius_miles"].as<double>();
        prefs.critical_alerts_any_dog = row["critical_alerts_any_dog"].as<bool>();

        // Urgency alerts
        prefs.receive_urgency_alerts = row["receive_urgency_alerts"].as<bool>();
        prefs.urgency_alert_radius_miles = row["urgency_alert_radius_miles"].as<double>();

        // Match alerts
        prefs.receive_match_alerts = row["receive_match_alerts"].as<bool>();
        prefs.min_match_score = row["min_match_score"].as<int>();
        prefs.receive_perfect_match_only = row["receive_perfect_match_only"].as<bool>();

        // Foster alerts
        prefs.receive_foster_alerts = row["receive_foster_alerts"].as<bool>();
        prefs.receive_foster_urgent_alerts = row["receive_foster_urgent_alerts"].as<bool>();
        prefs.receive_foster_followup = row["receive_foster_followup"].as<bool>();

        // Dog updates
        prefs.receive_dog_updates = row["receive_dog_updates"].as<bool>();
        prefs.receive_success_stories = row["receive_success_stories"].as<bool>();

        // Content
        prefs.receive_blog_posts = row["receive_blog_posts"].as<bool>();
        prefs.receive_tips = row["receive_tips"].as<bool>();

        // Channels
        prefs.enable_push = row["enable_push"].as<bool>();
        prefs.enable_email = row["enable_email"].as<bool>();
        prefs.enable_sms = row["enable_sms"].as<bool>();
        prefs.enable_websocket = row["enable_websocket"].as<bool>();

        // Quiet hours
        prefs.quiet_hours_enabled = row["quiet_hours_enabled"].as<bool>();
        prefs.quiet_hours_start = row["quiet_hours_start"].as<int>();
        prefs.quiet_hours_end = row["quiet_hours_end"].as<int>();
        prefs.timezone = row["timezone"].as<std::string>();

        // Frequency limits
        prefs.max_notifications_per_day = row["max_notifications_per_day"].as<int>();
        prefs.max_emails_per_day = row["max_emails_per_day"].as<int>();
        prefs.max_sms_per_week = row["max_sms_per_week"].as<int>();

        // Location
        if (!row["home_latitude"].is_null()) {
            prefs.home_latitude = row["home_latitude"].as<double>();
        }
        if (!row["home_longitude"].is_null()) {
            prefs.home_longitude = row["home_longitude"].as<double>();
        }
        if (!row["home_zip"].is_null()) {
            prefs.home_zip = row["home_zip"].as<std::string>();
        }

        // Preferred states (stored as PostgreSQL text array)
        if (!row["preferred_states"].is_null()) {
            std::string states_str = row["preferred_states"].as<std::string>();
            // Parse PostgreSQL array format: {TX,CA,FL}
            if (states_str.length() > 2 && states_str[0] == '{') {
                states_str = states_str.substr(1, states_str.length() - 2);
                std::istringstream ss(states_str);
                std::string state;
                while (std::getline(ss, state, ',')) {
                    prefs.preferred_states.push_back(state);
                }
            }
        }

        // Timestamps
        prefs.created_at = parseTimestamp(row["created_at"].as<std::string>());
        prefs.updated_at = parseTimestamp(row["updated_at"].as<std::string>());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to parse notification preferences from database row: " + std::string(e.what()),
            {{"user_id", prefs.user_id}}
        );
        throw;
    }

    return prefs;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool UserNotificationPreferences::isTypeEnabled(NotificationType type) const {
    switch (type) {
        case NotificationType::CRITICAL_ALERT:
            return receive_critical_alerts;
        case NotificationType::HIGH_URGENCY:
            return receive_urgency_alerts;
        case NotificationType::FOSTER_NEEDED_URGENT:
            return receive_foster_urgent_alerts;
        case NotificationType::PERFECT_MATCH:
            return receive_match_alerts;
        case NotificationType::GOOD_MATCH:
            return receive_match_alerts && !receive_perfect_match_only;
        case NotificationType::DOG_UPDATE:
            return receive_dog_updates;
        case NotificationType::FOSTER_FOLLOWUP:
            return receive_foster_followup;
        case NotificationType::SUCCESS_STORY:
            return receive_success_stories;
        case NotificationType::NEW_BLOG_POST:
            return receive_blog_posts;
        case NotificationType::TIP_OF_THE_DAY:
            return receive_tips;
        case NotificationType::SYSTEM_ALERT:
            return true; // System alerts always enabled
        default:
            return true;
    }
}

bool UserNotificationPreferences::isChannelEnabled(NotificationChannel channel) const {
    switch (channel) {
        case NotificationChannel::PUSH:
            return enable_push;
        case NotificationChannel::EMAIL:
            return enable_email;
        case NotificationChannel::SMS:
            return enable_sms;
        case NotificationChannel::WEBSOCKET:
            return enable_websocket;
        default:
            return false;
    }
}

bool UserNotificationPreferences::isQuietHoursNow() const {
    if (!quiet_hours_enabled) {
        return false;
    }

    int current_hour = getCurrentHourInTimezone();

    // Handle overnight quiet hours (e.g., 22:00 to 07:00)
    if (quiet_hours_start > quiet_hours_end) {
        // Quiet hours span midnight
        return current_hour >= quiet_hours_start || current_hour < quiet_hours_end;
    } else {
        // Quiet hours within same day
        return current_hour >= quiet_hours_start && current_hour < quiet_hours_end;
    }
}

bool UserNotificationPreferences::shouldDeliverNow(NotificationType type) const {
    // Critical alerts always deliver immediately
    if (bypassesQuietHours(type)) {
        return true;
    }

    // Check if in quiet hours
    if (isQuietHoursNow()) {
        return false;
    }

    // Check if notification type is enabled
    return isTypeEnabled(type);
}

bool UserNotificationPreferences::isWithinRadius(double latitude, double longitude, NotificationType type) const {
    // If user hasn't set location, allow all
    if (!home_latitude.has_value() || !home_longitude.has_value()) {
        return true;
    }

    // Determine which radius to use based on notification type
    double radius_miles;
    if (type == NotificationType::CRITICAL_ALERT) {
        radius_miles = critical_alert_radius_miles;
    } else if (type == NotificationType::HIGH_URGENCY || type == NotificationType::FOSTER_NEEDED_URGENT) {
        radius_miles = urgency_alert_radius_miles;
    } else {
        // Other types don't have radius limits
        return true;
    }

    // If radius is 0, alert for everywhere
    if (radius_miles <= 0) {
        return true;
    }

    double distance = calculateDistance(
        home_latitude.value(), home_longitude.value(),
        latitude, longitude
    );

    return distance <= radius_miles;
}

NotificationChannel UserNotificationPreferences::getBestChannel(NotificationType type) const {
    // Critical alerts: prefer push, then SMS, then email
    if (type == NotificationType::CRITICAL_ALERT) {
        if (enable_push) return NotificationChannel::PUSH;
        if (enable_sms) return NotificationChannel::SMS;
        if (enable_email) return NotificationChannel::EMAIL;
    }

    // High urgency: prefer push, then email
    if (type == NotificationType::HIGH_URGENCY || type == NotificationType::FOSTER_NEEDED_URGENT) {
        if (enable_push) return NotificationChannel::PUSH;
        if (enable_email) return NotificationChannel::EMAIL;
    }

    // Everything else: prefer push
    if (enable_push) return NotificationChannel::PUSH;
    if (enable_email) return NotificationChannel::EMAIL;

    return NotificationChannel::WEBSOCKET;
}

std::vector<NotificationChannel> UserNotificationPreferences::getEnabledChannels(NotificationType type) const {
    std::vector<NotificationChannel> channels;

    // WebSocket is always added for real-time updates (if enabled)
    if (enable_websocket) {
        channels.push_back(NotificationChannel::WEBSOCKET);
    }

    // Critical alerts can use all channels
    if (type == NotificationType::CRITICAL_ALERT) {
        if (enable_push) channels.push_back(NotificationChannel::PUSH);
        if (enable_sms) channels.push_back(NotificationChannel::SMS);
        if (enable_email) channels.push_back(NotificationChannel::EMAIL);
        return channels;
    }

    // High urgency: push and email
    if (type == NotificationType::HIGH_URGENCY || type == NotificationType::FOSTER_NEEDED_URGENT) {
        if (enable_push) channels.push_back(NotificationChannel::PUSH);
        if (enable_email) channels.push_back(NotificationChannel::EMAIL);
        return channels;
    }

    // Everything else: push preferred, email for important things
    if (enable_push) channels.push_back(NotificationChannel::PUSH);

    // Email for matches and updates
    if (type == NotificationType::PERFECT_MATCH ||
        type == NotificationType::GOOD_MATCH ||
        type == NotificationType::DOG_UPDATE) {
        if (enable_email) channels.push_back(NotificationChannel::EMAIL);
    }

    return channels;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::chrono::system_clock::time_point UserNotificationPreferences::parseTimestamp(const std::string& timestamp) {
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

double UserNotificationPreferences::calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    // Haversine formula for distance between two points on Earth
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;

    double a = std::sin(delta_lat / 2) * std::sin(delta_lat / 2) +
               std::cos(lat1_rad) * std::cos(lat2_rad) *
               std::sin(delta_lon / 2) * std::sin(delta_lon / 2);

    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return EARTH_RADIUS_MILES * c;
}

int UserNotificationPreferences::getCurrentHourInTimezone() const {
    // For simplicity, we use UTC time offset approximation
    // In production, use a proper timezone library like date.h or ICU

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm* utc_tm = std::gmtime(&time_t_now);

    int utc_hour = utc_tm->tm_hour;

    // Common US timezone offsets (simplified)
    int offset = 0;
    if (timezone == "America/New_York" || timezone == "EST" || timezone == "EDT") {
        offset = -5; // EST (not accounting for DST)
    } else if (timezone == "America/Chicago" || timezone == "CST" || timezone == "CDT") {
        offset = -6; // CST
    } else if (timezone == "America/Denver" || timezone == "MST" || timezone == "MDT") {
        offset = -7; // MST
    } else if (timezone == "America/Los_Angeles" || timezone == "PST" || timezone == "PDT") {
        offset = -8; // PST
    }

    int local_hour = (utc_hour + offset + 24) % 24;
    return local_hour;
}

// ============================================================================
// DEVICE TOKEN
// ============================================================================

Json::Value DeviceToken::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["user_id"] = user_id;
    json["token"] = token;
    json["platform"] = platform;
    json["device_name"] = device_name;
    json["is_active"] = is_active;

    auto registered_time = std::chrono::system_clock::to_time_t(registered_at);
    json["registered_at"] = static_cast<Json::Int64>(registered_time);

    if (last_used_at.has_value()) {
        auto last_used_time = std::chrono::system_clock::to_time_t(last_used_at.value());
        json["last_used_at"] = static_cast<Json::Int64>(last_used_time);
    }

    return json;
}

DeviceToken DeviceToken::fromJson(const Json::Value& json) {
    DeviceToken token_obj;
    token_obj.id = json.get("id", "").asString();
    token_obj.user_id = json.get("user_id", "").asString();
    token_obj.token = json.get("token", "").asString();
    token_obj.platform = json.get("platform", "").asString();
    token_obj.device_name = json.get("device_name", "").asString();
    token_obj.is_active = json.get("is_active", true).asBool();

    if (json.isMember("registered_at") && !json["registered_at"].isNull()) {
        auto registered_time = static_cast<std::time_t>(json["registered_at"].asInt64());
        token_obj.registered_at = std::chrono::system_clock::from_time_t(registered_time);
    } else {
        token_obj.registered_at = std::chrono::system_clock::now();
    }

    if (json.isMember("last_used_at") && !json["last_used_at"].isNull()) {
        auto last_used_time = static_cast<std::time_t>(json["last_used_at"].asInt64());
        token_obj.last_used_at = std::chrono::system_clock::from_time_t(last_used_time);
    }

    return token_obj;
}

DeviceToken DeviceToken::fromDbRow(const pqxx::row& row) {
    DeviceToken token_obj;
    token_obj.id = row["id"].as<std::string>();
    token_obj.user_id = row["user_id"].as<std::string>();
    token_obj.token = row["token"].as<std::string>();
    token_obj.platform = row["platform"].as<std::string>();
    token_obj.device_name = row["device_name"].as<std::string>();
    token_obj.is_active = row["is_active"].as<bool>();

    std::string registered_str = row["registered_at"].as<std::string>();
    token_obj.registered_at = UserNotificationPreferences::parseTimestamp(registered_str);

    if (!row["last_used_at"].is_null()) {
        std::string last_used_str = row["last_used_at"].as<std::string>();
        token_obj.last_used_at = UserNotificationPreferences::parseTimestamp(last_used_str);
    }

    return token_obj;
}

} // namespace wtl::notifications
