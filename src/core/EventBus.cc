/**
 * @file EventBus.cc
 * @brief Implementation of the EventBus singleton
 * @see EventBus.h for documentation
 */

#include "EventBus.h"

// Standard library includes
#include <algorithm>
#include <iostream>
#include <sstream>

namespace wtl::core {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

EventBus& EventBus::getInstance() {
    static EventBus instance;
    return instance;
}

EventBus::EventBus() {
    // Initialize to non-running state
    running_ = false;
}

EventBus::~EventBus() {
    // Ensure we stop cleanly
    if (running_) {
        stop();
    }
}

// ============================================================================
// SUBSCRIPTION MANAGEMENT
// ============================================================================

std::string EventBus::subscribe(EventType type, EventHandler handler) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    Subscription sub;
    sub.id = generateSubscriptionId();
    sub.type = type;
    sub.handler = std::move(handler);
    sub.all_events = false;

    subscriptions_[type].push_back(std::move(sub));

    return subscriptions_[type].back().id;
}

std::string EventBus::subscribeAll(EventHandler handler) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    Subscription sub;
    sub.id = generateSubscriptionId();
    sub.type = EventType::DOG_CREATED; // Placeholder, not used
    sub.handler = std::move(handler);
    sub.all_events = true;

    all_event_subscriptions_.push_back(std::move(sub));

    return all_event_subscriptions_.back().id;
}

void EventBus::unsubscribe(const std::string& subscription_id) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    // Search in type-specific subscriptions
    for (auto& [type, subs] : subscriptions_) {
        auto it = std::remove_if(subs.begin(), subs.end(),
            [&subscription_id](const Subscription& s) {
                return s.id == subscription_id;
            });
        if (it != subs.end()) {
            subs.erase(it, subs.end());
            return;
        }
    }

    // Search in all-events subscriptions
    auto it = std::remove_if(all_event_subscriptions_.begin(),
                              all_event_subscriptions_.end(),
        [&subscription_id](const Subscription& s) {
            return s.id == subscription_id;
        });
    if (it != all_event_subscriptions_.end()) {
        all_event_subscriptions_.erase(it, all_event_subscriptions_.end());
    }
}

std::string EventBus::generateSubscriptionId() {
    int counter = subscription_counter_++;
    std::ostringstream oss;
    oss << "sub_" << counter << "_" << std::chrono::system_clock::now().time_since_epoch().count();
    return oss.str();
}

// ============================================================================
// EVENT PUBLISHING
// ============================================================================

void EventBus::publish(const Event& event) {
    events_published_++;

    // Make a copy of handlers to avoid holding lock during callbacks
    std::vector<EventHandler> handlers;

    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);

        // Collect type-specific handlers
        auto it = subscriptions_.find(event.type);
        if (it != subscriptions_.end()) {
            for (const auto& sub : it->second) {
                handlers.push_back(sub.handler);
            }
        }

        // Collect wildcard handlers
        for (const auto& sub : all_event_subscriptions_) {
            handlers.push_back(sub.handler);
        }
    }

    // Invoke all handlers outside the lock
    for (const auto& handler : handlers) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            // Log error but don't propagate - one handler's failure
            // shouldn't affect others
            std::cerr << "[EventBus] Handler exception for "
                      << eventTypeToString(event.type) << ": "
                      << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[EventBus] Unknown handler exception for "
                      << eventTypeToString(event.type) << std::endl;
        }
    }
}

void EventBus::publishAsync(const Event& event) {
    if (!running_) {
        // If not running, fall back to synchronous publishing
        // This ensures events are not lost
        publish(event);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        async_queue_.push(event);
    }
    queue_cv_.notify_one();
}

void EventBus::publishBatch(const std::vector<Event>& events) {
    for (const auto& event : events) {
        publish(event);
    }
}

// ============================================================================
// ASYNC PROCESSING
// ============================================================================

void EventBus::processAsyncEvents() {
    while (running_) {
        Event event;
        bool has_event = false;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // Wait for event or stop signal
            queue_cv_.wait(lock, [this] {
                return !async_queue_.empty() || !running_;
            });

            // Check if we should stop
            if (!running_ && async_queue_.empty()) {
                break;
            }

            // Get event from queue
            if (!async_queue_.empty()) {
                event = async_queue_.front();
                async_queue_.pop();
                has_event = true;
            }
        }

        // Process event outside the lock
        if (has_event) {
            // Use the same publish logic for consistency
            // but track as processed
            std::vector<EventHandler> handlers;

            {
                std::lock_guard<std::mutex> lock(subscriptions_mutex_);

                auto it = subscriptions_.find(event.type);
                if (it != subscriptions_.end()) {
                    for (const auto& sub : it->second) {
                        handlers.push_back(sub.handler);
                    }
                }

                for (const auto& sub : all_event_subscriptions_) {
                    handlers.push_back(sub.handler);
                }
            }

            for (const auto& handler : handlers) {
                try {
                    handler(event);
                } catch (const std::exception& e) {
                    std::cerr << "[EventBus] Async handler exception for "
                              << eventTypeToString(event.type) << ": "
                              << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[EventBus] Unknown async handler exception for "
                              << eventTypeToString(event.type) << std::endl;
                }
            }

            events_processed_++;
        }
    }

    // Process any remaining events in queue before exiting
    while (true) {
        Event event;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (async_queue_.empty()) {
                break;
            }
            event = async_queue_.front();
            async_queue_.pop();
        }

        // Process remaining events synchronously
        publish(event);
        events_processed_++;
    }
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void EventBus::start() {
    if (running_) {
        return; // Already running
    }

    running_ = true;
    processor_thread_ = std::thread(&EventBus::processAsyncEvents, this);
}

void EventBus::stop() {
    if (!running_) {
        return; // Not running
    }

    running_ = false;
    queue_cv_.notify_all();

    if (processor_thread_.joinable()) {
        processor_thread_.join();
    }
}

bool EventBus::isRunning() const {
    return running_;
}

// ============================================================================
// STATISTICS
// ============================================================================

int EventBus::getSubscriberCount(EventType type) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    auto it = subscriptions_.find(type);
    if (it != subscriptions_.end()) {
        return static_cast<int>(it->second.size());
    }
    return 0;
}

int EventBus::getTotalSubscriberCount() {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    int count = 0;
    for (const auto& [type, subs] : subscriptions_) {
        count += static_cast<int>(subs.size());
    }
    count += static_cast<int>(all_event_subscriptions_.size());
    return count;
}

Json::Value EventBus::getStatistics() {
    Json::Value stats;

    stats["events_published"] = events_published_.load();
    stats["events_processed"] = events_processed_.load();
    stats["subscriber_count"] = getTotalSubscriberCount();
    stats["is_running"] = running_.load();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stats["queue_size"] = static_cast<int>(async_queue_.size());
    }

    // Subscriber breakdown by type
    Json::Value type_counts(Json::objectValue);
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        for (const auto& [type, subs] : subscriptions_) {
            type_counts[eventTypeToString(type)] = static_cast<int>(subs.size());
        }
    }
    stats["subscribers_by_type"] = type_counts;
    stats["wildcard_subscribers"] = static_cast<int>(all_event_subscriptions_.size());

    return stats;
}

} // namespace wtl::core
