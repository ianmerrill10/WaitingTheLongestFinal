/**
 * @file ModuleManager.cc
 * @brief Implementation of the ModuleManager singleton
 * @see ModuleManager.h for documentation
 */

#include "ModuleManager.h"

// Standard library includes
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

// Project includes
#include "core/EventBus.h"

namespace wtl::modules {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

ModuleManager& ModuleManager::getInstance() {
    static ModuleManager instance;
    return instance;
}

// ============================================================================
// REGISTRATION
// ============================================================================

void ModuleManager::registerModule(const std::string& name, std::unique_ptr<IModule> module) {
    std::lock_guard<std::mutex> lock(mutex_);

    // If module already exists, shut it down first
    auto it = modules_.find(name);
    if (it != modules_.end()) {
        if (it->second.loaded) {
            std::cerr << "[ModuleManager] Shutting down existing module: " << name << std::endl;
            try {
                it->second.module->shutdown();
            } catch (const std::exception& e) {
                std::cerr << "[ModuleManager] Error shutting down " << name << ": " << e.what() << std::endl;
            }
        }
    }

    ModuleEntry entry;
    entry.module = std::move(module);
    entry.enabled = false;
    entry.loaded = false;

    // Check if there's configuration for this module
    if (config_.isMember("modules") && config_["modules"].isMember(name)) {
        const Json::Value& module_config = config_["modules"][name];
        entry.enabled = module_config.get("enabled", false).asBool();
        entry.module->configure(module_config);
    } else {
        // Use default configuration
        entry.module->configure(entry.module->getDefaultConfig());
    }

    modules_[name] = std::move(entry);

    std::cout << "[ModuleManager] Registered module: " << name << std::endl;

    // Emit event
    wtl::core::emitEvent(
        wtl::core::EventType::MODULE_LOADED,
        name,
        "module",
        Json::Value(Json::objectValue),
        "ModuleManager"
    );
}

void ModuleManager::unregisterModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modules_.find(name);
    if (it == modules_.end()) {
        return;
    }

    // Shutdown if loaded
    if (it->second.loaded) {
        try {
            it->second.module->shutdown();
        } catch (const std::exception& e) {
            std::cerr << "[ModuleManager] Error shutting down " << name << ": " << e.what() << std::endl;
        }

        // Remove from initialization order
        auto order_it = std::find(initialization_order_.begin(),
                                   initialization_order_.end(), name);
        if (order_it != initialization_order_.end()) {
            initialization_order_.erase(order_it);
        }
    }

    modules_.erase(it);

    std::cout << "[ModuleManager] Unregistered module: " << name << std::endl;

    // Emit event
    wtl::core::emitEvent(
        wtl::core::EventType::MODULE_UNLOADED,
        name,
        "module",
        Json::Value(Json::objectValue),
        "ModuleManager"
    );
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void ModuleManager::initializeAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[ModuleManager] Initializing all enabled modules..." << std::endl;

    // Collect enabled modules
    std::vector<std::string> to_initialize;
    for (const auto& [name, entry] : modules_) {
        if (entry.enabled && !entry.loaded) {
            to_initialize.push_back(name);
        }
    }

    // Initialize with dependency ordering
    std::set<std::string> initialized;
    std::set<std::string> initializing; // For cycle detection

    std::function<bool(const std::string&)> initWithDeps =
        [&](const std::string& name) -> bool {
        // Already initialized
        if (initialized.count(name)) {
            return true;
        }

        // Cycle detection
        if (initializing.count(name)) {
            std::cerr << "[ModuleManager] Circular dependency detected for module: "
                      << name << std::endl;
            return false;
        }

        auto it = modules_.find(name);
        if (it == modules_.end()) {
            std::cerr << "[ModuleManager] Module not found: " << name << std::endl;
            return false;
        }

        // Mark as initializing
        initializing.insert(name);

        // Initialize dependencies first
        for (const auto& dep : it->second.module->getDependencies()) {
            // Enable dependency if not already
            auto dep_it = modules_.find(dep);
            if (dep_it == modules_.end()) {
                std::cerr << "[ModuleManager] Dependency not found: " << dep
                          << " (required by " << name << ")" << std::endl;
                initializing.erase(name);
                return false;
            }

            if (!dep_it->second.enabled) {
                std::cerr << "[ModuleManager] Dependency not enabled: " << dep
                          << " (required by " << name << ")" << std::endl;
                initializing.erase(name);
                return false;
            }

            if (!initWithDeps(dep)) {
                initializing.erase(name);
                return false;
            }
        }

        // Initialize this module
        try {
            std::cout << "[ModuleManager] Initializing module: " << name << std::endl;
            if (it->second.module->initialize()) {
                it->second.loaded = true;
                initialization_order_.push_back(name);
                initialized.insert(name);
                std::cout << "[ModuleManager] Module initialized: " << name << std::endl;
            } else {
                std::cerr << "[ModuleManager] Module initialization failed: " << name << std::endl;

                Json::Value error_data;
                error_data["module"] = name;
                error_data["error"] = "initialize() returned false";
                wtl::core::emitEvent(
                    wtl::core::EventType::MODULE_ERROR,
                    name,
                    "module",
                    error_data,
                    "ModuleManager"
                );

                initializing.erase(name);
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ModuleManager] Exception initializing " << name << ": "
                      << e.what() << std::endl;

            Json::Value error_data;
            error_data["module"] = name;
            error_data["error"] = e.what();
            wtl::core::emitEvent(
                wtl::core::EventType::MODULE_ERROR,
                name,
                "module",
                error_data,
                "ModuleManager"
            );

            initializing.erase(name);
            return false;
        }

        initializing.erase(name);
        return true;
    };

    for (const auto& name : to_initialize) {
        initWithDeps(name);
    }

    std::cout << "[ModuleManager] Initialization complete. "
              << initialization_order_.size() << " modules loaded." << std::endl;
}

void ModuleManager::shutdownAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[ModuleManager] Shutting down all modules..." << std::endl;

    // Shutdown in reverse initialization order
    for (auto it = initialization_order_.rbegin(); it != initialization_order_.rend(); ++it) {
        auto module_it = modules_.find(*it);
        if (module_it != modules_.end() && module_it->second.loaded) {
            try {
                std::cout << "[ModuleManager] Shutting down module: " << *it << std::endl;
                module_it->second.module->shutdown();
                module_it->second.loaded = false;
            } catch (const std::exception& e) {
                std::cerr << "[ModuleManager] Exception shutting down " << *it << ": "
                          << e.what() << std::endl;
            }
        }
    }

    initialization_order_.clear();
    std::cout << "[ModuleManager] All modules shut down." << std::endl;
}

bool ModuleManager::initializeModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modules_.find(name);
    if (it == modules_.end()) {
        std::cerr << "[ModuleManager] Module not found: " << name << std::endl;
        return false;
    }

    if (it->second.loaded) {
        return true; // Already loaded
    }

    // Check dependencies
    for (const auto& dep : it->second.module->getDependencies()) {
        auto dep_it = modules_.find(dep);
        if (dep_it == modules_.end() || !dep_it->second.loaded) {
            std::cerr << "[ModuleManager] Dependency not loaded: " << dep
                      << " (required by " << name << ")" << std::endl;
            return false;
        }
    }

    try {
        if (it->second.module->initialize()) {
            it->second.loaded = true;
            it->second.enabled = true;
            initialization_order_.push_back(name);
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ModuleManager] Exception initializing " << name << ": "
                  << e.what() << std::endl;
    }

    return false;
}

bool ModuleManager::shutdownModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modules_.find(name);
    if (it == modules_.end()) {
        return false;
    }

    if (!it->second.loaded) {
        return true; // Already not loaded
    }

    try {
        it->second.module->shutdown();
        it->second.loaded = false;

        // Remove from initialization order
        auto order_it = std::find(initialization_order_.begin(),
                                   initialization_order_.end(), name);
        if (order_it != initialization_order_.end()) {
            initialization_order_.erase(order_it);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ModuleManager] Exception shutting down " << name << ": "
                  << e.what() << std::endl;
    }

    return false;
}

// ============================================================================
// ACCESS
// ============================================================================

IModule* ModuleManager::getModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modules_.find(name);
    if (it != modules_.end()) {
        return it->second.module.get();
    }
    return nullptr;
}

std::vector<std::string> ModuleManager::getRegisteredModules() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(modules_.size());
    for (const auto& [name, entry] : modules_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> ModuleManager::getEnabledModules() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    for (const auto& [name, entry] : modules_) {
        if (entry.enabled) {
            names.push_back(name);
        }
    }
    return names;
}

std::vector<std::string> ModuleManager::getLoadedModules() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    for (const auto& [name, entry] : modules_) {
        if (entry.loaded) {
            names.push_back(name);
        }
    }
    return names;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void ModuleManager::loadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "[ModuleManager] Failed to open config file: " << config_path << std::endl;
        return;
    }

    Json::Value config;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, file, &config, &errors)) {
        std::cerr << "[ModuleManager] Failed to parse config file: " << errors << std::endl;
        return;
    }

    loadConfig(config);
}

void ModuleManager::loadConfig(const Json::Value& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_ = config;

    // Apply configuration to registered modules
    if (config_.isMember("modules")) {
        for (auto& [name, entry] : modules_) {
            if (config_["modules"].isMember(name)) {
                const Json::Value& module_config = config_["modules"][name];
                entry.enabled = module_config.get("enabled", false).asBool();
                entry.module->configure(module_config);
            }
        }
    }

    std::cout << "[ModuleManager] Configuration loaded." << std::endl;
}

bool ModuleManager::isModuleEnabled(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modules_.find(name);
    if (it != modules_.end()) {
        return it->second.enabled;
    }

    // Check config for modules not yet registered
    if (config_.isMember("modules") && config_["modules"].isMember(name)) {
        return config_["modules"][name].get("enabled", false).asBool();
    }

    return false;
}

void ModuleManager::enableModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modules_.find(name);
    if (it == modules_.end()) {
        std::cerr << "[ModuleManager] Cannot enable unknown module: " << name << std::endl;
        return;
    }

    it->second.enabled = true;

    // Initialize if not already loaded
    if (!it->second.loaded) {
        // Release lock and call initialize
        mutex_.unlock();
        initializeModule(name);
        mutex_.lock();
    }
}

void ModuleManager::disableModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modules_.find(name);
    if (it == modules_.end()) {
        return;
    }

    // Shutdown if loaded
    if (it->second.loaded) {
        try {
            it->second.module->shutdown();
            it->second.loaded = false;

            // Remove from initialization order
            auto order_it = std::find(initialization_order_.begin(),
                                       initialization_order_.end(), name);
            if (order_it != initialization_order_.end()) {
                initialization_order_.erase(order_it);
            }
        } catch (const std::exception& e) {
            std::cerr << "[ModuleManager] Exception shutting down " << name << ": "
                      << e.what() << std::endl;
        }
    }

    it->second.enabled = false;
}

// ============================================================================
// STATUS
// ============================================================================

Json::Value ModuleManager::getModuleStatuses() {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value statuses(Json::objectValue);

    for (const auto& [name, entry] : modules_) {
        Json::Value status = entry.module->getStatus();
        status["registered"] = true;
        status["enabled"] = entry.enabled;
        status["loaded"] = entry.loaded;
        statuses[name] = status;
    }

    return statuses;
}

Json::Value ModuleManager::getModuleMetrics() {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value metrics(Json::objectValue);

    for (const auto& [name, entry] : modules_) {
        if (entry.loaded) {
            metrics[name] = entry.module->getMetrics();
        }
    }

    return metrics;
}

bool ModuleManager::isModuleHealthy(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modules_.find(name);
    if (it == modules_.end() || !it->second.loaded) {
        return false;
    }

    try {
        return it->second.module->isHealthy();
    } catch (const std::exception& e) {
        std::cerr << "[ModuleManager] Exception checking health of " << name << ": "
                  << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool ModuleManager::checkDependencies(const std::string& name) {
    auto it = modules_.find(name);
    if (it == modules_.end()) {
        return false;
    }

    for (const auto& dep : it->second.module->getDependencies()) {
        auto dep_it = modules_.find(dep);
        if (dep_it == modules_.end() || !dep_it->second.loaded) {
            return false;
        }
    }

    return true;
}

void ModuleManager::initializeDependencies(const std::string& name) {
    auto it = modules_.find(name);
    if (it == modules_.end()) {
        return;
    }

    for (const auto& dep : it->second.module->getDependencies()) {
        auto dep_it = modules_.find(dep);
        if (dep_it != modules_.end() && !dep_it->second.loaded) {
            initializeDependencies(dep); // Recursive
            initializeModule(dep);
        }
    }
}

} // namespace wtl::modules
