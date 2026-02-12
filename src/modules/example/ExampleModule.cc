/**
 * @file ExampleModule.cc
 * @brief Implementation of the ExampleModule
 * @see ExampleModule.h for documentation
 */

#include "ExampleModule.h"

// Standard library includes
#include <iostream>

namespace wtl::modules::example {

// ============================================================================
// LIFECYCLE
// ============================================================================

bool ExampleModule::initialize() {
    std::cout << "[ExampleModule] Initializing..." << std::endl;
    std::cout << "[ExampleModule] " << greeting_message_ << std::endl;

    // Subscribe to events we're interested in
    auto& bus = core::EventBus::getInstance();

    // Subscribe to DOG_CREATED events
    auto sub_id = bus.subscribe(
        core::EventType::DOG_CREATED,
        [this](const core::Event& event) {
            handleDogCreated(event);
        }
    );
    subscription_ids_.push_back(sub_id);

    // Subscribe to DOG_UPDATED events
    sub_id = bus.subscribe(
        core::EventType::DOG_UPDATED,
        [this](const core::Event& event) {
            handleDogUpdated(event);
        }
    );
    subscription_ids_.push_back(sub_id);

    // Subscribe to DOG_BECAME_CRITICAL events
    sub_id = bus.subscribe(
        core::EventType::DOG_BECAME_CRITICAL,
        [this](const core::Event& event) {
            handleDogBecameCritical(event);
        }
    );
    subscription_ids_.push_back(sub_id);

    enabled_ = true;
    std::cout << "[ExampleModule] Initialized successfully. "
              << "Subscribed to " << subscription_ids_.size() << " event type(s)."
              << std::endl;

    return true;
}

void ExampleModule::shutdown() {
    std::cout << "[ExampleModule] Shutting down..." << std::endl;

    // Unsubscribe from all events
    auto& bus = core::EventBus::getInstance();
    for (const auto& sub_id : subscription_ids_) {
        bus.unsubscribe(sub_id);
    }
    subscription_ids_.clear();

    enabled_ = false;

    std::cout << "[ExampleModule] Shutdown complete. Processed "
              << events_received_.load() << " total event(s)." << std::endl;
}

void ExampleModule::onConfigUpdate(const Json::Value& config) {
    std::cout << "[ExampleModule] Configuration updated." << std::endl;

    // Re-apply configuration
    configure(config);
}

// ============================================================================
// STATUS
// ============================================================================

bool ExampleModule::isHealthy() const {
    // For this example, healthy means enabled
    // Real modules might check database connections, external APIs, etc.
    return enabled_;
}

Json::Value ExampleModule::getStatus() const {
    Json::Value status;

    status["name"] = getName();
    status["version"] = getVersion();
    status["description"] = getDescription();
    status["enabled"] = enabled_;
    status["healthy"] = isHealthy();

    // Configuration
    Json::Value config_status;
    config_status["log_events"] = log_events_;
    config_status["greeting_message"] = greeting_message_;
    status["configuration"] = config_status;

    // Subscription info
    status["active_subscriptions"] = static_cast<int>(subscription_ids_.size());

    return status;
}

Json::Value ExampleModule::getMetrics() const {
    Json::Value metrics;

    metrics["events_received"] = events_received_.load();
    metrics["dogs_created_count"] = dogs_created_count_.load();
    metrics["dogs_updated_count"] = dogs_updated_count_.load();
    metrics["critical_alerts_count"] = critical_alerts_count_.load();

    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void ExampleModule::configure(const Json::Value& config) {
    // Extract configuration values with defaults
    log_events_ = config.get("log_events", false).asBool();
    greeting_message_ = config.get("greeting_message",
                                    "Hello from ExampleModule!").asString();

    if (log_events_) {
        std::cout << "[ExampleModule] Event logging enabled." << std::endl;
    }
}

Json::Value ExampleModule::getDefaultConfig() const {
    Json::Value config;

    config["enabled"] = false;
    config["log_events"] = false;
    config["greeting_message"] = "Hello from ExampleModule!";

    return config;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void ExampleModule::handleDogCreated(const core::Event& event) {
    events_received_++;
    dogs_created_count_++;

    if (log_events_) {
        std::cout << "[ExampleModule] DOG_CREATED event received:"
                  << " entity_id=" << event.entity_id
                  << " source=" << event.source
                  << std::endl;

        // Log dog name if available in data
        if (event.data.isMember("name")) {
            std::cout << "[ExampleModule]   Dog name: "
                      << event.data["name"].asString() << std::endl;
        }
    }

    // Example: You could do something with this event here
    // - Update a cache
    // - Send a notification
    // - Trigger a workflow
    // - Update analytics
}

void ExampleModule::handleDogUpdated(const core::Event& event) {
    events_received_++;
    dogs_updated_count_++;

    if (log_events_) {
        std::cout << "[ExampleModule] DOG_UPDATED event received:"
                  << " entity_id=" << event.entity_id
                  << " source=" << event.source
                  << std::endl;
    }
}

void ExampleModule::handleDogBecameCritical(const core::Event& event) {
    events_received_++;
    critical_alerts_count_++;

    // Always log critical events, regardless of log_events_ setting
    std::cout << "[ExampleModule] CRITICAL ALERT! Dog became critical:"
              << " entity_id=" << event.entity_id
              << std::endl;

    // In a real module, you might:
    // - Send urgent notifications
    // - Update a dashboard
    // - Trigger emergency workflows
    // - Alert on-call staff

    if (event.data.isMember("urgency_level")) {
        std::cout << "[ExampleModule]   Urgency level: "
                  << event.data["urgency_level"].asString() << std::endl;
    }

    if (event.data.isMember("days_until_euthanasia")) {
        int days = event.data["days_until_euthanasia"].asInt();
        std::cout << "[ExampleModule]   Days until euthanasia: " << days << std::endl;

        if (days <= 1) {
            std::cout << "[ExampleModule]   !!! IMMEDIATE ACTION REQUIRED !!!" << std::endl;
        }
    }
}

} // namespace wtl::modules::example
