/**
 * @file Config.cc
 * @brief Implementation of the Config singleton configuration manager
 * @see Config.h for documentation
 */

#include "core/utils/Config.h"

// Standard library includes
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace wtl::core::utils {

// ============================================================================
// REQUIRED CONFIGURATION KEYS
// ============================================================================

// These keys must be present in the configuration for the application to start
const std::unordered_set<std::string> Config::REQUIRED_KEYS = {
    "server.host",
    "server.port",
    "database.host",
    "database.port",
    "database.name",
    "database.user",
    "database.password",
    "auth.jwt_secret"
};

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

Config& Config::getInstance() {
    // Meyer's singleton - thread-safe in C++11 and later
    static Config instance;
    return instance;
}

// ============================================================================
// LOADING & VALIDATION
// ============================================================================

bool Config::load(const std::string& file_path) {
    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "[Config] Error: Configuration file not found: "
                  << file_path << std::endl;
        return false;
    }

    // Read file contents
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "[Config] Error: Could not open configuration file: "
                  << file_path << std::endl;
        return false;
    }

    // Parse JSON
    Json::Value new_config;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, file, &new_config, &errors)) {
        std::cerr << "[Config] Error: Failed to parse configuration file: "
                  << errors << std::endl;
        return false;
    }

    // Acquire exclusive lock for writing
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Store the new configuration
    config_ = std::move(new_config);
    config_path_ = file_path;

    // Release lock before validation (uses read lock internally)
    lock.unlock();

    // Validate required keys
    if (!validateConfig()) {
        std::cerr << "[Config] Error: Configuration validation failed"
                  << std::endl;
        return false;
    }

    // Mark as loaded
    loaded_.store(true);

    std::cout << "[Config] Successfully loaded configuration from: "
              << file_path << std::endl;

    return true;
}

bool Config::reload() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::string path = config_path_;
    lock.unlock();

    if (path.empty()) {
        std::cerr << "[Config] Error: No configuration file has been loaded yet"
                  << std::endl;
        return false;
    }

    return load(path);
}

bool Config::isLoaded() const {
    return loaded_.load();
}

std::string Config::getConfigPath() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return config_path_;
}

// ============================================================================
// STRING GETTERS
// ============================================================================

std::string Config::getString(const std::string& key,
                               const std::string& default_value) const {
    auto value = getValue(key);
    if (!value.has_value() || !value->isString()) {
        return default_value;
    }
    return value->asString();
}

std::string Config::getStringRequired(const std::string& key) const {
    auto value = getValue(key);
    if (!value.has_value()) {
        throw std::runtime_error("[Config] Required key not found: " + key);
    }
    if (!value->isString()) {
        throw std::runtime_error("[Config] Key is not a string: " + key);
    }
    return value->asString();
}

// ============================================================================
// INTEGER GETTERS
// ============================================================================

int Config::getInt(const std::string& key, int default_value) const {
    auto value = getValue(key);
    if (!value.has_value() || !value->isInt()) {
        return default_value;
    }
    return value->asInt();
}

int Config::getIntRequired(const std::string& key) const {
    auto value = getValue(key);
    if (!value.has_value()) {
        throw std::runtime_error("[Config] Required key not found: " + key);
    }
    if (!value->isInt()) {
        throw std::runtime_error("[Config] Key is not an integer: " + key);
    }
    return value->asInt();
}

// ============================================================================
// DOUBLE GETTERS
// ============================================================================

double Config::getDouble(const std::string& key, double default_value) const {
    auto value = getValue(key);
    if (!value.has_value() || !value->isDouble()) {
        // Also accept integers as doubles
        if (value.has_value() && value->isInt()) {
            return static_cast<double>(value->asInt());
        }
        return default_value;
    }
    return value->asDouble();
}

double Config::getDoubleRequired(const std::string& key) const {
    auto value = getValue(key);
    if (!value.has_value()) {
        throw std::runtime_error("[Config] Required key not found: " + key);
    }
    if (!value->isDouble() && !value->isInt()) {
        throw std::runtime_error("[Config] Key is not a number: " + key);
    }
    return value->isDouble() ? value->asDouble()
                              : static_cast<double>(value->asInt());
}

// ============================================================================
// BOOLEAN GETTERS
// ============================================================================

bool Config::getBool(const std::string& key, bool default_value) const {
    auto value = getValue(key);
    if (!value.has_value() || !value->isBool()) {
        return default_value;
    }
    return value->asBool();
}

bool Config::getBoolRequired(const std::string& key) const {
    auto value = getValue(key);
    if (!value.has_value()) {
        throw std::runtime_error("[Config] Required key not found: " + key);
    }
    if (!value->isBool()) {
        throw std::runtime_error("[Config] Key is not a boolean: " + key);
    }
    return value->asBool();
}

// ============================================================================
// ARRAY GETTERS
// ============================================================================

std::vector<std::string> Config::getStringArray(const std::string& key) const {
    std::vector<std::string> result;
    auto value = getValue(key);

    if (!value.has_value() || !value->isArray()) {
        return result;
    }

    result.reserve(value->size());
    for (const auto& item : *value) {
        if (item.isString()) {
            result.push_back(item.asString());
        }
    }

    return result;
}

std::vector<int> Config::getIntArray(const std::string& key) const {
    std::vector<int> result;
    auto value = getValue(key);

    if (!value.has_value() || !value->isArray()) {
        return result;
    }

    result.reserve(value->size());
    for (const auto& item : *value) {
        if (item.isInt()) {
            result.push_back(item.asInt());
        }
    }

    return result;
}

// ============================================================================
// JSON GETTERS
// ============================================================================

Json::Value Config::getJson(const std::string& key) const {
    auto value = getValue(key);
    return value.has_value() ? *value : Json::Value::null;
}

Json::Value Config::getAllConfig() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return config_;
}

// ============================================================================
// KEY EXISTENCE
// ============================================================================

bool Config::hasKey(const std::string& key) const {
    return getValue(key).has_value();
}

// ============================================================================
// CONVENIENCE METHODS
// ============================================================================

std::string Config::getDatabaseConnectionString() const {
    std::ostringstream conn_str;

    // Environment variables override config file values (for Docker)
    auto env_or = [this](const char* env_var, const std::string& config_key, const std::string& def) -> std::string {
        const char* env_val = std::getenv(env_var);
        if (env_val && env_val[0] != '\0') return env_val;
        return getString(config_key, def);
    };

    auto env_or_int = [this](const char* env_var, const std::string& config_key, int def) -> int {
        const char* env_val = std::getenv(env_var);
        if (env_val && env_val[0] != '\0') return std::stoi(env_val);
        return getInt(config_key, def);
    };

    conn_str << "host=" << env_or("DB_HOST", "database.host", "localhost")
             << " port=" << env_or_int("DB_PORT", "database.port", 5432)
             << " dbname=" << env_or("DB_NAME", "database.name", "waitingthelongest")
             << " user=" << env_or("DB_USER", "database.user", "wtl_user")
             << " password=" << env_or("DB_PASSWORD", "database.password", "");

    // Add optional SSL mode
    std::string ssl_mode = getString("database.ssl_mode", "prefer");
    if (!ssl_mode.empty()) {
        conn_str << " sslmode=" << ssl_mode;
    }

    // Add application name for connection identification
    std::string app_name = getString("database.application_name",
                                     "WaitingTheLongest");
    if (!app_name.empty()) {
        conn_str << " application_name=" << app_name;
    }

    // Add connection timeout
    int timeout = getInt("database.connection_timeout_seconds", 30);
    conn_str << " connect_timeout=" << timeout;

    return conn_str.str();
}

bool Config::isProduction() const {
    return getBool("environment.is_production", false);
}

bool Config::isDebugMode() const {
    return getBool("environment.debug_mode", true);
}

std::string Config::getEnvironment() const {
    return getString("environment.name", "development");
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::optional<Json::Value> Config::getValue(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    // Split key into parts
    std::vector<std::string> parts = splitKey(key);
    if (parts.empty()) {
        return std::nullopt;
    }

    // Navigate through the JSON structure
    const Json::Value* current = &config_;
    for (const auto& part : parts) {
        if (current->isNull() || !current->isObject()) {
            return std::nullopt;
        }

        if (!current->isMember(part)) {
            return std::nullopt;
        }

        current = &(*current)[part];
    }

    return *current;
}

bool Config::validateConfig() const {
    bool all_valid = true;
    std::vector<std::string> missing_keys;

    for (const auto& key : REQUIRED_KEYS) {
        if (!hasKey(key)) {
            missing_keys.push_back(key);
            all_valid = false;
        }
    }

    // Log missing keys
    if (!missing_keys.empty()) {
        std::cerr << "[Config] Missing required configuration keys:"
                  << std::endl;
        for (const auto& key : missing_keys) {
            std::cerr << "  - " << key << std::endl;
        }
    }

    // Additional validation: JWT secret must be sufficiently long
    std::string jwt_secret = getString("auth.jwt_secret", "");
    if (jwt_secret.length() < 32) {
        std::cerr << "[Config] Warning: auth.jwt_secret should be at least "
                  << "32 characters for security" << std::endl;
        // Don't fail validation, just warn
    }

    // Validate port ranges
    int server_port = getInt("server.port", 8080);
    if (server_port < 1 || server_port > 65535) {
        std::cerr << "[Config] Error: server.port must be between 1 and 65535"
                  << std::endl;
        all_valid = false;
    }

    int db_port = getInt("database.port", 5432);
    if (db_port < 1 || db_port > 65535) {
        std::cerr << "[Config] Error: database.port must be between 1 and 65535"
                  << std::endl;
        all_valid = false;
    }

    // Validate pool size
    int pool_size = getInt("database.pool_size", 10);
    if (pool_size < 1 || pool_size > 100) {
        std::cerr << "[Config] Warning: database.pool_size should be between "
                  << "1 and 100, got " << pool_size << std::endl;
    }

    return all_valid;
}

std::vector<std::string> Config::splitKey(const std::string& key) const {
    std::vector<std::string> parts;
    std::istringstream stream(key);
    std::string part;

    while (std::getline(stream, part, '.')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }

    return parts;
}

} // namespace wtl::core::utils
