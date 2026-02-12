/**
 * @file WebSocketMessage.cc
 * @brief Implementation of WebSocket message types and structures
 * @see WebSocketMessage.h for documentation
 */

#include "core/websocket/WebSocketMessage.h"

#include <chrono>
#include <sstream>
#include <stdexcept>

namespace wtl::core::websocket {

// ============================================================================
// MESSAGE TYPE CONVERSION
// ============================================================================

std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::SUBSCRIBE_DOG:
            return "SUBSCRIBE_DOG";
        case MessageType::UNSUBSCRIBE_DOG:
            return "UNSUBSCRIBE_DOG";
        case MessageType::SUBSCRIBE_SHELTER:
            return "SUBSCRIBE_SHELTER";
        case MessageType::UNSUBSCRIBE_SHELTER:
            return "UNSUBSCRIBE_SHELTER";
        case MessageType::SUBSCRIBE_URGENT:
            return "SUBSCRIBE_URGENT";
        case MessageType::WAIT_TIME_UPDATE:
            return "WAIT_TIME_UPDATE";
        case MessageType::COUNTDOWN_UPDATE:
            return "COUNTDOWN_UPDATE";
        case MessageType::DOG_STATUS_CHANGE:
            return "DOG_STATUS_CHANGE";
        case MessageType::URGENT_ALERT:
            return "URGENT_ALERT";
        case MessageType::CONNECTION_ACK:
            return "CONNECTION_ACK";
        case MessageType::ERROR:
        default:
            return "ERROR";
    }
}

MessageType stringToMessageType(const std::string& str) {
    if (str == "SUBSCRIBE_DOG") return MessageType::SUBSCRIBE_DOG;
    if (str == "UNSUBSCRIBE_DOG") return MessageType::UNSUBSCRIBE_DOG;
    if (str == "SUBSCRIBE_SHELTER") return MessageType::SUBSCRIBE_SHELTER;
    if (str == "UNSUBSCRIBE_SHELTER") return MessageType::UNSUBSCRIBE_SHELTER;
    if (str == "SUBSCRIBE_URGENT") return MessageType::SUBSCRIBE_URGENT;
    if (str == "WAIT_TIME_UPDATE") return MessageType::WAIT_TIME_UPDATE;
    if (str == "COUNTDOWN_UPDATE") return MessageType::COUNTDOWN_UPDATE;
    if (str == "DOG_STATUS_CHANGE") return MessageType::DOG_STATUS_CHANGE;
    if (str == "URGENT_ALERT") return MessageType::URGENT_ALERT;
    if (str == "CONNECTION_ACK") return MessageType::CONNECTION_ACK;
    return MessageType::ERROR;
}

// ============================================================================
// SERIALIZATION
// ============================================================================

std::string WebSocketMessage::toJson() const {
    Json::Value root;
    root["type"] = messageTypeToString(type);

    if (!dog_id.empty()) {
        root["dog_id"] = dog_id;
    }

    if (!shelter_id.empty()) {
        root["shelter_id"] = shelter_id;
    }

    if (!payload.isNull()) {
        root["payload"] = payload;
    }

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";  // Compact output for WebSocket
    return Json::writeString(writer, root);
}

WebSocketMessage WebSocketMessage::fromJson(const std::string& json) {
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errors;

    std::istringstream stream(json);
    if (!Json::parseFromStream(reader, stream, &root, &errors)) {
        throw std::runtime_error("Failed to parse WebSocket message: " + errors);
    }

    WebSocketMessage msg;
    msg.type = stringToMessageType(root.get("type", "ERROR").asString());
    msg.dog_id = root.get("dog_id", "").asString();
    msg.shelter_id = root.get("shelter_id", "").asString();

    if (root.isMember("payload")) {
        msg.payload = root["payload"];
    }

    return msg;
}

// ============================================================================
// FACTORY METHODS
// ============================================================================

WebSocketMessage WebSocketMessage::createError(const std::string& error_message) {
    WebSocketMessage msg;
    msg.type = MessageType::ERROR;
    msg.payload["message"] = error_message;
    msg.payload["timestamp"] = static_cast<Json::Int64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    return msg;
}

WebSocketMessage WebSocketMessage::createConnectionAck(const std::string& connection_id) {
    WebSocketMessage msg;
    msg.type = MessageType::CONNECTION_ACK;
    msg.payload["connection_id"] = connection_id;
    msg.payload["status"] = "connected";
    msg.payload["timestamp"] = static_cast<Json::Int64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    return msg;
}

WebSocketMessage WebSocketMessage::createWaitTimeUpdate(
    const std::string& dog_id,
    const Json::Value& wait_time_data
) {
    WebSocketMessage msg;
    msg.type = MessageType::WAIT_TIME_UPDATE;
    msg.dog_id = dog_id;
    msg.payload = wait_time_data;
    return msg;
}

WebSocketMessage WebSocketMessage::createCountdownUpdate(
    const std::string& dog_id,
    const Json::Value& countdown_data
) {
    WebSocketMessage msg;
    msg.type = MessageType::COUNTDOWN_UPDATE;
    msg.dog_id = dog_id;
    msg.payload = countdown_data;
    return msg;
}

WebSocketMessage WebSocketMessage::createUrgentAlert(
    const std::string& dog_id,
    const std::string& urgency_level,
    const Json::Value& alert_data
) {
    WebSocketMessage msg;
    msg.type = MessageType::URGENT_ALERT;
    msg.dog_id = dog_id;
    msg.payload = alert_data;
    msg.payload["urgency_level"] = urgency_level;
    msg.payload["timestamp"] = static_cast<Json::Int64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    return msg;
}

WebSocketMessage WebSocketMessage::createDogStatusChange(
    const std::string& dog_id,
    const std::string& new_status,
    const Json::Value& status_data
) {
    WebSocketMessage msg;
    msg.type = MessageType::DOG_STATUS_CHANGE;
    msg.dog_id = dog_id;
    msg.payload = status_data;
    msg.payload["new_status"] = new_status;
    msg.payload["timestamp"] = static_cast<Json::Int64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    return msg;
}

} // namespace wtl::core::websocket
