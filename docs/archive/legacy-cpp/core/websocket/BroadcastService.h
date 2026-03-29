/**
 * @file BroadcastService.h
 * @brief Singleton service for broadcasting WebSocket updates to subscribers
 *
 * PURPOSE:
 * Provides centralized methods to push real-time updates to WebSocket clients.
 * Uses ConnectionManager to find subscribers and sends formatted messages.
 *
 * USAGE:
 * auto& broadcast = BroadcastService::getInstance();
 * broadcast.broadcastWaitTimeUpdate(dog_id, wait_time_components);
 * broadcast.broadcastUrgentAlert(dog_id, "critical");
 *
 * DEPENDENCIES:
 * - ConnectionManager (finding subscribers)
 * - WebSocketMessage (message formatting)
 * - DogService (for WaitTimeComponents, CountdownComponents)
 *
 * MODIFICATION GUIDE:
 * - Add new broadcast methods for new message types
 * - Always use ConnectionManager to find subscribers
 * - Handle connection failures gracefully (subscriber may have disconnected)
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <mutex>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "core/websocket/WebSocketMessage.h"

namespace wtl::core::websocket {

/**
 * @struct WaitTimeComponents
 * @brief Wait time broken down into components for display
 *
 * Represents how long a dog has been waiting for adoption,
 * broken down for LED counter display.
 */
struct WaitTimeComponents {
    int years = 0;
    int months = 0;
    int days = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    std::string formatted;  // "YY:MM:DD:HH:MM:SS"

    /**
     * @brief Convert to JSON for WebSocket transmission
     */
    Json::Value toJson() const;
};

/**
 * @struct CountdownComponents
 * @brief Countdown time for dogs facing euthanasia
 *
 * Represents remaining time until scheduled euthanasia,
 * used for critical countdown display.
 */
struct CountdownComponents {
    int days = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    std::string formatted;  // "DD:HH:MM:SS"
    bool is_critical = false;  // True if < 24 hours remaining
    std::string urgency_level;  // "high", "critical"

    /**
     * @brief Convert to JSON for WebSocket transmission
     */
    Json::Value toJson() const;
};

/**
 * @class BroadcastService
 * @brief Singleton service for pushing updates to WebSocket subscribers
 *
 * Centralized service for all real-time updates:
 * - Wait time updates (per dog)
 * - Countdown updates (for urgent dogs)
 * - Status changes (adoptions, transfers, etc.)
 * - Urgent alerts (new critical dogs)
 */
class BroadcastService {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the BroadcastService singleton
     */
    static BroadcastService& getInstance();

    // Prevent copying
    BroadcastService(const BroadcastService&) = delete;
    BroadcastService& operator=(const BroadcastService&) = delete;

    // ========================================================================
    // WAIT TIME BROADCASTS
    // ========================================================================

    /**
     * @brief Broadcast wait time update to all subscribers of a dog
     * @param dog_id The dog's unique identifier
     * @param wait_time The calculated wait time components
     *
     * Sends WAIT_TIME_UPDATE message to:
     * - Direct dog subscribers
     * - Subscribers of the dog's shelter
     */
    void broadcastWaitTimeUpdate(const std::string& dog_id,
                                  const WaitTimeComponents& wait_time);

    /**
     * @brief Broadcast wait time update using JSON payload
     * @param dog_id The dog's unique identifier
     * @param wait_time_json JSON with years, months, days, hours, minutes, seconds, formatted
     */
    void broadcastWaitTimeUpdate(const std::string& dog_id,
                                  const Json::Value& wait_time_json);

    // ========================================================================
    // COUNTDOWN BROADCASTS
    // ========================================================================

    /**
     * @brief Broadcast countdown update to subscribers of an urgent dog
     * @param dog_id The dog's unique identifier
     * @param countdown The calculated countdown components
     *
     * Sends COUNTDOWN_UPDATE message to:
     * - Direct dog subscribers
     * - Urgent alert subscribers
     */
    void broadcastCountdownUpdate(const std::string& dog_id,
                                   const CountdownComponents& countdown);

    /**
     * @brief Broadcast countdown update using JSON payload
     * @param dog_id The dog's unique identifier
     * @param countdown_json JSON with days, hours, minutes, seconds, formatted, is_critical
     */
    void broadcastCountdownUpdate(const std::string& dog_id,
                                   const Json::Value& countdown_json);

    // ========================================================================
    // STATUS BROADCASTS
    // ========================================================================

    /**
     * @brief Broadcast dog status change
     * @param dog_id The dog's unique identifier
     * @param new_status The new status (adopted, pending, transferred, etc.)
     *
     * Sends DOG_STATUS_CHANGE message to all subscribers
     */
    void broadcastDogStatusChange(const std::string& dog_id,
                                   const std::string& new_status);

    /**
     * @brief Broadcast dog status change with additional data
     * @param dog_id The dog's unique identifier
     * @param new_status The new status
     * @param additional_data Extra information about the status change
     */
    void broadcastDogStatusChange(const std::string& dog_id,
                                   const std::string& new_status,
                                   const Json::Value& additional_data);

    // ========================================================================
    // URGENT BROADCASTS
    // ========================================================================

    /**
     * @brief Broadcast urgent alert for a dog
     * @param dog_id The dog's unique identifier
     * @param urgency_level The urgency level (high, critical)
     *
     * Sends URGENT_ALERT message to all urgent subscribers
     */
    void broadcastUrgentAlert(const std::string& dog_id,
                               const std::string& urgency_level);

    /**
     * @brief Broadcast urgent alert with full dog information
     * @param dog_id The dog's unique identifier
     * @param urgency_level The urgency level
     * @param dog_info Additional dog information (name, shelter, countdown)
     */
    void broadcastUrgentAlert(const std::string& dog_id,
                               const std::string& urgency_level,
                               const Json::Value& dog_info);

    // ========================================================================
    // SHELTER BROADCASTS
    // ========================================================================

    /**
     * @brief Broadcast a message to all subscribers of a shelter
     * @param shelter_id The shelter's unique identifier
     * @param message The message to send
     */
    void broadcastToShelterSubscribers(const std::string& shelter_id,
                                        const WebSocketMessage& message);

    /**
     * @brief Broadcast update for all dogs in a shelter
     * @param shelter_id The shelter's unique identifier
     * @param update_type Type of update (e.g., "new_dog", "dog_adopted")
     * @param update_data Additional update information
     */
    void broadcastShelterUpdate(const std::string& shelter_id,
                                 const std::string& update_type,
                                 const Json::Value& update_data);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get broadcast statistics
     * @return JSON with broadcast counts and performance metrics
     */
    Json::Value getStats() const;

    /**
     * @brief Reset broadcast statistics
     */
    void resetStats();

private:
    BroadcastService() = default;

    /**
     * @brief Send a message to a specific connection
     * @param conn_id The connection identifier
     * @param message The message to send
     * @return true if sent successfully, false if connection not found
     */
    bool sendToConnection(const std::string& conn_id, const std::string& message);

    /**
     * @brief Send a message to multiple connections
     * @param conn_ids List of connection identifiers
     * @param message The message to send
     * @return Number of successful sends
     */
    size_t sendToConnections(const std::vector<std::string>& conn_ids,
                              const std::string& message);

    // Statistics
    mutable std::mutex stats_mutex_;
    size_t total_broadcasts_ = 0;
    size_t successful_sends_ = 0;
    size_t failed_sends_ = 0;
};

} // namespace wtl::core::websocket
