/**
 * @file WebSocketMessage.h
 * @brief WebSocket message types and structures for real-time updates
 *
 * PURPOSE:
 * Defines the message protocol for WebSocket communication between server and clients.
 * Handles serialization/deserialization of messages for wait time updates, countdowns,
 * and urgent alerts.
 *
 * USAGE:
 * WebSocketMessage msg;
 * msg.type = MessageType::WAIT_TIME_UPDATE;
 * msg.dog_id = "uuid-here";
 * msg.payload["formatted"] = "02:03:15:08:45:32";
 * std::string json = msg.toJson();
 *
 * DEPENDENCIES:
 * - jsoncpp (Json::Value)
 *
 * MODIFICATION GUIDE:
 * - To add new message types, add to MessageType enum and update toJson/fromJson
 * - Payload structure depends on message type - see INTEGRATION_CONTRACTS.md
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>

// Third-party includes
#include <json/json.h>

namespace wtl::core::websocket {

/**
 * @enum MessageType
 * @brief Types of WebSocket messages for the platform
 *
 * Client -> Server messages are requests for subscriptions
 * Server -> Client messages are updates and alerts
 */
enum class MessageType {
    // Client -> Server
    SUBSCRIBE_DOG,           // Subscribe to dog wait time updates
    UNSUBSCRIBE_DOG,         // Unsubscribe from dog updates
    SUBSCRIBE_SHELTER,       // Subscribe to shelter updates
    UNSUBSCRIBE_SHELTER,     // Unsubscribe from shelter updates
    SUBSCRIBE_URGENT,        // Subscribe to all urgent dogs

    // Server -> Client
    WAIT_TIME_UPDATE,        // Wait time update for subscribed dog
    COUNTDOWN_UPDATE,        // Countdown update for urgent dog
    DOG_STATUS_CHANGE,       // Dog adopted, status changed, etc.
    URGENT_ALERT,            // New critical dog alert
    CONNECTION_ACK,          // Connection acknowledged
    ERROR                    // Error message
};

/**
 * @brief Convert MessageType enum to string representation
 * @param type The message type to convert
 * @return String representation of the message type
 */
std::string messageTypeToString(MessageType type);

/**
 * @brief Convert string to MessageType enum
 * @param str The string representation
 * @return Corresponding MessageType, defaults to ERROR if unknown
 */
MessageType stringToMessageType(const std::string& str);

/**
 * @struct WebSocketMessage
 * @brief Represents a WebSocket message for serialization/deserialization
 *
 * This structure is used for all WebSocket communication.
 * The payload content varies based on message type.
 */
struct WebSocketMessage {
    MessageType type;                // Type of the message
    std::string dog_id;              // Optional, depends on type
    std::string shelter_id;          // Optional, depends on type
    Json::Value payload;             // Message-specific data

    /**
     * @brief Serialize the message to JSON string
     * @return JSON string representation of the message
     *
     * @example
     * WebSocketMessage msg;
     * msg.type = MessageType::WAIT_TIME_UPDATE;
     * std::string json = msg.toJson();
     * // {"type":"WAIT_TIME_UPDATE","dog_id":"...","payload":{...}}
     */
    std::string toJson() const;

    /**
     * @brief Deserialize a JSON string to WebSocketMessage
     * @param json The JSON string to parse
     * @return Parsed WebSocketMessage
     * @throws std::runtime_error if JSON is invalid
     *
     * @example
     * std::string json = R"({"type":"SUBSCRIBE_DOG","dog_id":"uuid"})";
     * auto msg = WebSocketMessage::fromJson(json);
     */
    static WebSocketMessage fromJson(const std::string& json);

    /**
     * @brief Create an error message
     * @param error_message The error description
     * @return WebSocketMessage with ERROR type
     */
    static WebSocketMessage createError(const std::string& error_message);

    /**
     * @brief Create a connection acknowledgment message
     * @param connection_id The assigned connection ID
     * @return WebSocketMessage with CONNECTION_ACK type
     */
    static WebSocketMessage createConnectionAck(const std::string& connection_id);

    /**
     * @brief Create a wait time update message
     * @param dog_id The dog's ID
     * @param wait_time_data JSON with years, months, days, hours, minutes, seconds, formatted
     * @return WebSocketMessage with WAIT_TIME_UPDATE type
     */
    static WebSocketMessage createWaitTimeUpdate(const std::string& dog_id,
                                                  const Json::Value& wait_time_data);

    /**
     * @brief Create a countdown update message
     * @param dog_id The dog's ID
     * @param countdown_data JSON with days, hours, minutes, seconds, formatted, is_critical
     * @return WebSocketMessage with COUNTDOWN_UPDATE type
     */
    static WebSocketMessage createCountdownUpdate(const std::string& dog_id,
                                                   const Json::Value& countdown_data);

    /**
     * @brief Create an urgent alert message
     * @param dog_id The dog's ID
     * @param urgency_level The urgency level (high, critical)
     * @param alert_data Additional alert information
     * @return WebSocketMessage with URGENT_ALERT type
     */
    static WebSocketMessage createUrgentAlert(const std::string& dog_id,
                                               const std::string& urgency_level,
                                               const Json::Value& alert_data);

    /**
     * @brief Create a dog status change message
     * @param dog_id The dog's ID
     * @param new_status The new status
     * @param status_data Additional status information
     * @return WebSocketMessage with DOG_STATUS_CHANGE type
     */
    static WebSocketMessage createDogStatusChange(const std::string& dog_id,
                                                   const std::string& new_status,
                                                   const Json::Value& status_data);
};

} // namespace wtl::core::websocket
