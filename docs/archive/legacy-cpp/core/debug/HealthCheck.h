/**
 * @file HealthCheck.h
 * @brief System health monitoring for WaitingTheLongest application
 *
 * PURPOSE:
 * Provides comprehensive health checking for all system components.
 * Monitors database connectivity, disk space, memory usage, and
 * external service availability.
 *
 * USAGE:
 * // Run all health checks
 * auto results = HealthCheck::getInstance().runAllChecks();
 *
 * // Run individual checks
 * auto db_health = HealthCheck::getInstance().checkDatabase();
 * auto mem_health = HealthCheck::getInstance().checkMemory();
 *
 * DEPENDENCIES:
 * - ConnectionPool (database checks)
 * - SelfHealing (circuit breaker checks)
 * - Standard library (filesystem for disk checks)
 *
 * MODIFICATION GUIDE:
 * - Add new health checks as methods
 * - Register new checks in runAllChecks()
 * - Customize thresholds via configuration
 *
 * @author Agent 1 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::core::debug {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum HealthStatus
 * @brief Status levels for health checks
 */
enum class HealthStatus {
    HEALTHY = 0,     ///< Component is functioning normally
    DEGRADED,        ///< Component has issues but is operational
    UNHEALTHY,       ///< Component is not functioning properly
    UNKNOWN          ///< Unable to determine status
};

/**
 * @enum CheckType
 * @brief Types of health checks available
 */
enum class CheckType {
    DATABASE,        ///< Database connectivity
    REDIS,           ///< Redis connectivity
    DISK,            ///< Disk space availability
    MEMORY,          ///< Memory usage
    CPU,             ///< CPU usage
    EXTERNAL_API,    ///< External API availability
    WEBSOCKET,       ///< WebSocket server status
    CIRCUIT_BREAKER  ///< Circuit breaker states
};

// ============================================================================
// CHECK RESULT STRUCTURE
// ============================================================================

/**
 * @struct CheckResult
 * @brief Result of a health check
 */
struct CheckResult {
    CheckType type;                   ///< Type of check performed
    std::string name;                 ///< Human-readable name
    HealthStatus status;              ///< Overall status
    std::string message;              ///< Status message
    std::chrono::milliseconds duration;  ///< Check duration
    std::chrono::system_clock::time_point timestamp;  ///< When check was run
    Json::Value details;              ///< Additional details

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Check if result indicates healthy status
     * @return true if HEALTHY
     */
    bool isHealthy() const { return status == HealthStatus::HEALTHY; }

    /**
     * @brief Check if result indicates problems
     * @return true if UNHEALTHY
     */
    bool isUnhealthy() const { return status == HealthStatus::UNHEALTHY; }
};

/**
 * @struct OverallHealth
 * @brief Aggregated health status of all checks
 */
struct OverallHealth {
    HealthStatus status;              ///< Worst status across all checks
    std::vector<CheckResult> checks;  ///< Individual check results
    std::chrono::system_clock::time_point timestamp;  ///< When checks were run
    std::chrono::milliseconds total_duration{0};  ///< Total time for all checks

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Get number of healthy checks
     * @return Count of healthy checks
     */
    int healthyCount() const;

    /**
     * @brief Get number of unhealthy checks
     * @return Count of unhealthy checks
     */
    int unhealthyCount() const;
};

// ============================================================================
// HEALTH CHECK CLASS
// ============================================================================

/**
 * @class HealthCheck
 * @brief Singleton class for system health monitoring
 *
 * Provides various health checks for monitoring system components.
 * Can run checks individually or all at once.
 */
class HealthCheck {
public:
    // Type for custom check functions
    using CheckFunction = std::function<CheckResult()>;

    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the HealthCheck singleton
     */
    static HealthCheck& getInstance();

    // Delete copy/move constructors
    HealthCheck(const HealthCheck&) = delete;
    HealthCheck& operator=(const HealthCheck&) = delete;
    HealthCheck(HealthCheck&&) = delete;
    HealthCheck& operator=(HealthCheck&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the health check system
     *
     * @param check_interval_seconds Interval for automatic checks (0 = disabled)
     */
    void initialize(int check_interval_seconds = 60);

    /**
     * @brief Initialize from Config singleton
     */
    void initializeFromConfig();

    // =========================================================================
    // RUN ALL CHECKS
    // =========================================================================

    /**
     * @brief Run all registered health checks
     *
     * @return OverallHealth with all check results
     *
     * Runs all standard and custom registered checks.
     */
    OverallHealth runAllChecks();

    /**
     * @brief Get the last cached health check results
     *
     * @return Last OverallHealth result, or runs checks if none cached
     */
    OverallHealth getLastHealth() const;

    // =========================================================================
    // INDIVIDUAL CHECKS
    // =========================================================================

    /**
     * @brief Check database connectivity
     *
     * @return CheckResult for database health
     *
     * Tests:
     * - Connection pool availability
     * - Simple query execution
     * - Query latency
     */
    CheckResult checkDatabase();

    /**
     * @brief Check Redis connectivity
     *
     * @return CheckResult for Redis health
     *
     * Tests:
     * - Connection availability
     * - PING/PONG latency
     */
    CheckResult checkRedis();

    /**
     * @brief Check disk space
     *
     * @param path Path to check (default: application directory)
     * @return CheckResult for disk health
     *
     * Reports:
     * - Available space
     * - Total space
     * - Usage percentage
     */
    CheckResult checkDisk(const std::string& path = ".");

    /**
     * @brief Check memory usage
     *
     * @return CheckResult for memory health
     *
     * Reports:
     * - Available memory
     * - Total memory
     * - Usage percentage
     */
    CheckResult checkMemory();

    /**
     * @brief Check external API connectivity
     *
     * @param api_name Name of the API to check
     * @param test_url URL to test
     * @param timeout_ms Timeout in milliseconds
     * @return CheckResult for API health
     */
    CheckResult checkExternalApi(const std::string& api_name,
                                  const std::string& test_url,
                                  int timeout_ms = 5000);

    /**
     * @brief Check circuit breaker states
     *
     * @return CheckResult for circuit breakers
     *
     * Reports status of all circuit breakers. UNHEALTHY if any are OPEN.
     */
    CheckResult checkCircuitBreakers();

    // =========================================================================
    // CUSTOM CHECKS
    // =========================================================================

    /**
     * @brief Register a custom health check
     *
     * @param name Unique name for the check
     * @param check_func Function that performs the check
     */
    void registerCheck(const std::string& name, CheckFunction check_func);

    /**
     * @brief Unregister a custom health check
     *
     * @param name Name of the check to remove
     */
    void unregisterCheck(const std::string& name);

    // =========================================================================
    // THRESHOLDS
    // =========================================================================

    /**
     * @brief Set disk space warning threshold
     *
     * @param percent_free Minimum percent free for HEALTHY (default: 10)
     */
    void setDiskWarningThreshold(double percent_free);

    /**
     * @brief Set disk space critical threshold
     *
     * @param percent_free Minimum percent free before UNHEALTHY (default: 5)
     */
    void setDiskCriticalThreshold(double percent_free);

    /**
     * @brief Set memory warning threshold
     *
     * @param percent_free Minimum percent free for HEALTHY (default: 15)
     */
    void setMemoryWarningThreshold(double percent_free);

    /**
     * @brief Set memory critical threshold
     *
     * @param percent_free Minimum percent free before UNHEALTHY (default: 5)
     */
    void setMemoryCriticalThreshold(double percent_free);

    /**
     * @brief Set database latency warning threshold
     *
     * @param ms Maximum latency in ms for HEALTHY (default: 100)
     */
    void setDbLatencyWarningThreshold(int ms);

    /**
     * @brief Set database latency critical threshold
     *
     * @param ms Maximum latency in ms before UNHEALTHY (default: 500)
     */
    void setDbLatencyCriticalThreshold(int ms);

    // =========================================================================
    // UTILITY
    // =========================================================================

    /**
     * @brief Convert HealthStatus to string
     * @param status The status
     * @return String representation
     */
    static std::string statusToString(HealthStatus status);

    /**
     * @brief Convert CheckType to string
     * @param type The check type
     * @return String representation
     */
    static std::string typeToString(CheckType type);

    /**
     * @brief Get status color code for display
     * @param status The status
     * @return ANSI color code string
     */
    static std::string statusColor(HealthStatus status);

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    HealthCheck() = default;
    ~HealthCheck() = default;

    /**
     * @brief Create a check result with timing
     *
     * @param type Check type
     * @param name Check name
     * @param start_time When check started
     * @return Partially filled CheckResult
     */
    CheckResult createResult(CheckType type, const std::string& name,
                             std::chrono::steady_clock::time_point start_time);

    // Custom registered checks
    std::unordered_map<std::string, CheckFunction> custom_checks_;

    // Last health check result
    OverallHealth last_health_;
    std::chrono::system_clock::time_point last_check_time_;

    // Thresholds
    double disk_warning_threshold_{10.0};    // % free
    double disk_critical_threshold_{5.0};
    double memory_warning_threshold_{15.0};
    double memory_critical_threshold_{5.0};
    int db_latency_warning_ms_{100};
    int db_latency_critical_ms_{500};

    // Thread safety
    mutable std::mutex mutex_;
    mutable std::mutex check_mutex_;

    // Initialized flag
    std::atomic<bool> initialized_{false};
};

} // namespace wtl::core::debug
