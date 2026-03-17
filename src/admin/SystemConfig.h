/**
 * @file SystemConfig.h
 * @brief Database-backed system configuration management
 *
 * PURPOSE:
 * Provides runtime configuration management that persists to database.
 * Allows admins to change system settings without restart and supports
 * feature flags for gradual rollouts.
 *
 * USAGE:
 * auto& config = SystemConfig::getInstance();
 * auto value = config.get("urgency.threshold.critical");
 * config.set("urgency.threshold.critical", "24");
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - Config (initial/fallback configuration)
 *
 * MODIFICATION GUIDE:
 * - Add validation rules for new config keys
 * - Update schema if adding new config categories
 * - All changes are persisted immediately
 *
 * @author Agent 20 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::admin {

/**
 * @struct ConfigValue
 * @brief Represents a configuration value with metadata
 */
struct ConfigValue {
    std::string key;
    std::string value;
    std::string category;
    std::string description;
    std::string value_type;     // string, int, bool, json
    bool is_sensitive;          // Should be masked in logs
    bool requires_restart;      // Requires app restart to take effect
    std::string created_at;
    std::string updated_at;
    std::string updated_by;     // User ID who last updated
};

/**
 * @struct FeatureFlag
 * @brief Represents a feature flag for gradual rollouts
 */
struct FeatureFlag {
    std::string key;
    std::string name;
    std::string description;
    bool enabled;
    int rollout_percentage;     // 0-100
    Json::Value conditions;     // Additional conditions (user roles, etc.)
    std::string created_at;
    std::string updated_at;
};

/**
 * @class SystemConfig
 * @brief Singleton for managing system configuration
 *
 * Thread-safe configuration management with:
 * - Database persistence
 * - In-memory caching
 * - Change validation
 * - Feature flags
 * - Hot reload support
 */
class SystemConfig {
public:
    // Type alias for change callback
    using ChangeCallback = std::function<void(const std::string& key,
                                              const std::string& old_value,
                                              const std::string& new_value)>;

    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the SystemConfig instance
     */
    static SystemConfig& getInstance();

    // Prevent copying and moving
    SystemConfig(const SystemConfig&) = delete;
    SystemConfig& operator=(const SystemConfig&) = delete;
    SystemConfig(SystemConfig&&) = delete;
    SystemConfig& operator=(SystemConfig&&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize the configuration system
     *
     * Loads all configuration from database into memory cache.
     * Should be called once at application startup.
     *
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Check if system is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    // ========================================================================
    // BASIC OPERATIONS
    // ========================================================================

    /**
     * @brief Get a configuration value
     *
     * @param key Configuration key (e.g., "urgency.threshold.critical")
     * @return Value if exists, std::nullopt otherwise
     */
    std::optional<std::string> get(const std::string& key) const;

    /**
     * @brief Get a configuration value with default
     *
     * @param key Configuration key
     * @param default_value Default if key doesn't exist
     * @return Value or default
     */
    std::string get(const std::string& key, const std::string& default_value) const;

    /**
     * @brief Get configuration value as integer
     *
     * @param key Configuration key
     * @param default_value Default if key doesn't exist or isn't numeric
     * @return Integer value
     */
    int getInt(const std::string& key, int default_value = 0) const;

    /**
     * @brief Get configuration value as boolean
     *
     * @param key Configuration key
     * @param default_value Default if key doesn't exist
     * @return Boolean value
     */
    bool getBool(const std::string& key, bool default_value = false) const;

    /**
     * @brief Get configuration value as double
     *
     * @param key Configuration key
     * @param default_value Default if key doesn't exist
     * @return Double value
     */
    double getDouble(const std::string& key, double default_value = 0.0) const;

    /**
     * @brief Get configuration value as JSON
     *
     * @param key Configuration key
     * @return JSON value or null
     */
    Json::Value getJson(const std::string& key) const;

    /**
     * @brief Set a configuration value
     *
     * Validates, persists to database, and updates cache.
     *
     * @param key Configuration key
     * @param value New value
     * @param updated_by User ID making the change (optional)
     * @return true if set successfully
     */
    bool set(const std::string& key, const std::string& value,
             const std::string& updated_by = "");

    /**
     * @brief Set a configuration value with full metadata
     *
     * @param config_value Full configuration value object
     * @return true if set successfully
     */
    bool set(const ConfigValue& config_value);

    /**
     * @brief Delete a configuration key
     *
     * @param key Configuration key to delete
     * @return true if deleted
     */
    bool remove(const std::string& key);

    /**
     * @brief Check if a configuration key exists
     *
     * @param key Configuration key
     * @return true if exists
     */
    bool exists(const std::string& key) const;

    // ========================================================================
    // BULK OPERATIONS
    // ========================================================================

    /**
     * @brief Get all configuration values
     *
     * @return JSON object with all config key-value pairs
     */
    Json::Value getAll() const;

    /**
     * @brief Get configuration values by category
     *
     * @param category Category name
     * @return JSON object with matching config values
     */
    Json::Value getByCategory(const std::string& category) const;

    /**
     * @brief Get all configuration with metadata
     *
     * @return Vector of ConfigValue objects
     */
    std::vector<ConfigValue> getAllWithMetadata() const;

    /**
     * @brief Set multiple configuration values
     *
     * @param values Map of key-value pairs
     * @param updated_by User ID making the changes
     * @return Number of values successfully set
     */
    int setMultiple(const std::unordered_map<std::string, std::string>& values,
                    const std::string& updated_by = "");

    // ========================================================================
    // FEATURE FLAGS
    // ========================================================================

    /**
     * @brief Check if a feature flag is enabled
     *
     * @param key Feature flag key
     * @param user_id Optional user ID for percentage rollout
     * @return true if feature is enabled for this context
     */
    bool isFeatureEnabled(const std::string& key,
                         const std::string& user_id = "") const;

    /**
     * @brief Get all feature flags
     *
     * @return JSON array of all feature flags
     */
    Json::Value getFeatureFlags() const;

    /**
     * @brief Get a specific feature flag
     *
     * @param key Feature flag key
     * @return FeatureFlag if exists
     */
    std::optional<FeatureFlag> getFeatureFlag(const std::string& key) const;

    /**
     * @brief Set a feature flag
     *
     * @param key Feature flag key
     * @param enabled Whether the flag is enabled
     * @param rollout_percentage Percentage of users (0-100)
     * @return true if set successfully
     */
    bool setFeatureFlag(const std::string& key, bool enabled,
                        int rollout_percentage = 100);

    /**
     * @brief Create a new feature flag
     *
     * @param flag Full feature flag object
     * @return true if created successfully
     */
    bool createFeatureFlag(const FeatureFlag& flag);

    /**
     * @brief Delete a feature flag
     *
     * @param key Feature flag key
     * @return true if deleted
     */
    bool deleteFeatureFlag(const std::string& key);

    // ========================================================================
    // VALIDATION
    // ========================================================================

    /**
     * @brief Validate a configuration value
     *
     * @param key Configuration key
     * @param value Value to validate
     * @return Empty string if valid, error message otherwise
     */
    std::string validate(const std::string& key, const std::string& value) const;

    /**
     * @brief Register a custom validator for a key
     *
     * @param key Configuration key
     * @param validator Validation function (returns error message or empty)
     */
    void registerValidator(const std::string& key,
                          std::function<std::string(const std::string&)> validator);

    // ========================================================================
    // RELOAD AND SYNC
    // ========================================================================

    /**
     * @brief Reload all configuration from database
     *
     * Refreshes the in-memory cache from database.
     * Use when you know external changes have been made.
     *
     * @return true if reload successful
     */
    bool reload();

    /**
     * @brief Sync in-memory cache to database
     *
     * Ensures all cached values are persisted.
     *
     * @return true if sync successful
     */
    bool sync();

    // ========================================================================
    // CHANGE NOTIFICATIONS
    // ========================================================================

    /**
     * @brief Register a callback for configuration changes
     *
     * @param callback Function to call when config changes
     * @return Callback ID for unregistering
     */
    std::string onChanged(ChangeCallback callback);

    /**
     * @brief Unregister a change callback
     *
     * @param callback_id ID returned from onChanged
     */
    void removeOnChanged(const std::string& callback_id);

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    SystemConfig();
    ~SystemConfig();

    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Load configuration from database
     */
    bool loadFromDatabase();

    /**
     * @brief Load feature flags from database
     */
    bool loadFeatureFlags();

    /**
     * @brief Save a configuration value to database
     */
    bool saveToDatabase(const ConfigValue& config_value);

    /**
     * @brief Delete a configuration value from database
     */
    bool deleteFromDatabase(const std::string& key);

    /**
     * @brief Notify all change callbacks
     */
    void notifyChanged(const std::string& key,
                      const std::string& old_value,
                      const std::string& new_value);

    /**
     * @brief Hash user ID for consistent percentage rollout
     */
    static int hashUserIdForRollout(const std::string& user_id,
                                    const std::string& flag_key);

    /**
     * @brief Initialize default configuration values
     */
    void initializeDefaults();

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    // In-memory cache of configuration values
    std::unordered_map<std::string, ConfigValue> config_cache_;

    // Feature flags cache
    std::unordered_map<std::string, FeatureFlag> feature_flags_;

    // Custom validators
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> validators_;

    // Change callbacks
    std::unordered_map<std::string, ChangeCallback> change_callbacks_;

    // State
    bool initialized_{false};

    // Thread safety
    mutable std::shared_mutex config_mutex_;
    mutable std::mutex callback_mutex_;

    // Callback ID counter
    uint64_t callback_id_counter_{0};
};

} // namespace wtl::admin
