/**
 * @file ConnectionManager.h
 * @brief Manages WebSocket connections and subscriptions
 *
 * PURPOSE:
 * Tracks all active WebSocket connections and their subscriptions to dogs,
 * shelters, and urgent alerts. Thread-safe implementation for concurrent access.
 *
 * USAGE:
 * auto& manager = ConnectionManager::getInstance();
 * manager.addConnection(conn_id, connection_ptr);
 * manager.subscribeToDog(conn_id, dog_id);
 * auto subscribers = manager.getSubscribersForDog(dog_id);
 *
 * DEPENDENCIES:
 * - Drogon WebSocket (drogon::WebSocketConnectionPtr)
 *
 * MODIFICATION GUIDE:
 * - All public methods must be thread-safe (use mutex_)
 * - Connection IDs should be unique per connection
 * - Subscriptions are automatically cleaned up on disconnect
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Third-party includes
#include <drogon/WebSocketConnection.h>
#include <json/json.h>

namespace wtl::core::websocket {

/**
 * @class ConnectionManager
 * @brief Thread-safe singleton that manages WebSocket connections and subscriptions
 *
 * Tracks active connections and their subscriptions:
 * - Dog subscriptions: Receive wait time updates for specific dogs
 * - Shelter subscriptions: Receive updates for all dogs in a shelter
 * - Urgent subscriptions: Receive alerts for all critical/urgent dogs
 */
class ConnectionManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the ConnectionManager singleton
     */
    static ConnectionManager& getInstance();

    // Prevent copying
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // ========================================================================
    // CONNECTION MANAGEMENT
    // ========================================================================

    /**
     * @brief Add a new WebSocket connection
     * @param conn_id Unique connection identifier
     * @param conn The WebSocket connection pointer
     */
    void addConnection(const std::string& conn_id,
                       const drogon::WebSocketConnectionPtr& conn);

    /**
     * @brief Remove a WebSocket connection and all its subscriptions
     * @param conn_id The connection identifier to remove
     */
    void removeConnection(const std::string& conn_id);

    /**
     * @brief Get a connection by its ID
     * @param conn_id The connection identifier
     * @return The connection pointer, or nullptr if not found
     */
    drogon::WebSocketConnectionPtr getConnection(const std::string& conn_id);

    /**
     * @brief Get the total number of active connections
     * @return Number of active connections
     */
    size_t getConnectionCount() const;

    // ========================================================================
    // DOG SUBSCRIPTIONS
    // ========================================================================

    /**
     * @brief Subscribe a connection to dog updates
     * @param conn_id The connection identifier
     * @param dog_id The dog ID to subscribe to
     */
    void subscribeToDog(const std::string& conn_id, const std::string& dog_id);

    /**
     * @brief Unsubscribe a connection from dog updates
     * @param conn_id The connection identifier
     * @param dog_id The dog ID to unsubscribe from
     */
    void unsubscribeFromDog(const std::string& conn_id, const std::string& dog_id);

    /**
     * @brief Get all connection IDs subscribed to a specific dog
     * @param dog_id The dog ID
     * @return Vector of connection IDs
     */
    std::vector<std::string> getSubscribersForDog(const std::string& dog_id);

    /**
     * @brief Get all dog IDs that have at least one subscriber
     * @return Set of dog IDs with active subscribers
     */
    std::set<std::string> getDogsWithSubscribers();

    // ========================================================================
    // SHELTER SUBSCRIPTIONS
    // ========================================================================

    /**
     * @brief Subscribe a connection to shelter updates
     * @param conn_id The connection identifier
     * @param shelter_id The shelter ID to subscribe to
     */
    void subscribeToShelter(const std::string& conn_id, const std::string& shelter_id);

    /**
     * @brief Unsubscribe a connection from shelter updates
     * @param conn_id The connection identifier
     * @param shelter_id The shelter ID to unsubscribe from
     */
    void unsubscribeFromShelter(const std::string& conn_id, const std::string& shelter_id);

    /**
     * @brief Get all connection IDs subscribed to a specific shelter
     * @param shelter_id The shelter ID
     * @return Vector of connection IDs
     */
    std::vector<std::string> getSubscribersForShelter(const std::string& shelter_id);

    // ========================================================================
    // URGENT SUBSCRIPTIONS
    // ========================================================================

    /**
     * @brief Subscribe a connection to urgent dog alerts
     * @param conn_id The connection identifier
     */
    void subscribeToUrgent(const std::string& conn_id);

    /**
     * @brief Unsubscribe a connection from urgent dog alerts
     * @param conn_id The connection identifier
     */
    void unsubscribeFromUrgent(const std::string& conn_id);

    /**
     * @brief Get all connection IDs subscribed to urgent alerts
     * @return Vector of connection IDs
     */
    std::vector<std::string> getUrgentSubscribers();

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get statistics about connections and subscriptions
     * @return JSON object with connection and subscription stats
     */
    Json::Value getStats() const;

private:
    ConnectionManager() = default;

    // Connection storage: conn_id -> WebSocketConnectionPtr
    std::unordered_map<std::string, drogon::WebSocketConnectionPtr> connections_;

    // Dog subscriptions: dog_id -> set of conn_ids
    std::unordered_map<std::string, std::unordered_set<std::string>> dog_subscribers_;

    // Reverse mapping: conn_id -> set of dog_ids (for cleanup on disconnect)
    std::unordered_map<std::string, std::unordered_set<std::string>> connection_dogs_;

    // Shelter subscriptions: shelter_id -> set of conn_ids
    std::unordered_map<std::string, std::unordered_set<std::string>> shelter_subscribers_;

    // Reverse mapping: conn_id -> set of shelter_ids
    std::unordered_map<std::string, std::unordered_set<std::string>> connection_shelters_;

    // Urgent subscriptions: set of conn_ids
    std::unordered_set<std::string> urgent_subscribers_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::core::websocket
