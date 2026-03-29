/**
 * @file DogsController.cc
 * @brief Implementation of DogsController
 * @see DogsController.h for documentation
 */

#include "core/controllers/DogsController.h"

// Project includes
#include "core/utils/ApiResponse.h"
#include "core/services/DogService.h"
#include "core/services/SearchService.h"
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

void DogsController::getAllDogs(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        int page, per_page;
        parsePaginationParams(req, page, per_page);

        auto& dog_service = DogService::getInstance();

        int offset = (page - 1) * per_page;
        auto dogs = dog_service.findAll(per_page, offset);
        int total = dog_service.countAll();

        // Convert dogs to JSON array
        Json::Value dogs_json(Json::arrayValue);
        for (const auto& dog : dogs) {
            dogs_json.append(dog.toJson());
        }

        callback(ApiResponse::success(dogs_json, total, page, per_page));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get all dogs: " + std::string(e.what()),
            {{"endpoint", "/api/dogs"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve dogs"));
    }
}

void DogsController::getDogById(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                 const std::string& id) {
    try {
        auto& dog_service = DogService::getInstance();
        auto dog = dog_service.findById(id);

        if (!dog) {
            callback(ApiResponse::notFound("Dog"));
            return;
        }

        callback(ApiResponse::success(dog->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get dog by ID: " + std::string(e.what()),
            {{"endpoint", "/api/dogs/:id"}, {"method", "GET"}, {"dog_id", id}}
        );
        callback(ApiResponse::serverError("Failed to retrieve dog"));
    }
}

void DogsController::searchDogs(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        // Build search filters from query parameters
        SearchFilters filters;

        auto state_code = req->getParameter("state_code");
        if (!state_code.empty()) {
            filters.state_code = state_code;
        }

        auto shelter_id = req->getParameter("shelter_id");
        if (!shelter_id.empty()) {
            filters.shelter_id = shelter_id;
        }

        auto breed = req->getParameter("breed");
        if (!breed.empty()) {
            filters.breed = breed;
        }

        auto size = req->getParameter("size");
        if (!size.empty()) {
            filters.size = size;
        }

        auto age_category = req->getParameter("age_category");
        if (!age_category.empty()) {
            filters.age_category = age_category;
        }

        auto gender = req->getParameter("gender");
        if (!gender.empty()) {
            filters.gender = gender;
        }

        // Boolean filters
        auto good_with_kids = req->getParameter("good_with_kids");
        if (!good_with_kids.empty()) {
            filters.good_with_kids = (good_with_kids == "true" || good_with_kids == "1");
        }

        auto good_with_dogs = req->getParameter("good_with_dogs");
        if (!good_with_dogs.empty()) {
            filters.good_with_dogs = (good_with_dogs == "true" || good_with_dogs == "1");
        }

        auto good_with_cats = req->getParameter("good_with_cats");
        if (!good_with_cats.empty()) {
            filters.good_with_cats = (good_with_cats == "true" || good_with_cats == "1");
        }

        auto house_trained = req->getParameter("house_trained");
        if (!house_trained.empty()) {
            filters.house_trained = (house_trained == "true" || house_trained == "1");
        }

        auto urgency_level = req->getParameter("urgency_level");
        if (!urgency_level.empty()) {
            filters.urgency_level = urgency_level;
        }

        auto query = req->getParameter("query");
        if (!query.empty()) {
            filters.query = query;
        }

        // Build search options
        SearchOptions options;

        auto page_str = req->getParameter("page");
        if (!page_str.empty()) {
            options.page = std::max(1, std::stoi(page_str));
        }

        auto per_page_str = req->getParameter("per_page");
        if (!per_page_str.empty()) {
            options.per_page = std::min(100, std::max(1, std::stoi(per_page_str)));
        }

        auto sort_by = req->getParameter("sort_by");
        if (!sort_by.empty()) {
            options.sort_by = sort_by;
        }

        auto sort_order = req->getParameter("sort_order");
        if (!sort_order.empty()) {
            options.sort_order = sort_order;
        }

        // Execute search
        auto& search_service = SearchService::getInstance();
        auto result = search_service.searchDogs(filters, options);

        // Convert to JSON array
        Json::Value dogs_json(Json::arrayValue);
        for (const auto& dog : result.items) {
            dogs_json.append(dog.toJson());
        }

        callback(ApiResponse::success(dogs_json, result.total, result.page, result.per_page));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to search dogs: " + std::string(e.what()),
            {{"endpoint", "/api/dogs/search"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to search dogs"));
    }
}

void DogsController::getUrgentDogs(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        int limit = 10;
        auto limit_str = req->getParameter("limit");
        if (!limit_str.empty()) {
            limit = std::min(100, std::max(1, std::stoi(limit_str)));
        }

        auto& search_service = SearchService::getInstance();
        auto dogs = search_service.getMostUrgent(limit);

        Json::Value dogs_json(Json::arrayValue);
        for (const auto& dog : dogs) {
            dogs_json.append(dog.toJson());
        }

        callback(ApiResponse::success(dogs_json));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get urgent dogs: " + std::string(e.what()),
            {{"endpoint", "/api/dogs/urgent"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve urgent dogs"));
    }
}

void DogsController::getLongestWaiting(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        int limit = 10;
        auto limit_str = req->getParameter("limit");
        if (!limit_str.empty()) {
            limit = std::min(100, std::max(1, std::stoi(limit_str)));
        }

        auto& search_service = SearchService::getInstance();
        auto dogs = search_service.getLongestWaiting(limit);

        Json::Value dogs_json(Json::arrayValue);
        for (const auto& dog : dogs) {
            dogs_json.append(dog.toJson());
        }

        callback(ApiResponse::success(dogs_json));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get longest waiting dogs: " + std::string(e.what()),
            {{"endpoint", "/api/dogs/longest-waiting"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve longest waiting dogs"));
    }
}

// ============================================================================
// ADMIN WRITE ENDPOINTS
// ============================================================================

void DogsController::createDog(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        // Validate dog data
        std::vector<std::pair<std::string, std::string>> errors;
        if (!validateDogData(*json, errors)) {
            callback(ApiResponse::validationError(errors));
            return;
        }

        // Create dog from JSON
        auto dog = Dog::fromJson(*json);

        auto& dog_service = DogService::getInstance();
        auto created_dog = dog_service.save(dog);

        callback(ApiResponse::created(created_dog.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to create dog: " + std::string(e.what()),
            {{"endpoint", "/api/dogs"}, {"method", "POST"}}
        );
        callback(ApiResponse::serverError("Failed to create dog"));
    }
}

void DogsController::updateDog(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                const std::string& id) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto& dog_service = DogService::getInstance();

        // Check if dog exists
        auto existing_dog = dog_service.findById(id);
        if (!existing_dog) {
            callback(ApiResponse::notFound("Dog"));
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        // Merge updates into existing dog
        // Note: fromJson should handle partial updates when merged with existing
        auto updated_dog = Dog::fromJson(*json);
        updated_dog.id = id; // Preserve the ID

        auto result = dog_service.update(updated_dog);

        callback(ApiResponse::success(result.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to update dog: " + std::string(e.what()),
            {{"endpoint", "/api/dogs/:id"}, {"method", "PUT"}, {"dog_id", id}}
        );
        callback(ApiResponse::serverError("Failed to update dog"));
    }
}

void DogsController::deleteDog(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                const std::string& id) {
    // Require admin role
    REQUIRE_ROLE(req, callback, "admin");

    try {
        auto& dog_service = DogService::getInstance();

        // Check if dog exists
        auto existing_dog = dog_service.findById(id);
        if (!existing_dog) {
            callback(ApiResponse::notFound("Dog"));
            return;
        }

        bool deleted = dog_service.deleteById(id);
        if (!deleted) {
            callback(ApiResponse::serverError("Failed to delete dog"));
            return;
        }

        callback(ApiResponse::noContent());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to delete dog: " + std::string(e.what()),
            {{"endpoint", "/api/dogs/:id"}, {"method", "DELETE"}, {"dog_id", id}}
        );
        callback(ApiResponse::serverError("Failed to delete dog"));
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void DogsController::parsePaginationParams(const drogon::HttpRequestPtr& req,
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

bool DogsController::validateDogData(const Json::Value& json,
                                      std::vector<std::pair<std::string, std::string>>& errors) {
    bool is_valid = true;

    // Required fields
    if (!json.isMember("name") || json["name"].asString().empty()) {
        errors.emplace_back("name", "Name is required");
        is_valid = false;
    }

    if (!json.isMember("shelter_id") || json["shelter_id"].asString().empty()) {
        errors.emplace_back("shelter_id", "Shelter ID is required");
        is_valid = false;
    }

    // Validate size if provided
    if (json.isMember("size")) {
        std::string size = json["size"].asString();
        if (size != "small" && size != "medium" && size != "large" && size != "xlarge") {
            errors.emplace_back("size", "Size must be one of: small, medium, large, xlarge");
            is_valid = false;
        }
    }

    // Validate age_category if provided
    if (json.isMember("age_category")) {
        std::string age_category = json["age_category"].asString();
        if (age_category != "puppy" && age_category != "young" &&
            age_category != "adult" && age_category != "senior") {
            errors.emplace_back("age_category", "Age category must be one of: puppy, young, adult, senior");
            is_valid = false;
        }
    }

    // Validate gender if provided
    if (json.isMember("gender")) {
        std::string gender = json["gender"].asString();
        if (gender != "male" && gender != "female") {
            errors.emplace_back("gender", "Gender must be one of: male, female");
            is_valid = false;
        }
    }

    // Validate status if provided
    if (json.isMember("status")) {
        std::string status = json["status"].asString();
        if (status != "adoptable" && status != "pending" && status != "adopted" &&
            status != "hold" && status != "medical_hold") {
            errors.emplace_back("status", "Invalid status value");
            is_valid = false;
        }
    }

    return is_valid;
}

} // namespace wtl::core::controllers
