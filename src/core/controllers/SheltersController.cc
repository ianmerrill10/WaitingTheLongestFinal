/**
 * @file SheltersController.cc
 * @brief Implementation of SheltersController
 * @see SheltersController.h for documentation
 */

#include "core/controllers/SheltersController.h"

// Project includes
#include "core/utils/ApiResponse.h"
#include "core/services/ShelterService.h"
#include "core/services/DogService.h"
#include "core/auth/AuthMiddleware.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::core::controllers {

using namespace ::wtl::core::utils;
using namespace ::wtl::core::services;
using namespace ::wtl::core::models;
using namespace ::wtl::core::auth;

// ============================================================================
// PUBLIC READ ENDPOINTS
// ============================================================================

void SheltersController::getAllShelters(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        int page, per_page;
        parsePaginationParams(req, page, per_page);

        auto& shelter_service = ShelterService::getInstance();

        int offset = (page - 1) * per_page;

        // Check for state filter
        std::vector<Shelter> shelters;
        int total;

        auto state_code = req->getParameter("state_code");
        if (!state_code.empty()) {
            shelters = shelter_service.findByStateCode(state_code);
            total = shelter_service.countByStateCode(state_code);
        } else {
            shelters = shelter_service.findAll(per_page, offset);
            total = shelter_service.countAll();
        }

        // Convert shelters to JSON array
        Json::Value shelters_json(Json::arrayValue);
        for (const auto& shelter : shelters) {
            shelters_json.append(shelter.toJson());
        }

        callback(ApiResponse::success(shelters_json, total, page, per_page));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get all shelters: " + std::string(e.what()),
            {{"endpoint", "/api/shelters"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve shelters"));
    }
}

void SheltersController::getShelterById(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                         const std::string& id) {
    try {
        auto& shelter_service = ShelterService::getInstance();
        auto shelter = shelter_service.findById(id);

        if (!shelter) {
            callback(ApiResponse::notFound("Shelter"));
            return;
        }

        callback(ApiResponse::success(shelter->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get shelter by ID: " + std::string(e.what()),
            {{"endpoint", "/api/shelters/:id"}, {"method", "GET"}, {"shelter_id", id}}
        );
        callback(ApiResponse::serverError("Failed to retrieve shelter"));
    }
}

void SheltersController::getDogsAtShelter(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                           const std::string& id) {
    try {
        // First verify the shelter exists
        auto& shelter_service = ShelterService::getInstance();
        auto shelter = shelter_service.findById(id);

        if (!shelter) {
            callback(ApiResponse::notFound("Shelter"));
            return;
        }

        int page, per_page;
        parsePaginationParams(req, page, per_page);

        auto& dog_service = DogService::getInstance();
        auto dogs = dog_service.findByShelterId(id);
        int total = dog_service.countByShelterId(id);

        // Convert dogs to JSON array
        Json::Value dogs_json(Json::arrayValue);
        for (const auto& dog : dogs) {
            dogs_json.append(dog.toJson());
        }

        callback(ApiResponse::success(dogs_json, total, page, per_page));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get dogs at shelter: " + std::string(e.what()),
            {{"endpoint", "/api/shelters/:id/dogs"}, {"method", "GET"}, {"shelter_id", id}}
        );
        callback(ApiResponse::serverError("Failed to retrieve dogs at shelter"));
    }
}

void SheltersController::getKillShelters(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        int page, per_page;
        parsePaginationParams(req, page, per_page);

        auto& shelter_service = ShelterService::getInstance();
        auto shelters = shelter_service.findKillShelters();

        // Convert shelters to JSON array
        Json::Value shelters_json(Json::arrayValue);
        for (const auto& shelter : shelters) {
            shelters_json.append(shelter.toJson());
        }

        int total = static_cast<int>(shelters.size());
        callback(ApiResponse::success(shelters_json, total, page, per_page));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get kill shelters: " + std::string(e.what()),
            {{"endpoint", "/api/shelters/kill-shelters"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve kill shelters"));
    }
}

// ============================================================================
// ADMIN WRITE ENDPOINTS
// ============================================================================

void SheltersController::createShelter(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        // Validate shelter data
        std::vector<std::pair<std::string, std::string>> errors;
        if (!validateShelterData(*json, errors)) {
            callback(ApiResponse::validationError(errors));
            return;
        }

        // Create shelter from JSON
        auto shelter = Shelter::fromJson(*json);

        auto& shelter_service = ShelterService::getInstance();
        auto created_shelter = shelter_service.save(shelter);

        callback(ApiResponse::created(created_shelter.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to create shelter: " + std::string(e.what()),
            {{"endpoint", "/api/shelters"}, {"method", "POST"}}
        );
        callback(ApiResponse::serverError("Failed to create shelter"));
    }
}

void SheltersController::updateShelter(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                        const std::string& id) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto& shelter_service = ShelterService::getInstance();

        // Check if shelter exists
        auto existing_shelter = shelter_service.findById(id);
        if (!existing_shelter) {
            callback(ApiResponse::notFound("Shelter"));
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        // Merge updates into existing shelter
        auto updated_shelter = Shelter::fromJson(*json);
        updated_shelter.id = id; // Preserve the ID

        auto result = shelter_service.update(updated_shelter);

        callback(ApiResponse::success(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to update shelter: " + std::string(e.what()),
            {{"endpoint", "/api/shelters/:id"}, {"method", "PUT"}, {"shelter_id", id}}
        );
        callback(ApiResponse::serverError("Failed to update shelter"));
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void SheltersController::parsePaginationParams(const drogon::HttpRequestPtr& req,
                                                int& page, int& per_page) {
    page = 1;
    per_page = 20;

    auto page_str = req->getParameter("page");
    if (!page_str.empty()) {
        try {
            page = std::max(1, std::stoi(page_str));
        } catch (...) {
            page = 1;
        }
    }

    auto per_page_str = req->getParameter("per_page");
    if (!per_page_str.empty()) {
        try {
            per_page = std::min(100, std::max(1, std::stoi(per_page_str)));
        } catch (...) {
            per_page = 20;
        }
    }
}

bool SheltersController::validateShelterData(const Json::Value& json,
                                              std::vector<std::pair<std::string, std::string>>& errors) {
    bool is_valid = true;

    // Required fields
    if (!json.isMember("name") || json["name"].asString().empty()) {
        errors.emplace_back("name", "Name is required");
        is_valid = false;
    }

    if (!json.isMember("state_code") || json["state_code"].asString().empty()) {
        errors.emplace_back("state_code", "State code is required");
        is_valid = false;
    } else {
        // Validate state code format (2 uppercase letters)
        std::string state_code = json["state_code"].asString();
        if (state_code.length() != 2) {
            errors.emplace_back("state_code", "State code must be 2 letters");
            is_valid = false;
        }
    }

    if (!json.isMember("city") || json["city"].asString().empty()) {
        errors.emplace_back("city", "City is required");
        is_valid = false;
    }

    // Validate shelter_type if provided
    if (json.isMember("shelter_type")) {
        std::string shelter_type = json["shelter_type"].asString();
        if (shelter_type != "municipal" && shelter_type != "rescue" && shelter_type != "private") {
            errors.emplace_back("shelter_type", "Shelter type must be one of: municipal, rescue, private");
            is_valid = false;
        }
    }

    // Validate latitude if provided
    if (json.isMember("latitude")) {
        double lat = json["latitude"].asDouble();
        if (lat < -90.0 || lat > 90.0) {
            errors.emplace_back("latitude", "Latitude must be between -90 and 90");
            is_valid = false;
        }
    }

    // Validate longitude if provided
    if (json.isMember("longitude")) {
        double lng = json["longitude"].asDouble();
        if (lng < -180.0 || lng > 180.0) {
            errors.emplace_back("longitude", "Longitude must be between -180 and 180");
            is_valid = false;
        }
    }

    return is_valid;
}

} // namespace wtl::core::controllers
