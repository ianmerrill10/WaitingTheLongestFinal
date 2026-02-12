/**
 * @file SystemConfig.cc
 * @brief Implementation of SystemConfig
 * @see SystemConfig.h for documentation
 */

#include "admin/SystemConfig.h"

// Standard library includes
#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/utils/Config.h"

namespace wtl::admin {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

SystemConfig& SystemConfig::getInstance() {
    static SystemConfig instance;
    return instance;
}

SystemConfig::SystemConfig() = default;
SystemConfig::~SystemConfig() = default;

// ============================================================================
// INITIALIZATION
// ============================================================================

bool SystemConfig::initialize() {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);

    if (initialized_) {
        return true;
    }

    try {
        // Initialize default values first
        initializeDefaults();

        // Load from database (overrides defaults)
        if (!loadFromDatabase()) {
            WTL_CAPTURE_WARNING(ErrorCategory::CONFIGURATION,
                               "Failed to load config from database, using defaults",
                               {});
        }

        // Load feature flags
        if (!loadFeatureFlags()) {
            WTL_CAPTURE_WARNING(ErrorCategory::CONFIGURATION,
                               "Failed to load feature flags from database",
                               {});
        }

        initialized_ = true;
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONFIGURATION,
                         "Failed to initialize SystemConfig: " + std::string(e.what()),
                         {});
        return false;
    }
}

bool SystemConfig::isInitialized() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return initialized_;
}

void SystemConfig::initializeDefaults() {
    // Urgency thresholds
    ConfigValue critical;
    critical.key = "urgency.threshold.critical_hours";
    critical.value = "24";
    critical.category = "urgency";
    critical.description = "Hours before euthanasia to mark as CRITICAL";
    critical.value_type = "int";
    critical.is_sensitive = false;
    critical.requires_restart = false;
    config_cache_[critical.key] = critical;

    ConfigValue high;
    high.key = "urgency.threshold.high_hours";
    high.value = "72";
    high.category = "urgency";
    high.description = "Hours before euthanasia to mark as HIGH";
    high.value_type = "int";
    high.is_sensitive = false;
    high.requires_restart = false;
    config_cache_[high.key] = high;

    ConfigValue medium;
    medium.key = "urgency.threshold.medium_hours";
    medium.value = "168";
    medium.category = "urgency";
    medium.description = "Hours before euthanasia to mark as MEDIUM";
    medium.value_type = "int";
    medium.is_sensitive = false;
    medium.requires_restart = false;
    config_cache_[medium.key] = medium;

    // Worker intervals
    ConfigValue urgency_interval;
    urgency_interval.key = "worker.urgency.interval_minutes";
    urgency_interval.value = "15";
    urgency_interval.category = "workers";
    urgency_interval.description = "Minutes between urgency recalculations";
    urgency_interval.value_type = "int";
    urgency_interval.is_sensitive = false;
    urgency_interval.requires_restart = true;
    config_cache_[urgency_interval.key] = urgency_interval;

    // Pagination defaults
    ConfigValue default_page_size;
    default_page_size.key = "pagination.default_page_size";
    default_page_size.value = "20";
    default_page_size.category = "pagination";
    default_page_size.description = "Default items per page";
    default_page_size.value_type = "int";
    default_page_size.is_sensitive = false;
    default_page_size.requires_restart = false;
    config_cache_[default_page_size.key] = default_page_size;

    ConfigValue max_page_size;
    max_page_size.key = "pagination.max_page_size";
    max_page_size.value = "100";
    max_page_size.category = "pagination";
    max_page_size.description = "Maximum items per page";
    max_page_size.value_type = "int";
    max_page_size.is_sensitive = false;
    max_page_size.requires_restart = false;
    config_cache_[max_page_size.key] = max_page_size;

    // Session configuration
    ConfigValue access_token_ttl;
    access_token_ttl.key = "auth.access_token_ttl_hours";
    access_token_ttl.value = "1";
    access_token_ttl.category = "auth";
    access_token_ttl.description = "Access token time-to-live in hours";
    access_token_ttl.value_type = "int";
    access_token_ttl.is_sensitive = false;
    access_token_ttl.requires_restart = false;
    config_cache_[access_token_ttl.key] = access_token_ttl;

    ConfigValue refresh_token_ttl;
    refresh_token_ttl.key = "auth.refresh_token_ttl_days";
    refresh_token_ttl.value = "7";
    refresh_token_ttl.category = "auth";
    refresh_token_ttl.description = "Refresh token time-to-live in days";
    refresh_token_ttl.value_type = "int";
    refresh_token_ttl.is_sensitive = false;
    refresh_token_ttl.requires_restart = false;
    config_cache_[refresh_token_ttl.key] = refresh_token_ttl;

    // Site configuration
    ConfigValue site_name;
    site_name.key = "site.name";
    site_name.value = "WaitingTheLongest";
    site_name.category = "site";
    site_name.description = "Site display name";
    site_name.value_type = "string";
    site_name.is_sensitive = false;
    site_name.requires_restart = false;
    config_cache_[site_name.key] = site_name;

    ConfigValue site_tagline;
    site_tagline.key = "site.tagline";
    site_tagline.value = "Saving Dogs One Day at a Time";
    site_tagline.category = "site";
    site_tagline.description = "Site tagline";
    site_tagline.value_type = "string";
    site_tagline.is_sensitive = false;
    site_tagline.requires_restart = false;
    config_cache_[site_tagline.key] = site_tagline;

    // Notification settings
    ConfigValue email_notifications;
    email_notifications.key = "notifications.email_enabled";
    email_notifications.value = "true";
    email_notifications.category = "notifications";
    email_notifications.description = "Enable email notifications";
    email_notifications.value_type = "bool";
    email_notifications.is_sensitive = false;
    email_notifications.requires_restart = false;
    config_cache_[email_notifications.key] = email_notifications;

    ConfigValue critical_alert_email;
    critical_alert_email.key = "notifications.critical_alert_email";
    critical_alert_email.value = "";
    critical_alert_email.category = "notifications";
    critical_alert_email.description = "Email for critical dog alerts";
    critical_alert_email.value_type = "string";
    critical_alert_email.is_sensitive = false;
    critical_alert_email.requires_restart = false;
    config_cache_[critical_alert_email.key] = critical_alert_email;
}

bool SystemConfig::loadFromDatabase() {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT key, value, category, description, value_type, "
            "is_sensitive, requires_restart, created_at, updated_at, updated_by "
            "FROM system_config"
        );

        for (const auto& row : result) {
            ConfigValue cv;
            cv.key = row["key"].c_str();
            cv.value = row["value"].c_str();
            cv.category = row["category"].is_null() ? "" : row["category"].c_str();
            cv.description = row["description"].is_null() ? "" : row["description"].c_str();
            cv.value_type = row["value_type"].is_null() ? "string" : row["value_type"].c_str();
            cv.is_sensitive = row["is_sensitive"].as<bool>();
            cv.requires_restart = row["requires_restart"].as<bool>();
            cv.created_at = row["created_at"].is_null() ? "" : row["created_at"].c_str();
            cv.updated_at = row["updated_at"].is_null() ? "" : row["updated_at"].c_str();
            cv.updated_by = row["updated_by"].is_null() ? "" : row["updated_by"].c_str();

            config_cache_[cv.key] = cv;
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to load config from database: " + std::string(e.what()),
                         {});
        return false;
    }
}

bool SystemConfig::loadFeatureFlags() {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT key, name, description, enabled, rollout_percentage, "
            "conditions, created_at, updated_at "
            "FROM feature_flags"
        );

        for (const auto& row : result) {
            FeatureFlag flag;
            flag.key = row["key"].c_str();
            flag.name = row["name"].is_null() ? "" : row["name"].c_str();
            flag.description = row["description"].is_null() ? "" : row["description"].c_str();
            flag.enabled = row["enabled"].as<bool>();
            flag.rollout_percentage = row["rollout_percentage"].as<int>();

            // Parse conditions JSON
            if (!row["conditions"].is_null()) {
                Json::CharReaderBuilder builder;
                std::string conditions_str = row["conditions"].c_str();
                std::istringstream stream(conditions_str);
                std::string errors;
                Json::parseFromStream(builder, stream, &flag.conditions, &errors);
            }

            flag.created_at = row["created_at"].is_null() ? "" : row["created_at"].c_str();
            flag.updated_at = row["updated_at"].is_null() ? "" : row["updated_at"].c_str();

            feature_flags_[flag.key] = flag;
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to load feature flags: " + std::string(e.what()),
                         {});
        return false;
    }
}

// ============================================================================
// BASIC OPERATIONS
// ============================================================================

std::optional<std::string> SystemConfig::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    auto it = config_cache_.find(key);
    if (it != config_cache_.end()) {
        return it->second.value;
    }

    return std::nullopt;
}

std::string SystemConfig::get(const std::string& key, const std::string& default_value) const {
    auto value = get(key);
    return value ? *value : default_value;
}

int SystemConfig::getInt(const std::string& key, int default_value) const {
    auto value = get(key);
    if (!value) return default_value;

    try {
        return std::stoi(*value);
    } catch (...) {
        return default_value;
    }
}

bool SystemConfig::getBool(const std::string& key, bool default_value) const {
    auto value = get(key);
    if (!value) return default_value;

    std::string v = *value;
    return v == "true" || v == "1" || v == "yes" || v == "on";
}

double SystemConfig::getDouble(const std::string& key, double default_value) const {
    auto value = get(key);
    if (!value) return default_value;

    try {
        return std::stod(*value);
    } catch (...) {
        return default_value;
    }
}

Json::Value SystemConfig::getJson(const std::string& key) const {
    auto value = get(key);
    if (!value) return Json::Value();

    try {
        Json::CharReaderBuilder builder;
        std::istringstream stream(*value);
        Json::Value json;
        std::string errors;
        Json::parseFromStream(builder, stream, &json, &errors);
        return json;
    } catch (...) {
        return Json::Value();
    }
}

bool SystemConfig::set(const std::string& key, const std::string& value,
                       const std::string& updated_by) {
    // Validate
    std::string error = validate(key, value);
    if (!error.empty()) {
        WTL_CAPTURE_WARNING(ErrorCategory::VALIDATION,
                           "Config validation failed: " + error,
                           {{"key", key}, {"value", value}});
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(config_mutex_);

    // Get old value for notification
    std::string old_value;
    auto it = config_cache_.find(key);
    if (it != config_cache_.end()) {
        old_value = it->second.value;
    }

    // Create or update config value
    ConfigValue cv;
    if (it != config_cache_.end()) {
        cv = it->second;
    } else {
        cv.key = key;
        cv.category = "";
        cv.description = "";
        cv.value_type = "string";
        cv.is_sensitive = false;
        cv.requires_restart = false;
    }

    cv.value = value;
    cv.updated_by = updated_by;

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&time_t_now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    cv.updated_at = oss.str();

    if (cv.created_at.empty()) {
        cv.created_at = cv.updated_at;
    }

    // Save to database
    if (!saveToDatabase(cv)) {
        return false;
    }

    // Update cache
    config_cache_[key] = cv;

    lock.unlock();

    // Notify listeners
    notifyChanged(key, old_value, value);

    return true;
}

bool SystemConfig::set(const ConfigValue& config_value) {
    // Validate
    std::string error = validate(config_value.key, config_value.value);
    if (!error.empty()) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(config_mutex_);

    // Get old value
    std::string old_value;
    auto it = config_cache_.find(config_value.key);
    if (it != config_cache_.end()) {
        old_value = it->second.value;
    }

    // Save to database
    if (!saveToDatabase(config_value)) {
        return false;
    }

    // Update cache
    config_cache_[config_value.key] = config_value;

    lock.unlock();

    // Notify listeners
    notifyChanged(config_value.key, old_value, config_value.value);

    return true;
}

bool SystemConfig::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);

    auto it = config_cache_.find(key);
    if (it == config_cache_.end()) {
        return false;
    }

    std::string old_value = it->second.value;

    if (!deleteFromDatabase(key)) {
        return false;
    }

    config_cache_.erase(it);

    lock.unlock();

    notifyChanged(key, old_value, "");

    return true;
}

bool SystemConfig::exists(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return config_cache_.find(key) != config_cache_.end();
}

// ============================================================================
// BULK OPERATIONS
// ============================================================================

Json::Value SystemConfig::getAll() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    Json::Value result;
    for (const auto& [key, cv] : config_cache_) {
        if (!cv.is_sensitive) {
            result[key] = cv.value;
        } else {
            result[key] = "********";
        }
    }

    return result;
}

Json::Value SystemConfig::getByCategory(const std::string& category) const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    Json::Value result;
    for (const auto& [key, cv] : config_cache_) {
        if (cv.category == category) {
            if (!cv.is_sensitive) {
                result[key] = cv.value;
            } else {
                result[key] = "********";
            }
        }
    }

    return result;
}

std::vector<ConfigValue> SystemConfig::getAllWithMetadata() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    std::vector<ConfigValue> result;
    result.reserve(config_cache_.size());

    for (const auto& [key, cv] : config_cache_) {
        result.push_back(cv);
    }

    return result;
}

int SystemConfig::setMultiple(const std::unordered_map<std::string, std::string>& values,
                              const std::string& updated_by) {
    int success_count = 0;

    for (const auto& [key, value] : values) {
        if (set(key, value, updated_by)) {
            success_count++;
        }
    }

    return success_count;
}

// ============================================================================
// FEATURE FLAGS
// ============================================================================

bool SystemConfig::isFeatureEnabled(const std::string& key,
                                    const std::string& user_id) const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    auto it = feature_flags_.find(key);
    if (it == feature_flags_.end()) {
        return false;
    }

    const FeatureFlag& flag = it->second;

    if (!flag.enabled) {
        return false;
    }

    // Check rollout percentage
    if (flag.rollout_percentage < 100 && !user_id.empty()) {
        int user_bucket = hashUserIdForRollout(user_id, key);
        if (user_bucket >= flag.rollout_percentage) {
            return false;
        }
    } else if (flag.rollout_percentage < 100 && user_id.empty()) {
        // No user ID and partial rollout - return false for safety
        return false;
    }

    return true;
}

Json::Value SystemConfig::getFeatureFlags() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    Json::Value result(Json::arrayValue);
    for (const auto& [key, flag] : feature_flags_) {
        Json::Value item;
        item["key"] = flag.key;
        item["name"] = flag.name;
        item["description"] = flag.description;
        item["enabled"] = flag.enabled;
        item["rollout_percentage"] = flag.rollout_percentage;
        item["conditions"] = flag.conditions;
        item["created_at"] = flag.created_at;
        item["updated_at"] = flag.updated_at;
        result.append(item);
    }

    return result;
}

std::optional<FeatureFlag> SystemConfig::getFeatureFlag(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    auto it = feature_flags_.find(key);
    if (it != feature_flags_.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool SystemConfig::setFeatureFlag(const std::string& key, bool enabled,
                                  int rollout_percentage) {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time_t_now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        std::string timestamp = oss.str();

        txn.exec_params(
            "INSERT INTO feature_flags (key, enabled, rollout_percentage, updated_at) "
            "VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (key) DO UPDATE SET "
            "enabled = EXCLUDED.enabled, "
            "rollout_percentage = EXCLUDED.rollout_percentage, "
            "updated_at = EXCLUDED.updated_at",
            key, enabled, rollout_percentage, timestamp
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        // Update cache
        auto it = feature_flags_.find(key);
        if (it != feature_flags_.end()) {
            it->second.enabled = enabled;
            it->second.rollout_percentage = rollout_percentage;
            it->second.updated_at = timestamp;
        } else {
            FeatureFlag flag;
            flag.key = key;
            flag.enabled = enabled;
            flag.rollout_percentage = rollout_percentage;
            flag.created_at = timestamp;
            flag.updated_at = timestamp;
            feature_flags_[key] = flag;
        }

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to set feature flag: " + std::string(e.what()),
                         {{"key", key}});
        return false;
    }
}

bool SystemConfig::createFeatureFlag(const FeatureFlag& flag) {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Serialize conditions to JSON
        Json::StreamWriterBuilder builder;
        std::string conditions_json = Json::writeString(builder, flag.conditions);

        txn.exec_params(
            "INSERT INTO feature_flags (key, name, description, enabled, "
            "rollout_percentage, conditions, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, NOW(), NOW())",
            flag.key, flag.name, flag.description, flag.enabled,
            flag.rollout_percentage, conditions_json
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        // Update cache
        feature_flags_[flag.key] = flag;

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to create feature flag: " + std::string(e.what()),
                         {{"key", flag.key}});
        return false;
    }
}

bool SystemConfig::deleteFeatureFlag(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params("DELETE FROM feature_flags WHERE key = $1", key);

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        feature_flags_.erase(key);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to delete feature flag: " + std::string(e.what()),
                         {{"key", key}});
        return false;
    }
}

// ============================================================================
// VALIDATION
// ============================================================================

std::string SystemConfig::validate(const std::string& key, const std::string& value) const {
    // Check custom validators
    std::lock_guard<std::mutex> lock(callback_mutex_);

    auto it = validators_.find(key);
    if (it != validators_.end()) {
        return it->second(value);
    }

    // Built-in validation for known keys
    if (key.find("_hours") != std::string::npos ||
        key.find("_minutes") != std::string::npos ||
        key.find("_days") != std::string::npos ||
        key.find("_size") != std::string::npos) {
        try {
            int val = std::stoi(value);
            if (val < 0) {
                return "Value must be non-negative";
            }
        } catch (...) {
            return "Value must be a valid integer";
        }
    }

    if (key.find("percentage") != std::string::npos) {
        try {
            int val = std::stoi(value);
            if (val < 0 || val > 100) {
                return "Percentage must be between 0 and 100";
            }
        } catch (...) {
            return "Value must be a valid integer";
        }
    }

    return "";
}

void SystemConfig::registerValidator(const std::string& key,
                                     std::function<std::string(const std::string&)> validator) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    validators_[key] = std::move(validator);
}

// ============================================================================
// RELOAD AND SYNC
// ============================================================================

bool SystemConfig::reload() {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);

    config_cache_.clear();
    feature_flags_.clear();

    initializeDefaults();

    if (!loadFromDatabase()) {
        return false;
    }

    if (!loadFeatureFlags()) {
        return false;
    }

    return true;
}

bool SystemConfig::sync() {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    bool success = true;
    for (const auto& [key, cv] : config_cache_) {
        if (!saveToDatabase(cv)) {
            success = false;
        }
    }

    return success;
}

// ============================================================================
// CHANGE NOTIFICATIONS
// ============================================================================

std::string SystemConfig::onChanged(ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);

    std::string id = std::to_string(++callback_id_counter_);
    change_callbacks_[id] = std::move(callback);
    return id;
}

void SystemConfig::removeOnChanged(const std::string& callback_id) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    change_callbacks_.erase(callback_id);
}

void SystemConfig::notifyChanged(const std::string& key,
                                 const std::string& old_value,
                                 const std::string& new_value) {
    std::vector<ChangeCallback> callbacks;

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callbacks.reserve(change_callbacks_.size());
        for (const auto& [id, cb] : change_callbacks_) {
            callbacks.push_back(cb);
        }
    }

    for (const auto& cb : callbacks) {
        try {
            cb(key, old_value, new_value);
        } catch (const std::exception& e) {
            WTL_CAPTURE_WARNING(ErrorCategory::BUSINESS_LOGIC,
                               "Config change callback failed: " + std::string(e.what()),
                               {{"key", key}});
        }
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool SystemConfig::saveToDatabase(const ConfigValue& config_value) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "INSERT INTO system_config (key, value, category, description, "
            "value_type, is_sensitive, requires_restart, created_at, updated_at, updated_by) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, NOW(), NOW(), $8) "
            "ON CONFLICT (key) DO UPDATE SET "
            "value = EXCLUDED.value, "
            "category = EXCLUDED.category, "
            "description = EXCLUDED.description, "
            "value_type = EXCLUDED.value_type, "
            "is_sensitive = EXCLUDED.is_sensitive, "
            "requires_restart = EXCLUDED.requires_restart, "
            "updated_at = NOW(), "
            "updated_by = EXCLUDED.updated_by",
            config_value.key,
            config_value.value,
            config_value.category,
            config_value.description,
            config_value.value_type,
            config_value.is_sensitive,
            config_value.requires_restart,
            config_value.updated_by
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to save config to database: " + std::string(e.what()),
                         {{"key", config_value.key}});
        return false;
    }
}

bool SystemConfig::deleteFromDatabase(const std::string& key) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params("DELETE FROM system_config WHERE key = $1", key);

        txn.commit();
        ConnectionPool::getInstance().release(conn);
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to delete config from database: " + std::string(e.what()),
                         {{"key", key}});
        return false;
    }
}

int SystemConfig::hashUserIdForRollout(const std::string& user_id,
                                       const std::string& flag_key) {
    // Simple hash for consistent bucket assignment
    std::string combined = user_id + ":" + flag_key;
    std::hash<std::string> hasher;
    size_t hash = hasher(combined);
    return static_cast<int>(hash % 100);
}

} // namespace wtl::admin
