/**
 * @file UrgencyWebSocket.cc
 * @brief Implementation of UrgencyWebSocket controller
 * @see UrgencyWebSocket.h for documentation
 */

#include "core/websocket/UrgencyWebSocket.h"

#include <drogon/drogon.h>
#include <random>
#include <sstream>

#include "core/websocket/ConnectionManager.h"
#include "core/websocket/WebSocketMessage.h"

namespace wtl::core::websocket {

// ============================================================================
// CONNECTION HANDLING
// ============================================================================

void UrgencyWebSocket::handleNewConnection(
    const drogon::HttpRequestPtr& req,
    const drogon::WebSocketConnectionPtr& conn
) {
    // Generate unique connection ID
    std::string conn_id = generateConnectionId();

    // Store connection ID in the connection context
    conn->setContext(std::make_shared<std::string>(conn_id));

    auto& manager = ConnectionManager::getInstance();

    // Register with ConnectionManager
    manager.addConnection(conn_id, conn);

    // Automatically subscribe to urgent alerts (this is the urgent channel)
    manager.subscribeToUrgent(conn_id);

    // Send connection acknowledgment
    Json::Value ack_payload;
    ack_payload["connection_id"] = conn_id;
    ack_payload["status"] = "connected";
    ack_payload["channel"] = "urgent";
    ack_payload["auto_subscribed"] = true;
    ack_payload["timestamp"] = static_cast<Json::Int64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    WebSocketMessage ack_msg;
    ack_msg.type = MessageType::CONNECTION_ACK;
    ack_msg.payload = ack_payload;
    conn->send(ack_msg.toJson());

    // Send current urgent dogs (if any)
    sendCurrentUrgentDogs(conn);

    LOG_INFO << "UrgencyWebSocket: New connection " << conn_id
             << " from " << req->getPeerAddr().toIp();
}

void UrgencyWebSocket::handleNewMessage(
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
            // Allow subscribing to specific dogs for countdown updates
            if (msg.dog_id.empty()) {
                sendError(conn, "dog_id is required for SUBSCRIBE_DOG");
                return;
            }
            manager.subscribeToDog(conn_id, msg.dog_id);
            {
                Json::Value payload;
                payload["action"] = "subscribed_urgent_dog";
                payload["dog_id"] = msg.dog_id;
                payload["status"] = "success";

                WebSocketMessage confirm;
                confirm.type = MessageType::CONNECTION_ACK;
                confirm.dog_id = msg.dog_id;
                confirm.payload = payload;
                conn->send(confirm.toJson());
            }
            LOG_DEBUG << "UrgencyWebSocket: " << conn_id << " subscribed to urgent dog " << msg.dog_id;
            break;

        case MessageType::UNSUBSCRIBE_DOG:
            if (msg.dog_id.empty()) {
                sendError(conn, "dog_id is required for UNSUBSCRIBE_DOG");
                return;
            }
            manager.unsubscribeFromDog(conn_id, msg.dog_id);
            {
                Json::Value payload;
                payload["action"] = "unsubscribed_urgent_dog";
                payload["dog_id"] = msg.dog_id;
                payload["status"] = "success";

                WebSocketMessage confirm;
                confirm.type = MessageType::CONNECTION_ACK;
                confirm.dog_id = msg.dog_id;
                confirm.payload = payload;
                conn->send(confirm.toJson());
            }
            LOG_DEBUG << "UrgencyWebSocket: " << conn_id << " unsubscribed from urgent dog " << msg.dog_id;
            break;

        default:
            // Ignore other message types on urgent channel
            // (ping messages, etc. are handled by Drogon)
            break;
    }
}

void UrgencyWebSocket::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) {
    // Get connection ID from context
    auto ctx = conn->getContext<std::string>();
    if (!ctx) {
        return;
    }
    const std::string& conn_id = *ctx;

    // Remove from ConnectionManager (also cleans up subscriptions)
    ConnectionManager::getInstance().removeConnection(conn_id);

    LOG_INFO << "UrgencyWebSocket: Connection closed " << conn_id;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::string UrgencyWebSocket::generateConnectionId() {
    // Generate UUID-like connection ID with "urg-" prefix for debugging
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    std::stringstream ss;
    ss << "urg-" << std::hex << dist(gen) << "-" << dist(gen);
    return ss.str();
}

void UrgencyWebSocket::sendError(
    const drogon::WebSocketConnectionPtr& conn,
    const std::string& error_message
) {
    auto error_msg = WebSocketMessage::createError(error_message);
    conn->send(error_msg.toJson());
}

void UrgencyWebSocket::sendCurrentUrgentDogs(const drogon::WebSocketConnectionPtr& conn) {
    // This method would query DogService to get current urgent dogs
    // and send their information to the newly connected client.
    // For now, we send a placeholder message indicating the feature is ready.

    // In production, this would be:
    // auto& dogService = DogService::getInstance();
    // auto urgentDogs = dogService.findByUrgencyLevel("critical");
    // for (const auto& dog : urgentDogs) {
    //     auto countdown = dogService.calculateCountdown(dog);
    //     if (countdown) {
    //         auto msg = WebSocketMessage::createCountdownUpdate(dog.id, countdown->toJson());
    //         conn->send(msg.toJson());
    //     }
    // }

    // For now, send a status message
    Json::Value payload;
    payload["status"] = "ready";
    payload["message"] = "Urgent alert channel ready";
    payload["channel"] = "urgent";

    WebSocketMessage status_msg;
    status_msg.type = MessageType::CONNECTION_ACK;
    status_msg.payload = payload;
    conn->send(status_msg.toJson());
}

} // namespace wtl::core::websocket
