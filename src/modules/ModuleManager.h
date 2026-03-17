/**
 * @file ModuleManager.h
 * @brief Singleton manager for all modules in the WaitingTheLongest platform
 *
 * PURPOSE:
 * Central management of all modules including registration, lifecycle,
 * configuration, and status monitoring. Handles module dependencies and
 * ensures proper initialization order.
 *
 * USAGE:
 * // Register a module
 * ModuleManager::getInstance().registerModule("mymodule",
 *     std::make_unique<MyModule>());
 *
 * // Initialize all enabled modules
 * ModuleManager::getInstance().loadConfig("config.json");
 * ModuleManager::getInstance().initializeAll();
 *
 * // Access a module
 * auto* module = ModuleManager::getInstance().getModuleAs<MyModule>("mymodule");
 *
 * // Shutdown
 * ModuleManager::getInstance().shutdownAll();
 *
 * DEPENDENCIES:
 * - IModule.h (module interface)
 * - json/json.h (configuration)
 *
 * MODIFICATION GUIDE:
 * - ModuleManager is a singleton; do not create instances directly
 * - Register modules before calling initializeAll()
 * - Modules with dependencies are initialized after their dependencies
 *
 * @author Agent 10 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "IModule.h"

namespace wtl::modules {

/**
 * @class ModuleManager
 * @brief Singleton manager for module lifecycle and configuration
 *
 * Thread-safe implementation that manages:
 * - Module registration and unregistration
 * - Configuration loading and application
 * - Module lifecycle (initialize, shutdown)
 * - Dependency resolution
 * - Status and metrics aggregation
 */
class ModuleManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the ModuleManager singleton
     */
    static ModuleManager& getInstance();

    // ========================================================================
    // REGISTRATION
    // ========================================================================

    /**
     * @brief Register a module
     *
     * @param name Unique name for the module
     * @param module The module instance
     *
     * @note If a module with the same name already exists, it will be replaced
     *       after being shut down.
     *
     * @example
     * ModuleManager::getInstance().registerModule("urgency_engine",
     *     std::make_unique<UrgencyEngineModule>());
     */
    void registerModule(const std::string& name, std::unique_ptr<IModule> module);

    /**
     * @brief Unregister a module
     *
     * Shuts down the module if it's loaded, then removes it from registration.
     *
     * @param name The module name to unregister
     */
    void unregisterModule(const std::string& name);

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * @brief Initialize all enabled modules
     *
     * Initializes modules in dependency order. If a module fails to initialize,
     * it will be marked as not loaded and an error will be logged.
     */
    void initializeAll();

    /**
     * @brief Shutdown all loaded modules
     *
     * Shuts down modules in reverse initialization order.
     */
    void shutdownAll();

    /**
     * @brief Initialize a specific module
     *
     * @param name The module name to initialize
     * @return true if initialization succeeded, false otherwise
     */
    bool initializeModule(const std::string& name);

    /**
     * @brief Shutdown a specific module
     *
     * @param name The module name to shutdown
     * @return true if shutdown succeeded, false otherwise
     */
    bool shutdownModule(const std::string& name);

    // ========================================================================
    // ACCESS
    // ========================================================================

    /**
     * @brief Get a module by name
     *
     * @param name The module name
     * @return IModule* Pointer to the module, or nullptr if not found
     */
    IModule* getModule(const std::string& name);

    /**
     * @brief Get a module by name with type casting
     *
     * @tparam T The module type to cast to
     * @param name The module name
     * @return T* Pointer to the module cast to type T, or nullptr if not found
     *             or if the cast fails
     *
     * @example
     * auto* urgency = ModuleManager::getInstance()
     *     .getModuleAs<UrgencyEngineModule>("urgency_engine");
     */
    template<typename T>
    T* getModuleAs(const std::string& name) {
        return dynamic_cast<T*>(getModule(name));
    }

    /**
     * @brief Get list of all registered module names
     *
     * @return std::vector<std::string> List of module names
     */
    std::vector<std::string> getRegisteredModules();

    /**
     * @brief Get list of enabled module names
     *
     * @return std::vector<std::string> List of enabled module names
     */
    std::vector<std::string> getEnabledModules();

    /**
     * @brief Get list of loaded (initialized) module names
     *
     * @return std::vector<std::string> List of loaded module names
     */
    std::vector<std::string> getLoadedModules();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Load configuration from a JSON file
     *
     * @param config_path Path to the configuration file
     *
     * @note The configuration file should have a "modules" section with
     *       settings for each module.
     */
    void loadConfig(const std::string& config_path);

    /**
     * @brief Load configuration from a JSON object
     *
     * @param config The configuration object
     */
    void loadConfig(const Json::Value& config);

    /**
     * @brief Check if a module is enabled in configuration
     *
     * @param name The module name
     * @return true if the module is enabled, false otherwise
     */
    bool isModuleEnabled(const std::string& name);

    /**
     * @brief Enable a module
     *
     * Marks the module as enabled and initializes it if not already loaded.
     *
     * @param name The module name to enable
     */
    void enableModule(const std::string& name);

    /**
     * @brief Disable a module
     *
     * Shuts down the module if loaded and marks it as disabled.
     *
     * @param name The module name to disable
     */
    void disableModule(const std::string& name);

    // ========================================================================
    // STATUS
    // ========================================================================

    /**
     * @brief Get status of all registered modules
     *
     * @return Json::Value Object with module names as keys and status objects
     *         as values
     */
    Json::Value getModuleStatuses();

    /**
     * @brief Get metrics from all loaded modules
     *
     * @return Json::Value Object with module names as keys and metrics objects
     *         as values
     */
    Json::Value getModuleMetrics();

    /**
     * @brief Check if a specific module is healthy
     *
     * @param name The module name
     * @return true if the module is healthy, false otherwise or if not found
     */
    bool isModuleHealthy(const std::string& name);

    // Prevent copying
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;

private:
    ModuleManager() = default;

    /**
     * @struct ModuleEntry
     * @brief Internal representation of a registered module
     */
    struct ModuleEntry {
        std::unique_ptr<IModule> module;   ///< The module instance
        bool enabled = false;              ///< Whether the module is enabled
        bool loaded = false;               ///< Whether the module is initialized
    };

    /**
     * @brief Check if a module's dependencies are satisfied
     *
     * @param name The module name to check
     * @return true if all dependencies are loaded, false otherwise
     */
    bool checkDependencies(const std::string& name);

    /**
     * @brief Initialize dependencies for a module
     *
     * Recursively initializes all dependencies before the module itself.
     *
     * @param name The module name
     */
    void initializeDependencies(const std::string& name);

    /// Map of module name to module entry
    std::map<std::string, ModuleEntry> modules_;

    /// Module configuration
    Json::Value config_;

    /// Mutex for thread-safe access
    std::mutex mutex_;

    /// Tracks initialization order for proper shutdown
    std::vector<std::string> initialization_order_;
};

} // namespace wtl::modules
