/**
 * @file Notification.cc
 * @brief Implementation of Notification data model
 * @see Notification.h for documentation
 */

#include "notifications/Notification.h"

// Standard library includes
#include <iomanip>
#include <sstream>

// Project includes
#include "core/debug/ErrorCapture.h"

namespace wtl::notifications {

// ============================================================================
// JSON CONVERSION
// ============================================================================

Json::Value Notification::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    json["user_id"] = user_id;
    json["type"] = notificationTypeToString(type);

    // Content
    json["title"] = title;
    json["body"] = body;

    if (image_url.has_value()) {
        json["image_url"] = image_url.value();
    }

    if (action_url.has_value()) {
        json["action_url"] = action_url.value();
    }

    // Related entities
    if (dog_id.has_value()) {
        json["dog_id"] = dog_id.value();
    }

    if (shelter_id.has_value()) {
        json["shelter_id"] = shelter_id.value();
    }

    if (foster_id.has_value()) {
        json["foster_id"] = foster_id.value();
    }

    // Data payload
    if (!data.isNull()) {
        json["data"] = data;
    }

    // Delivery status
    Json::Value channels_json(Json::arrayValue);
    for (const auto& channel_status : channels_sent) {
        channels_json.append(channel_status.toJson());
    }
    json["channels_sent"] = channels_json;

    // Timestamps
    auto created_time = std::chrono::system_clock::to_time_t(created_at);
    json["created_at"] = static_cast<Json::Int64>(created_time);

    if (scheduled_for.has_value()) {
        auto scheduled_time = std::chrono::system_clock::to_time_t(scheduled_for.value());
        json["scheduled_for"] = static_cast<Json::Int64>(scheduled_time);
    }

    json["is_sent"] = is_sent;
    json["retry_count"] = retry_count;

    // Engagement
    if (opened_at.has_value()) {
        auto opened_time = std::chrono::system_clock::to_time_t(opened_at.value());
        json["opened_at"] = static_cast<Json::Int64>(opened_time);
    }

    if (clicked_at.has_value()) {
        auto clicked_time = std::chrono::system_clock::to_time_t(clicked_at.value());
        json["clicked_at"] = static_cast<Json::Int64>(clicked_time);
    }

    if (action_taken.has_value()) {
        json["action_taken"] = action_taken.value();
    }

    json["is_read"] = is_read;

    // Additional display fields
    json["priority"] = getPriority();
    json["icon"] = getNotificationIcon(type);
    json["color"] = getNotificationColor(type);

    return json;
}

Notification Notification::fromJson(const Json::Value& json) {
    Notification notif;

    // Identity
    notif.id = json.get("id", "").asString();
    notif.user_id = json.get("user_id", "").asString();
    notif.type = stringToNotificationType(json.get("type", "system_alert").asString());

    // Content
    notif.title = json.get("title", "").asString();
    notif.body = json.get("body", "").asString();

    if (json.isMember("image_url") && !json["image_url"].isNull()) {
        notif.image_url = json["image_url"].asString();
    }

    if (json.isMember("action_url") && !json["action_url"].isNull()) {
        notif.action_url = json["action_url"].asString();
    }

    // Related entities
    if (json.isMember("dog_id") && !json["dog_id"].isNull()) {
        notif.dog_id = json["dog_id"].asString();
    }

    if (json.isMember("shelter_id") && !json["shelter_id"].isNull()) {
        notif.shelter_id = json["shelter_id"].asString();
    }

    if (json.isMember("foster_id") && !json["foster_id"].isNull()) {
        notif.foster_id = json["foster_id"].asString();
    }

    // Data payload
    if (json.isMember("data") && !json["data"].isNull()) {
        notif.data = json["data"];
    }

    // Parse channels_sent array
    if (json.isMember("channels_sent") && json["channels_sent"].isArray()) {
        for (const auto& channel_json : json["channels_sent"]) {
            ChannelDeliveryStatus status;
            status.channel = stringToChannel(channel_json.get("channel", "push").asString());
            status.sent = channel_json.get("sent", false).asBool();
            status.delivered = channel_json.get("delivered", false).asBool();

            if (channel_json.isMember("sent_at") && !channel_json["sent_at"].isNull()) {
                auto sent_time = static_cast<std::time_t>(channel_json["sent_at"].asInt64());
                status.sent_at = std::chrono::system_clock::from_time_t(sent_time);
            }

            if (channel_json.isMember("delivered_at") && !channel_json["delivered_at"].isNull()) {
                auto delivered_time = static_cast<std::time_t>(channel_json["delivered_at"].asInt64());
                status.delivered_at = std::chrono::system_clock::from_time_t(delivered_time);
            }

            if (channel_json.isMember("error_message") && !channel_json["error_message"].isNull()) {
                status.error_message = channel_json["error_message"].asString();
            }

            if (channel_json.isMember("external_id") && !channel_json["external_id"].isNull()) {
                status.external_id = channel_json["external_id"].asString();
            }

            notif.channels_sent.push_back(status);
        }
    }

    // Timestamps
    if (json.isMember("created_at") && !json["created_at"].isNull()) {
        auto created_time = static_cast<std::time_t>(json["created_at"].asInt64());
        notif.created_at = std::chrono::system_clock::from_time_t(created_time);
    } else {
        notif.created_at = std::chrono::system_clock::now();
    }

    if (json.isMember("scheduled_for") && !json["scheduled_for"].isNull()) {
        auto scheduled_time = static_cast<std::time_t>(json["scheduled_for"].asInt64());
        notif.scheduled_for = std::chrono::system_clock::from_time_t(scheduled_time);
    }

    notif.is_sent = json.get("is_sent", false).asBool();
    notif.retry_count = json.get("retry_count", 0).asInt();

    // Engagement
    if (json.isMember("opened_at") && !json["opened_at"].isNull()) {
        auto opened_time = static_cast<std::time_t>(json["opened_at"].asInt64());
        notif.opened_at = std::chrono::system_clock::from_time_t(opened_time);
    }

    if (json.isMember("clicked_at") && !json["clicked_at"].isNull()) {
        auto clicked_time = static_cast<std::time_t>(json["clicked_at"].asInt64());
        notif.clicked_at = std::chrono::system_clock::from_time_t(clicked_time);
    }

    if (json.isMember("action_taken") && !json["action_taken"].isNull()) {
        notif.action_taken = json["action_taken"].asString();
    }

    notif.is_read = json.get("is_read", false).asBool();

    return notif;
}

// ============================================================================
// DATABASE CONVERSION
// ============================================================================

Notification Notification::fromDbRow(const pqxx::row& row) {
    Notification notif;

    try {
        // Identity
        notif.id = row["id"].as<std::string>();
        notif.user_id = row["user_id"].as<std::string>();
        notif.type = stringToNotificationType(row["type"].as<std::string>());

        // Content
        notif.title = row["title"].as<std::string>();
        notif.body = row["body"].as<std::string>();

        if (!row["image_url"].is_null()) {
            notif.image_url = row["image_url"].as<std::string>();
        }

        if (!row["action_url"].is_null()) {
            notif.action_url = row["action_url"].as<std::string>();
        }

        // Related entities
        if (!row["dog_id"].is_null()) {
            notif.dog_id = row["dog_id"].as<std::string>();
        }

        if (!row["shelter_id"].is_null()) {
            notif.shelter_id = row["shelter_id"].as<std::string>();
        }

        if (!row["foster_id"].is_null()) {
            notif.foster_id = row["foster_id"].as<std::string>();
        }

        // Data payload (JSONB)
        if (!row["data"].is_null()) {
            std::string data_str = row["data"].as<std::string>();
            Json::Reader reader;
            reader.parse(data_str, notif.data);
        }

        // Channels sent (JSONB)
        if (!row["channels_sent"].is_null()) {
            notif.channels_sent = parseChannelsJson(row["channels_sent"].as<std::string>());
        }

        // Timestamps
        notif.created_at = parseTimestamp(row["created_at"].as<std::string>());

        if (!row["scheduled_for"].is_null()) {
            notif.scheduled_for = parseTimestamp(row["scheduled_for"].as<std::string>());
        }

        notif.is_sent = row["is_sent"].as<bool>();
        notif.retry_count = row["retry_count"].as<int>();

        // Engagement
        if (!row["opened_at"].is_null()) {
            notif.opened_at = parseTimestamp(row["opened_at"].as<std::string>());
        }

        if (!row["clicked_at"].is_null()) {
            notif.clicked_at = parseTimestamp(row["clicked_at"].as<std::string>());
        }

        if (!row["action_taken"].is_null()) {
            notif.action_taken = row["action_taken"].as<std::string>();
        }

        notif.is_read = row["is_read"].as<bool>();

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to parse notification from database row: " + std::string(e.what()),
            {{"notification_id", notif.id}}
        );
        throw;
    }

    return notif;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool Notification::wasDelivered() const {
    for (const auto& channel : channels_sent) {
        if (channel.delivered) {
            return true;
        }
    }
    return false;
}

std::optional<NotificationChannel> Notification::getDeliveredChannel() const {
    for (const auto& channel : channels_sent) {
        if (channel.delivered) {
            return channel.channel;
        }
    }
    return std::nullopt;
}

bool Notification::isReadyToSend() const {
    if (!scheduled_for.has_value()) {
        return true; // No scheduled time means send immediately
    }

    auto now = std::chrono::system_clock::now();
    return now >= scheduled_for.value();
}

int Notification::getPriority() const {
    return getNotificationPriority(type);
}

void Notification::markSent(NotificationChannel channel, const std::string& external_id) {
    // Find existing channel status or create new one
    for (auto& status : channels_sent) {
        if (status.channel == channel) {
            status.sent = true;
            status.sent_at = std::chrono::system_clock::now();
            if (!external_id.empty()) {
                status.external_id = external_id;
            }
            return;
        }
    }

    // Create new channel status
    ChannelDeliveryStatus status;
    status.channel = channel;
    status.sent = true;
    status.delivered = false;
    status.sent_at = std::chrono::system_clock::now();
    if (!external_id.empty()) {
        status.external_id = external_id;
    }
    channels_sent.push_back(status);

    // Mark notification as sent
    is_sent = true;
}

void Notification::markDelivered(NotificationChannel channel) {
    for (auto& status : channels_sent) {
        if (status.channel == channel) {
            status.delivered = true;
            status.delivered_at = std::chrono::system_clock::now();
            return;
        }
    }
}

void Notification::markFailed(NotificationChannel channel, const std::string& error) {
    for (auto& status : channels_sent) {
        if (status.channel == channel) {
            status.error_message = error;
            return;
        }
    }

    // Create new channel status with error
    ChannelDeliveryStatus status;
    status.channel = channel;
    status.sent = false;
    status.delivered = false;
    status.error_message = error;
    channels_sent.push_back(status);

    retry_count++;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::chrono::system_clock::time_point Notification::parseTimestamp(const std::string& timestamp) {
    std::tm tm = {};
    std::istringstream ss(timestamp);

    // Parse ISO 8601 format: "2024-01-28 12:34:56"
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        // Try alternative format: "2024-01-28T12:34:56"
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string Notification::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::gmtime(&time_t_val);

    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::vector<ChannelDeliveryStatus> Notification::parseChannelsJson(const std::string& json_str) {
    std::vector<ChannelDeliveryStatus> channels;

    Json::Value json;
    Json::Reader reader;
    if (!reader.parse(json_str, json) || !json.isArray()) {
        return channels;
    }

    for (const auto& channel_json : json) {
        ChannelDeliveryStatus status;
        status.channel = stringToChannel(channel_json.get("channel", "push").asString());
        status.sent = channel_json.get("sent", false).asBool();
        status.delivered = channel_json.get("delivered", false).asBool();

        if (channel_json.isMember("sent_at") && !channel_json["sent_at"].isNull()) {
            status.sent_at = parseTimestamp(channel_json["sent_at"].asString());
        }

        if (channel_json.isMember("delivered_at") && !channel_json["delivered_at"].isNull()) {
            status.delivered_at = parseTimestamp(channel_json["delivered_at"].asString());
        }

        if (channel_json.isMember("error_message") && !channel_json["error_message"].isNull()) {
            status.error_message = channel_json["error_message"].asString();
        }

        if (channel_json.isMember("external_id") && !channel_json["external_id"].isNull()) {
            status.external_id = channel_json["external_id"].asString();
        }

        channels.push_back(status);
    }

    return channels;
}

} // namespace wtl::notifications
