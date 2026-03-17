/**
 * @file HealthCheckWorker.cc
 * @brief Implementation of HealthCheckWorker
 */

#include "HealthCheckWorker.h"
#include "WorkerManager.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/EventBus.h"
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#endif

namespace wtl::workers {

using namespace ::wtl::core::debug;
using namespace ::wtl::core::db;

HealthCheckWorker::HealthCheckWorker()
    : BaseWorker("health_check_worker",
                 "Monitors system health and triggers alerts",
                 std::chrono::seconds(300),  // Every 5 minutes
                 WorkerPriority::NORMAL)
{
}

HealthCheckWorker::HealthCheckWorker(const HealthCheckConfig& config)
    : BaseWorker("health_check_worker",
                 "Monitors system health and triggers alerts",
                 std::chrono::seconds(300),
                 WorkerPriority::NORMAL)
    , config_(config)
{
}

HealthCheckResult HealthCheckWorker::getLastResult() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return last_result_;
}

HealthStatus HealthCheckWorker::getOverallStatus() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return last_result_.overall_status;
}

HealthCheckResult HealthCheckWorker::checkNow() {
    HealthCheckResult result;
    result.check_time = std::chrono::system_clock::now();

    // Run all health checks
    result.components.push_back(checkDatabase());
    result.components.push_back(checkRedis());

    if (config_.check_external_apis) {
        result.components.push_back(checkRescueGroupsApi());
    }

    if (config_.check_disk_space) {
        result.components.push_back(checkFileSystem());
    }

    if (config_.check_memory) {
        result.components.push_back(checkMemory());
    }

    result.components.push_back(checkWorkerManager());

    // Calculate overall status
    for (const auto& component : result.components) {
        switch (component.status) {
            case HealthStatus::HEALTHY:
                result.healthy_count++;
                break;
            case HealthStatus::DEGRADED:
                result.degraded_count++;
                break;
            case HealthStatus::UNHEALTHY:
                result.unhealthy_count++;
                break;
            default:
                break;
        }
    }

    // Determine overall status
    if (result.unhealthy_count > 0) {
        result.overall_status = HealthStatus::UNHEALTHY;
    } else if (result.degraded_count > 0) {
        result.overall_status = HealthStatus::DEGRADED;
    } else if (result.healthy_count > 0) {
        result.overall_status = HealthStatus::HEALTHY;
    } else {
        result.overall_status = HealthStatus::UNKNOWN;
    }

    // Trigger or resolve alerts
    for (const auto& component : result.components) {
        if (component.status == HealthStatus::UNHEALTHY) {
            consecutive_failures_[component.name]++;
            if (consecutive_failures_[component.name] >= config_.max_consecutive_failures) {
                if (!active_alerts_[component.name]) {
                    triggerAlert(component.name, component.status, component.message);
                    result.alerts_triggered++;
                    active_alerts_[component.name] = true;
                }
            }
        } else {
            consecutive_failures_[component.name] = 0;
            if (active_alerts_[component.name]) {
                resolveAlert(component.name);
                active_alerts_[component.name] = false;
            }
        }
    }

    recordHealthMetrics(result);

    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        last_result_ = result;
    }

    return result;
}

void HealthCheckWorker::setConfig(const HealthCheckConfig& config) {
    config_ = config;
}

WorkerResult HealthCheckWorker::doExecute() {
    HealthCheckResult result = checkNow();

    std::string status_str = ComponentHealth::statusToString(result.overall_status);

    WorkerResult wr = WorkerResult::Success(
        "System " + status_str + ": " +
        std::to_string(result.healthy_count) + " healthy, " +
        std::to_string(result.degraded_count) + " degraded, " +
        std::to_string(result.unhealthy_count) + " unhealthy",
        result.healthy_count);

    wr.metadata = result.toJson();

    return wr;
}

void HealthCheckWorker::beforeExecute() {
    logInfo("Starting health check...");
}

void HealthCheckWorker::afterExecute(const WorkerResult& result) {
    if (result.success) {
        logInfo("Health check complete: " + result.message);
    }
}

ComponentHealth HealthCheckWorker::checkDatabase() {
    ComponentHealth health;
    health.name = "database";
    health.last_check = std::chrono::system_clock::now();

    auto start = std::chrono::steady_clock::now();

    try {
        auto conn = ConnectionPool::getInstance().acquire();

        pqxx::work txn(*conn);
        auto result = txn.exec("SELECT 1 AS health_check");
        txn.abort();

        ConnectionPool::getInstance().release(conn);

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (health.latency.count() > config_.db_timeout_ms) {
            health.status = HealthStatus::DEGRADED;
            health.message = "Database responding slowly (" +
                           std::to_string(health.latency.count()) + "ms)";
        } else {
            health.status = HealthStatus::HEALTHY;
            health.message = "Database connection OK";
        }

    } catch (const std::exception& e) {
        health.status = HealthStatus::UNHEALTHY;
        health.message = "Database connection failed: " + std::string(e.what());

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }

    return health;
}

ComponentHealth HealthCheckWorker::checkRedis() {
    ComponentHealth health;
    health.name = "redis";
    health.last_check = std::chrono::system_clock::now();

    auto start = std::chrono::steady_clock::now();

    try {
        // Try to ping Redis via command line
        // In production, would use a proper Redis client
        #ifdef _WIN32
        int result = std::system("redis-cli ping > nul 2>&1");
        #else
        int result = std::system("redis-cli ping > /dev/null 2>&1");
        #endif

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (result == 0) {
            health.status = HealthStatus::HEALTHY;
            health.message = "Redis connection OK";
        } else {
            health.status = HealthStatus::DEGRADED;
            health.message = "Redis not available, using in-memory fallback";
        }

    } catch (const std::exception& e) {
        health.status = HealthStatus::DEGRADED;
        health.message = "Redis check failed: " + std::string(e.what());

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }

    return health;
}

ComponentHealth HealthCheckWorker::checkRescueGroupsApi() {
    ComponentHealth health;
    health.name = "rescuegroups_api";
    health.last_check = std::chrono::system_clock::now();

    auto start = std::chrono::steady_clock::now();

    try {
        // Check if we can reach the RescueGroups API
        // In production, would use proper HTTP client
        #ifdef _WIN32
        int result = std::system("curl -s --max-time 10 https://api.rescuegroups.org/v5/public/animals?limit=1 > nul 2>&1");
        #else
        int result = std::system("curl -s --max-time 10 https://api.rescuegroups.org/v5/public/animals?limit=1 > /dev/null 2>&1");
        #endif

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (result == 0) {
            if (health.latency.count() > config_.api_timeout_ms) {
                health.status = HealthStatus::DEGRADED;
                health.message = "RescueGroups API slow (" +
                               std::to_string(health.latency.count()) + "ms)";
            } else {
                health.status = HealthStatus::HEALTHY;
                health.message = "RescueGroups API reachable";
            }
        } else {
            health.status = HealthStatus::UNHEALTHY;
            health.message = "RescueGroups API unreachable";
        }

    } catch (const std::exception& e) {
        health.status = HealthStatus::UNHEALTHY;
        health.message = "API check failed: " + std::string(e.what());

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }

    return health;
}

ComponentHealth HealthCheckWorker::checkFileSystem() {
    ComponentHealth health;
    health.name = "filesystem";
    health.last_check = std::chrono::system_clock::now();

    auto start = std::chrono::steady_clock::now();

    try {
        int usage_percent = 0;

        #ifdef _WIN32
        ULARGE_INTEGER free_bytes, total_bytes;
        if (GetDiskFreeSpaceExA("C:\\", nullptr, &total_bytes, &free_bytes)) {
            uint64_t used = total_bytes.QuadPart - free_bytes.QuadPart;
            usage_percent = static_cast<int>((used * 100) / total_bytes.QuadPart);
        }
        #else
        struct statvfs stat;
        if (statvfs("/", &stat) == 0) {
            uint64_t total = stat.f_blocks * stat.f_frsize;
            uint64_t available = stat.f_bavail * stat.f_frsize;
            uint64_t used = total - available;
            usage_percent = static_cast<int>((used * 100) / total);
        }
        #endif

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (usage_percent >= config_.disk_critical_threshold_percent) {
            health.status = HealthStatus::UNHEALTHY;
            health.message = "Disk usage critical: " + std::to_string(usage_percent) + "%";
        } else if (usage_percent >= config_.disk_warning_threshold_percent) {
            health.status = HealthStatus::DEGRADED;
            health.message = "Disk usage high: " + std::to_string(usage_percent) + "%";
        } else {
            health.status = HealthStatus::HEALTHY;
            health.message = "Disk usage OK: " + std::to_string(usage_percent) + "%";
        }

    } catch (const std::exception& e) {
        health.status = HealthStatus::UNKNOWN;
        health.message = "Disk check failed: " + std::string(e.what());

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }

    return health;
}

ComponentHealth HealthCheckWorker::checkMemory() {
    ComponentHealth health;
    health.name = "memory";
    health.last_check = std::chrono::system_clock::now();

    auto start = std::chrono::steady_clock::now();

    try {
        int usage_percent = 0;

        #ifdef _WIN32
        MEMORYSTATUSEX mem_info;
        mem_info.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&mem_info)) {
            usage_percent = static_cast<int>(mem_info.dwMemoryLoad);
        }
        #else
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            uint64_t total = info.totalram;
            uint64_t free = info.freeram;
            uint64_t used = total - free;
            usage_percent = static_cast<int>((used * 100) / total);
        }
        #endif

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (usage_percent >= config_.memory_critical_threshold_percent) {
            health.status = HealthStatus::UNHEALTHY;
            health.message = "Memory usage critical: " + std::to_string(usage_percent) + "%";
        } else if (usage_percent >= config_.memory_warning_threshold_percent) {
            health.status = HealthStatus::DEGRADED;
            health.message = "Memory usage high: " + std::to_string(usage_percent) + "%";
        } else {
            health.status = HealthStatus::HEALTHY;
            health.message = "Memory usage OK: " + std::to_string(usage_percent) + "%";
        }

    } catch (const std::exception& e) {
        health.status = HealthStatus::UNKNOWN;
        health.message = "Memory check failed: " + std::string(e.what());

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }

    return health;
}

ComponentHealth HealthCheckWorker::checkWorkerManager() {
    ComponentHealth health;
    health.name = "worker_manager";
    health.last_check = std::chrono::system_clock::now();

    auto start = std::chrono::steady_clock::now();

    try {
        auto& manager = WorkerManager::getInstance();
        auto statuses = manager.getAllStatus();

        int running = 0;
        int error = 0;

        auto members = statuses.getMemberNames();
        for (const auto& name : members) {
            const auto& status_str = statuses[name].asString();
            if (status_str == "running") {
                running++;
            } else if (status_str == "error") {
                error++;
            }
        }

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (error > 0) {
            health.status = HealthStatus::DEGRADED;
            health.message = std::to_string(error) + " workers in error state";
        } else if (running == 0 && !statuses.empty()) {
            health.status = HealthStatus::UNHEALTHY;
            health.message = "No workers running";
        } else {
            health.status = HealthStatus::HEALTHY;
            health.message = std::to_string(running) + " workers running";
        }

    } catch (const std::exception& e) {
        health.status = HealthStatus::UNKNOWN;
        health.message = "Worker manager check failed: " + std::string(e.what());

        auto end = std::chrono::steady_clock::now();
        health.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }

    return health;
}

void HealthCheckWorker::triggerAlert(const std::string& component,
                                     HealthStatus status,
                                     const std::string& message) {
    logError("ALERT: " + component + " is " +
             ComponentHealth::statusToString(status) + ": " + message, {});

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(INSERT INTO health_alerts (component, status, message, created_at)
               VALUES ($1, $2, $3, NOW()))",
            component,
            ComponentHealth::statusToString(status),
            message
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        // Emit event for alert handlers
        Json::Value data;
        data["component"] = component;
        data["status"] = ComponentHealth::statusToString(status);
        data["message"] = message;

        wtl::core::emitEvent(
            wtl::core::EventType::CUSTOM,
            component,
            "health_check",
            data,
            "HealthCheckWorker"
        );

    } catch (const std::exception& e) {
        logError("Failed to record health alert: " + std::string(e.what()), {});
    }
}

void HealthCheckWorker::resolveAlert(const std::string& component) {
    logInfo("RESOLVED: " + component + " is now healthy");

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(UPDATE health_alerts
               SET resolved_at = NOW(), status = 'resolved'
               WHERE component = $1 AND resolved_at IS NULL)",
            component
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logWarning("Failed to resolve health alert: " + std::string(e.what()));
    }
}

void HealthCheckWorker::recordHealthMetrics(const HealthCheckResult& result) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string status_str = ComponentHealth::statusToString(result.overall_status);

        txn.exec_params(
            R"(INSERT INTO health_metrics (
                overall_status, healthy_count, degraded_count, unhealthy_count,
                check_time, details
            ) VALUES ($1, $2, $3, $4, NOW(), $5))",
            status_str,
            result.healthy_count,
            result.degraded_count,
            result.unhealthy_count,
            result.toJson().toStyledString()
        );

        // Keep only last 24 hours of metrics
        txn.exec("DELETE FROM health_metrics WHERE check_time < NOW() - INTERVAL '24 hours'");

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        logWarning("Failed to record health metrics: " + std::string(e.what()));
    }
}

} // namespace wtl::workers
