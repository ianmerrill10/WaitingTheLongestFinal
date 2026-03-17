/**
 * @file EventBus.h
 * @brief Central event bus for publish-subscribe messaging
 *
 * PURPOSE:
 * Provides a centralized event bus for decoupled communication between
 * components and modules. Supports both synchronous and asynchronous
 * event publishing with subscription management.
 *
 * USAGE:
 * // Subscribe to events
 * auto sub_id = EventBus::getInstance().subscribe(
 *     EventType::DOG_CREATED,
 *     [](const Event& e) { handleDogCreated(e); }
 * );
 *
 * // Publish events (synchronous)
 * EventBus::getInstance().publish(event);
 *
 * // Publish events (asynchronous)
 * EventBus::getInstance().publishAsync(event);
 *
 * // Convenience function
 * emitEvent(EventType::DOG_CREATED, dog.id, "dog", data, "DogService");
 *
 * DEPENDENCIES:
 * - Event.h (event structure)
 * - EventType.h (event type enumeration)
 *
 * MODIFICATION GUIDE:
 * - The EventBus is a singleton; do not create instances directly
 * - Call start() before publishing async events
 * - Call stop() during application shutdown
 * - Subscription handlers should be lightweight; use async for heavy work
 *
 * @author Agent 10 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "Event.h"

namespace wtl::core {

/// Type alias for event handler callback functions
using EventHandler = std::function<void(const Event&)>;

/**
 * @class EventBus
 * @brief Singleton event bus for publish-subscribe messaging
 *
 * Thread-safe implementation supporting:
 * - Type-specific subscriptions
 * - Wildcard subscriptions (receive all events)
 * - Synchronous publishing (immediate delivery)
 * - Asynchronous publishing (background thread delivery)
 * - Statistics and monitoring
 */
class EventBus {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the EventBus singleton
     */
    static EventBus& getInstance();

    // ========================================================================
    // SUBSCRIPTION MANAGEMENT
    // ========================================================================

    /**
     * @brief Subscribe to a specific event type
     *
     * @param type The event type to subscribe to
     * @param handler Callback function to invoke when event is published
     * @return std::string Subscription ID for later unsubscription
     *
     * @example
     * auto id = EventBus::getInstance().subscribe(
     *     EventType::DOG_CREATED,
     *     [](const Event& e) { std::cout << "Dog created: " << e.entity_id; }
     * );
     */
    std::string subscribe(EventType type, EventHandler handler);

    /**
     * @brief Subscribe to a specific event type with a debug name
     *
     * @param type The event type to subscribe to
     * @param handler Callback function to invoke when event is published
     * @param handler_name Debug name for this subscription (for logging)
     * @return std::string Subscription ID for later unsubscription
     */
    std::string subscribe(EventType type, EventHandler handler, const std::string& handler_name) {
        (void)handler_name; // Used for debug logging only
        return subscribe(type, std::move(handler));
    }

    /**
     * @brief Subscribe to all event types (wildcard subscription)
     *
     * @param handler Callback function to invoke for every event
     * @return std::string Subscription ID for later unsubscription
     *
     * @example
     * auto id = EventBus::getInstance().subscribeAll(
     *     [](const Event& e) { logEvent(e); }
     * );
     */
    std::string subscribeAll(EventHandler handler);

    /**
     * @brief Unsubscribe from events
     *
     * @param subscription_id The ID returned from subscribe() or subscribeAll()
     *
     * @example
     * auto id = EventBus::getInstance().subscribe(...);
     * // Later:
     * EventBus::getInstance().unsubscribe(id);
     */
    void unsubscribe(const std::string& subscription_id);

    // ========================================================================
    // EVENT PUBLISHING
    // ========================================================================

    /**
     * @brief Publish event synchronously
     *
     * All subscribed handlers are called immediately in the current thread.
     * Use for events where immediate processing is required.
     *
     * @param event The event to publish
     *
     * @example
     * Event event;
     * event.type = EventType::DOG_CREATED;
     * // ... populate event ...
     * EventBus::getInstance().publish(event);
     */
    void publish(const Event& event);

    /**
     * @brief Publish event asynchronously
     *
     * Event is queued for processing by a background thread.
     * Use for events where immediate processing is not required.
     * Note: start() must be called before using async publishing.
     *
     * @param event The event to publish
     *
     * @example
     * EventBus::getInstance().start();
     * // ...
     * EventBus::getInstance().publishAsync(event);
     */
    void publishAsync(const Event& event);

    /**
     * @brief Publish multiple events synchronously
     *
     * @param events Vector of events to publish
     */
    void publishBatch(const std::vector<Event>& events);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get number of subscribers for a specific event type
     *
     * @param type The event type to query
     * @return int Number of subscribers
     */
    int getSubscriberCount(EventType type);

    /**
     * @brief Get total number of all subscribers
     *
     * @return int Total subscriber count across all event types
     */
    int getTotalSubscriberCount();

    /**
     * @brief Get comprehensive statistics
     *
     * @return Json::Value Statistics including:
     *   - events_published: Total events published
     *   - events_processed: Total async events processed
     *   - subscriber_count: Total subscribers
     *   - queue_size: Current async queue size
     *   - is_running: Whether async processor is running
     */
    Json::Value getStatistics();

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Start the async event processor thread
     *
     * Must be called before using publishAsync().
     */
    void start();

    /**
     * @brief Stop the async event processor thread
     *
     * Processes remaining queued events before stopping.
     * Should be called during application shutdown.
     */
    void stop();

    /**
     * @brief Check if async processor is running
     *
     * @return bool True if running, false otherwise
     */
    bool isRunning() const;

    // Prevent copying
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

private:
    EventBus();
    ~EventBus();

    /**
     * @brief Background thread function for async event processing
     */
    void processAsyncEvents();

    /**
     * @brief Generate a unique subscription ID
     * @return std::string Unique subscription identifier
     */
    std::string generateSubscriptionId();

    /**
     * @struct Subscription
     * @brief Internal representation of an event subscription
     */
    struct Subscription {
        std::string id;         ///< Unique subscription identifier
        EventType type;         ///< Event type (ignored if all_events is true)
        EventHandler handler;   ///< Callback function
        bool all_events;        ///< True if subscribed to all events
    };

    /// Map of event type to list of subscriptions
    std::map<EventType, std::vector<Subscription>> subscriptions_;

    /// List of subscriptions to all events (wildcard)
    std::vector<Subscription> all_event_subscriptions_;

    /// Mutex for subscription management
    std::mutex subscriptions_mutex_;

    /// Queue for async events
    std::queue<Event> async_queue_;

    /// Mutex for async queue
    std::mutex queue_mutex_;

    /// Condition variable for async queue
    std::condition_variable queue_cv_;

    /// Background processor thread
    std::thread processor_thread_;

    /// Flag indicating whether async processor is running
    std::atomic<bool> running_{false};

    /// Counter for subscription IDs
    std::atomic<int> subscription_counter_{0};

    /// Statistics: total events published
    std::atomic<int> events_published_{0};

    /// Statistics: total async events processed
    std::atomic<int> events_processed_{0};
};

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

/**
 * @brief Convenience function for emitting events synchronously
 *
 * @param type Event type
 * @param entity_id ID of affected entity
 * @param entity_type Type of entity (e.g., "dog", "shelter")
 * @param data Event-specific data payload
 * @param source Component that is emitting the event
 *
 * @example
 * emitEvent(EventType::DOG_CREATED, dog.id, "dog", dogData, "DogService");
 */
inline void emitEvent(EventType type,
                      const std::string& entity_id,
                      const std::string& entity_type = "",
                      const Json::Value& data = {},
                      const std::string& source = "core") {
    EventBus::getInstance().publish({
        type,
        entity_id,
        entity_type,
        data,
        std::chrono::system_clock::now(),
        source
    });
}

/**
 * @brief Convenience function for emitting events asynchronously
 *
 * @param type Event type
 * @param entity_id ID of affected entity
 * @param entity_type Type of entity (e.g., "dog", "shelter")
 * @param data Event-specific data payload
 * @param source Component that is emitting the event
 *
 * @example
 * emitEventAsync(EventType::DOG_UPDATED, dog.id, "dog", dogData, "DogService");
 */
inline void emitEventAsync(EventType type,
                           const std::string& entity_id,
                           const std::string& entity_type = "",
                           const Json::Value& data = {},
                           const std::string& source = "core") {
    EventBus::getInstance().publishAsync({
        type,
        entity_id,
        entity_type,
        data,
        std::chrono::system_clock::now(),
        source
    });
}

} // namespace wtl::core
