/**
 * @file Config.h
 * @brief Singleton configuration loader for WaitingTheLongest application
 *
 * PURPOSE:
 * Provides centralized, thread-safe access to application configuration.
 * Loads configuration from JSON files and provides typed getters for all
 * configuration values. Validates required fields on load.
 *
 * USAGE:
 * // Initialize at startup
 * Config::getInstance().load("config/config.json");
 *
 * // Access configuration values
 * int port = Config::getInstance().getInt("server.port");
 * std::string host = Config::getInstance().getString("database.host");
 *
 * DEPENDENCIES:
 * - jsoncpp (JSON parsing)
 * - Standard library (filesystem, mutex)
 *
 * MODIFICATION GUIDE:
 * - Add new configuration keys by updating config.example.json
 * - Add validation rules in validateConfig() method
 * - All public getters are thread-safe
 *
 * @author Agent 1 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <fstream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::core::utils {

/**
 * @class Config
 * @brief Thread-safe singleton configuration manager
 *
 * Loads JSON configuration files and provides type-safe access to values.
 * Uses dot notation for nested keys (e.g., "server.port", "database.host").
 * Thread-safe for concurrent reads via shared_mutex.
 */
class Config {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance of Config
     * @return Reference to the Config singleton
     *
     * Thread-safe initialization via Meyer's singleton pattern.
     */
    static Config& getInstance();

    // Delete copy/move constructors and assignment operators
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;

    // =========================================================================
    // LOADING & VALIDATION
    // =========================================================================

    /**
     * @brief Load configuration from a JSON file
     *
     * @param file_path Path to the JSON configuration file
     * @return true if loaded successfully, false otherwise
     * @throws std::runtime_error if file cannot be read or parsed
     *
     * Validates required fields after loading. If validation fails,
     * the previous configuration is preserved.
     *
     * @example
     * if (!Config::getInstance().load("config/config.json")) {
     *     std::cerr << "Failed to load config" << std::endl;
     * }
     */
    bool load(const std::string& file_path);

    /**
     * @brief Reload configuration from the last loaded file
     *
     * @return true if reloaded successfully, false otherwise
     *
     * Useful for hot-reloading configuration without restart.
     */
    bool reload();

    /**
     * @brief Check if configuration has been loaded
     * @return true if configuration is loaded, false otherwise
     */
    bool isLoaded() const;

    /**
     * @brief Get the path of the currently loaded config file
     * @return Path to the config file, or empty string if not loaded
     */
    std::string getConfigPath() const;

    // =========================================================================
    // STRING GETTERS
    // =========================================================================

    /**
     * @brief Get a string value from configuration
     *
     * @param key Dot-notation key (e.g., "server.host")
     * @param default_value Value to return if key not found
     * @return The configuration value or default
     *
     * @example
     * std::string host = config.getString("database.host", "localhost");
     */
    std::string getString(const std::string& key,
                          const std::string& default_value = "") const;

    /**
     * @brief Get a required string value (throws if missing)
     *
     * @param key Dot-notation key
     * @return The configuration value
     * @throws std::runtime_error if key not found
     */
    std::string getStringRequired(const std::string& key) const;

    // =========================================================================
    // INTEGER GETTERS
    // =========================================================================

    /**
     * @brief Get an integer value from configuration
     *
     * @param key Dot-notation key
     * @param default_value Value to return if key not found
     * @return The configuration value or default
     */
    int getInt(const std::string& key, int default_value = 0) const;

    /**
     * @brief Get a required integer value (throws if missing)
     *
     * @param key Dot-notation key
     * @return The configuration value
     * @throws std::runtime_error if key not found
     */
    int getIntRequired(const std::string& key) const;

    // =========================================================================
    // DOUBLE GETTERS
    // =========================================================================

    /**
     * @brief Get a double value from configuration
     *
     * @param key Dot-notation key
     * @param default_value Value to return if key not found
     * @return The configuration value or default
     */
    double getDouble(const std::string& key, double default_value = 0.0) const;

    /**
     * @brief Get a required double value (throws if missing)
     *
     * @param key Dot-notation key
     * @return The configuration value
     * @throws std::runtime_error if key not found
     */
    double getDoubleRequired(const std::string& key) const;

    // =========================================================================
    // BOOLEAN GETTERS
    // =========================================================================

    /**
     * @brief Get a boolean value from configuration
     *
     * @param key Dot-notation key
     * @param default_value Value to return if key not found
     * @return The configuration value or default
     */
    bool getBool(const std::string& key, bool default_value = false) const;

    /**
     * @brief Get a required boolean value (throws if missing)
     *
     * @param key Dot-notation key
     * @return The configuration value
     * @throws std::runtime_error if key not found
     */
    bool getBoolRequired(const std::string& key) const;

    // =========================================================================
    // ARRAY GETTERS
    // =========================================================================

    /**
     * @brief Get a string array from configuration
     *
     * @param key Dot-notation key
     * @return Vector of strings, empty if key not found
     */
    std::vector<std::string> getStringArray(const std::string& key) const;

    /**
     * @brief Get an integer array from configuration
     *
     * @param key Dot-notation key
     * @return Vector of integers, empty if key not found
     */
    std::vector<int> getIntArray(const std::string& key) const;

    // =========================================================================
    // JSON GETTERS
    // =========================================================================

    /**
     * @brief Get a JSON object/value from configuration
     *
     * @param key Dot-notation key
     * @return The JSON value, or null value if not found
     *
     * Useful for accessing complex nested structures.
     */
    Json::Value getJson(const std::string& key) const;

    /**
     * @brief Get the entire configuration as JSON
     * @return The complete configuration object
     */
    Json::Value getAllConfig() const;

    // =========================================================================
    // KEY EXISTENCE
    // =========================================================================

    /**
     * @brief Check if a key exists in configuration
     *
     * @param key Dot-notation key
     * @return true if key exists, false otherwise
     */
    bool hasKey(const std::string& key) const;

    // =========================================================================
    // CONVENIENCE METHODS
    // =========================================================================

    /**
     * @brief Get database connection string for pqxx
     * @return PostgreSQL connection string built from config values
     *
     * Builds: "host=X port=X dbname=X user=X password=X ..."
     */
    std::string getDatabaseConnectionString() const;

    /**
     * @brief Check if running in production mode
     * @return true if environment.is_production is true
     */
    bool isProduction() const;

    /**
     * @brief Check if debug mode is enabled
     * @return true if environment.debug_mode is true
     */
    bool isDebugMode() const;

    /**
     * @brief Get the environment name
     * @return Environment name (development, staging, production)
     */
    std::string getEnvironment() const;

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    Config() = default;
    ~Config() = default;

    /**
     * @brief Navigate to a JSON value using dot notation
     *
     * @param key Dot-notation key (e.g., "server.port")
     * @return Optional containing the value, or nullopt if not found
     */
    std::optional<Json::Value> getValue(const std::string& key) const;

    /**
     * @brief Validate that all required configuration keys are present
     *
     * @return true if all required keys are present, false otherwise
     *
     * Logs missing keys for debugging.
     */
    bool validateConfig() const;

    /**
     * @brief Split a dot-notation key into parts
     *
     * @param key The key to split (e.g., "server.port")
     * @return Vector of key parts (e.g., ["server", "port"])
     */
    std::vector<std::string> splitKey(const std::string& key) const;

    // Configuration data
    Json::Value config_;

    // Path to the loaded config file
    std::string config_path_;

    // Flag indicating if config has been loaded
    std::atomic<bool> loaded_{false};

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Required configuration keys that must be present
    static const std::unordered_set<std::string> REQUIRED_KEYS;
};

} // namespace wtl::core::utils
