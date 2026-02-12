/**
 * @file AggregatorController.cc
 * @brief Implementation of REST API controller for aggregator management
 *
 * PURPOSE:
 * Implements admin endpoints for managing and monitoring data source
 * aggregators, triggering syncs, and viewing statistics.
 *
 * USAGE:
 * Automatically registered with Drogon via HttpController.
 *
 * DEPENDENCIES:
 * - AggregatorManager for aggregator operations
 * - AuthMiddleware for admin authentication
 * - ErrorCapture for error handling
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

// Project includes
#include "controllers/AggregatorController.h"
#include "aggregators/AggregatorManager.h"
#include "core/debug/ErrorCapture.h"

// Third-party includes
#include <drogon/drogon.h>

namespace wtl::controllers {

using namespace drogon;
using namespace ::wtl::aggregators;
using ::wtl::core::debug::ErrorCategory;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

namespace {

/**
 * @brief Create a JSON error response
 */
HttpResponsePtr makeErrorResponse(HttpStatusCode status, const std::string& message) {
    Json::Value json;
    json["success"] = false;
    json["error"] = message;
    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(status);
    return resp;
}

/**
 * @brief Create a JSON success response
 */
HttpResponsePtr makeSuccessResponse(const Json::Value& data) {
    Json::Value json;
    json["success"] = true;
    json["data"] = data;
    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(k200OK);
    return resp;
}

/**
 * @brief Check if request has admin role
 * @note In production, this would verify JWT claims
 */
bool isAdmin(const HttpRequestPtr& req) {
    // Check for admin attribute set by AuthMiddleware
    auto admin_attr = req->getAttributes()->get<bool>("is_admin");
    if (admin_attr) {
        return admin_attr;
    }

    // Fallback: check Authorization header for development
    auto auth_header = req->getHeader("Authorization");
    if (auth_header.empty()) {
        return false;
    }

    // In production, validate JWT and check admin claim
    // For now, accept any Bearer token in dev mode
    return auth_header.find("Bearer ") == 0;
}

} // anonymous namespace

// =============================================================================
// HANDLER IMPLEMENTATIONS
// =============================================================================

void AggregatorController::getAllAggregators(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();
        auto infos = manager.getAllAggregatorInfo();

        Json::Value aggregators(Json::arrayValue);
        for (const auto& info : infos) {
            aggregators.append(info.toJson());
        }

        Json::Value data;
        data["aggregators"] = aggregators;
        data["count"] = static_cast<Json::UInt64>(infos.size());

        callback(makeSuccessResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::getAllAggregators failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to get aggregators"));
    }
}

void AggregatorController::getAggregator(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& name) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();

        if (!manager.hasAggregator(name)) {
            callback(makeErrorResponse(k404NotFound, "Aggregator not found: " + name));
            return;
        }

        auto status = manager.getStatus(name);
        callback(makeSuccessResponse(status));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::getAggregator failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to get aggregator status"));
    }
}

void AggregatorController::syncAll(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();

        // Check if sync is already running
        if (manager.isSyncing()) {
            callback(makeErrorResponse(k409Conflict, "Sync already in progress"));
            return;
        }

        // Get optional parameters
        bool async = false;
        auto json_body = req->getJsonObject();
        if (json_body && json_body->isMember("async")) {
            async = (*json_body)["async"].asBool();
        }

        if (async) {
            // Start async sync
            manager.syncAllAsync([](SyncAllResult result) {
                // Result handled by callbacks set on manager
                LOG_INFO << "Async sync completed: "
                         << result.successful << "/" << result.total_aggregators
                         << " successful";
            });

            Json::Value data;
            data["message"] = "Sync started asynchronously";
            data["status"] = "in_progress";
            callback(makeSuccessResponse(data));

        } else {
            // Synchronous sync
            auto result = manager.syncAll();
            callback(makeSuccessResponse(result.toJson()));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::syncAll failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to start sync"));
    }
}

void AggregatorController::syncOne(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& name) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();

        if (!manager.hasAggregator(name)) {
            callback(makeErrorResponse(k404NotFound, "Aggregator not found: " + name));
            return;
        }

        if (manager.isSyncing(name)) {
            callback(makeErrorResponse(k409Conflict, "Aggregator already syncing: " + name));
            return;
        }

        // Get optional parameters
        bool async = false;
        auto json_body = req->getJsonObject();
        if (json_body && json_body->isMember("async")) {
            async = (*json_body)["async"].asBool();
        }

        if (async) {
            manager.syncOneAsync(name, [name](SyncStats stats) {
                LOG_INFO << "Async sync completed for " << name
                         << ": " << stats.dogs_added << " added, "
                         << stats.dogs_updated << " updated";
            });

            Json::Value data;
            data["message"] = "Sync started asynchronously for " + name;
            data["status"] = "in_progress";
            callback(makeSuccessResponse(data));

        } else {
            auto stats = manager.syncOne(name);
            callback(makeSuccessResponse(stats.toJson()));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::syncOne failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to sync aggregator"));
    }
}

void AggregatorController::getStats(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();
        auto stats = manager.getCombinedStats();
        callback(makeSuccessResponse(stats));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::getStats failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to get stats"));
    }
}

void AggregatorController::getErrors(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        // Get limit parameter
        size_t limit = 100;
        auto limit_param = req->getParameter("limit");
        if (!limit_param.empty()) {
            limit = static_cast<size_t>(std::stoul(limit_param));
        }

        auto& manager = AggregatorManager::getInstance();
        auto errors = manager.getRecentErrors(limit);

        Json::Value data;
        data["errors"] = errors;
        data["count"] = static_cast<Json::UInt64>(errors.size());

        callback(makeSuccessResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::getErrors failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to get errors"));
    }
}

void AggregatorController::cancelSync(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& name) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();

        if (!manager.hasAggregator(name)) {
            callback(makeErrorResponse(k404NotFound, "Aggregator not found: " + name));
            return;
        }

        if (!manager.isSyncing(name)) {
            callback(makeErrorResponse(k400BadRequest, "Aggregator not currently syncing: " + name));
            return;
        }

        manager.cancelSync(name);

        Json::Value data;
        data["message"] = "Sync cancelled for " + name;
        callback(makeSuccessResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::cancelSync failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to cancel sync"));
    }
}

void AggregatorController::cancelAllSyncs(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();

        if (!manager.isSyncing()) {
            callback(makeErrorResponse(k400BadRequest, "No syncs currently running"));
            return;
        }

        manager.cancelAllSyncs();

        Json::Value data;
        data["message"] = "All syncs cancelled";
        callback(makeSuccessResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::cancelAllSyncs failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to cancel syncs"));
    }
}

void AggregatorController::setAggregatorEnabled(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& name) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();

        if (!manager.hasAggregator(name)) {
            callback(makeErrorResponse(k404NotFound, "Aggregator not found: " + name));
            return;
        }

        auto json_body = req->getJsonObject();
        if (!json_body || !json_body->isMember("enabled")) {
            callback(makeErrorResponse(k400BadRequest, "Missing 'enabled' field in request body"));
            return;
        }

        bool enabled = (*json_body)["enabled"].asBool();
        auto* aggregator = manager.getAggregator(name);

        if (aggregator) {
            aggregator->setEnabled(enabled);

            Json::Value data;
            data["name"] = name;
            data["enabled"] = enabled;
            data["message"] = enabled ? "Aggregator enabled" : "Aggregator disabled";
            callback(makeSuccessResponse(data));
        } else {
            callback(makeErrorResponse(k404NotFound, "Aggregator not found: " + name));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::setAggregatorEnabled failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to set aggregator state"));
    }
}

void AggregatorController::getHealth(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& name) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();

        if (!manager.hasAggregator(name)) {
            callback(makeErrorResponse(k404NotFound, "Aggregator not found: " + name));
            return;
        }

        auto* aggregator = manager.getAggregator(name);
        if (!aggregator) {
            callback(makeErrorResponse(k404NotFound, "Aggregator not found: " + name));
            return;
        }

        auto info = aggregator->getInfo();
        bool is_healthy = aggregator->isHealthy();
        auto last_stats = manager.getLastSyncStats(name);

        Json::Value health;
        health["name"] = name;
        health["healthy"] = is_healthy;
        health["enabled"] = info.is_enabled;
        health["syncing"] = aggregator->isSyncing();
        health["dog_count"] = static_cast<Json::UInt64>(aggregator->getDogCount());
        health["shelter_count"] = static_cast<Json::UInt64>(aggregator->getShelterCount());
        health["last_sync_success"] = last_stats.isSuccessful();
        health["last_sync_errors"] = static_cast<Json::UInt64>(last_stats.error_count);

        // Format last sync time
        auto last_sync_time = aggregator->getLastSyncTime();
        if (last_sync_time.time_since_epoch().count() > 0) {
            auto time_t_val = std::chrono::system_clock::to_time_t(last_sync_time);
            char buffer[30];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t_val));
            health["last_sync"] = std::string(buffer);
        } else {
            health["last_sync"] = Json::nullValue;
        }

        callback(makeSuccessResponse(health));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::getHealth failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to get health status"));
    }
}

void AggregatorController::testConnection(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& name) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();

        if (!manager.hasAggregator(name)) {
            callback(makeErrorResponse(k404NotFound, "Aggregator not found: " + name));
            return;
        }

        auto* aggregator = manager.getAggregator(name);
        if (!aggregator) {
            callback(makeErrorResponse(k404NotFound, "Aggregator not found: " + name));
            return;
        }

        bool success = aggregator->testConnection();

        Json::Value data;
        data["name"] = name;
        data["connection_ok"] = success;
        data["message"] = success ? "Connection successful" : "Connection failed";

        callback(makeSuccessResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::testConnection failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to test connection"));
    }
}

void AggregatorController::getSyncHistory(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        // Get pagination parameters
        size_t limit = 50;
        size_t offset = 0;

        auto limit_param = req->getParameter("limit");
        if (!limit_param.empty()) {
            limit = static_cast<size_t>(std::stoul(limit_param));
        }

        auto offset_param = req->getParameter("offset");
        if (!offset_param.empty()) {
            offset = static_cast<size_t>(std::stoul(offset_param));
        }

        // Get optional aggregator filter
        auto aggregator_filter = req->getParameter("aggregator");

        auto& manager = AggregatorManager::getInstance();
        auto all_names = manager.getAggregatorNames();

        Json::Value history(Json::arrayValue);
        size_t total_count = 0;

        for (const auto& name : all_names) {
            // Apply filter if specified
            if (!aggregator_filter.empty() && name != aggregator_filter) {
                continue;
            }

            auto stats = manager.getLastSyncStats(name);
            auto last_sync = manager.getLastSync(name);

            if (last_sync.time_since_epoch().count() > 0) {
                total_count++;

                if (total_count > offset && history.size() < limit) {
                    Json::Value entry;
                    entry["aggregator"] = name;
                    entry["stats"] = stats.toJson();

                    auto time_t_val = std::chrono::system_clock::to_time_t(last_sync);
                    char buffer[30];
                    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t_val));
                    entry["completed_at"] = std::string(buffer);

                    history.append(entry);
                }
            }
        }

        Json::Value data;
        data["history"] = history;
        data["total_count"] = static_cast<Json::UInt64>(total_count);
        data["limit"] = static_cast<Json::UInt64>(limit);
        data["offset"] = static_cast<Json::UInt64>(offset);

        callback(makeSuccessResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::getSyncHistory failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to get sync history"));
    }
}

void AggregatorController::getAutoSyncStatus(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto& manager = AggregatorManager::getInstance();

        Json::Value data;
        data["enabled"] = manager.isAutoSyncEnabled();

        auto next_sync = manager.getNextScheduledSync();
        if (next_sync.time_since_epoch().count() > 0 && manager.isAutoSyncEnabled()) {
            auto time_t_val = std::chrono::system_clock::to_time_t(next_sync);
            char buffer[30];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t_val));
            data["next_sync"] = std::string(buffer);

            // Calculate seconds until next sync
            auto now = std::chrono::system_clock::now();
            auto seconds_until = std::chrono::duration_cast<std::chrono::seconds>(
                next_sync - now).count();
            data["seconds_until_next"] = static_cast<Json::Int64>(seconds_until);
        } else {
            data["next_sync"] = Json::nullValue;
            data["seconds_until_next"] = Json::nullValue;
        }

        callback(makeSuccessResponse(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::getAutoSyncStatus failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to get auto-sync status"));
    }
}

void AggregatorController::setAutoSync(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {

    if (!isAdmin(req)) {
        callback(makeErrorResponse(k401Unauthorized, "Admin authentication required"));
        return;
    }

    try {
        auto json_body = req->getJsonObject();
        if (!json_body) {
            callback(makeErrorResponse(k400BadRequest, "Invalid JSON body"));
            return;
        }

        auto& manager = AggregatorManager::getInstance();

        if (json_body->isMember("enabled")) {
            bool enabled = (*json_body)["enabled"].asBool();

            if (enabled) {
                // Get interval (default 60 minutes)
                int interval = 60;
                if (json_body->isMember("interval_minutes")) {
                    interval = (*json_body)["interval_minutes"].asInt();
                    if (interval < 5) {
                        callback(makeErrorResponse(k400BadRequest,
                            "Interval must be at least 5 minutes"));
                        return;
                    }
                }

                manager.startAutoSync(interval);

                Json::Value data;
                data["enabled"] = true;
                data["interval_minutes"] = interval;
                data["message"] = "Auto-sync enabled";
                callback(makeSuccessResponse(data));

            } else {
                manager.stopAutoSync();

                Json::Value data;
                data["enabled"] = false;
                data["message"] = "Auto-sync disabled";
                callback(makeSuccessResponse(data));
            }
        } else {
            callback(makeErrorResponse(k400BadRequest, "Missing 'enabled' field"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::EXTERNAL_API, "AggregatorController::setAutoSync failed", {"detail", e.what()});
        callback(makeErrorResponse(k500InternalServerError, "Failed to configure auto-sync"));
    }
}

} // namespace wtl::controllers
