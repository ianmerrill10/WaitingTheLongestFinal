/**
 * @file ModuleConfig.h
 * @brief Helper singleton for loading and managing module configurations
 *
 * PURPOSE:
 * Provides centralized access to module configuration settings loaded
 * from the application's config.json file. Allows runtime querying
 * and modification of module enable/disable states and settings.
 *
 * USAGE:
 * // Load configuration
 * ModuleConfig::getInstance().load("config.json");
 *
 * // Check if module is enabled
 * bool enabled = ModuleConfig::getInstance().isModuleEnabled("urgency_engine");
 *
 * // Get module-specific configuration
 * auto config = ModuleConfig::getInstance().getModuleConfig("urgency_engine");
 *
 * DEPENDENCIES:
 * - json/json.h (JSON parsing)
 *
 * MODIFICATION GUIDE:
 * - ModuleConfig is a singleton; do not create instances directly
 * - Configuration format is defined by the main config.json schema
 * - Changes made via set* methods are in-memory only until saveToFile() is called
 *
 * @author Agent 10 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::modules {

/**
 * @class ModuleConfig
 * @brief Singleton for module configuration management
 *
 * Thread-safe implementation that provides:
 * - Configuration loading from JSON files
 * - Module enable/disable status queries
 * - Module-specific configuration access
 * - Runtime configuration modification
 * - Configuration persistence
 */
class ModuleConfig {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the ModuleConfig singleton
     */
    static ModuleConfig& getInstance();

    // ========================================================================
    // LOADING
    // ========================================================================

    /**
     * @brief Load configuration from a JSON file
     *
     * @param config_path Path to the configuration file
     * @throws std::runtime_error if file cannot be read or parsed
     *
     * @note The file should have a "modules" section containing
     *       per-module configuration objects.
     *
     * @example
     * ModuleConfig::getInstance().load("config/config.json");
     */
    void load(const std::string& config_path);

    /**
     * @brief Load configuration from a JSON object
     *
     * @param root_config The root configuration object
     *
     * @example
     * Json::Value config;
     * // ... populate config ...
     * ModuleConfig::getInstance().load(config);
     */
    void load(const Json::Value& root_config);

    // ========================================================================
    // QUERY
    // ========================================================================

    /**
     * @brief Check if a module is enabled
     *
     * @param name The module name
     * @return true if the module is enabled, false otherwise or if not found
     */
    bool isModuleEnabled(const std::string& name) const;

    /**
     * @brief Get configuration for a specific module
     *
     * @param name The module name
     * @return Json::Value The module's configuration, or empty object if not found
     */
    Json::Value getModuleConfig(const std::string& name) const;

    /**
     * @brief Get list of all modules that are enabled
     *
     * @return std::vector<std::string> Names of enabled modules
     */
    std::vector<std::string> getEnabledModules() const;

    /**
     * @brief Get the entire modules configuration section
     *
     * @return Json::Value The modules configuration object
     */
    Json::Value getAllModulesConfig() const;

    /**
     * @brief Check if configuration has been loaded
     *
     * @return true if configuration has been loaded
     */
    bool isLoaded() const;

    // ========================================================================
    // MODIFY (RUNTIME)
    // ========================================================================

    /**
     * @brief Set module enabled state
     *
     * @param name The module name
     * @param enabled The new enabled state
     *
     * @note This changes the in-memory configuration only.
     *       Call saveToFile() to persist changes.
     */
    void setModuleEnabled(const std::string& name, bool enabled);

    /**
     * @brief Set module configuration
     *
     * @param name The module name
     * @param config The new configuration for the module
     *
     * @note This changes the in-memory configuration only.
     *       Call saveToFile() to persist changes.
     */
    void setModuleConfig(const std::string& name, const Json::Value& config);

    // ========================================================================
    // PERSISTENCE
    // ========================================================================

    /**
     * @brief Save current configuration to a file
     *
     * @param config_path Path to save the configuration file
     * @return true if saved successfully, false otherwise
     */
    bool saveToFile(const std::string& config_path);

    /**
     * @brief Get the full configuration as JSON
     *
     * @return Json::Value The complete configuration object
     */
    Json::Value getFullConfig() const;

    // Prevent copying
    ModuleConfig(const ModuleConfig&) = delete;
    ModuleConfig& operator=(const ModuleConfig&) = delete;

private:
    ModuleConfig() = default;

    /// The loaded configuration
    Json::Value config_;

    /// Mutex for thread-safe access
    mutable std::mutex mutex_;

    /// Flag indicating if configuration has been loaded
    bool loaded_ = false;
};

} // namespace wtl::modules
