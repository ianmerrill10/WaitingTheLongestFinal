/**
 * @file HealthController.cc
 * @brief Implementation of HealthController
 * @see HealthController.h for documentation
 */

#include "core/controllers/HealthController.h"

// Standard library includes
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>

// Project includes
#include "core/utils/ApiResponse.h"
#include "core/db/ConnectionPool.h"
#include "core/auth/AuthMiddleware.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::core::controllers {

using namespace ::wtl::core::utils;
using namespace ::wtl::core::db;
using namespace ::wtl::core::auth;
using namespace ::wtl::core::debug;

// Application start time for uptime calculation
static const auto APP_START_TIME = std::chrono::steady_clock::now();

// Application version (would typically come from config)
static const std::string APP_VERSION = "1.0.0";

// ============================================================================
// PUBLIC ENDPOINTS
// ============================================================================

void HealthController::getHealth(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        Json::Value health;
        health["status"] = "healthy";
        health["timestamp"] = static_cast<Json::Int64>(std::time(nullptr));

        callback(ApiResponse::success(health));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Health check failed: " + std::string(e.what()),
            {{"endpoint", "/api/health"}, {"method", "GET"}}
        );

        Json::Value health;
        health["status"] = "unhealthy";
        health["timestamp"] = static_cast<Json::Int64>(std::time(nullptr));

        auto resp = drogon::HttpResponse::newHttpJsonResponse(health);
        resp->setStatusCode(drogon::k503ServiceUnavailable);
        callback(resp);
    }
}

void HealthController::getDetailedHealth(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require admin role for detailed health info
    REQUIRE_ROLE(req, callback, "admin");

    try {
        Json::Value health;

        // Basic info
        health["status"] = "healthy";
        health["version"] = APP_VERSION;
        health["uptime"] = getUptime();
        health["timestamp"] = static_cast<Json::Int64>(std::time(nullptr));

        // Database health
        Json::Value database;
        bool db_healthy = checkDatabaseHealth();
        database["status"] = db_healthy ? "connected" : "disconnected";

        // Get connection pool stats
        try {
            auto pool_stats = ConnectionPool::getInstance().getStats();
            database["pool"] = pool_stats;
        } catch (...) {
            database["pool"]["error"] = "Unable to retrieve pool stats";
        }

        health["database"] = database;

        // Memory stats
        health["memory"] = getMemoryStats();

        // Circuit breaker states
        try {
            Json::Value circuits;
            auto& healing = SelfHealing::getInstance();
            auto circuit_states = healing.getAllCircuitStates();
            for (const auto& [name, state] : circuit_states) {
                circuits[name] = state.toJson();
            }
            health["circuit_breakers"] = circuits;
        } catch (...) {
            health["circuit_breakers"]["error"] = "Unable to retrieve circuit states";
        }

        // System info
        Json::Value system_info;
#ifdef _WIN32
        system_info["platform"] = "windows";
#elif __linux__
        system_info["platform"] = "linux";
#elif __APPLE__
        system_info["platform"] = "macos";
#else
        system_info["platform"] = "unknown";
#endif

        health["system"] = system_info;

        // Overall status based on checks
        if (!db_healthy) {
            health["status"] = "degraded";
        }

        callback(ApiResponse::success(health));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Detailed health check failed: " + std::string(e.what()),
            {{"endpoint", "/api/health/detailed"}, {"method", "GET"}}
        );

        Json::Value health;
        health["status"] = "unhealthy";
        health["error"] = e.what();
        health["timestamp"] = static_cast<Json::Int64>(std::time(nullptr));

        auto resp = drogon::HttpResponse::newHttpJsonResponse(health);
        resp->setStatusCode(drogon::k503ServiceUnavailable);
        callback(resp);
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

bool HealthController::checkDatabaseHealth() {
    try {
        auto& pool = ConnectionPool::getInstance();
        auto conn = pool.acquire();

        if (conn) {
            // Simple query to verify connection
            pqxx::work txn(*conn);
            txn.exec("SELECT 1");
            txn.commit();

            pool.release(conn);
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Database health check failed: " + std::string(e.what()),
            {{"operation", "health_check"}}
        );
        return false;
    }
}

std::string HealthController::getUptime() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now - APP_START_TIME;

    auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    int days = total_seconds / (24 * 3600);
    int hours = (total_seconds % (24 * 3600)) / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    std::ostringstream oss;
    if (days > 0) {
        oss << days << "d ";
    }
    oss << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;

    return oss.str();
}

Json::Value HealthController::getMemoryStats() {
    Json::Value memory;

    // Note: Cross-platform memory stats are complex
    // This provides a basic structure that can be enhanced per platform

#ifdef _WIN32
    // Windows-specific memory stats would go here
    memory["note"] = "Memory stats not implemented for this platform";
#elif __linux__
    // Linux can read from /proc/self/status
    try {
        std::ifstream status_file("/proc/self/status");
        std::string line;

        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                // Resident Set Size
                std::istringstream iss(line.substr(6));
                int value;
                std::string unit;
                iss >> value >> unit;
                memory["rss_kb"] = value;
            }
            if (line.substr(0, 7) == "VmSize:") {
                // Virtual Memory Size
                std::istringstream iss(line.substr(7));
                int value;
                std::string unit;
                iss >> value >> unit;
                memory["virtual_kb"] = value;
            }
        }
    } catch (...) {
        memory["error"] = "Unable to read memory stats";
    }
#else
    memory["note"] = "Memory stats not implemented for this platform";
#endif

    return memory;
}

} // namespace wtl::core::controllers
