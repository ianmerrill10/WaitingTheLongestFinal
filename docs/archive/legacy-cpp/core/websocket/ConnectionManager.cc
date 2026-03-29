/**
 * @file ConnectionManager.cc
 * @brief Implementation of WebSocket connection management
 * @see ConnectionManager.h for documentation
 */

#include "core/websocket/ConnectionManager.h"

#include <json/json.h>

namespace wtl::core::websocket {

// ============================================================================
// SINGLETON
// ============================================================================

ConnectionManager& ConnectionManager::getInstance() {
    static ConnectionManager instance;
    return instance;
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

void ConnectionManager::addConnection(
    const std::string& conn_id,
    const drogon::WebSocketConnectionPtr& conn
) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[conn_id] = conn;
}

void ConnectionManager::removeConnection(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove from connections
    connections_.erase(conn_id);

    // Clean up dog subscriptions
    if (auto it = connection_dogs_.find(conn_id); it != connection_dogs_.end()) {
        for (const auto& dog_id : it->second) {
            if (auto dog_it = dog_subscribers_.find(dog_id); dog_it != dog_subscribers_.end()) {
                dog_it->second.erase(conn_id);
                if (dog_it->second.empty()) {
                    dog_subscribers_.erase(dog_it);
                }
            }
        }
        connection_dogs_.erase(it);
    }

    // Clean up shelter subscriptions
    if (auto it = connection_shelters_.find(conn_id); it != connection_shelters_.end()) {
        for (const auto& shelter_id : it->second) {
            if (auto shelter_it = shelter_subscribers_.find(shelter_id);
                shelter_it != shelter_subscribers_.end()) {
                shelter_it->second.erase(conn_id);
                if (shelter_it->second.empty()) {
                    shelter_subscribers_.erase(shelter_it);
                }
            }
        }
        connection_shelters_.erase(it);
    }

    // Clean up urgent subscriptions
    urgent_subscribers_.erase(conn_id);
}

drogon::WebSocketConnectionPtr ConnectionManager::getConnection(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(conn_id);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

size_t ConnectionManager::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.size();
}

// ============================================================================
// DOG SUBSCRIPTIONS
// ============================================================================

void ConnectionManager::subscribeToDog(const std::string& conn_id, const std::string& dog_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if connection exists
    if (connections_.find(conn_id) == connections_.end()) {
        return;
    }

    // Add to dog subscribers
    dog_subscribers_[dog_id].insert(conn_id);

    // Add to connection's dog list (for cleanup)
    connection_dogs_[conn_id].insert(dog_id);
}

void ConnectionManager::unsubscribeFromDog(const std::string& conn_id, const std::string& dog_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove from dog subscribers
    if (auto it = dog_subscribers_.find(dog_id); it != dog_subscribers_.end()) {
        it->second.erase(conn_id);
        if (it->second.empty()) {
            dog_subscribers_.erase(it);
        }
    }

    // Remove from connection's dog list
    if (auto it = connection_dogs_.find(conn_id); it != connection_dogs_.end()) {
        it->second.erase(dog_id);
        if (it->second.empty()) {
            connection_dogs_.erase(it);
        }
    }
}

std::vector<std::string> ConnectionManager::getSubscribersForDog(const std::string& dog_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;

    if (auto it = dog_subscribers_.find(dog_id); it != dog_subscribers_.end()) {
        result.reserve(it->second.size());
        for (const auto& conn_id : it->second) {
            result.push_back(conn_id);
        }
    }

    return result;
}

std::set<std::string> ConnectionManager::getDogsWithSubscribers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::set<std::string> result;

    for (const auto& [dog_id, subscribers] : dog_subscribers_) {
        if (!subscribers.empty()) {
            result.insert(dog_id);
        }
    }

    return result;
}

// ============================================================================
// SHELTER SUBSCRIPTIONS
// ============================================================================

void ConnectionManager::subscribeToShelter(
    const std::string& conn_id,
    const std::string& shelter_id
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if connection exists
    if (connections_.find(conn_id) == connections_.end()) {
        return;
    }

    // Add to shelter subscribers
    shelter_subscribers_[shelter_id].insert(conn_id);

    // Add to connection's shelter list (for cleanup)
    connection_shelters_[conn_id].insert(shelter_id);
}

void ConnectionManager::unsubscribeFromShelter(
    const std::string& conn_id,
    const std::string& shelter_id
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove from shelter subscribers
    if (auto it = shelter_subscribers_.find(shelter_id); it != shelter_subscribers_.end()) {
        it->second.erase(conn_id);
        if (it->second.empty()) {
            shelter_subscribers_.erase(it);
        }
    }

    // Remove from connection's shelter list
    if (auto it = connection_shelters_.find(conn_id); it != connection_shelters_.end()) {
        it->second.erase(shelter_id);
        if (it->second.empty()) {
            connection_shelters_.erase(it);
        }
    }
}

std::vector<std::string> ConnectionManager::getSubscribersForShelter(
    const std::string& shelter_id
) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;

    if (auto it = shelter_subscribers_.find(shelter_id); it != shelter_subscribers_.end()) {
        result.reserve(it->second.size());
        for (const auto& conn_id : it->second) {
            result.push_back(conn_id);
        }
    }

    return result;
}

// ============================================================================
// URGENT SUBSCRIPTIONS
// ============================================================================

void ConnectionManager::subscribeToUrgent(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if connection exists
    if (connections_.find(conn_id) == connections_.end()) {
        return;
    }

    urgent_subscribers_.insert(conn_id);
}

void ConnectionManager::unsubscribeFromUrgent(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    urgent_subscribers_.erase(conn_id);
}

std::vector<std::string> ConnectionManager::getUrgentSubscribers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(urgent_subscribers_.size());

    for (const auto& conn_id : urgent_subscribers_) {
        result.push_back(conn_id);
    }

    return result;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value ConnectionManager::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    stats["total_connections"] = static_cast<Json::UInt64>(connections_.size());
    stats["dogs_with_subscribers"] = static_cast<Json::UInt64>(dog_subscribers_.size());
    stats["shelters_with_subscribers"] = static_cast<Json::UInt64>(shelter_subscribers_.size());
    stats["urgent_subscribers"] = static_cast<Json::UInt64>(urgent_subscribers_.size());

    // Count total subscriptions
    size_t total_dog_subs = 0;
    for (const auto& [dog_id, subs] : dog_subscribers_) {
        total_dog_subs += subs.size();
    }
    stats["total_dog_subscriptions"] = static_cast<Json::UInt64>(total_dog_subs);

    size_t total_shelter_subs = 0;
    for (const auto& [shelter_id, subs] : shelter_subscribers_) {
        total_shelter_subs += subs.size();
    }
    stats["total_shelter_subscriptions"] = static_cast<Json::UInt64>(total_shelter_subs);

    return stats;
}

} // namespace wtl::core::websocket
