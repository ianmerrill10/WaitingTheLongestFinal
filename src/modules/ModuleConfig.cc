/**
 * @file ModuleConfig.cc
 * @brief Implementation of the ModuleConfig singleton
 * @see ModuleConfig.h for documentation
 */

#include "ModuleConfig.h"

// Standard library includes
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace wtl::modules {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

ModuleConfig& ModuleConfig::getInstance() {
    static ModuleConfig instance;
    return instance;
}

// ============================================================================
// LOADING
// ============================================================================

void ModuleConfig::load(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_path);
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        throw std::runtime_error("Failed to parse config file: " + errors);
    }

    load(root);
}

void ModuleConfig::load(const Json::Value& root_config) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_ = root_config;
    loaded_ = true;

    std::cout << "[ModuleConfig] Configuration loaded successfully." << std::endl;

    // Log enabled modules
    if (config_.isMember("modules")) {
        int enabled_count = 0;
        for (const auto& name : config_["modules"].getMemberNames()) {
            if (config_["modules"][name].get("enabled", false).asBool()) {
                enabled_count++;
            }
        }
        std::cout << "[ModuleConfig] Found " << enabled_count << " enabled module(s)." << std::endl;
    }
}

// ============================================================================
// QUERY
// ============================================================================

bool ModuleConfig::isModuleEnabled(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.isMember("modules")) {
        return false;
    }

    if (!config_["modules"].isMember(name)) {
        return false;
    }

    return config_["modules"][name].get("enabled", false).asBool();
}

Json::Value ModuleConfig::getModuleConfig(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.isMember("modules")) {
        return Json::Value(Json::objectValue);
    }

    if (!config_["modules"].isMember(name)) {
        return Json::Value(Json::objectValue);
    }

    return config_["modules"][name];
}

std::vector<std::string> ModuleConfig::getEnabledModules() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> enabled;

    if (!config_.isMember("modules")) {
        return enabled;
    }

    for (const auto& name : config_["modules"].getMemberNames()) {
        if (config_["modules"][name].get("enabled", false).asBool()) {
            enabled.push_back(name);
        }
    }

    return enabled;
}

Json::Value ModuleConfig::getAllModulesConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.isMember("modules")) {
        return Json::Value(Json::objectValue);
    }

    return config_["modules"];
}

bool ModuleConfig::isLoaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_;
}

// ============================================================================
// MODIFY (RUNTIME)
// ============================================================================

void ModuleConfig::setModuleEnabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Ensure modules section exists
    if (!config_.isMember("modules")) {
        config_["modules"] = Json::Value(Json::objectValue);
    }

    // Ensure module section exists
    if (!config_["modules"].isMember(name)) {
        config_["modules"][name] = Json::Value(Json::objectValue);
    }

    config_["modules"][name]["enabled"] = enabled;

    std::cout << "[ModuleConfig] Module '" << name << "' "
              << (enabled ? "enabled" : "disabled") << "." << std::endl;
}

void ModuleConfig::setModuleConfig(const std::string& name, const Json::Value& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Ensure modules section exists
    if (!config_.isMember("modules")) {
        config_["modules"] = Json::Value(Json::objectValue);
    }

    config_["modules"][name] = config;

    std::cout << "[ModuleConfig] Configuration updated for module: " << name << std::endl;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

bool ModuleConfig::saveToFile(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "[ModuleConfig] Failed to open file for writing: "
                  << config_path << std::endl;
        return false;
    }

    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "    "; // 4 spaces

    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(config_, &file);
    file << std::endl;

    std::cout << "[ModuleConfig] Configuration saved to: " << config_path << std::endl;
    return true;
}

Json::Value ModuleConfig::getFullConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

} // namespace wtl::modules
