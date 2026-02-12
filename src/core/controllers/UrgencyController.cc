/**
 * @file UrgencyController.cc
 * @brief Implementation of UrgencyController REST API endpoints
 * @see UrgencyController.h for documentation
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/controllers/UrgencyController.h"

// Project includes
#include "core/services/UrgencyLevel.h"
#include "core/services/UrgencyCalculator.h"
#include "core/services/KillShelterMonitor.h"
#include "core/services/AlertService.h"
#include "core/services/EuthanasiaTracker.h"
#include "core/services/UrgencyWorker.h"

// Auth include for token validation
#include "core/auth/AuthService.h"

// These will be provided by other agents
// #include "core/utils/ApiResponse.h"
// #include "core/auth/AuthMiddleware.h"
// #include "core/debug/ErrorCapture.h"

namespace wtl::core::controllers {

// ============================================================================
// PUBLIC DOG ENDPOINTS
// ============================================================================

void UrgencyController::getCriticalDogs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        // Get query parameters
        std::string state = getStringParam(req, "state");
        int limit = getIntParam(req, "limit", 50);

        auto& monitor = services::KillShelterMonitor::getInstance();
        auto dogs = monitor.getCriticalDogs();

        // TODO: Filter by state when models available
        // if (!state.empty()) {
        //     dogs = filterByState(dogs, state);
        // }

        // Build response
        Json::Value data(Json::arrayValue);
        int count = 0;
        for (const auto& dog : dogs) {
            if (count >= limit) break;
            data.append(dog.toJson());
            count++;
        }

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        Json::Value meta;
        meta["total"] = static_cast<int>(dogs.size());
        meta["returned"] = count;
        meta["urgency_level"] = "critical";
        response["meta"] = meta;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }
}

void UrgencyController::getHighUrgencyDogs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        std::string state = getStringParam(req, "state");
        int limit = getIntParam(req, "limit", 50);

        auto& monitor = services::KillShelterMonitor::getInstance();
        auto dogs = monitor.getHighUrgencyDogs();

        Json::Value data(Json::arrayValue);
        int count = 0;
        for (const auto& dog : dogs) {
            if (count >= limit) break;
            data.append(dog.toJson());
            count++;
        }

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        Json::Value meta;
        meta["total"] = static_cast<int>(dogs.size());
        meta["returned"] = count;
        meta["urgency_level"] = "high";
        response["meta"] = meta;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }
}

void UrgencyController::getUrgentDogs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int limit = getIntParam(req, "limit", 100);

        auto& monitor = services::KillShelterMonitor::getInstance();

        // Get both critical and high urgency dogs
        auto critical = monitor.getCriticalDogs();
        auto high = monitor.getHighUrgencyDogs();

        Json::Value data(Json::arrayValue);
        int count = 0;

        // Critical first
        for (const auto& dog : critical) {
            if (count >= limit) break;
            data.append(dog.toJson());
            count++;
        }

        // Then high
        for (const auto& dog : high) {
            if (count >= limit) break;
            data.append(dog.toJson());
            count++;
        }

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        Json::Value meta;
        meta["critical_count"] = static_cast<int>(critical.size());
        meta["high_count"] = static_cast<int>(high.size());
        meta["total"] = static_cast<int>(critical.size() + high.size());
        meta["returned"] = count;
        response["meta"] = meta;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }
}

void UrgencyController::getDogsAtRisk(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int days = getIntParam(req, "days", 7);
        std::string state = getStringParam(req, "state");
        std::string shelter_id = getStringParam(req, "shelter_id");
        int limit = getIntParam(req, "limit", 100);

        auto& monitor = services::KillShelterMonitor::getInstance();

        std::vector<models::Dog> dogs;
        if (!shelter_id.empty()) {
            dogs = monitor.getDogsAtRiskByShelter(shelter_id, days);
        } else if (!state.empty()) {
            dogs = monitor.getDogsAtRiskByState(state, days);
        } else {
            dogs = monitor.getDogsAtRisk(days);
        }

        Json::Value data(Json::arrayValue);
        int count = 0;
        for (const auto& dog : dogs) {
            if (count >= limit) break;
            data.append(dog.toJson());
            count++;
        }

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        Json::Value meta;
        meta["days_threshold"] = days;
        meta["total"] = static_cast<int>(dogs.size());
        meta["returned"] = count;
        if (!state.empty()) meta["state"] = state;
        if (!shelter_id.empty()) meta["shelter_id"] = shelter_id;
        response["meta"] = meta;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }
}

// ============================================================================
// SHELTER ENDPOINTS
// ============================================================================

void UrgencyController::getShelterUrgencyStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& shelter_id) {

    try {
        auto& monitor = services::KillShelterMonitor::getInstance();
        auto status = monitor.getShelterStatus(shelter_id);

        if (!status.has_value()) {
            callback(notFoundResponse("Shelter not found: " + shelter_id));
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["data"] = status->toJson();

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }

    (void)req;  // Unused but required by Drogon
}

void UrgencyController::getMostUrgentShelters(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        int limit = getIntParam(req, "limit", 20);

        auto& monitor = services::KillShelterMonitor::getInstance();
        auto shelters = monitor.getMostUrgentShelters(limit);

        Json::Value data(Json::arrayValue);
        for (const auto& shelter : shelters) {
            data.append(shelter.toJson());
        }

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        Json::Value meta;
        meta["returned"] = static_cast<int>(shelters.size());
        response["meta"] = meta;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }
}

// ============================================================================
// ALERT ENDPOINTS
// ============================================================================

void UrgencyController::getActiveAlerts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        // TODO: Add authentication check for sensitive data
        // REQUIRE_AUTH(req, callback);

        // Authentication check
        auto token = req->getHeader("Authorization");
        if (token.empty() || token.substr(0, 7) != "Bearer ") {
            auto errResp = drogon::HttpResponse::newHttpJsonResponse(
                Json::Value(Json::objectValue));
            errResp->setStatusCode(drogon::k401Unauthorized);
            (*errResp)["error"] = "Authentication required";
            callback(errResp);
            return;
        }

        std::string shelter_id = getStringParam(req, "shelter_id");
        std::string state = getStringParam(req, "state");

        auto& alert_service = services::AlertService::getInstance();

        std::vector<services::UrgencyAlert> alerts;
        if (!shelter_id.empty()) {
            alerts = alert_service.getAlertsForShelter(shelter_id);
        } else if (!state.empty()) {
            alerts = alert_service.getAlertsForState(state);
        } else {
            alerts = alert_service.getActiveAlerts();
        }

        Json::Value data(Json::arrayValue);
        for (const auto& alert : alerts) {
            data.append(alert.toJson());
        }

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        Json::Value meta;
        meta["total"] = static_cast<int>(alerts.size());
        response["meta"] = meta;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }
}

void UrgencyController::getCriticalAlerts(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto& alert_service = services::AlertService::getInstance();
        auto alerts = alert_service.getCriticalAlerts();

        Json::Value data(Json::arrayValue);
        for (const auto& alert : alerts) {
            data.append(alert.toJson());
        }

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        Json::Value meta;
        meta["total"] = static_cast<int>(alerts.size());
        meta["urgency_level"] = "critical";
        response["meta"] = meta;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }

    (void)req;
}

void UrgencyController::acknowledgeAlert(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& alert_id) {

    try {
        // TODO: Add authentication check
        // REQUIRE_ROLE(req, callback, "admin");

        // Authentication check
        auto token = req->getHeader("Authorization");
        if (token.empty() || token.substr(0, 7) != "Bearer ") {
            auto errResp = drogon::HttpResponse::newHttpJsonResponse(
                Json::Value(Json::objectValue));
            errResp->setStatusCode(drogon::k401Unauthorized);
            (*errResp)["error"] = "Authentication required";
            callback(errResp);
            return;
        }

        // Extract user ID from auth token
        std::string user_id = "";
        auto authHeader = req->getHeader("Authorization");
        if (authHeader.size() > 7 && authHeader.substr(0, 7) == "Bearer ") {
            try {
                auto& authService = wtl::core::auth::AuthService::getInstance();
                auto tokenData = authService.validateToken(authHeader.substr(7));
                if (tokenData) {
                    user_id = tokenData->user_id;
                }
            } catch (...) {
                // Token validation failed, proceed without user_id
            }
        }

        auto& alert_service = services::AlertService::getInstance();
        alert_service.acknowledgeAlert(alert_id, user_id);

        Json::Value response;
        response["success"] = true;
        response["message"] = "Alert acknowledged";
        response["alert_id"] = alert_id;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }

    (void)req;
}

void UrgencyController::resolveAlert(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& alert_id) {

    try {
        // TODO: Add authentication check
        // REQUIRE_ROLE(req, callback, "admin");

        // Authentication check
        auto token = req->getHeader("Authorization");
        if (token.empty() || token.substr(0, 7) != "Bearer ") {
            auto errResp = drogon::HttpResponse::newHttpJsonResponse(
                Json::Value(Json::objectValue));
            errResp->setStatusCode(drogon::k401Unauthorized);
            (*errResp)["error"] = "Authentication required";
            callback(errResp);
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            callback(errorResponse("INVALID_REQUEST", "Request body must be JSON"));
            return;
        }

        std::string resolution = (*json)["resolution"].asString();
        if (resolution.empty()) {
            callback(errorResponse("INVALID_REQUEST", "Resolution is required"));
            return;
        }

        std::string user_id = "";
        auto authHeader = req->getHeader("Authorization");
        if (authHeader.size() > 7 && authHeader.substr(0, 7) == "Bearer ") {
            try {
                auto& authService = wtl::core::auth::AuthService::getInstance();
                auto tokenData = authService.validateToken(authHeader.substr(7));
                if (tokenData) {
                    user_id = tokenData->user_id;
                }
            } catch (...) {
                // Token validation failed, proceed without user_id
            }
        }

        auto& alert_service = services::AlertService::getInstance();
        alert_service.resolveAlert(alert_id, resolution, user_id);

        Json::Value response;
        response["success"] = true;
        response["message"] = "Alert resolved";
        response["alert_id"] = alert_id;
        response["resolution"] = resolution;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }
}

// ============================================================================
// STATISTICS ENDPOINTS
// ============================================================================

void UrgencyController::getStatistics(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto& monitor = services::KillShelterMonitor::getInstance();
        auto& calculator = services::UrgencyCalculator::getInstance();
        auto& alert_service = services::AlertService::getInstance();

        Json::Value data;
        data["shelter_statistics"] = monitor.getUrgencyStatistics();
        data["calculator_statistics"] = calculator.getStatistics();
        data["alert_statistics"] = alert_service.getStatistics();

        // Summary counts
        Json::Value summary;
        summary["total_kill_shelters"] = monitor.getTotalKillShelters();
        summary["total_critical_dogs"] = monitor.getTotalCriticalDogs();
        summary["total_dogs_at_risk_7_days"] = monitor.getTotalDogsAtRisk(7);
        summary["total_on_euthanasia_list"] = monitor.getTotalOnEuthanasiaList();
        data["summary"] = summary;

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }

    (void)req;
}

void UrgencyController::getWorkerStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        auto& worker = services::UrgencyWorker::getInstance();

        Json::Value data = worker.getStatistics();

        // Add run history
        auto history = worker.getRunHistory(10);
        Json::Value history_json(Json::arrayValue);
        for (const auto& run : history) {
            history_json.append(run.toJson());
        }
        data["recent_runs"] = history_json;

        Json::Value response;
        response["success"] = true;
        response["data"] = data;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }

    (void)req;
}

// ============================================================================
// ADMIN ENDPOINTS
// ============================================================================

void UrgencyController::forceRecalculate(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    try {
        // TODO: Add authentication check
        // REQUIRE_ROLE(req, callback, "admin");

        // Authentication check
        auto token = req->getHeader("Authorization");
        if (token.empty() || token.substr(0, 7) != "Bearer ") {
            auto errResp = drogon::HttpResponse::newHttpJsonResponse(
                Json::Value(Json::objectValue));
            errResp->setStatusCode(drogon::k401Unauthorized);
            (*errResp)["error"] = "Authentication required";
            callback(errResp);
            return;
        }

        auto& worker = services::UrgencyWorker::getInstance();
        auto result = worker.runNow();

        Json::Value data = result.toJson();

        Json::Value response;
        response["success"] = result.success;
        response["data"] = data;
        response["message"] = result.success ?
            "Recalculation completed successfully" :
            "Recalculation failed: " + result.error_message;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }

    (void)req;
}

void UrgencyController::forceRecalculateShelter(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& shelter_id) {

    try {
        // TODO: Add authentication check
        // REQUIRE_ROLE(req, callback, "admin");

        // Authentication check
        auto token = req->getHeader("Authorization");
        if (token.empty() || token.substr(0, 7) != "Bearer ") {
            auto errResp = drogon::HttpResponse::newHttpJsonResponse(
                Json::Value(Json::objectValue));
            errResp->setStatusCode(drogon::k401Unauthorized);
            (*errResp)["error"] = "Authentication required";
            callback(errResp);
            return;
        }

        auto& worker = services::UrgencyWorker::getInstance();
        auto result = worker.runForShelter(shelter_id);

        Json::Value data = result.toJson();
        data["shelter_id"] = shelter_id;

        Json::Value response;
        response["success"] = result.success;
        response["data"] = data;
        response["message"] = result.success ?
            "Shelter recalculation completed" :
            "Recalculation failed: " + result.error_message;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }

    (void)req;
}

// ============================================================================
// DOG-SPECIFIC ENDPOINTS
// ============================================================================

void UrgencyController::getDogUrgency(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    try {
        // TODO: Implementation when DogService and ShelterService are available
        // auto& dog_service = DogService::getInstance();
        // auto& shelter_service = ShelterService::getInstance();
        // auto& calculator = services::UrgencyCalculator::getInstance();
        //
        // auto dog = dog_service.findById(dog_id);
        // if (!dog) {
        //     callback(notFoundResponse("Dog not found: " + dog_id));
        //     return;
        // }
        //
        // auto shelter = shelter_service.findById(dog->shelter_id);
        // if (!shelter) {
        //     callback(errorResponse("SHELTER_NOT_FOUND", "Shelter not found for dog"));
        //     return;
        // }
        //
        // auto result = calculator.calculateDetailed(*dog, *shelter);

        Json::Value response;
        response["success"] = true;
        // response["data"] = result.toJson();

        Json::Value placeholder;
        placeholder["dog_id"] = dog_id;
        placeholder["message"] = "Full implementation pending DogService integration";
        response["data"] = placeholder;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }

    (void)req;
}

void UrgencyController::getDogCountdown(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dog_id) {

    try {
        auto& tracker = services::EuthanasiaTracker::getInstance();
        auto countdown = tracker.getCountdown(dog_id);

        if (!countdown.has_value()) {
            // No euthanasia date set
            Json::Value response;
            response["success"] = true;
            response["data"]["dog_id"] = dog_id;
            response["data"]["has_countdown"] = false;
            response["data"]["message"] = "No euthanasia date set for this dog";

            auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
            callback(resp);
            return;
        }

        Json::Value response;
        response["success"] = true;

        Json::Value data = countdown->toJson();
        data["dog_id"] = dog_id;
        data["has_countdown"] = true;
        response["data"] = data;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception& e) {
        callback(errorResponse("INTERNAL_ERROR", e.what(),
                               drogon::k500InternalServerError));
    }

    (void)req;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

drogon::HttpResponsePtr UrgencyController::successResponse(const Json::Value& data) {
    Json::Value response;
    response["success"] = true;
    response["data"] = data;
    return drogon::HttpResponse::newHttpJsonResponse(response);
}

drogon::HttpResponsePtr UrgencyController::successResponse(
    const Json::Value& data,
    int total,
    int page,
    int per_page) {

    Json::Value response;
    response["success"] = true;
    response["data"] = data;

    Json::Value meta;
    meta["total"] = total;
    meta["page"] = page;
    meta["per_page"] = per_page;
    meta["total_pages"] = (total + per_page - 1) / per_page;
    response["meta"] = meta;

    return drogon::HttpResponse::newHttpJsonResponse(response);
}

drogon::HttpResponsePtr UrgencyController::errorResponse(
    const std::string& code,
    const std::string& message,
    drogon::HttpStatusCode status) {

    Json::Value response;
    response["success"] = false;

    Json::Value error;
    error["code"] = code;
    error["message"] = message;
    response["error"] = error;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(status);
    return resp;
}

drogon::HttpResponsePtr UrgencyController::notFoundResponse(const std::string& resource) {
    return errorResponse("NOT_FOUND", resource, drogon::k404NotFound);
}

int UrgencyController::getIntParam(
    const drogon::HttpRequestPtr& req,
    const std::string& name,
    int default_value) {

    auto value = req->getParameter(name);
    if (value.empty()) {
        return default_value;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return default_value;
    }
}

std::string UrgencyController::getStringParam(
    const drogon::HttpRequestPtr& req,
    const std::string& name,
    const std::string& default_value) {

    auto value = req->getParameter(name);
    return value.empty() ? default_value : value;
}

} // namespace wtl::core::controllers
