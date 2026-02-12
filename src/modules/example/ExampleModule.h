/**
 * @file ExampleModule.h
 * @brief Example module demonstrating the module pattern
 *
 * PURPOSE:
 * Provides a complete, working example of how to implement a module
 * for the WaitingTheLongest platform. Other modules can use this as
 * a template for their implementations.
 *
 * USAGE:
 * This module is automatically registered by ModuleLoader.
 * Enable it in config.json:
 * {
 *     "modules": {
 *         "example": {
 *             "enabled": true,
 *             "log_events": true
 *         }
 *     }
 * }
 *
 * DEPENDENCIES:
 * - IModule.h (module interface)
 * - EventBus.h (event subscription)
 *
 * MODIFICATION GUIDE:
 * - Copy this file as a starting point for new modules
 * - Replace "example" with your module name throughout
 * - Add your module-specific configuration and logic
 * - Subscribe to events relevant to your module
 *
 * @author Agent 10 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "modules/IModule.h"
#include "core/EventBus.h"

namespace wtl::modules::example {

/**
 * @class ExampleModule
 * @brief A complete example module demonstrating the module pattern
 *
 * This module demonstrates:
 * - Proper implementation of IModule interface
 * - Event subscription and handling
 * - Configuration management
 * - Status reporting
 * - Metrics collection
 */
class ExampleModule : public IModule {
public:
    ExampleModule() = default;
    ~ExampleModule() override = default;

    // ========================================================================
    // IModule INTERFACE - LIFECYCLE
    // ========================================================================

    /**
     * @brief Initialize the module
     *
     * Sets up event subscriptions and initializes internal state.
     *
     * @return true if initialization succeeded
     */
    bool initialize() override;

    /**
     * @brief Shutdown the module
     *
     * Unsubscribes from events and cleans up resources.
     */
    void shutdown() override;

    /**
     * @brief Handle configuration updates at runtime
     *
     * @param config New configuration values
     */
    void onConfigUpdate(const Json::Value& config) override;

    // ========================================================================
    // IModule INTERFACE - IDENTITY
    // ========================================================================

    /**
     * @brief Get module name
     * @return "example"
     */
    std::string getName() const override { return "example"; }

    /**
     * @brief Get module version
     * @return Version string
     */
    std::string getVersion() const override { return "1.0.0"; }

    /**
     * @brief Get module description
     * @return Human-readable description
     */
    std::string getDescription() const override {
        return "Example module demonstrating the module pattern for WaitingTheLongest.com";
    }

    /**
     * @brief Get module dependencies
     * @return Empty list (no dependencies)
     */
    std::vector<std::string> getDependencies() const override {
        // Example module has no dependencies
        // Other modules might return: { "urgency_engine", "notifications" }
        return {};
    }

    // ========================================================================
    // IModule INTERFACE - STATUS
    // ========================================================================

    /**
     * @brief Check if module is enabled
     * @return true if enabled and running
     */
    bool isEnabled() const override { return enabled_; }

    /**
     * @brief Check if module is healthy
     * @return true if functioning properly
     */
    bool isHealthy() const override;

    /**
     * @brief Get module status
     * @return JSON object with status information
     */
    Json::Value getStatus() const override;

    /**
     * @brief Get module metrics
     * @return JSON object with performance metrics
     */
    Json::Value getMetrics() const override;

    // ========================================================================
    // IModule INTERFACE - CONFIGURATION
    // ========================================================================

    /**
     * @brief Configure the module
     * @param config Configuration from config.json
     */
    void configure(const Json::Value& config) override;

    /**
     * @brief Get default configuration
     * @return Default config values
     */
    Json::Value getDefaultConfig() const override;

private:
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    /**
     * @brief Handle DOG_CREATED events
     * @param event The event
     */
    void handleDogCreated(const core::Event& event);

    /**
     * @brief Handle DOG_UPDATED events
     * @param event The event
     */
    void handleDogUpdated(const core::Event& event);

    /**
     * @brief Handle DOG_BECAME_CRITICAL events
     * @param event The event
     */
    void handleDogBecameCritical(const core::Event& event);

    // ========================================================================
    // STATE
    // ========================================================================

    /// Whether the module is enabled
    bool enabled_ = false;

    /// Configuration: whether to log events
    bool log_events_ = false;

    /// Configuration: custom greeting message
    std::string greeting_message_ = "Hello from ExampleModule!";

    /// Subscription IDs for cleanup
    std::vector<std::string> subscription_ids_;

    /// Metrics: events received count
    std::atomic<int> events_received_{0};

    /// Metrics: dogs created count
    std::atomic<int> dogs_created_count_{0};

    /// Metrics: dogs updated count
    std::atomic<int> dogs_updated_count_{0};

    /// Metrics: critical alerts count
    std::atomic<int> critical_alerts_count_{0};
};

} // namespace wtl::modules::example
