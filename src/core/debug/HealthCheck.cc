/**
 * @file HealthCheck.cc
 * @brief Implementation of system health monitoring
 * @see HealthCheck.h for documentation
 */

#include "core/debug/HealthCheck.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

// Standard library includes
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

// Platform-specific includes for memory info
#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#else
    #include <sys/sysinfo.h>
    #include <unistd.h>
#endif

namespace wtl::core::debug {

// ============================================================================
// CHECK RESULT IMPLEMENTATION
// ============================================================================

Json::Value CheckResult::toJson() const {
    Json::Value json;

    json["type"] = HealthCheck::typeToString(type);
    json["name"] = name;
    json["status"] = HealthCheck::statusToString(status);
    json["message"] = message;
    json["duration_ms"] = static_cast<Json::Int64>(duration.count());

    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    std::ostringstream ts_stream;
    ts_stream << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    json["timestamp"] = ts_stream.str();

    json["details"] = details;

    return json;
}

// ============================================================================
// OVERALL HEALTH IMPLEMENTATION
// ============================================================================

Json::Value OverallHealth::toJson() const {
    Json::Value json;

    json["status"] = HealthCheck::statusToString(status);
    json["total_duration_ms"] = static_cast<Json::Int64>(total_duration.count());
    json["healthy_count"] = healthyCount();
    json["unhealthy_count"] = unhealthyCount();

    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    std::ostringstream ts_stream;
    ts_stream << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    json["timestamp"] = ts_stream.str();

    // Individual checks
    Json::Value checks_json(Json::arrayValue);
    for (const auto& check : checks) {
        checks_json.append(check.toJson());
    }
    json["checks"] = checks_json;

    return json;
}

int OverallHealth::healthyCount() const {
    return std::count_if(checks.begin(), checks.end(),
                         [](const CheckResult& r) { return r.isHealthy(); });
}

int OverallHealth::unhealthyCount() const {
    return std::count_if(checks.begin(), checks.end(),
                         [](const CheckResult& r) { return r.isUnhealthy(); });
}

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

HealthCheck& HealthCheck::getInstance() {
    static HealthCheck instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void HealthCheck::initialize(int check_interval_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    initialized_.store(true);

    std::cout << "[HealthCheck] Initialized";
    if (check_interval_seconds > 0) {
        std::cout << " with auto-check interval=" << check_interval_seconds << "s";
    }
    std::cout << std::endl;
}

void HealthCheck::initializeFromConfig() {
    auto& config = wtl::core::utils::Config::getInstance();

    int interval = config.getInt("debug.health_check_interval_seconds", 60);
    initialize(interval);
}

// ============================================================================
// RUN ALL CHECKS
// ============================================================================

OverallHealth HealthCheck::runAllChecks() {
    std::lock_guard<std::mutex> lock(check_mutex_);

    auto start_time = std::chrono::steady_clock::now();

    OverallHealth health;
    health.timestamp = std::chrono::system_clock::now();
    health.status = HealthStatus::HEALTHY;

    // Run standard checks
    health.checks.push_back(checkDatabase());
    health.checks.push_back(checkDisk());
    health.checks.push_back(checkMemory());
    health.checks.push_back(checkCircuitBreakers());

    // Run custom checks
    {
        std::lock_guard<std::mutex> custom_lock(mutex_);
        for (const auto& [name, check_func] : custom_checks_) {
            try {
                health.checks.push_back(check_func());
            } catch (const std::exception& e) {
                CheckResult error_result;
                error_result.type = CheckType::EXTERNAL_API;
                error_result.name = name;
                error_result.status = HealthStatus::UNHEALTHY;
                error_result.message = std::string("Check threw exception: ") + e.what();
                error_result.timestamp = std::chrono::system_clock::now();
                health.checks.push_back(error_result);
            }
        }
    }

    // Determine overall status (worst status wins)
    for (const auto& check : health.checks) {
        if (check.status == HealthStatus::UNHEALTHY) {
            health.status = HealthStatus::UNHEALTHY;
        } else if (check.status == HealthStatus::DEGRADED &&
                   health.status != HealthStatus::UNHEALTHY) {
            health.status = HealthStatus::DEGRADED;
        } else if (check.status == HealthStatus::UNKNOWN &&
                   health.status == HealthStatus::HEALTHY) {
            health.status = HealthStatus::DEGRADED;  // Unknown counts as degraded
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    health.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Cache result
    {
        std::lock_guard<std::mutex> cache_lock(mutex_);
        last_health_ = health;
        last_check_time_ = health.timestamp;
    }

    return health;
}

OverallHealth HealthCheck::getLastHealth() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // If no cached result, run checks
    if (last_health_.checks.empty()) {
        // Can't call non-const method, return empty result
        OverallHealth empty;
        empty.status = HealthStatus::UNKNOWN;
        empty.timestamp = std::chrono::system_clock::now();
        return empty;
    }

    return last_health_;
}

// ============================================================================
// INDIVIDUAL CHECKS
// ============================================================================

CheckResult HealthCheck::checkDatabase() {
    auto start_time = std::chrono::steady_clock::now();
    CheckResult result = createResult(CheckType::DATABASE, "PostgreSQL Database", start_time);

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();

        // Check if pool is initialized
        if (!pool.isInitialized()) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Connection pool not initialized";
            return result;
        }

        // Try to acquire a connection
        auto conn = pool.acquireWithTimeout(5);  // 5 second timeout
        if (!conn) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Could not acquire connection from pool";
            return result;
        }

        // Run a simple query and measure latency
        auto query_start = std::chrono::steady_clock::now();
        pqxx::nontransaction txn(*conn);
        auto query_result = txn.exec("SELECT 1");
        auto query_end = std::chrono::steady_clock::now();

        pool.release(conn);

        auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            query_end - query_start).count();

        // Add pool stats to details
        auto stats = pool.getStats();
        result.details["pool_size"] = stats["pool_size"];
        result.details["active_connections"] = stats["active"];
        result.details["available_connections"] = stats["available"];
        result.details["query_latency_ms"] = static_cast<Json::Int64>(latency_ms);

        // Determine status based on latency
        if (latency_ms > db_latency_critical_ms_) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Database latency critical: " +
                             std::to_string(latency_ms) + "ms";
        } else if (latency_ms > db_latency_warning_ms_) {
            result.status = HealthStatus::DEGRADED;
            result.message = "Database latency elevated: " +
                             std::to_string(latency_ms) + "ms";
        } else {
            result.status = HealthStatus::HEALTHY;
            result.message = "Database responding normally (" +
                             std::to_string(latency_ms) + "ms)";
        }

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = std::string("Database check failed: ") + e.what();

        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                          "Health check failed: " + std::string(e.what()), {});
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

CheckResult HealthCheck::checkRedis() {
    auto start_time = std::chrono::steady_clock::now();
    CheckResult result = createResult(CheckType::REDIS, "Redis Cache", start_time);

    // Redis is optional - check config first
    auto& config = wtl::core::utils::Config::getInstance();
    if (!config.getBool("redis.enabled", false)) {
        result.status = HealthStatus::HEALTHY;
        result.message = "Redis is disabled in configuration";
        result.details["enabled"] = false;
        return result;
    }

    // Redis check would go here - placeholder since we don't have Redis client
    result.status = HealthStatus::UNKNOWN;
    result.message = "Redis check not implemented";
    result.details["enabled"] = true;

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

CheckResult HealthCheck::checkDisk(const std::string& path) {
    auto start_time = std::chrono::steady_clock::now();
    CheckResult result = createResult(CheckType::DISK, "Disk Space", start_time);

    try {
        std::filesystem::space_info space = std::filesystem::space(path);

        double total_gb = static_cast<double>(space.capacity) / (1024.0 * 1024.0 * 1024.0);
        double free_gb = static_cast<double>(space.free) / (1024.0 * 1024.0 * 1024.0);
        double available_gb = static_cast<double>(space.available) / (1024.0 * 1024.0 * 1024.0);
        double percent_free = (static_cast<double>(space.available) /
                               static_cast<double>(space.capacity)) * 100.0;

        result.details["path"] = path;
        result.details["total_gb"] = total_gb;
        result.details["free_gb"] = free_gb;
        result.details["available_gb"] = available_gb;
        result.details["percent_free"] = percent_free;

        // Determine status based on thresholds
        if (percent_free < disk_critical_threshold_) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Disk space critically low: " +
                             std::to_string(static_cast<int>(percent_free)) + "% free";
        } else if (percent_free < disk_warning_threshold_) {
            result.status = HealthStatus::DEGRADED;
            result.message = "Disk space low: " +
                             std::to_string(static_cast<int>(percent_free)) + "% free";
        } else {
            result.status = HealthStatus::HEALTHY;
            std::ostringstream msg;
            msg << std::fixed << std::setprecision(1)
                << available_gb << "GB available (" << percent_free << "% free)";
            result.message = msg.str();
        }

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNKNOWN;
        result.message = std::string("Could not check disk space: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

CheckResult HealthCheck::checkMemory() {
    auto start_time = std::chrono::steady_clock::now();
    CheckResult result = createResult(CheckType::MEMORY, "System Memory", start_time);

    try {
        uint64_t total_memory = 0;
        uint64_t available_memory = 0;

#ifdef _WIN32
        MEMORYSTATUSEX mem_info;
        mem_info.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&mem_info)) {
            total_memory = mem_info.ullTotalPhys;
            available_memory = mem_info.ullAvailPhys;
        }
#else
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            total_memory = si.totalram * si.mem_unit;
            available_memory = si.freeram * si.mem_unit;
        }
#endif

        if (total_memory > 0) {
            double total_gb = static_cast<double>(total_memory) / (1024.0 * 1024.0 * 1024.0);
            double available_gb = static_cast<double>(available_memory) / (1024.0 * 1024.0 * 1024.0);
            double percent_free = (static_cast<double>(available_memory) /
                                   static_cast<double>(total_memory)) * 100.0;
            double percent_used = 100.0 - percent_free;

            result.details["total_gb"] = total_gb;
            result.details["available_gb"] = available_gb;
            result.details["percent_used"] = percent_used;
            result.details["percent_free"] = percent_free;

            // Determine status based on thresholds
            if (percent_free < memory_critical_threshold_) {
                result.status = HealthStatus::UNHEALTHY;
                result.message = "Memory critically low: " +
                                 std::to_string(static_cast<int>(percent_free)) + "% free";
            } else if (percent_free < memory_warning_threshold_) {
                result.status = HealthStatus::DEGRADED;
                result.message = "Memory usage high: " +
                                 std::to_string(static_cast<int>(percent_used)) + "% used";
            } else {
                result.status = HealthStatus::HEALTHY;
                std::ostringstream msg;
                msg << std::fixed << std::setprecision(1)
                    << available_gb << "GB available (" << percent_free << "% free)";
                result.message = msg.str();
            }
        } else {
            result.status = HealthStatus::UNKNOWN;
            result.message = "Could not determine memory status";
        }

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNKNOWN;
        result.message = std::string("Memory check failed: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

CheckResult HealthCheck::checkExternalApi(const std::string& api_name,
                                           const std::string& test_url,
                                           int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    CheckResult result = createResult(CheckType::EXTERNAL_API, api_name, start_time);

    // External API check would typically use HTTP client
    // Placeholder implementation
    result.status = HealthStatus::UNKNOWN;
    result.message = "External API check not implemented";
    result.details["api_name"] = api_name;
    result.details["test_url"] = test_url;

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

CheckResult HealthCheck::checkCircuitBreakers() {
    auto start_time = std::chrono::steady_clock::now();
    CheckResult result = createResult(CheckType::CIRCUIT_BREAKER,
                                       "Circuit Breakers", start_time);

    try {
        auto& healing = SelfHealing::getInstance();
        auto states = healing.getAllCircuitStates();

        result.details["circuit_count"] = static_cast<int>(states.size());

        int open_count = 0;
        int half_open_count = 0;

        Json::Value circuits(Json::objectValue);
        for (const auto& [name, state] : states) {
            circuits[name] = state.toJson();

            if (state.state == CircuitState::OPEN) {
                ++open_count;
            } else if (state.state == CircuitState::HALF_OPEN) {
                ++half_open_count;
            }
        }

        result.details["circuits"] = circuits;
        result.details["open_count"] = open_count;
        result.details["half_open_count"] = half_open_count;

        // Determine status
        if (open_count > 0) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = std::to_string(open_count) +
                             " circuit breaker(s) OPEN";
        } else if (half_open_count > 0) {
            result.status = HealthStatus::DEGRADED;
            result.message = std::to_string(half_open_count) +
                             " circuit breaker(s) recovering";
        } else if (states.empty()) {
            result.status = HealthStatus::HEALTHY;
            result.message = "No circuit breakers registered";
        } else {
            result.status = HealthStatus::HEALTHY;
            result.message = "All " + std::to_string(states.size()) +
                             " circuit breakers CLOSED";
        }

    } catch (const std::exception& e) {
        result.status = HealthStatus::UNKNOWN;
        result.message = std::string("Circuit breaker check failed: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

// ============================================================================
// CUSTOM CHECKS
// ============================================================================

void HealthCheck::registerCheck(const std::string& name, CheckFunction check_func) {
    std::lock_guard<std::mutex> lock(mutex_);
    custom_checks_[name] = std::move(check_func);

    std::cout << "[HealthCheck] Registered custom check: " << name << std::endl;
}

void HealthCheck::unregisterCheck(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    custom_checks_.erase(name);
}

// ============================================================================
// THRESHOLDS
// ============================================================================

void HealthCheck::setDiskWarningThreshold(double percent_free) {
    disk_warning_threshold_ = percent_free;
}

void HealthCheck::setDiskCriticalThreshold(double percent_free) {
    disk_critical_threshold_ = percent_free;
}

void HealthCheck::setMemoryWarningThreshold(double percent_free) {
    memory_warning_threshold_ = percent_free;
}

void HealthCheck::setMemoryCriticalThreshold(double percent_free) {
    memory_critical_threshold_ = percent_free;
}

void HealthCheck::setDbLatencyWarningThreshold(int ms) {
    db_latency_warning_ms_ = ms;
}

void HealthCheck::setDbLatencyCriticalThreshold(int ms) {
    db_latency_critical_ms_ = ms;
}

// ============================================================================
// UTILITY
// ============================================================================

std::string HealthCheck::statusToString(HealthStatus status) {
    switch (status) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::DEGRADED: return "DEGRADED";
        case HealthStatus::UNHEALTHY: return "UNHEALTHY";
        case HealthStatus::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

std::string HealthCheck::typeToString(CheckType type) {
    switch (type) {
        case CheckType::DATABASE: return "DATABASE";
        case CheckType::REDIS: return "REDIS";
        case CheckType::DISK: return "DISK";
        case CheckType::MEMORY: return "MEMORY";
        case CheckType::CPU: return "CPU";
        case CheckType::EXTERNAL_API: return "EXTERNAL_API";
        case CheckType::WEBSOCKET: return "WEBSOCKET";
        case CheckType::CIRCUIT_BREAKER: return "CIRCUIT_BREAKER";
        default: return "UNKNOWN";
    }
}

std::string HealthCheck::statusColor(HealthStatus status) {
    switch (status) {
        case HealthStatus::HEALTHY: return "\033[32m";   // Green
        case HealthStatus::DEGRADED: return "\033[33m";  // Yellow
        case HealthStatus::UNHEALTHY: return "\033[31m"; // Red
        case HealthStatus::UNKNOWN: return "\033[37m";   // Gray
        default: return "\033[0m";  // Reset
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

CheckResult HealthCheck::createResult(CheckType type, const std::string& name,
                                       std::chrono::steady_clock::time_point start_time) {
    CheckResult result;
    result.type = type;
    result.name = name;
    result.status = HealthStatus::UNKNOWN;
    result.timestamp = std::chrono::system_clock::now();
    result.details = Json::Value(Json::objectValue);
    return result;
}

} // namespace wtl::core::debug
