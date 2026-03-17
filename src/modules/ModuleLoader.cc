/**
 * @file ModuleLoader.cc
 * @brief Implementation of the ModuleLoader singleton
 * @see ModuleLoader.h for documentation
 */

#include "ModuleLoader.h"
#include "ModuleManager.h"
#include "ModuleConfig.h"

// Standard library includes
#include <iostream>

// Include built-in module headers
#include "modules/example/ExampleModule.h"

// Intervention modules
#include "modules/intervention/SurrenderPreventionModule.h"
#include "modules/intervention/ReturnPreventionModule.h"
#include "modules/intervention/TransportNetworkModule.h"
#include "modules/intervention/AIMediaModule.h"
#include "modules/intervention/BreedInterventionModule.h"
#include "modules/intervention/RescueCoordinationModule.h"
#include "modules/intervention/PredictiveMLModule.h"
#include "modules/intervention/EmployerPartnershipsModule.h"

namespace wtl::modules {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

ModuleLoader& ModuleLoader::getInstance() {
    static ModuleLoader instance;
    return instance;
}

// ============================================================================
// DYNAMIC LOADING
// ============================================================================

std::unique_ptr<IModule> ModuleLoader::loadFromLibrary(const std::string& path) {
    // Placeholder for future dynamic loading support
    // This would use dlopen/LoadLibrary to load shared libraries

    std::cerr << "[ModuleLoader] Dynamic module loading not yet implemented." << std::endl;
    std::cerr << "[ModuleLoader] Attempted to load: " << path << std::endl;

    // Future implementation would look something like:
    //
    // #ifdef _WIN32
    //     HMODULE handle = LoadLibrary(path.c_str());
    //     if (!handle) { return nullptr; }
    //     auto factory = (CreateModuleFunc)GetProcAddress(handle, "createModule");
    // #else
    //     void* handle = dlopen(path.c_str(), RTLD_NOW);
    //     if (!handle) { return nullptr; }
    //     auto factory = (CreateModuleFunc)dlsym(handle, "createModule");
    // #endif
    //
    // if (!factory) { return nullptr; }
    // return std::unique_ptr<IModule>(factory());

    return nullptr;
}

// ============================================================================
// BUILT-IN MODULES
// ============================================================================

void ModuleLoader::registerBuiltinModules() {
    std::cout << "[ModuleLoader] Registering built-in modules..." << std::endl;

    auto& manager = ModuleManager::getInstance();
    auto& config = ModuleConfig::getInstance();

    // ========================================================================
    // Example Module (for demonstration/template)
    // ========================================================================
    manager.registerModule("example",
        std::make_unique<example::ExampleModule>());

    // ========================================================================
    // Intervention Modules
    // ========================================================================

    // Surrender Prevention Module
    // Prevents dog surrenders through proactive intervention and resources
    if (config.isModuleEnabled("surrender_prevention") || !config.isLoaded()) {
        manager.registerModule("surrender_prevention",
            std::make_unique<intervention::SurrenderPreventionModule>());
        std::cout << "[ModuleLoader] Registered: SurrenderPreventionModule" << std::endl;
    }

    // Return Prevention Module
    // Prevents post-adoption returns through follow-up support
    if (config.isModuleEnabled("return_prevention") || !config.isLoaded()) {
        manager.registerModule("return_prevention",
            std::make_unique<intervention::ReturnPreventionModule>());
        std::cout << "[ModuleLoader] Registered: ReturnPreventionModule" << std::endl;
    }

    // Transport Network Module
    // Coordinates volunteer transport network for moving dogs
    if (config.isModuleEnabled("transport_network") || !config.isLoaded()) {
        manager.registerModule("transport_network",
            std::make_unique<intervention::TransportNetworkModule>());
        std::cout << "[ModuleLoader] Registered: TransportNetworkModule" << std::endl;
    }

    // AI Media Module
    // AI-powered content generation for dog profiles and social media
    if (config.isModuleEnabled("ai_media") || !config.isLoaded()) {
        manager.registerModule("ai_media",
            std::make_unique<intervention::AIMediaModule>());
        std::cout << "[ModuleLoader] Registered: AIMediaModule" << std::endl;
    }

    // Breed Intervention Module
    // Intervention for overlooked and discriminated breeds including BSL tracking
    if (config.isModuleEnabled("breed_intervention") || !config.isLoaded()) {
        manager.registerModule("breed_intervention",
            std::make_unique<intervention::BreedInterventionModule>());
        std::cout << "[ModuleLoader] Registered: BreedInterventionModule" << std::endl;
    }

    // Rescue Coordination Module
    // Coordinates rescue operations between shelters and rescue organizations
    if (config.isModuleEnabled("rescue_coordination") || !config.isLoaded()) {
        manager.registerModule("rescue_coordination",
            std::make_unique<intervention::RescueCoordinationModule>());
        std::cout << "[ModuleLoader] Registered: RescueCoordinationModule" << std::endl;
    }

    // Predictive ML Module
    // Predictive ML for identifying at-risk dogs and recommending interventions
    if (config.isModuleEnabled("predictive_ml") || !config.isLoaded()) {
        manager.registerModule("predictive_ml",
            std::make_unique<intervention::PredictiveMLModule>());
        std::cout << "[ModuleLoader] Registered: PredictiveMLModule" << std::endl;
    }

    // Employer Partnerships Module
    // Employer-sponsored pet adoption programs with benefits and pawternity leave
    if (config.isModuleEnabled("employer_partnerships") || !config.isLoaded()) {
        manager.registerModule("employer_partnerships",
            std::make_unique<intervention::EmployerPartnershipsModule>());
        std::cout << "[ModuleLoader] Registered: EmployerPartnershipsModule" << std::endl;
    }

    std::cout << "[ModuleLoader] Built-in module registration complete." << std::endl;
    std::cout << "[ModuleLoader] Registered "
              << manager.getRegisteredModules().size()
              << " module(s)." << std::endl;
}

} // namespace wtl::modules
