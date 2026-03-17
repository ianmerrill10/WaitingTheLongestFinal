/**
 * @file WaitTimeWebSocket.h
 * @brief Drogon WebSocket controller for dog wait time updates
 *
 * PURPOSE:
 * Handles WebSocket connections at /ws/dogs for real-time wait time updates.
 * Clients can subscribe to specific dogs or shelters to receive live updates.
 *
 * USAGE:
 * Client connects to ws://server/ws/dogs
 * Send: {"type":"SUBSCRIBE_DOG","dog_id":"uuid"}
 * Receive: {"type":"WAIT_TIME_UPDATE","dog_id":"uuid","payload":{...}}
 *
 * DEPENDENCIES:
 * - ConnectionManager (connection tracking)
 * - WebSocketMessage (message serialization)
 *
 * MODIFICATION GUIDE:
 * - Add new message handlers in handleNewMessage()
 * - Use ConnectionManager for all subscription management
 * - Always send CONNECTION_ACK on new connection
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Third-party includes
#include <drogon/WebSocketController.h>

namespace wtl::core::websocket {

/**
 * @class WaitTimeWebSocket
 * @brief WebSocket controller for dog wait time updates at /ws/dogs
 *
 * Handles:
 * - New client connections with acknowledgment
 * - Subscribe/unsubscribe requests for dogs and shelters
 * - Connection cleanup on disconnect
 */
class WaitTimeWebSocket : public drogon::WebSocketController<WaitTimeWebSocket> {
public:
    /**
     * @brief Handle new WebSocket connection
     * @param req The HTTP request that initiated the connection
     * @param conn The WebSocket connection pointer
     *
     * Sends CONNECTION_ACK message with assigned connection ID
     */
    void handleNewConnection(const drogon::HttpRequestPtr& req,
                             const drogon::WebSocketConnectionPtr& conn) override;

    /**
     * @brief Handle incoming WebSocket message
     * @param conn The WebSocket connection pointer
     * @param message The message content
     * @param type The message type (text, binary, etc.)
     *
     * Processes:
     * - SUBSCRIBE_DOG: Subscribe to a dog's wait time updates
     * - UNSUBSCRIBE_DOG: Unsubscribe from a dog's updates
     * - SUBSCRIBE_SHELTER: Subscribe to all dogs in a shelter
     * - UNSUBSCRIBE_SHELTER: Unsubscribe from a shelter's updates
     * - SUBSCRIBE_URGENT: Subscribe to urgent dog alerts
     */
    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                          std::string&& message,
                          const drogon::WebSocketMessageType& type) override;

    /**
     * @brief Handle WebSocket connection closed
     * @param conn The WebSocket connection pointer
     *
     * Cleans up all subscriptions for the disconnected client
     */
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    // Register the WebSocket path
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/dogs");
    WS_PATH_LIST_END

private:
    /**
     * @brief Generate a unique connection ID
     * @return Unique connection identifier string
     */
    static std::string generateConnectionId();

    /**
     * @brief Send an error message to the client
     * @param conn The WebSocket connection
     * @param error_message The error description
     */
    static void sendError(const drogon::WebSocketConnectionPtr& conn,
                          const std::string& error_message);

    /**
     * @brief Send a confirmation message to the client
     * @param conn The WebSocket connection
     * @param action The action that was confirmed
     * @param entity_id The dog or shelter ID
     */
    static void sendConfirmation(const drogon::WebSocketConnectionPtr& conn,
                                  const std::string& action,
                                  const std::string& entity_id);
};

} // namespace wtl::core::websocket
