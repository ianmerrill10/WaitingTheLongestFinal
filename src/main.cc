/**
 * @file main.cc
 * @brief Application entry point for WaitingTheLongest.com
 *
 * PURPOSE:
 * Entry point for the WaitingTheLongest dog rescue platform.
 * Initializes all subsystems in the correct dependency order,
 * configures the Drogon HTTP server, registers controllers,
 * and starts the application.
 *
 * INITIALIZATION ORDER:
 * 1. Config loading
 * 2. Database ConnectionPool
 * 3. Debug system (ErrorCapture, SelfHealing, HealthCheck)
 * 4. EventBus
 * 5. Core services (DogService, ShelterService, StateService, UserService)
 * 6. Auth system (AuthService, SessionManager)
 * 7. Search service
 * 8. Foster service
 * 9. Urgency services (Calculator, Monitor, Tracker, AlertService)
 * 10. Content engines (TikTok, Blog, Social, Media, Templates)
 * 11. Analytics service
 * 12. Notification service
 * 13. Aggregator manager
 * 14. Module manager (load all modules)
 * 15. Worker manager (start all workers)
 * 16. WebSocket handlers
 * 17. Drogon HTTP server
 *
 * USAGE:
 * ./WaitingTheLongest [--config=config/config.json]
 *
 * Arguments:
 *   --config=PATH - Path to config.json (default: config/config.json)
 *   --help, -h    - Show help message
 *
 * DEPENDENCIES:
 * - Config (configuration loading)
 * - ConnectionPool (database connections)
 * - ErrorCapture (error logging)
 * - SelfHealing (retry/circuit breaker)
 * - LogGenerator (report generation)
 * - HealthCheck (system monitoring)
 * - Drogon (HTTP framework)
 * - All service classes
 *
 * MODIFICATION GUIDE:
 * - Add new controller registrations in the appropriate section
 * - Add new initialization steps in the appropriate initialize function
 * - Add signal handlers for graceful shutdown as needed
 *
 * @author Final Integration Agent - WaitingTheLongest Build System
 * @date 2024-01-28
 */

// Standard library includes
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Third-party includes
#include <drogon/drogon.h>
#include <json/json.h>

// Core utilities
#include "core/utils/Config.h"

// Database
#include "core/db/ConnectionPool.h"

// Debug system
#include "core/debug/ErrorCapture.h"
#include "core/debug/HealthCheck.h"
#include "core/debug/LogGenerator.h"
#include "core/debug/SelfHealing.h"

// EventBus
#include "core/EventBus.h"

// Core Services
#include "core/services/DogService.h"
#include "core/services/ShelterService.h"
#include "core/services/StateService.h"
#include "core/services/UserService.h"
#include "core/services/SearchService.h"
#include "core/services/FosterService.h"

// Urgency Services
#include "core/services/UrgencyCalculator.h"
#include "core/services/KillShelterMonitor.h"
#include "core/services/EuthanasiaTracker.h"
#include "core/services/AlertService.h"

// Auth System
#include "core/auth/AuthService.h"
#include "core/auth/SessionManager.h"

// WebSocket
#include "core/websocket/BroadcastService.h"
#include "core/websocket/WaitTimeWorker.h"
#include "core/websocket/ConnectionManager.h"

// Content Engines
#include "content/tiktok/TikTokEngine.h"
#include "content/tiktok/TikTokWorker.h"
#include "content/blog/BlogEngine.h"
#include "content/social/SocialMediaEngine.h"
#include "content/social/SocialWorker.h"
#include "content/media/VideoGenerator.h"
#include "content/media/ImageProcessor.h"
#include "content/media/MediaWorker.h"
#include "content/templates/TemplateEngine.h"
#include "content/templates/TemplateRepository.h"

// Analytics
#include "analytics/AnalyticsService.h"
#include "analytics/AnalyticsWorker.h"
#include "analytics/MetricsAggregator.h"
#include "analytics/ImpactTracker.h"

// Notifications
#include "notifications/NotificationService.h"
#include "notifications/NotificationWorker.h"
#include "notifications/UrgentAlertService.h"

// Aggregators
#include "aggregators/AggregatorManager.h"

// Modules
#include "modules/ModuleManager.h"
#include "modules/ModuleLoader.h"

// Workers
#include "workers/WorkerManager.h"
#include "workers/scheduler/Scheduler.h"

// Admin
#include "admin/AdminService.h"
#include "admin/SystemConfig.h"
#include "admin/AuditLog.h"

// ============================================================================
// NAMESPACE ALIASES
// ============================================================================

namespace config = wtl::core::utils;
namespace db = wtl::core::db;
namespace debug = wtl::core::debug;
namespace services = wtl::core::services;
namespace auth = wtl::core::auth;
namespace websocket = wtl::core::websocket;
namespace tiktok = wtl::content::tiktok;
namespace blog = wtl::content::blog;
namespace social = wtl::content::social;
namespace media = wtl::content::media;
namespace templates = wtl::content::templates;
namespace analytics = wtl::analytics;
namespace notifications = wtl::notifications;
namespace aggregators = wtl::aggregators;
namespace modules = wtl::modules;
namespace workers = wtl::workers;
namespace admin = wtl::admin;

using namespace ::wtl::core;

// ============================================================================
// GLOBAL STATE
// ============================================================================

// Flag for graceful shutdown
static std::atomic<bool> g_shutdown_requested{false};

// Flag indicating full initialization complete
static std::atomic<bool> g_initialized{false};

// Application start time
static std::chrono::system_clock::time_point g_app_start_time;

// ============================================================================
// LOGGING MACROS
// ============================================================================

#define LOG_INIT(component) \
    std::cout << "[Init] " << component << "..." << std::endl

#define LOG_OK(component) \
    std::cout << "[OK]   " << component << " initialized" << std::endl

#ifdef LOG_WARN
#undef LOG_WARN
#endif
#define LOG_WARN(msg) \
    std::cout << "[WARN] " << msg << std::endl

#ifdef LOG_ERROR
#undef LOG_ERROR
#endif
#define LOG_ERROR(msg) \
    std::cerr << "[ERROR] " << msg << std::endl

#ifdef LOG_INFO
#undef LOG_INFO
#endif
#define LOG_INFO(msg) \
    std::cout << "[INFO] " << msg << std::endl

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void printBanner();
std::string parseArguments(int argc, char* argv[]);
bool initializeConfig(const std::string& config_path);
bool initializeDatabase();
bool initializeDebugSystem();
bool initializeEventBus();
bool initializeCoreServices();
bool initializeAuthSystem();
bool initializeSearchService();
bool initializeFosterService();
bool initializeUrgencyServices();
bool initializeContentEngines();
bool initializeAnalyticsService();
bool initializeNotificationService();
bool initializeAggregatorManager();
bool initializeModuleManager();
bool initializeWorkerManager();
bool initializeWebSocketHandlers();
bool initializeAdminServices();
bool configureDrogon();
void registerShutdownHandlers();
void signalHandler(int signal);
void performShutdown();
bool runStartupHealthCheck();

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main(int argc, char* argv[]) {
    // Record start time
    g_app_start_time = std::chrono::system_clock::now();

    // Print banner
    printBanner();

    // Parse command line arguments
    std::string config_path = parseArguments(argc, argv);
    if (config_path.empty()) {
        return EXIT_SUCCESS; // Help was shown
    }

    // ========================================================================
    // INITIALIZATION SEQUENCE
    // ========================================================================

    LOG_INFO("Starting initialization sequence...");
    std::cout << std::endl;

    // 1. Configuration (CRITICAL - must succeed)
    if (!initializeConfig(config_path)) {
        LOG_ERROR("FATAL: Failed to load configuration - aborting");
        return EXIT_FAILURE;
    }

    // 2. Database (CRITICAL - must succeed)
    if (!initializeDatabase()) {
        LOG_ERROR("FATAL: Failed to initialize database - aborting");
        return EXIT_FAILURE;
    }

    // 3. Debug System (CRITICAL - must succeed)
    if (!initializeDebugSystem()) {
        LOG_ERROR("FATAL: Failed to initialize debug system - aborting");
        return EXIT_FAILURE;
    }

    // 4. EventBus (CRITICAL - must succeed)
    if (!initializeEventBus()) {
        LOG_ERROR("FATAL: Failed to initialize EventBus - aborting");
        return EXIT_FAILURE;
    }

    // 5. Core Services (CRITICAL - must succeed)
    if (!initializeCoreServices()) {
        LOG_ERROR("FATAL: Failed to initialize core services - aborting");
        return EXIT_FAILURE;
    }

    // 6. Auth System (CRITICAL - must succeed)
    if (!initializeAuthSystem()) {
        LOG_ERROR("FATAL: Failed to initialize auth system - aborting");
        return EXIT_FAILURE;
    }

    // 7. Search Service (CRITICAL - must succeed)
    if (!initializeSearchService()) {
        LOG_ERROR("FATAL: Failed to initialize search service - aborting");
        return EXIT_FAILURE;
    }

    // 8. Foster Service (CRITICAL - must succeed)
    if (!initializeFosterService()) {
        LOG_ERROR("FATAL: Failed to initialize foster service - aborting");
        return EXIT_FAILURE;
    }

    // 9. Urgency Services (CRITICAL - must succeed)
    if (!initializeUrgencyServices()) {
        LOG_ERROR("FATAL: Failed to initialize urgency services - aborting");
        return EXIT_FAILURE;
    }

    // 10. Content Engines (NON-CRITICAL - continue with reduced functionality)
    if (!initializeContentEngines()) {
        LOG_WARN("Content engines had initialization issues - continuing with reduced functionality");
    }

    // 11. Analytics Service (NON-CRITICAL - continue without analytics)
    if (!initializeAnalyticsService()) {
        LOG_WARN("Analytics service initialization failed - continuing without analytics");
    }

    // 12. Notification Service (NON-CRITICAL - continue without notifications)
    if (!initializeNotificationService()) {
        LOG_WARN("Notification service initialization failed - continuing without notifications");
    }

    // 13. Aggregator Manager (NON-CRITICAL - continue without external sync)
    if (!initializeAggregatorManager()) {
        LOG_WARN("Aggregator manager initialization failed - external sync disabled");
    }

    // 14. Module Manager (NON-CRITICAL - continue without modules)
    if (!initializeModuleManager()) {
        LOG_WARN("Module manager initialization failed - some features may be unavailable");
    }

    // 15. Worker Manager (NON-CRITICAL - continue without background tasks)
    if (!initializeWorkerManager()) {
        LOG_WARN("Worker manager initialization failed - background tasks disabled");
    }

    // 16. WebSocket Handlers (NON-CRITICAL - continue without real-time updates)
    if (!initializeWebSocketHandlers()) {
        LOG_WARN("WebSocket handlers initialization failed - real-time updates disabled");
    }

    // 17. Admin Services (NON-CRITICAL - continue without admin features)
    if (!initializeAdminServices()) {
        LOG_WARN("Admin services initialization failed - admin features limited");
    }

    // Run startup health check
    std::cout << std::endl;
    LOG_INFO("Running startup health check...");

    if (!runStartupHealthCheck()) {
        LOG_WARN("Some health checks failed - system may have degraded functionality");
    }

    // Configure Drogon HTTP server
    std::cout << std::endl;
    LOG_INIT("Drogon HTTP server");

    if (!configureDrogon()) {
        LOG_ERROR("FATAL: Failed to configure HTTP server - aborting");
        performShutdown();
        return EXIT_FAILURE;
    }

    LOG_OK("Drogon HTTP server");

    // Register shutdown handlers
    registerShutdownHandlers();

    // Mark as fully initialized
    g_initialized = true;

    // ========================================================================
    // START SERVER
    // ========================================================================

    auto& cfg = config::Config::getInstance();
    std::string host = cfg.getString("server.host", "0.0.0.0");
    int port = cfg.getInt("server.port", 8080);
    int threads = cfg.getInt("server.threads", 4);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  WaitingTheLongest.com Server Ready" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  Host:    " << host << std::endl;
    std::cout << "  Port:    " << port << std::endl;
    std::cout << "  Threads: " << threads << std::endl;
    std::cout << "  Env:     " << cfg.getEnvironment() << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  Press Ctrl+C to stop" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Run Drogon (blocks until shutdown)
    drogon::app().run();

    // Perform graceful shutdown
    performShutdown();

    // Final message
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  WaitingTheLongest.com - Goodbye!" << std::endl;
    std::cout << "  Every dog deserves a home." << std::endl;
    std::cout << "========================================" << std::endl;

    return EXIT_SUCCESS;
}

// ============================================================================
// IMPLEMENTATION
// ============================================================================

void printBanner() {
    std::cout << R"(
 __        __    _ _   _             _____ _          _                            _
 \ \      / /_ _(_) |_(_)_ __   __ _|_   _| |__   ___| |    ___  _ __   __ _  ___ | |_
  \ \ /\ / / _` | | __| | '_ \ / _` | | | | '_ \ / _ \ |   / _ \| '_ \ / _` |/ _ \| __|
   \ V  V / (_| | | |_| | | | | (_| | | | | | | |  __/ |__| (_) | | | | (_| |  __/| |_
    \_/\_/ \__,_|_|\__|_|_| |_|\__, | |_| |_| |_|\___|_____\___/|_| |_|\__, |\___| \__|
                               |___/                                  |___/
)" << std::endl;

    std::cout << "  Dog Rescue Platform - Finding Homes for Dogs Waiting the Longest" << std::endl;
    std::cout << "  Version 1.0.0" << std::endl;
    std::cout << std::endl;
}

std::string parseArguments(int argc, char* argv[]) {
    std::string config_path = "config/config.json";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg.find("--config=") == 0) {
            config_path = arg.substr(9);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --config=PATH    Path to configuration file (default: config/config.json)" << std::endl;
            std::cout << "  --help, -h       Show this help message" << std::endl;
            std::cout << std::endl;
            std::cout << "Environment Variables:" << std::endl;
            std::cout << "  WTL_CONFIG_PATH  Override config file path" << std::endl;
            std::cout << "  WTL_ENV          Set environment (development, staging, production)" << std::endl;
            return ""; // Signal to exit
        }
    }

    // Check environment variable override
    if (const char* env_config = std::getenv("WTL_CONFIG_PATH")) {
        config_path = env_config;
    }

    // Try to find config file
    if (!std::filesystem::exists(config_path)) {
        LOG_WARN("Config file not found at: " + config_path);

        std::vector<std::string> alternate_paths = {
            "config.json",
            "../config/config.json",
            "./config/config.json",
            "/etc/waitingthelongest/config.json"
        };

        for (const auto& path : alternate_paths) {
            if (std::filesystem::exists(path)) {
                LOG_INFO("Found config at: " + path);
                config_path = path;
                break;
            }
        }
    }

    return config_path;
}

bool initializeConfig(const std::string& config_path) {
    LOG_INIT("Configuration loader");

    try {
        auto& cfg = config::Config::getInstance();

        if (!cfg.load(config_path)) {
            LOG_ERROR("Failed to load configuration from: " + config_path);
            return false;
        }

        LOG_OK("Configuration loader");
        LOG_INFO("Config path: " + config_path);
        LOG_INFO("Environment: " + cfg.getEnvironment());

        if (cfg.isDebugMode()) {
            LOG_WARN("Debug mode is enabled - NOT for production use");
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Configuration error: ") + e.what());
        return false;
    }
}

bool initializeDatabase() {
    LOG_INIT("Database connection pool");

    try {
        auto& pool = db::ConnectionPool::getInstance();

        if (!pool.initializeFromConfig()) {
            LOG_ERROR("Failed to initialize database connection pool");
            return false;
        }

        // Test connectivity
        if (!pool.testConnectivity()) {
            LOG_ERROR("Database connectivity test failed");
            return false;
        }

        LOG_OK("Database connection pool");
        LOG_INFO("Pool size: " + std::to_string(pool.getPoolSize()) +
                 ", Available: " + std::to_string(pool.getAvailableCount()));

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Database error: ") + e.what());
        return false;
    }
}

bool initializeDebugSystem() {
    LOG_INIT("Debug system");

    try {
        // Initialize ErrorCapture first (for logging errors from other inits)
        auto& errorCapture = debug::ErrorCapture::getInstance();
        errorCapture.initializeFromConfig();

        // Initialize SelfHealing
        auto& selfHealing = debug::SelfHealing::getInstance();
        selfHealing.initializeFromConfig();

        // Initialize LogGenerator
        auto& logGenerator = debug::LogGenerator::getInstance();
        logGenerator.initializeFromConfig();
        logGenerator.setAppStartTime(g_app_start_time);

        // Initialize HealthCheck
        auto& healthCheck = debug::HealthCheck::getInstance();
        healthCheck.initializeFromConfig();

        // Register database health check
        healthCheck.registerCheck("database", []() -> debug::CheckResult {
            debug::CheckResult result;
            result.type = debug::CheckType::DATABASE;
            result.name = "database";
            result.timestamp = std::chrono::system_clock::now();
            result.duration = std::chrono::milliseconds(0);
            try {
                bool ok = db::ConnectionPool::getInstance().testConnectivity();
                result.status = ok ? debug::HealthStatus::HEALTHY : debug::HealthStatus::UNHEALTHY;
                result.message = ok ? "Database connectivity OK" : "Database connectivity failed";
            } catch (const std::exception& e) {
                result.status = debug::HealthStatus::UNHEALTHY;
                result.message = std::string("Database error: ") + e.what();
            }
            return result;
        });

        // Register memory health check
        healthCheck.registerCheck("memory", []() -> debug::CheckResult {
            debug::CheckResult result;
            result.type = debug::CheckType::MEMORY;
            result.name = "memory";
            result.timestamp = std::chrono::system_clock::now();
            result.duration = std::chrono::milliseconds(0);
            result.status = debug::HealthStatus::HEALTHY;
            result.message = "Memory check OK";
            return result;
        });

        LOG_OK("Debug system");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Debug system error: ") + e.what());
        return false;
    }
}

bool initializeEventBus() {
    LOG_INIT("EventBus");

    try {
        auto& eventBus = EventBus::getInstance();
        (void)eventBus;
        // EventBus initializes on first getInstance() call

        LOG_OK("EventBus");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("EventBus error: ") + e.what());
        return false;
    }
}

bool initializeCoreServices() {
    LOG_INIT("Core services");

    try {
        // Initialize in dependency order

        // StateService first (no dependencies)
        auto& stateService = services::StateService::getInstance();
        (void)stateService;
        LOG_INFO("  - StateService initialized");

        // ShelterService (depends on StateService)
        auto& shelterService = services::ShelterService::getInstance();
        (void)shelterService;
        LOG_INFO("  - ShelterService initialized");

        // DogService (depends on ShelterService)
        auto& dogService = services::DogService::getInstance();
        (void)dogService;
        LOG_INFO("  - DogService initialized");

        // UserService (no service dependencies)
        auto& userService = services::UserService::getInstance();
        (void)userService;
        LOG_INFO("  - UserService initialized");

        LOG_OK("Core services");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Core services error: ") + e.what());
        return false;
    }
}

bool initializeAuthSystem() {
    LOG_INIT("Auth system");

    try {
        // SessionManager first
        auto& sessionManager = auth::SessionManager::getInstance();
        (void)sessionManager;
        LOG_INFO("  - SessionManager initialized");

        // AuthService (depends on SessionManager, UserService)
        auto& authService = auth::AuthService::getInstance();
        (void)authService;
        LOG_INFO("  - AuthService initialized");

        LOG_OK("Auth system");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Auth system error: ") + e.what());
        return false;
    }
}

bool initializeSearchService() {
    LOG_INIT("Search service");

    try {
        auto& searchService = services::SearchService::getInstance();
        (void)searchService;
        LOG_OK("Search service");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Search service error: ") + e.what());
        return false;
    }
}

bool initializeFosterService() {
    LOG_INIT("Foster service");

    try {
        auto& fosterService = services::FosterService::getInstance();
        (void)fosterService;
        LOG_OK("Foster service");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Foster service error: ") + e.what());
        return false;
    }
}

bool initializeUrgencyServices() {
    LOG_INIT("Urgency services");

    try {
        // UrgencyCalculator
        auto& calculator = services::UrgencyCalculator::getInstance();
        (void)calculator;
        LOG_INFO("  - UrgencyCalculator initialized");

        // KillShelterMonitor
        auto& monitor = services::KillShelterMonitor::getInstance();
        (void)monitor;
        LOG_INFO("  - KillShelterMonitor initialized");

        // EuthanasiaTracker
        auto& tracker = services::EuthanasiaTracker::getInstance();
        (void)tracker;
        LOG_INFO("  - EuthanasiaTracker initialized");

        // AlertService
        auto& alertService = services::AlertService::getInstance();
        (void)alertService;
        LOG_INFO("  - AlertService initialized");

        LOG_OK("Urgency services");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Urgency services error: ") + e.what());
        return false;
    }
}

bool initializeContentEngines() {
    LOG_INIT("Content engines");

    bool allSucceeded = true;

    try {
        // TemplateEngine first (used by other engines)
        auto& templateEngine = templates::TemplateEngine::getInstance();
        templateEngine.initializeFromConfig();
        LOG_INFO("  - TemplateEngine initialized");

        // TemplateRepository
        auto& templateRepo = templates::TemplateRepository::getInstance();
        (void)templateRepo;
        LOG_INFO("  - TemplateRepository initialized");

    } catch (const std::exception& e) {
        LOG_WARN(std::string("  - TemplateEngine error: ") + e.what());
        allSucceeded = false;
    }

    try {
        // VideoGenerator and ImageProcessor
        auto& videoGenerator = media::VideoGenerator::getInstance();
        if (videoGenerator.initializeFromConfig()) {
            LOG_INFO("  - VideoGenerator initialized");
        } else {
            LOG_WARN("  - VideoGenerator initialization failed (media features disabled)");
            allSucceeded = false;
        }

        auto& imageProcessor = media::ImageProcessor::getInstance();
        (void)imageProcessor;
        LOG_INFO("  - ImageProcessor initialized");

    } catch (const std::exception& e) {
        LOG_WARN(std::string("  - Media processing error: ") + e.what());
        allSucceeded = false;
    }

    try {
        // TikTokEngine
        auto& tiktokEngine = tiktok::TikTokEngine::getInstance();
        if (tiktokEngine.initializeFromConfig()) {
            LOG_INFO("  - TikTokEngine initialized");
        } else {
            LOG_WARN("  - TikTokEngine initialization failed (TikTok features disabled)");
            allSucceeded = false;
        }
    } catch (const std::exception& e) {
        LOG_WARN(std::string("  - TikTokEngine error: ") + e.what());
        allSucceeded = false;
    }

    try {
        // BlogEngine
        auto& blogEngine = blog::BlogEngine::getInstance();
        blogEngine.initialize();
        LOG_INFO("  - BlogEngine initialized");
    } catch (const std::exception& e) {
        LOG_WARN(std::string("  - BlogEngine error: ") + e.what());
        allSucceeded = false;
    }

    try {
        // SocialMediaEngine
        auto& socialEngine = social::SocialMediaEngine::getInstance();
        socialEngine.initializeFromConfig();
        LOG_INFO("  - SocialMediaEngine initialized");
    } catch (const std::exception& e) {
        LOG_WARN(std::string("  - SocialMediaEngine error: ") + e.what());
        allSucceeded = false;
    }

    if (allSucceeded) {
        LOG_OK("Content engines");
    } else {
        LOG_WARN("Content engines (partial)");
    }

    return true; // Continue even if some engines fail
}

bool initializeAnalyticsService() {
    LOG_INIT("Analytics service");

    try {
        // MetricsAggregator
        auto& metricsAggregator = analytics::MetricsAggregator::getInstance();
        (void)metricsAggregator;
        LOG_INFO("  - MetricsAggregator initialized");

        // ImpactTracker
        auto& impactTracker = analytics::ImpactTracker::getInstance();
        (void)impactTracker;
        LOG_INFO("  - ImpactTracker initialized");

        // AnalyticsService
        auto& analyticsService = analytics::AnalyticsService::getInstance();
        (void)analyticsService;
        LOG_INFO("  - AnalyticsService initialized");

        LOG_OK("Analytics service");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Analytics service error: ") + e.what());
        return false;
    }
}

bool initializeNotificationService() {
    LOG_INIT("Notification service");

    try {
        // NotificationService
        auto& notificationService = notifications::NotificationService::getInstance();
        if (notificationService.initialize()) {
            LOG_INFO("  - NotificationService initialized");
        } else {
            LOG_WARN("  - NotificationService initialization failed");
            return false;
        }

        // UrgentAlertService
        auto& urgentAlertService = notifications::UrgentAlertService::getInstance();
        (void)urgentAlertService;
        LOG_INFO("  - UrgentAlertService initialized");

        LOG_OK("Notification service");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Notification service error: ") + e.what());
        return false;
    }
}

bool initializeAggregatorManager() {
    LOG_INIT("Aggregator manager");

    try {
        auto& aggregatorManager = aggregators::AggregatorManager::getInstance();

        auto& cfg = config::Config::getInstance();
        std::string aggregatorConfigPath = cfg.getString("aggregators.config_path", "config/aggregators.json");

        if (aggregatorManager.initializeFromFile(aggregatorConfigPath)) {
            LOG_OK("Aggregator manager");
            LOG_INFO("Config: " + aggregatorConfigPath);
            return true;
        } else {
            LOG_WARN("Aggregator manager initialization failed");
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Aggregator manager error: ") + e.what());
        return false;
    }
}

bool initializeModuleManager() {
    LOG_INIT("Module manager");

    try {
        auto& moduleManager = modules::ModuleManager::getInstance();

        // Register built-in modules
        modules::ModuleLoader::getInstance().registerBuiltinModules();

        // Load configuration
        auto& cfg = config::Config::getInstance();
        std::string moduleConfigPath = cfg.getString("modules.config_path", "config/config.json");
        moduleManager.loadConfig(moduleConfigPath);

        // Initialize all enabled modules
        moduleManager.initializeAll();

        auto loadedModules = moduleManager.getLoadedModules();
        LOG_OK("Module manager");
        LOG_INFO("Modules loaded: " + std::to_string(loadedModules.size()));

        for (const auto& moduleName : loadedModules) {
            LOG_INFO("  - " + moduleName);
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Module manager error: ") + e.what());
        return false;
    }
}

bool initializeWorkerManager() {
    LOG_INIT("Worker manager");

    try {
        auto& workerManager = workers::WorkerManager::getInstance();
        workerManager.initializeFromConfig();

        // Start all registered workers
        workerManager.startAll();

        auto workerNames = workerManager.getWorkerNames();
        LOG_OK("Worker manager");
        LOG_INFO("Workers registered: " + std::to_string(workerNames.size()));

        for (const auto& workerName : workerNames) {
            auto status = workerManager.getWorkerStatus(workerName);
            LOG_INFO("  - " + workerName + " [" + workers::workerStatusToString(status) + "]");
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Worker manager error: ") + e.what());
        return false;
    }
}

bool initializeWebSocketHandlers() {
    LOG_INIT("WebSocket handlers");

    try {
        // ConnectionManager
        auto& connectionManager = websocket::ConnectionManager::getInstance();
        (void)connectionManager;
        LOG_INFO("  - ConnectionManager initialized");

        // BroadcastService
        auto& broadcastService = websocket::BroadcastService::getInstance();
        (void)broadcastService;
        LOG_INFO("  - BroadcastService initialized");

        // WaitTimeWorker (1-second tick for wait time updates)
        auto& waitTimeWorker = websocket::WaitTimeWorker::getInstance();
        waitTimeWorker.start();
        LOG_INFO("  - WaitTimeWorker started");

        LOG_OK("WebSocket handlers");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("WebSocket handlers error: ") + e.what());
        return false;
    }
}

bool initializeAdminServices() {
    LOG_INIT("Admin services");

    try {
        // SystemConfig
        auto& systemConfig = admin::SystemConfig::getInstance();
        (void)systemConfig;
        LOG_INFO("  - SystemConfig initialized");

        // AuditLog
        auto& auditLog = admin::AuditLog::getInstance();
        (void)auditLog;
        LOG_INFO("  - AuditLog initialized");

        // AdminService
        auto& adminService = admin::AdminService::getInstance();
        (void)adminService;
        LOG_INFO("  - AdminService initialized");

        LOG_OK("Admin services");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Admin services error: ") + e.what());
        return false;
    }
}

bool configureDrogon() {
    auto& cfg = config::Config::getInstance();

    try {
        auto& app = drogon::app();

        // Server configuration
        std::string host = cfg.getString("server.host", "0.0.0.0");
        int port = cfg.getInt("server.port", 8080);
        int threads = cfg.getInt("server.threads", 4);

        app.addListener(host, static_cast<uint16_t>(port));
        app.setThreadNum(static_cast<size_t>(threads));

        // Logging configuration
        std::string logPath = cfg.getString("server.log_path", "./logs");
        std::string logLevel = cfg.getString("server.log_level", "WARN");

        app.setLogPath(logPath);

        if (logLevel == "DEBUG" || logLevel == "TRACE") {
            app.setLogLevel(trantor::Logger::kDebug);
        } else if (logLevel == "INFO") {
            app.setLogLevel(trantor::Logger::kInfo);
        } else if (logLevel == "ERROR") {
            app.setLogLevel(trantor::Logger::kError);
        } else {
            app.setLogLevel(trantor::Logger::kWarn);
        }

        // Static files
        std::string documentRoot = cfg.getString("server.document_root", "./static");
        std::string uploadPath = cfg.getString("server.upload_path", "./uploads");
        app.setDocumentRoot(documentRoot);
        app.setUploadPath(uploadPath);

        // Connection limits
        int maxConnections = cfg.getInt("server.max_connections", 100000);
        int maxConnectionsPerIP = cfg.getInt("websocket.max_connections_per_ip", 10);
        app.setMaxConnectionNum(maxConnections);
        app.setMaxConnectionNumPerIP(maxConnectionsPerIP);

        // Request size limit
        int maxBodySizeMB = cfg.getInt("server.max_upload_size_mb", 10);
        app.setClientMaxBodySize(static_cast<size_t>(maxBodySizeMB) * 1024 * 1024);

        // Idle timeout
        int idleTimeout = cfg.getInt("server.idle_timeout_seconds", 60);
        app.setIdleConnectionTimeout(idleTimeout);

        // Session configuration
        if (cfg.getBool("server.sessions.enabled", true)) {
            int sessionTimeout = cfg.getInt("server.session_timeout_seconds", 3600);
            app.enableSession(sessionTimeout);
        }

        // Compression
        if (cfg.getBool("server.enable_gzip", true)) {
            app.enableGzip(true);
        }

        // SSL/TLS configuration
        if (cfg.getBool("server.enable_ssl", false)) {
            std::string certPath = cfg.getString("server.ssl_cert_path", "");
            std::string keyPath = cfg.getString("server.ssl_key_path", "");

            if (!certPath.empty() && !keyPath.empty()) {
                app.setSSLFiles(certPath, keyPath);
                LOG_INFO("SSL/TLS enabled");
            } else {
                LOG_WARN("SSL enabled but certificate paths not configured");
            }
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Drogon configuration error: ") + e.what());
        return false;
    }
}

void registerShutdownHandlers() {
    // Signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

#ifdef SIGQUIT
    std::signal(SIGQUIT, signalHandler);
#endif

#ifdef _WIN32
    // Windows-specific console handler
    SetConsoleCtrlHandler(nullptr, FALSE);
#endif

    // Register atexit handler for cleanup on normal exit
    std::atexit([]() {
        if (g_initialized && !g_shutdown_requested) {
            performShutdown();
        }
    });

    LOG_INFO("Shutdown handlers registered");
}

void signalHandler(int signal) {
    std::cout << std::endl;
    LOG_INFO("Received signal " + std::to_string(signal) + ", initiating shutdown...");

    g_shutdown_requested = true;
    drogon::app().quit();
}

void performShutdown() {
    if (!g_initialized) {
        return;
    }

    LOG_INFO("Performing graceful shutdown...");

    // 1. Stop WebSocket workers first
    LOG_INFO("Stopping WebSocket workers...");
    try {
        websocket::WaitTimeWorker::getInstance().stop();
    } catch (...) {}

    // 2. Stop worker manager
    LOG_INFO("Stopping background workers...");
    try {
        workers::WorkerManager::getInstance().stopAll();
    } catch (...) {}

    // 3. Shutdown content engine workers
    LOG_INFO("Stopping content workers...");
    try {
        tiktok::TikTokWorker::getInstance().stop();
        social::SocialWorker::getInstance().stop();
        media::MediaWorker::getInstance().stop();
    } catch (...) {}

    // 4. Shutdown notification worker
    try {
        notifications::NotificationWorker::getInstance().stop();
    } catch (...) {}

    // 5. Shutdown analytics worker
    try {
        analytics::AnalyticsWorker::getInstance().stop();
    } catch (...) {}

    // 6. Shutdown module manager
    LOG_INFO("Shutting down modules...");
    try {
        modules::ModuleManager::getInstance().shutdownAll();
    } catch (...) {}

    // 7. Shutdown aggregator manager
    LOG_INFO("Shutting down aggregators...");
    try {
        aggregators::AggregatorManager::getInstance().shutdown();
    } catch (...) {}

    // 8. Shutdown content engines
    LOG_INFO("Shutting down content engines...");
    try {
        tiktok::TikTokEngine::getInstance().shutdown();
        blog::BlogEngine::getInstance().shutdown();
        social::SocialMediaEngine::getInstance().shutdown();
    } catch (...) {}

    // 9. Shutdown analytics
    LOG_INFO("Shutting down analytics...");
    try {
        analytics::AnalyticsService::getInstance().shutdown();
    } catch (...) {}

    // 10. Generate final error report
    if (debug::ErrorCapture::getInstance().getCount() > 0) {
        LOG_INFO("Generating shutdown error report...");
        debug::LogGenerator::getInstance().exportRecentErrors(60);
    }

    // 11. Shutdown database pool
    LOG_INFO("Closing database connections...");
    try {
        db::ConnectionPool::getInstance().shutdown();
    } catch (...) {}

    // 12. Log final statistics
    auto stats = debug::ErrorCapture::getInstance().getStats();
    LOG_INFO("Total errors captured: " + std::to_string(stats["total_captured"].asUInt64()));

    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - g_app_start_time);
    LOG_INFO("Uptime: " + std::to_string(uptime.count()) + " seconds");

    LOG_INFO("Shutdown complete");
}

bool runStartupHealthCheck() {
    auto& healthCheck = debug::HealthCheck::getInstance();
    auto health = healthCheck.runAllChecks();

    std::cout << std::endl;
    LOG_INFO("Health Check Results:");
    LOG_INFO("  Overall Status: " + debug::HealthCheck::statusToString(health.status));

    for (const auto& check : health.checks) {
        std::string status = debug::HealthCheck::statusToString(check.status);
        std::string message = check.name + ": " + status;

        if (!check.message.empty()) {
            message += " (" + check.message + ")";
        }

        if (check.status == debug::HealthStatus::HEALTHY) {
            LOG_INFO("  - " + message);
        } else if (check.status == debug::HealthStatus::DEGRADED) {
            LOG_WARN("  - " + message);
        } else {
            LOG_ERROR("  - " + message);
        }
    }

    return health.status == debug::HealthStatus::HEALTHY ||
           health.status == debug::HealthStatus::DEGRADED;
}
