/**
 * @file UrgencyWebSocket.h
 * @brief Drogon WebSocket controller for urgent dog alerts
 *
 * PURPOSE:
 * Handles WebSocket connections at /ws/urgent for real-time urgent dog alerts.
 * Provides dedicated channel for critical dogs on euthanasia lists or
 * those needing immediate rescue.
 *
 * USAGE:
 * Client connects to ws://server/ws/urgent
 * Receive: {"type":"URGENT_ALERT","dog_id":"uuid","payload":{...}}
 * Receive: {"type":"COUNTDOWN_UPDATE","dog_id":"uuid","payload":{...}}
 *
 * DEPENDENCIES:
 * - ConnectionManager (connection tracking)
 * - WebSocketMessage (message serialization)
 *
 * MODIFICATION GUIDE:
 * - All connections are automatically subscribed to urgent alerts
 * - Use BroadcastService to push alerts
 * - Countdown updates are sent every second for critical dogs
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Third-party includes
#include <drogon/WebSocketController.h>

namespace wtl::core::websocket {

/**
 * @class UrgencyWebSocket
 * @brief WebSocket controller for urgent dog alerts at /ws/urgent
 *
 * Handles:
 * - New client connections with automatic urgent subscription
 * - Broadcasting critical dog alerts
 * - Countdown updates for dogs facing euthanasia
 * - Connection cleanup on disconnect
 */
class UrgencyWebSocket : public drogon::WebSocketController<UrgencyWebSocket> {
public:
    /**
     * @brief Handle new WebSocket connection
     * @param req The HTTP request that initiated the connection
     * @param conn The WebSocket connection pointer
     *
     * Automatically subscribes the connection to urgent alerts
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
     * - SUBSCRIBE_DOG: Subscribe to a specific dog's countdown
     * - UNSUBSCRIBE_DOG: Unsubscribe from a specific dog's countdown
     * - Ping messages for keep-alive
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
    WS_PATH_ADD("/ws/urgent");
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
     * @brief Send current urgent dogs list to a new connection
     * @param conn The WebSocket connection
     */
    void sendCurrentUrgentDogs(const drogon::WebSocketConnectionPtr& conn);
};

} // namespace wtl::core::websocket
