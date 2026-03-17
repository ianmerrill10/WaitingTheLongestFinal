/**
 * @file AdminController.cc
 * @brief Implementation of AdminController
 * @see AdminController.h for documentation
 */

#include "controllers/AdminController.h"

// Standard library includes
#include <algorithm>
#include <sstream>
#include <chrono>
#include <ctime>

// Project includes
#include "admin/AdminService.h"
#include "admin/SystemConfig.h"
#include "admin/AuditLog.h"
#include "core/auth/AuthMiddleware.h"
#include "core/services/DogService.h"
#include "core/services/ShelterService.h"
#include "core/services/UserService.h"
#include "core/utils/ApiResponse.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/debug/HealthCheck.h"

namespace wtl::core::controllers {

using namespace ::wtl::core::utils;
using namespace ::wtl::core::auth;
using namespace ::wtl::core::debug;
using namespace ::wtl::admin;

// ============================================================================
// DASHBOARD ENDPOINTS
// ============================================================================

void AdminController::getDashboardStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        Json::Value stats = admin_service.getDashboardStats();

        // Log the admin action
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "VIEW_DASHBOARD",
            "system",
            "admin_dashboard",
            Json::Value()
        );

        callback(ApiResponse::success(stats));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get dashboard stats: " + std::string(e.what()),
                         {{"user_id", auth_token->user_id}});
        callback(ApiResponse::serverError("Failed to retrieve dashboard statistics"));
    }
}

void AdminController::getSystemHealth(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        Json::Value health = admin_service.getSystemHealth();

        callback(ApiResponse::success(health));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get system health: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve system health"));
    }
}

// ============================================================================
// DOG MANAGEMENT ENDPOINTS
// ============================================================================

void AdminController::getDogs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        int page, per_page;
        parsePagination(req, page, per_page);

        std::string sort_by, sort_dir;
        parseSortParams(req, sort_by, sort_dir, "intake_date");

        std::string status = getStringParam(req, "status", "");
        std::string shelter_id = getStringParam(req, "shelter_id", "");
        std::string urgency = getStringParam(req, "urgency", "");
        std::string search = getStringParam(req, "search", "");

        auto& dog_service = services::DogService::getInstance();

        // Get total count for pagination
        int total = dog_service.countAll();

        // Get dogs with pagination
        int offset = (page - 1) * per_page;
        auto dogs = dog_service.findAll(per_page, offset);

        // Convert to JSON array
        Json::Value dogs_json(Json::arrayValue);
        for (const auto& dog : dogs) {
            dogs_json.append(dog.toJson());
        }

        callback(ApiResponse::success(dogs_json, total, page, per_page));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get dogs: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve dogs"));
    }
}

void AdminController::getDogById(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& dog_service = services::DogService::getInstance();
        auto dog = dog_service.findById(id);

        if (!dog) {
            callback(ApiResponse::notFound("Dog"));
            return;
        }

        callback(ApiResponse::success(dog->toJson()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get dog: " + std::string(e.what()),
                         {{"dog_id", id}});
        callback(ApiResponse::serverError("Failed to retrieve dog"));
    }
}

void AdminController::updateDog(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        auto& dog_service = services::DogService::getInstance();

        // Get existing dog
        auto existing = dog_service.findById(id);
        if (!existing) {
            callback(ApiResponse::notFound("Dog"));
            return;
        }

        // Create updated dog from JSON
        auto updated_dog = models::Dog::fromJson(*json);
        updated_dog.id = id; // Ensure ID is preserved

        // Update in database
        auto result = dog_service.update(updated_dog);

        // Log the action
        Json::Value details;
        details["dog_id"] = id;
        details["changes"] = *json;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "UPDATE_DOG",
            "dog",
            id,
            details
        );

        callback(ApiResponse::success(result.toJson()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to update dog: " + std::string(e.what()),
                         {{"dog_id", id}});
        callback(ApiResponse::serverError("Failed to update dog"));
    }
}

void AdminController::deleteDog(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& dog_service = services::DogService::getInstance();

        // Verify dog exists
        auto existing = dog_service.findById(id);
        if (!existing) {
            callback(ApiResponse::notFound("Dog"));
            return;
        }

        // Delete the dog
        bool deleted = dog_service.deleteById(id);

        if (deleted) {
            // Log the action
            Json::Value details;
            details["dog_id"] = id;
            details["dog_name"] = existing->name;
            AuditLog::getInstance().logAction(
                auth_token->user_id,
                "DELETE_DOG",
                "dog",
                id,
                details
            );

            callback(ApiResponse::noContent());
        } else {
            callback(ApiResponse::serverError("Failed to delete dog"));
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to delete dog: " + std::string(e.what()),
                         {{"dog_id", id}});
        callback(ApiResponse::serverError("Failed to delete dog"));
    }
}

void AdminController::batchUpdateDogs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json || !(*json).isMember("dog_ids") || !(*json).isMember("updates")) {
            callback(ApiResponse::badRequest("Missing dog_ids or updates"));
            return;
        }

        const Json::Value& dog_ids = (*json)["dog_ids"];
        const Json::Value& updates = (*json)["updates"];

        if (!dog_ids.isArray() || dog_ids.empty()) {
            callback(ApiResponse::badRequest("dog_ids must be a non-empty array"));
            return;
        }

        auto& dog_service = services::DogService::getInstance();

        int updated_count = 0;
        Json::Value failed_ids(Json::arrayValue);

        for (const auto& id_val : dog_ids) {
            std::string id = id_val.asString();
            try {
                auto existing = dog_service.findById(id);
                if (existing) {
                    // Apply updates to existing dog
                    auto dog = *existing;

                    // Apply each update field
                    if (updates.isMember("status")) {
                        dog.status = updates["status"].asString();
                    }
                    if (updates.isMember("urgency_level")) {
                        dog.urgency_level = updates["urgency_level"].asString();
                    }
                    if (updates.isMember("is_available")) {
                        dog.is_available = updates["is_available"].asBool();
                    }

                    dog_service.update(dog);
                    updated_count++;
                } else {
                    failed_ids.append(id);
                }
            } catch (...) {
                failed_ids.append(id);
            }
        }

        // Log the batch action
        Json::Value details;
        details["updated_count"] = updated_count;
        details["total_requested"] = static_cast<int>(dog_ids.size());
        details["updates"] = updates;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "BATCH_UPDATE_DOGS",
            "dog",
            "batch",
            details
        );

        Json::Value result;
        result["updated_count"] = updated_count;
        result["failed_ids"] = failed_ids;

        callback(ApiResponse::success(result));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to batch update dogs: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to batch update dogs"));
    }
}

// ============================================================================
// SHELTER MANAGEMENT ENDPOINTS
// ============================================================================

void AdminController::getShelters(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        int page, per_page;
        parsePagination(req, page, per_page);

        std::string state = getStringParam(req, "state", "");
        std::string search = getStringParam(req, "search", "");
        bool verified_only = getBoolParam(req, "verified", false);

        // These filters are parsed for future use
        (void)verified_only;

        auto& shelter_service = services::ShelterService::getInstance();

        int total = shelter_service.countAll();
        int offset = (page - 1) * per_page;

        std::vector<models::Shelter> shelters;
        if (!state.empty()) {
            shelters = shelter_service.findByStateCode(state);
        } else {
            shelters = shelter_service.findAll(per_page, offset);
        }

        Json::Value shelters_json(Json::arrayValue);
        for (const auto& shelter : shelters) {
            shelters_json.append(shelter.toJson());
        }

        callback(ApiResponse::success(shelters_json, total, page, per_page));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get shelters: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve shelters"));
    }
}

void AdminController::getShelterById(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& shelter_service = services::ShelterService::getInstance();
        auto shelter = shelter_service.findById(id);

        if (!shelter) {
            callback(ApiResponse::notFound("Shelter"));
            return;
        }

        callback(ApiResponse::success(shelter->toJson()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get shelter: " + std::string(e.what()),
                         {{"shelter_id", id}});
        callback(ApiResponse::serverError("Failed to retrieve shelter"));
    }
}

void AdminController::updateShelter(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        auto& shelter_service = services::ShelterService::getInstance();

        auto existing = shelter_service.findById(id);
        if (!existing) {
            callback(ApiResponse::notFound("Shelter"));
            return;
        }

        auto updated_shelter = models::Shelter::fromJson(*json);
        updated_shelter.id = id;

        auto result = shelter_service.update(updated_shelter);

        // Log the action
        Json::Value details;
        details["shelter_id"] = id;
        details["changes"] = *json;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "UPDATE_SHELTER",
            "shelter",
            id,
            details
        );

        callback(ApiResponse::success(result.toJson()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to update shelter: " + std::string(e.what()),
                         {{"shelter_id", id}});
        callback(ApiResponse::serverError("Failed to update shelter"));
    }
}

void AdminController::verifyShelter(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json || !(*json).isMember("verified")) {
            callback(ApiResponse::badRequest("Missing 'verified' field"));
            return;
        }

        bool verified = (*json)["verified"].asBool();
        std::string notes = (*json).get("notes", "").asString();

        auto& shelter_service = services::ShelterService::getInstance();

        auto shelter = shelter_service.findById(id);
        if (!shelter) {
            callback(ApiResponse::notFound("Shelter"));
            return;
        }

        shelter->is_verified = verified;
        auto result = shelter_service.update(*shelter);

        // Log the action
        Json::Value details;
        details["shelter_id"] = id;
        details["verified"] = verified;
        details["notes"] = notes;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            verified ? "VERIFY_SHELTER" : "UNVERIFY_SHELTER",
            "shelter",
            id,
            details
        );

        callback(ApiResponse::success(result.toJson()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to verify shelter: " + std::string(e.what()),
                         {{"shelter_id", id}});
        callback(ApiResponse::serverError("Failed to verify shelter"));
    }
}

void AdminController::getShelterStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& shelter_service = services::ShelterService::getInstance();
        auto& dog_service = services::DogService::getInstance();

        auto shelter = shelter_service.findById(id);
        if (!shelter) {
            callback(ApiResponse::notFound("Shelter"));
            return;
        }

        Json::Value stats;
        stats["shelter_id"] = id;
        stats["shelter_name"] = shelter->name;
        stats["total_dogs"] = dog_service.countByShelterId(id);
        stats["is_kill_shelter"] = shelter->is_kill_shelter;
        stats["is_verified"] = shelter->is_verified;

        // Get dogs by status
        auto dogs = dog_service.findByShelterId(id);
        int available = 0, adopted = 0, fostered = 0, urgent = 0;
        for (const auto& dog : dogs) {
            if (dog.status == "available") available++;
            else if (dog.status == "adopted") adopted++;
            else if (dog.status == "fostered") fostered++;

            if (dog.urgency_level == "CRITICAL" || dog.urgency_level == "HIGH") {
                urgent++;
            }
        }

        stats["available_dogs"] = available;
        stats["adopted_dogs"] = adopted;
        stats["fostered_dogs"] = fostered;
        stats["urgent_dogs"] = urgent;

        callback(ApiResponse::success(stats));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get shelter stats: " + std::string(e.what()),
                         {{"shelter_id", id}});
        callback(ApiResponse::serverError("Failed to retrieve shelter statistics"));
    }
}

// ============================================================================
// USER MANAGEMENT ENDPOINTS
// ============================================================================

void AdminController::getUsers(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        int page, per_page;
        parsePagination(req, page, per_page);

        std::string role = getStringParam(req, "role", "");
        std::string search = getStringParam(req, "search", "");

        auto& admin_service = AdminService::getInstance();
        auto result = admin_service.getUsers(page, per_page, role, search);

        callback(ApiResponse::success(result["users"],
                                      result["total"].asInt(),
                                      page, per_page));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get users: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve users"));
    }
}

void AdminController::getUserById(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& user_service = services::UserService::getInstance();
        auto user = user_service.findById(id);

        if (!user) {
            callback(ApiResponse::notFound("User"));
            return;
        }

        // Return private JSON (includes more details for admin)
        callback(ApiResponse::success(user->toJsonPrivate()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get user: " + std::string(e.what()),
                         {{"user_id", id}});
        callback(ApiResponse::serverError("Failed to retrieve user"));
    }
}

void AdminController::updateUser(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        auto& user_service = services::UserService::getInstance();

        auto existing = user_service.findById(id);
        if (!existing) {
            callback(ApiResponse::notFound("User"));
            return;
        }

        // Update allowed fields
        auto user = *existing;
        if ((*json).isMember("display_name")) {
            user.display_name = (*json)["display_name"].asString();
        }
        if ((*json).isMember("email_verified")) {
            user.email_verified = (*json)["email_verified"].asBool();
        }

        auto result = user_service.update(user);

        // Log the action
        Json::Value details;
        details["user_id"] = id;
        details["changes"] = *json;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "UPDATE_USER",
            "user",
            id,
            details
        );

        callback(ApiResponse::success(result.toJson()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to update user: " + std::string(e.what()),
                         {{"user_id", id}});
        callback(ApiResponse::serverError("Failed to update user"));
    }
}

void AdminController::changeUserRole(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json || !(*json).isMember("role")) {
            callback(ApiResponse::badRequest("Missing 'role' field"));
            return;
        }

        std::string new_role = (*json)["role"].asString();

        // Validate role
        if (new_role != "user" && new_role != "foster" &&
            new_role != "shelter_admin" && new_role != "admin") {
            callback(ApiResponse::badRequest("Invalid role. Must be: user, foster, shelter_admin, or admin"));
            return;
        }

        // Prevent admin from demoting themselves
        if (id == auth_token->user_id && new_role != "admin") {
            callback(ApiResponse::badRequest("Cannot change your own role"));
            return;
        }

        auto& user_service = services::UserService::getInstance();

        auto user = user_service.findById(id);
        if (!user) {
            callback(ApiResponse::notFound("User"));
            return;
        }

        std::string old_role = user->role;
        user->role = new_role;

        auto result = user_service.update(*user);

        // Log the action
        Json::Value details;
        details["user_id"] = id;
        details["old_role"] = old_role;
        details["new_role"] = new_role;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "CHANGE_USER_ROLE",
            "user",
            id,
            details
        );

        callback(ApiResponse::success(result.toJson()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to change user role: " + std::string(e.what()),
                         {{"user_id", id}});
        callback(ApiResponse::serverError("Failed to change user role"));
    }
}

void AdminController::disableUser(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        // Prevent admin from disabling themselves
        if (id == auth_token->user_id) {
            callback(ApiResponse::badRequest("Cannot disable your own account"));
            return;
        }

        auto& user_service = services::UserService::getInstance();

        auto user = user_service.findById(id);
        if (!user) {
            callback(ApiResponse::notFound("User"));
            return;
        }

        user->is_active = false;
        auto result = user_service.update(*user);

        // Log the action
        Json::Value details;
        details["user_id"] = id;
        details["user_email"] = user->email;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "DISABLE_USER",
            "user",
            id,
            details
        );

        callback(ApiResponse::success(result.toJson()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to disable user: " + std::string(e.what()),
                         {{"user_id", id}});
        callback(ApiResponse::serverError("Failed to disable user"));
    }
}

void AdminController::enableUser(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& user_service = services::UserService::getInstance();

        auto user = user_service.findById(id);
        if (!user) {
            callback(ApiResponse::notFound("User"));
            return;
        }

        user->is_active = true;
        auto result = user_service.update(*user);

        // Log the action
        Json::Value details;
        details["user_id"] = id;
        details["user_email"] = user->email;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "ENABLE_USER",
            "user",
            id,
            details
        );

        callback(ApiResponse::success(result.toJson()));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to enable user: " + std::string(e.what()),
                         {{"user_id", id}});
        callback(ApiResponse::serverError("Failed to enable user"));
    }
}

// ============================================================================
// SYSTEM CONFIGURATION ENDPOINTS
// ============================================================================

void AdminController::getConfig(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& config = SystemConfig::getInstance();
        Json::Value all_config = config.getAll();

        callback(ApiResponse::success(all_config));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONFIGURATION,
                         "Failed to get config: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve configuration"));
    }
}

void AdminController::updateConfig(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        auto& config = SystemConfig::getInstance();

        Json::Value updated(Json::arrayValue);
        Json::Value failed(Json::arrayValue);

        // Update each config key
        for (const auto& key : (*json).getMemberNames()) {
            std::string value = (*json)[key].asString();
            bool success = config.set(key, value);

            if (success) {
                updated.append(key);
            } else {
                failed.append(key);
            }
        }

        // Log the action
        Json::Value details;
        details["updated_keys"] = updated;
        details["failed_keys"] = failed;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "UPDATE_CONFIG",
            "config",
            "batch",
            details
        );

        Json::Value result;
        result["updated"] = updated;
        result["failed"] = failed;

        callback(ApiResponse::success(result));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONFIGURATION,
                         "Failed to update config: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to update configuration"));
    }
}

void AdminController::getConfigByKey(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& key) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& config = SystemConfig::getInstance();
        auto value = config.get(key);

        if (!value) {
            callback(ApiResponse::notFound("Configuration key"));
            return;
        }

        Json::Value result;
        result["key"] = key;
        result["value"] = *value;

        callback(ApiResponse::success(result));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONFIGURATION,
                         "Failed to get config key: " + std::string(e.what()),
                         {{"key", key}});
        callback(ApiResponse::serverError("Failed to retrieve configuration"));
    }
}

void AdminController::setConfigValue(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& key) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json || !(*json).isMember("value")) {
            callback(ApiResponse::badRequest("Missing 'value' field"));
            return;
        }

        std::string value = (*json)["value"].asString();

        auto& config = SystemConfig::getInstance();

        // Get old value for audit
        auto old_value = config.get(key);

        bool success = config.set(key, value);

        if (!success) {
            callback(ApiResponse::badRequest("Failed to set configuration value"));
            return;
        }

        // Log the action
        Json::Value details;
        details["key"] = key;
        details["old_value"] = old_value ? *old_value : "";
        details["new_value"] = value;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "SET_CONFIG",
            "config",
            key,
            details
        );

        Json::Value result;
        result["key"] = key;
        result["value"] = value;

        callback(ApiResponse::success(result));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONFIGURATION,
                         "Failed to set config: " + std::string(e.what()),
                         {{"key", key}});
        callback(ApiResponse::serverError("Failed to set configuration"));
    }
}

// ============================================================================
// AUDIT LOG ENDPOINTS
// ============================================================================

void AdminController::getAuditLog(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        int page, per_page;
        parsePagination(req, page, per_page);

        std::string action = getStringParam(req, "action", "");
        std::string entity_type = getStringParam(req, "entity_type", "");
        std::string user_id = getStringParam(req, "user_id", "");

        auto& audit_log = AuditLog::getInstance();

        AuditLogFilters filters;
        filters.action = action;
        filters.entity_type = entity_type;
        filters.user_id = user_id;

        auto result = audit_log.getAuditLog(filters, page, per_page);

        callback(ApiResponse::success(result["entries"],
                                      result["total"].asInt(),
                                      page, per_page));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get audit log: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve audit log"));
    }
}

void AdminController::getAuditLogByUser(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& user_id) {

    REQUIRE_ADMIN(req, callback);

    try {
        int page, per_page;
        parsePagination(req, page, per_page);

        auto& audit_log = AuditLog::getInstance();

        AuditLogFilters filters;
        filters.user_id = user_id;

        auto result = audit_log.getAuditLog(filters, page, per_page);

        callback(ApiResponse::success(result["entries"],
                                      result["total"].asInt(),
                                      page, per_page));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get user audit log: " + std::string(e.what()),
                         {{"user_id", user_id}});
        callback(ApiResponse::serverError("Failed to retrieve audit log"));
    }
}

void AdminController::getAuditLogByEntity(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& entity_type,
    const std::string& entity_id) {

    REQUIRE_ADMIN(req, callback);

    try {
        int page, per_page;
        parsePagination(req, page, per_page);

        auto& audit_log = AuditLog::getInstance();

        AuditLogFilters filters;
        filters.entity_type = entity_type;
        filters.entity_id = entity_id;

        auto result = audit_log.getAuditLog(filters, page, per_page);

        callback(ApiResponse::success(result["entries"],
                                      result["total"].asInt(),
                                      page, per_page));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get entity audit log: " + std::string(e.what()),
                         {{"entity_type", entity_type}, {"entity_id", entity_id}});
        callback(ApiResponse::serverError("Failed to retrieve audit log"));
    }
}

// ============================================================================
// CONTENT MANAGEMENT ENDPOINTS
// ============================================================================

void AdminController::getContentQueue(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        auto queue = admin_service.getContentQueue();

        callback(ApiResponse::success(queue));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get content queue: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve content queue"));
    }
}

void AdminController::approveContent(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        bool success = admin_service.approveContent(id);

        if (!success) {
            callback(ApiResponse::notFound("Content"));
            return;
        }

        // Log the action
        Json::Value details;
        details["content_id"] = id;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "APPROVE_CONTENT",
            "content",
            id,
            details
        );

        callback(ApiResponse::noContent());
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to approve content: " + std::string(e.what()),
                         {{"content_id", id}});
        callback(ApiResponse::serverError("Failed to approve content"));
    }
}

void AdminController::rejectContent(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        std::string reason = "";
        if (json && (*json).isMember("reason")) {
            reason = (*json)["reason"].asString();
        }

        auto& admin_service = AdminService::getInstance();
        bool success = admin_service.rejectContent(id, reason);

        if (!success) {
            callback(ApiResponse::notFound("Content"));
            return;
        }

        // Log the action
        Json::Value details;
        details["content_id"] = id;
        details["reason"] = reason;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "REJECT_CONTENT",
            "content",
            id,
            details
        );

        callback(ApiResponse::noContent());
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to reject content: " + std::string(e.what()),
                         {{"content_id", id}});
        callback(ApiResponse::serverError("Failed to reject content"));
    }
}

// ============================================================================
// ANALYTICS ENDPOINTS
// ============================================================================

void AdminController::getAnalytics(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        std::string period = getStringParam(req, "period", "week");

        auto& admin_service = AdminService::getInstance();
        auto analytics = admin_service.getAnalytics(period);

        callback(ApiResponse::success(analytics));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get analytics: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve analytics"));
    }
}

void AdminController::getTrafficStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        auto stats = admin_service.getTrafficStats();

        callback(ApiResponse::success(stats));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get traffic stats: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve traffic statistics"));
    }
}

void AdminController::getAdoptionFunnel(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        auto funnel = admin_service.getAdoptionFunnel();

        callback(ApiResponse::success(funnel));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get adoption funnel: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve adoption funnel"));
    }
}

void AdminController::getSocialPerformance(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        auto social = admin_service.getSocialStats();

        callback(ApiResponse::success(social));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get social performance: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve social performance"));
    }
}

void AdminController::getImpactMetrics(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        auto impact = admin_service.getImpactMetrics();

        callback(ApiResponse::success(impact));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get impact metrics: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve impact metrics"));
    }
}

// ============================================================================
// WORKER MANAGEMENT ENDPOINTS
// ============================================================================

void AdminController::getWorkerStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        auto workers = admin_service.getWorkerStatus();

        callback(ApiResponse::success(workers));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get worker status: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve worker status"));
    }
}

void AdminController::restartWorker(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& name) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& admin_service = AdminService::getInstance();
        bool success = admin_service.restartWorker(name);

        if (!success) {
            callback(ApiResponse::notFound("Worker"));
            return;
        }

        // Log the action
        Json::Value details;
        details["worker_name"] = name;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "RESTART_WORKER",
            "worker",
            name,
            details
        );

        Json::Value result;
        result["worker"] = name;
        result["status"] = "restarting";

        callback(ApiResponse::success(result));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to restart worker: " + std::string(e.what()),
                         {{"worker", name}});
        callback(ApiResponse::serverError("Failed to restart worker"));
    }
}

// ============================================================================
// FEATURE FLAGS ENDPOINTS
// ============================================================================

void AdminController::getFeatureFlags(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto& config = SystemConfig::getInstance();
        auto flags = config.getFeatureFlags();

        callback(ApiResponse::success(flags));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONFIGURATION,
                         "Failed to get feature flags: " + std::string(e.what()),
                         {});
        callback(ApiResponse::serverError("Failed to retrieve feature flags"));
    }
}

void AdminController::setFeatureFlag(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& key) {

    REQUIRE_ADMIN(req, callback);

    try {
        auto json = req->getJsonObject();
        if (!json || !(*json).isMember("enabled")) {
            callback(ApiResponse::badRequest("Missing 'enabled' field"));
            return;
        }

        bool enabled = (*json)["enabled"].asBool();
        int rollout = (*json).get("rollout_percentage", 100).asInt();

        auto& config = SystemConfig::getInstance();
        bool success = config.setFeatureFlag(key, enabled, rollout);

        if (!success) {
            callback(ApiResponse::badRequest("Failed to set feature flag"));
            return;
        }

        // Log the action
        Json::Value details;
        details["flag_key"] = key;
        details["enabled"] = enabled;
        details["rollout_percentage"] = rollout;
        AuditLog::getInstance().logAction(
            auth_token->user_id,
            "SET_FEATURE_FLAG",
            "feature_flag",
            key,
            details
        );

        Json::Value result;
        result["key"] = key;
        result["enabled"] = enabled;
        result["rollout_percentage"] = rollout;

        callback(ApiResponse::success(result));
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONFIGURATION,
                         "Failed to set feature flag: " + std::string(e.what()),
                         {{"key", key}});
        callback(ApiResponse::serverError("Failed to set feature flag"));
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void AdminController::parsePagination(const drogon::HttpRequestPtr& req,
                                      int& page, int& per_page) {
    page = getIntParam(req, "page", 1);
    per_page = getIntParam(req, "per_page", 20);

    // Enforce limits
    if (page < 1) page = 1;
    if (per_page < 1) per_page = 1;
    if (per_page > 100) per_page = 100;
}

void AdminController::parseSortParams(const drogon::HttpRequestPtr& req,
                                      std::string& sort_by, std::string& sort_dir,
                                      const std::string& default_sort) {
    sort_by = getStringParam(req, "sort_by", default_sort);
    sort_dir = getStringParam(req, "sort_dir", "desc");

    // Validate sort direction
    if (sort_dir != "asc" && sort_dir != "desc") {
        sort_dir = "desc";
    }
}

std::string AdminController::getStringParam(const drogon::HttpRequestPtr& req,
                                           const std::string& name,
                                           const std::string& default_value) {
    auto param = req->getParameter(name);
    return param.empty() ? default_value : param;
}

int AdminController::getIntParam(const drogon::HttpRequestPtr& req,
                                const std::string& name,
                                int default_value) {
    auto param = req->getParameter(name);
    if (param.empty()) return default_value;

    try {
        return std::stoi(param);
    } catch (...) {
        return default_value;
    }
}

bool AdminController::getBoolParam(const drogon::HttpRequestPtr& req,
                                  const std::string& name,
                                  bool default_value) {
    auto param = req->getParameter(name);
    if (param.empty()) return default_value;

    return param == "true" || param == "1" || param == "yes";
}

} // namespace wtl::core::controllers
