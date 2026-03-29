/**
 * @file WaitTimeWebSocket.cc
 * @brief Implementation of WaitTimeWebSocket controller
 * @see WaitTimeWebSocket.h for documentation
 */

#include "core/websocket/WaitTimeWebSocket.h"

#include <drogon/drogon.h>
#include <random>
#include <sstream>

#include "core/websocket/ConnectionManager.h"
#include "core/websocket/WebSocketMessage.h"

namespace wtl::core::websocket {

// ============================================================================
// CONNECTION HANDLING
// ============================================================================

void WaitTimeWebSocket::handleNewConnection(
    const drogon::HttpRequestPtr& req,
    const drogon::WebSocketConnectionPtr& conn
) {
    // Generate unique connection ID
    std::string conn_id = generateConnectionId();

    // Store connection ID in the connection context
    conn->setContext(std::make_shared<std::string>(conn_id));

    // Register with ConnectionManager
    ConnectionManager::getInstance().addConnection(conn_id, conn);

    // Send connection acknowledgment
    auto ack_msg = WebSocketMessage::createConnectionAck(conn_id);
    conn->send(ack_msg.toJson());

    // Log connection (in production, use proper logging)
    LOG_INFO << "WaitTimeWebSocket: New connection " << conn_id
             << " from " << req->getPeerAddr().toIp();
}

void WaitTimeWebSocket::handleNewMessage(
    const drogon::WebSocketConnectionPtr& conn,
    std::string&& message,
    const drogon::WebSocketMessageType& type
) {
    // Only handle text messages
    if (type != drogon::WebSocketMessageType::Text) {
        sendError(conn, "Only text messages are supported");
        return;
    }

    // Get connection ID from context
    auto ctx = conn->getContext<std::string>();
    if (!ctx) {
        sendError(conn, "Connection not properly initialized");
        return;
    }
    const std::string& conn_id = *ctx;

    // Parse the message
    WebSocketMessage msg;
    try {
        msg = WebSocketMessage::fromJson(message);
    } catch (const std::exception& e) {
        sendError(conn, "Invalid message format: " + std::string(e.what()));
        return;
    }

    auto& manager = ConnectionManager::getInstance();

    // Handle message based on type
    switch (msg.type) {
        case MessageType::SUBSCRIBE_DOG:
            if (msg.dog_id.empty()) {
                sendError(conn, "dog_id is required for SUBSCRIBE_DOG");
                return;
            }
            manager.subscribeToDog(conn_id, msg.dog_id);
            sendConfirmation(conn, "subscribed_dog", msg.dog_id);
            LOG_DEBUG << "WaitTimeWebSocket: " << conn_id << " subscribed to dog " << msg.dog_id;
            break;

        case MessageType::UNSUBSCRIBE_DOG:
            if (msg.dog_id.empty()) {
                sendError(conn, "dog_id is required for UNSUBSCRIBE_DOG");
                return;
            }
            manager.unsubscribeFromDog(conn_id, msg.dog_id);
            sendConfirmation(conn, "unsubscribed_dog", msg.dog_id);
            LOG_DEBUG << "WaitTimeWebSocket: " << conn_id << " unsubscribed from dog " << msg.dog_id;
            break;

        case MessageType::SUBSCRIBE_SHELTER:
            if (msg.shelter_id.empty()) {
                sendError(conn, "shelter_id is required for SUBSCRIBE_SHELTER");
                return;
            }
            manager.subscribeToShelter(conn_id, msg.shelter_id);
            sendConfirmation(conn, "subscribed_shelter", msg.shelter_id);
            LOG_DEBUG << "WaitTimeWebSocket: " << conn_id << " subscribed to shelter " << msg.shelter_id;
            break;

        case MessageType::UNSUBSCRIBE_SHELTER:
            if (msg.shelter_id.empty()) {
                sendError(conn, "shelter_id is required for UNSUBSCRIBE_SHELTER");
                return;
            }
            manager.unsubscribeFromShelter(conn_id, msg.shelter_id);
            sendConfirmation(conn, "unsubscribed_shelter", msg.shelter_id);
            LOG_DEBUG << "WaitTimeWebSocket: " << conn_id << " unsubscribed from shelter " << msg.shelter_id;
            break;

        case MessageType::SUBSCRIBE_URGENT:
            manager.subscribeToUrgent(conn_id);
            sendConfirmation(conn, "subscribed_urgent", "all");
            LOG_DEBUG << "WaitTimeWebSocket: " << conn_id << " subscribed to urgent alerts";
            break;

        default:
            sendError(conn, "Unsupported message type: " + messageTypeToString(msg.type));
            break;
    }
}

void WaitTimeWebSocket::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) {
    // Get connection ID from context
    auto ctx = conn->getContext<std::string>();
    if (!ctx) {
        return;
    }
    const std::string& conn_id = *ctx;

    // Remove from ConnectionManager (also cleans up subscriptions)
    ConnectionManager::getInstance().removeConnection(conn_id);

    LOG_INFO << "WaitTimeWebSocket: Connection closed " << conn_id;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::string WaitTimeWebSocket::generateConnectionId() {
    // Generate UUID-like connection ID
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    std::stringstream ss;
    ss << std::hex << dist(gen) << "-" << dist(gen);
    return ss.str();
}

void WaitTimeWebSocket::sendError(
    const drogon::WebSocketConnectionPtr& conn,
    const std::string& error_message
) {
    auto error_msg = WebSocketMessage::createError(error_message);
    conn->send(error_msg.toJson());
}

void WaitTimeWebSocket::sendConfirmation(
    const drogon::WebSocketConnectionPtr& conn,
    const std::string& action,
    const std::string& entity_id
) {
    Json::Value payload;
    payload["action"] = action;
    payload["entity_id"] = entity_id;
    payload["status"] = "success";
    payload["timestamp"] = static_cast<Json::Int64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    WebSocketMessage msg;
    msg.type = MessageType::CONNECTION_ACK;
    msg.payload = payload;

    conn->send(msg.toJson());
}

} // namespace wtl::core::websocket
