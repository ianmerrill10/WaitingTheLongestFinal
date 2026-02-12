/**
 * @file IModule.h
 * @brief Interface for modular components in the WaitingTheLongest platform
 *
 * PURPOSE:
 * Defines the base interface that all modules must implement. Modules are
 * optional, self-contained features that can be enabled/disabled via
 * configuration without affecting the core platform.
 *
 * USAGE:
 * class MyModule : public IModule {
 * public:
 *     bool initialize() override;
 *     void shutdown() override;
 *     std::string getName() const override { return "my_module"; }
 *     // ... implement other pure virtual methods
 * };
 *
 * DEPENDENCIES:
 * - json/json.h (for configuration and status)
 *
 * MODIFICATION GUIDE:
 * - Keep the interface minimal; add methods only if needed by ALL modules
 * - Use configure() for module-specific settings
 * - Use getStatus() for monitoring and health checks
 * - Modules should be self-contained and not depend on other modules directly
 *
 * @author Agent 10 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::modules {

/**
 * @class IModule
 * @brief Base interface for all modules
 *
 * Modules provide optional functionality that can be enabled or disabled
 * at runtime. Each module manages its own lifecycle, configuration, and
 * event subscriptions.
 *
 * Module lifecycle:
 * 1. Module is registered with ModuleManager
 * 2. configure() is called with module settings
 * 3. initialize() is called when module is enabled
 * 4. onConfigUpdate() may be called if config changes at runtime
 * 5. shutdown() is called when module is disabled or app exits
 */
class IModule {
public:
    virtual ~IModule() = default;

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Initialize the module
     *
     * Called when the module is enabled. Modules should:
     * - Subscribe to events they're interested in
     * - Initialize internal state
     * - Validate configuration
     *
     * @return true if initialization succeeded, false otherwise
     *
     * @note If initialize() returns false, the module will not be marked as loaded
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown the module
     *
     * Called when the module is disabled or the application is shutting down.
     * Modules should:
     * - Unsubscribe from all events
     * - Release resources
     * - Clean up internal state
     */
    virtual void shutdown() = 0;

    /**
     * @brief Handle configuration updates
     *
     * Called when the module's configuration is updated at runtime.
     * Default implementation does nothing.
     *
     * @param config New configuration values
     */
    virtual void onConfigUpdate(const Json::Value& config) {
        (void)config; // Suppress unused parameter warning
    }

    // ========================================================================
    // IDENTITY
    // ========================================================================

    /**
     * @brief Get the module's unique name
     *
     * @return std::string The module name (e.g., "urgency_engine", "foster")
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Get the module's version
     *
     * @return std::string The version string (e.g., "1.0.0")
     */
    virtual std::string getVersion() const = 0;

    /**
     * @brief Get the module's description
     *
     * @return std::string Human-readable description of what the module does
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief Get list of module dependencies
     *
     * Returns the names of other modules this module depends on.
     * The ModuleManager will ensure dependencies are initialized first.
     *
     * @return std::vector<std::string> List of module names this module depends on
     *
     * @note Default implementation returns empty list (no dependencies)
     */
    virtual std::vector<std::string> getDependencies() const {
        return {};
    }

    // ========================================================================
    // STATUS
    // ========================================================================

    /**
     * @brief Check if the module is enabled
     *
     * @return true if the module is enabled, false otherwise
     */
    virtual bool isEnabled() const = 0;

    /**
     * @brief Check if the module is healthy
     *
     * Called periodically by health check systems.
     * Default implementation returns true if module is enabled.
     *
     * @return true if the module is functioning properly
     */
    virtual bool isHealthy() const {
        return isEnabled();
    }

    /**
     * @brief Get the module's current status
     *
     * @return Json::Value Status information including:
     *   - name: Module name
     *   - version: Module version
     *   - enabled: Whether module is enabled
     *   - healthy: Whether module is healthy
     *   - (module-specific additional fields)
     */
    virtual Json::Value getStatus() const = 0;

    /**
     * @brief Get the module's metrics
     *
     * Returns performance and usage metrics for monitoring.
     * Default implementation returns empty object.
     *
     * @return Json::Value Metrics data (module-specific)
     */
    virtual Json::Value getMetrics() const {
        return Json::Value(Json::objectValue);
    }

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Configure the module
     *
     * Called with the module's configuration from the config file.
     * Should be called before initialize().
     *
     * @param config Module-specific configuration values
     */
    virtual void configure(const Json::Value& config) = 0;

    /**
     * @brief Get default configuration values
     *
     * Returns the default configuration for this module.
     * Used when no configuration is provided.
     *
     * @return Json::Value Default configuration values
     */
    virtual Json::Value getDefaultConfig() const {
        return Json::Value(Json::objectValue);
    }
};

} // namespace wtl::modules
