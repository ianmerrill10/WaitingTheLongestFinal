/**
 * @file ModuleLoader.h
 * @brief Dynamic module loading and built-in module registration
 *
 * PURPOSE:
 * Provides functionality for loading modules either dynamically from
 * shared libraries (future) or from built-in registrations. This is
 * the entry point for populating the ModuleManager with available modules.
 *
 * USAGE:
 * // Register all built-in modules
 * ModuleLoader::getInstance().registerBuiltinModules();
 *
 * // Future: Load from shared library
 * auto module = ModuleLoader::getInstance().loadFromLibrary("path/to/module.so");
 *
 * DEPENDENCIES:
 * - IModule.h (module interface)
 * - ModuleManager.h (for registration)
 *
 * MODIFICATION GUIDE:
 * - Add new built-in modules to registerBuiltinModules()
 * - Dynamic loading will be implemented when plugin support is needed
 * - Keep module registration simple and fail-safe
 *
 * @author Agent 10 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <memory>
#include <string>

// Project includes
#include "IModule.h"

namespace wtl::modules {

/**
 * @class ModuleLoader
 * @brief Singleton for loading and registering modules
 *
 * Handles both static (built-in) module registration and
 * dynamic module loading from shared libraries.
 */
class ModuleLoader {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the ModuleLoader singleton
     */
    static ModuleLoader& getInstance();

    // ========================================================================
    // DYNAMIC LOADING (FUTURE)
    // ========================================================================

    /**
     * @brief Load a module from a shared library
     *
     * @param path Path to the shared library (.so/.dll)
     * @return std::unique_ptr<IModule> The loaded module, or nullptr if failed
     *
     * @note Currently a placeholder for future plugin support.
     *       Returns nullptr with a warning message.
     */
    std::unique_ptr<IModule> loadFromLibrary(const std::string& path);

    // ========================================================================
    // BUILT-IN MODULES
    // ========================================================================

    /**
     * @brief Register all built-in modules with ModuleManager
     *
     * Registers all modules that are compiled into the application.
     * Should be called during application startup before loading
     * configuration and initializing modules.
     *
     * @example
     * // In main.cc
     * ModuleLoader::getInstance().registerBuiltinModules();
     * ModuleManager::getInstance().loadConfig("config.json");
     * ModuleManager::getInstance().initializeAll();
     */
    void registerBuiltinModules();

    // Prevent copying
    ModuleLoader(const ModuleLoader&) = delete;
    ModuleLoader& operator=(const ModuleLoader&) = delete;

private:
    ModuleLoader() = default;
};

} // namespace wtl::modules
