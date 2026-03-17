/**
 * @file HealthCheckWorker.h
 * @brief Worker for monitoring system health
 *
 * PURPOSE:
 * Monitors system health including database connections,
 * external API connectivity, memory usage, and disk space.
 * Triggers alerts when issues are detected.
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <json/json.h>
#include "BaseWorker.h"

namespace wtl::workers {

/**
 * @enum HealthStatus
 * @brief Overall health status
 */
enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    UNKNOWN
};

/**
 * @struct ComponentHealth
 * @brief Health status of a single component
 */
struct ComponentHealth {
    std::string name;
    HealthStatus status{HealthStatus::UNKNOWN};
    std::string message;
    std::chrono::milliseconds latency{0};
    std::chrono::system_clock::time_point last_check;

    Json::Value toJson() const {
        Json::Value json;
        json["name"] = name;
        json["status"] = statusToString(status);
        json["message"] = message;
        json["latency_ms"] = static_cast<Json::Int64>(latency.count());
        return json;
    }

    static std::string statusToString(HealthStatus s) {
        switch (s) {
            case HealthStatus::HEALTHY: return "healthy";
            case HealthStatus::DEGRADED: return "degraded";
            case HealthStatus::UNHEALTHY: return "unhealthy";
            default: return "unknown";
        }
    }
};

/**
 * @struct HealthCheckResult
 * @brief Overall health check result
 */
struct HealthCheckResult {
    HealthStatus overall_status{HealthStatus::UNKNOWN};
    std::vector<ComponentHealth> components;
    int healthy_count{0};
    int degraded_count{0};
    int unhealthy_count{0};
    int alerts_triggered{0};
    std::chrono::system_clock::time_point check_time;

    Json::Value toJson() const {
        Json::Value json;
        json["overall_status"] = ComponentHealth::statusToString(overall_status);
        json["healthy_count"] = healthy_count;
        json["degraded_count"] = degraded_count;
        json["unhealthy_count"] = unhealthy_count;
        json["alerts_triggered"] = alerts_triggered;

        Json::Value components_json(Json::arrayValue);
        for (const auto& c : components) {
            components_json.append(c.toJson());
        }
        json["components"] = components_json;

        return json;
    }
};

/**
 * @struct HealthCheckConfig
 * @brief Configuration for health checks
 */
struct HealthCheckConfig {
    int db_timeout_ms{5000};
    int api_timeout_ms{10000};
    int max_consecutive_failures{3};
    bool check_external_apis{true};
    bool check_disk_space{true};
    bool check_memory{true};
    int disk_warning_threshold_percent{80};
    int disk_critical_threshold_percent{95};
    int memory_warning_threshold_percent{80};
    int memory_critical_threshold_percent{95};
};

/**
 * @class HealthCheckWorker
 * @brief Worker for system health monitoring
 */
class HealthCheckWorker : public BaseWorker {
public:
    /**
     * @brief Constructor - runs every 5 minutes by default
     */
    HealthCheckWorker();
    explicit HealthCheckWorker(const HealthCheckConfig& config);

    ~HealthCheckWorker() override = default;

    HealthCheckResult getLastResult() const;
    HealthCheckResult checkNow();

    void setConfig(const HealthCheckConfig& config);
    HealthStatus getOverallStatus() const;

    // Individual checks
    ComponentHealth checkDatabase();
    ComponentHealth checkRedis();
    ComponentHealth checkRescueGroupsApi();
    ComponentHealth checkFileSystem();
    ComponentHealth checkMemory();
    ComponentHealth checkWorkerManager();

protected:
    WorkerResult doExecute() override;
    void beforeExecute() override;
    void afterExecute(const WorkerResult& result) override;

private:
    void triggerAlert(const std::string& component, HealthStatus status,
                     const std::string& message);
    void resolveAlert(const std::string& component);
    void recordHealthMetrics(const HealthCheckResult& result);

    HealthCheckConfig config_;
    mutable std::mutex result_mutex_;
    HealthCheckResult last_result_;
    std::map<std::string, int> consecutive_failures_;
    std::map<std::string, bool> active_alerts_;
};

} // namespace wtl::workers
