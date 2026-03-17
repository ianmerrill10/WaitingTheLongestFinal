/**
 * @file BroadcastService.cc
 * @brief Implementation of BroadcastService for WebSocket updates
 * @see BroadcastService.h for documentation
 */

#include "core/websocket/BroadcastService.h"

#include <set>

#include <trantor/utils/Logger.h>

#include "core/websocket/ConnectionManager.h"
#include "core/websocket/WebSocketMessage.h"

namespace wtl::core::websocket {

// ============================================================================
// WAIT TIME / COUNTDOWN COMPONENTS
// ============================================================================

Json::Value WaitTimeComponents::toJson() const {
    Json::Value json;
    json["years"] = years;
    json["months"] = months;
    json["days"] = days;
    json["hours"] = hours;
    json["minutes"] = minutes;
    json["seconds"] = seconds;
    json["formatted"] = formatted;
    return json;
}

Json::Value CountdownComponents::toJson() const {
    Json::Value json;
    json["days"] = days;
    json["hours"] = hours;
    json["minutes"] = minutes;
    json["seconds"] = seconds;
    json["formatted"] = formatted;
    json["is_critical"] = is_critical;
    json["urgency_level"] = urgency_level;
    return json;
}

// ============================================================================
// SINGLETON
// ============================================================================

BroadcastService& BroadcastService::getInstance() {
    static BroadcastService instance;
    return instance;
}

// ============================================================================
// WAIT TIME BROADCASTS
// ============================================================================

void BroadcastService::broadcastWaitTimeUpdate(
    const std::string& dog_id,
    const WaitTimeComponents& wait_time
) {
    broadcastWaitTimeUpdate(dog_id, wait_time.toJson());
}

void BroadcastService::broadcastWaitTimeUpdate(
    const std::string& dog_id,
    const Json::Value& wait_time_json
) {
    auto msg = WebSocketMessage::createWaitTimeUpdate(dog_id, wait_time_json);
    std::string message_str = msg.toJson();

    auto& manager = ConnectionManager::getInstance();

    // Get direct dog subscribers
    auto dog_subscribers = manager.getSubscribersForDog(dog_id);

    // Send to all subscribers
    size_t sent = sendToConnections(dog_subscribers, message_str);

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_broadcasts_++;
        successful_sends_ += sent;
        failed_sends_ += (dog_subscribers.size() - sent);
    }

    LOG_TRACE << "BroadcastService: Sent wait time update for dog " << dog_id
              << " to " << sent << "/" << dog_subscribers.size() << " subscribers";
}

// ============================================================================
// COUNTDOWN BROADCASTS
// ============================================================================

void BroadcastService::broadcastCountdownUpdate(
    const std::string& dog_id,
    const CountdownComponents& countdown
) {
    broadcastCountdownUpdate(dog_id, countdown.toJson());
}

void BroadcastService::broadcastCountdownUpdate(
    const std::string& dog_id,
    const Json::Value& countdown_json
) {
    auto msg = WebSocketMessage::createCountdownUpdate(dog_id, countdown_json);
    std::string message_str = msg.toJson();

    auto& manager = ConnectionManager::getInstance();

    // Get direct dog subscribers
    auto dog_subscribers = manager.getSubscribersForDog(dog_id);

    // Also send to urgent subscribers for critical countdowns
    auto urgent_subscribers = manager.getUrgentSubscribers();

    // Merge subscriber lists (removing duplicates)
    std::set<std::string> all_subscribers(dog_subscribers.begin(), dog_subscribers.end());
    all_subscribers.insert(urgent_subscribers.begin(), urgent_subscribers.end());

    std::vector<std::string> merged_list(all_subscribers.begin(), all_subscribers.end());

    // Send to all
    size_t sent = sendToConnections(merged_list, message_str);

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_broadcasts_++;
        successful_sends_ += sent;
        failed_sends_ += (merged_list.size() - sent);
    }

    LOG_TRACE << "BroadcastService: Sent countdown update for dog " << dog_id
              << " to " << sent << "/" << merged_list.size() << " subscribers";
}

// ============================================================================
// STATUS BROADCASTS
// ============================================================================

void BroadcastService::broadcastDogStatusChange(
    const std::string& dog_id,
    const std::string& new_status
) {
    Json::Value empty;
    broadcastDogStatusChange(dog_id, new_status, empty);
}

void BroadcastService::broadcastDogStatusChange(
    const std::string& dog_id,
    const std::string& new_status,
    const Json::Value& additional_data
) {
    auto msg = WebSocketMessage::createDogStatusChange(dog_id, new_status, additional_data);
    std::string message_str = msg.toJson();

    auto& manager = ConnectionManager::getInstance();

    // Get direct dog subscribers
    auto dog_subscribers = manager.getSubscribersForDog(dog_id);

    // Also notify urgent subscribers if status change is significant
    // (e.g., dog was adopted - no longer urgent)
    auto urgent_subscribers = manager.getUrgentSubscribers();

    // Merge subscriber lists
    std::set<std::string> all_subscribers(dog_subscribers.begin(), dog_subscribers.end());
    all_subscribers.insert(urgent_subscribers.begin(), urgent_subscribers.end());

    std::vector<std::string> merged_list(all_subscribers.begin(), all_subscribers.end());

    // Send to all
    size_t sent = sendToConnections(merged_list, message_str);

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_broadcasts_++;
        successful_sends_ += sent;
        failed_sends_ += (merged_list.size() - sent);
    }

    LOG_INFO << "BroadcastService: Sent status change for dog " << dog_id
             << " (new status: " << new_status << ") to " << sent << " subscribers";
}

// ============================================================================
// URGENT BROADCASTS
// ============================================================================

void BroadcastService::broadcastUrgentAlert(
    const std::string& dog_id,
    const std::string& urgency_level
) {
    Json::Value empty;
    broadcastUrgentAlert(dog_id, urgency_level, empty);
}

void BroadcastService::broadcastUrgentAlert(
    const std::string& dog_id,
    const std::string& urgency_level,
    const Json::Value& dog_info
) {
    auto msg = WebSocketMessage::createUrgentAlert(dog_id, urgency_level, dog_info);
    std::string message_str = msg.toJson();

    auto& manager = ConnectionManager::getInstance();

    // Get all urgent subscribers
    auto urgent_subscribers = manager.getUrgentSubscribers();

    // Send to all urgent subscribers
    size_t sent = sendToConnections(urgent_subscribers, message_str);

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_broadcasts_++;
        successful_sends_ += sent;
        failed_sends_ += (urgent_subscribers.size() - sent);
    }

    LOG_WARN << "BroadcastService: Sent URGENT ALERT for dog " << dog_id
             << " (level: " << urgency_level << ") to " << sent << " subscribers";
}

// ============================================================================
// SHELTER BROADCASTS
// ============================================================================

void BroadcastService::broadcastToShelterSubscribers(
    const std::string& shelter_id,
    const WebSocketMessage& message
) {
    std::string message_str = message.toJson();

    auto& manager = ConnectionManager::getInstance();
    auto shelter_subscribers = manager.getSubscribersForShelter(shelter_id);

    size_t sent = sendToConnections(shelter_subscribers, message_str);

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_broadcasts_++;
        successful_sends_ += sent;
        failed_sends_ += (shelter_subscribers.size() - sent);
    }

    LOG_DEBUG << "BroadcastService: Sent message to shelter " << shelter_id
              << " subscribers: " << sent << "/" << shelter_subscribers.size();
}

void BroadcastService::broadcastShelterUpdate(
    const std::string& shelter_id,
    const std::string& update_type,
    const Json::Value& update_data
) {
    WebSocketMessage msg;
    msg.type = MessageType::DOG_STATUS_CHANGE;  // Reuse for shelter updates
    msg.shelter_id = shelter_id;
    msg.payload = update_data;
    msg.payload["update_type"] = update_type;
    msg.payload["shelter_id"] = shelter_id;

    broadcastToShelterSubscribers(shelter_id, msg);
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value BroadcastService::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Json::Value stats;
    stats["total_broadcasts"] = static_cast<Json::UInt64>(total_broadcasts_);
    stats["successful_sends"] = static_cast<Json::UInt64>(successful_sends_);
    stats["failed_sends"] = static_cast<Json::UInt64>(failed_sends_);

    if (total_broadcasts_ > 0) {
        double success_rate = static_cast<double>(successful_sends_) /
                              static_cast<double>(successful_sends_ + failed_sends_) * 100.0;
        stats["success_rate_percent"] = success_rate;
    } else {
        stats["success_rate_percent"] = 100.0;
    }

    // Include connection stats
    stats["connection_stats"] = ConnectionManager::getInstance().getStats();

    return stats;
}

void BroadcastService::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_broadcasts_ = 0;
    successful_sends_ = 0;
    failed_sends_ = 0;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

bool BroadcastService::sendToConnection(
    const std::string& conn_id,
    const std::string& message
) {
    auto& manager = ConnectionManager::getInstance();
    auto conn = manager.getConnection(conn_id);

    if (!conn) {
        return false;
    }

    try {
        conn->send(message);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "BroadcastService: Failed to send to " << conn_id << ": " << e.what();
        return false;
    }
}

size_t BroadcastService::sendToConnections(
    const std::vector<std::string>& conn_ids,
    const std::string& message
) {
    size_t successful = 0;

    for (const auto& conn_id : conn_ids) {
        if (sendToConnection(conn_id, message)) {
            successful++;
        }
    }

    return successful;
}

} // namespace wtl::core::websocket
