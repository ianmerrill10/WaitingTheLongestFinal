/**
 * @file StatesController.cc
 * @brief Implementation of StatesController
 * @see StatesController.h for documentation
 */

#include "core/controllers/StatesController.h"

// Standard library includes
#include <algorithm>
#include <cctype>

// Project includes
#include "core/utils/ApiResponse.h"
#include "core/services/StateService.h"
#include "core/services/DogService.h"
#include "core/services/ShelterService.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::core::controllers {

using namespace ::wtl::core::utils;
using namespace ::wtl::core::services;

// ============================================================================
// PUBLIC READ ENDPOINTS
// ============================================================================

void StatesController::getAllStates(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        auto& state_service = StateService::getInstance();
        auto states = state_service.findAll();

        // Convert states to JSON array
        Json::Value states_json(Json::arrayValue);
        for (const auto& state : states) {
            states_json.append(state.toJson());
        }

        callback(ApiResponse::success(states_json));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get all states: " + std::string(e.what()),
            {{"endpoint", "/api/states"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve states"));
    }
}

void StatesController::getActiveStates(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        auto& state_service = StateService::getInstance();
        auto states = state_service.findActive();

        // Convert states to JSON array
        Json::Value states_json(Json::arrayValue);
        for (const auto& state : states) {
            states_json.append(state.toJson());
        }

        callback(ApiResponse::success(states_json));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get active states: " + std::string(e.what()),
            {{"endpoint", "/api/states/active"}, {"method", "GET"}}
        );
        callback(ApiResponse::serverError("Failed to retrieve active states"));
    }
}

void StatesController::getStateByCode(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                       const std::string& code) {
    try {
        std::string normalized_code = normalizeStateCode(code);

        auto& state_service = StateService::getInstance();
        auto state = state_service.findByCode(normalized_code);

        if (!state) {
            callback(ApiResponse::notFound("State"));
            return;
        }

        callback(ApiResponse::success(state->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get state by code: " + std::string(e.what()),
            {{"endpoint", "/api/states/:code"}, {"method", "GET"}, {"state_code", code}}
        );
        callback(ApiResponse::serverError("Failed to retrieve state"));
    }
}

void StatesController::getDogsInState(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                       const std::string& code) {
    try {
        std::string normalized_code = normalizeStateCode(code);

        // Verify state exists
        auto& state_service = StateService::getInstance();
        auto state = state_service.findByCode(normalized_code);

        if (!state) {
            callback(ApiResponse::notFound("State"));
            return;
        }

        int page, per_page;
        parsePaginationParams(req, page, per_page);

        auto& dog_service = DogService::getInstance();
        auto dogs = dog_service.findByStateCode(normalized_code);
        int total = dog_service.countByStateCode(normalized_code);

        // Convert dogs to JSON array
        Json::Value dogs_json(Json::arrayValue);
        for (const auto& dog : dogs) {
            dogs_json.append(dog.toJson());
        }

        callback(ApiResponse::success(dogs_json, total, page, per_page));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get dogs in state: " + std::string(e.what()),
            {{"endpoint", "/api/states/:code/dogs"}, {"method", "GET"}, {"state_code", code}}
        );
        callback(ApiResponse::serverError("Failed to retrieve dogs in state"));
    }
}

void StatesController::getSheltersInState(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                           const std::string& code) {
    try {
        std::string normalized_code = normalizeStateCode(code);

        // Verify state exists
        auto& state_service = StateService::getInstance();
        auto state = state_service.findByCode(normalized_code);

        if (!state) {
            callback(ApiResponse::notFound("State"));
            return;
        }

        int page, per_page;
        parsePaginationParams(req, page, per_page);

        auto& shelter_service = ShelterService::getInstance();
        auto shelters = shelter_service.findByStateCode(normalized_code);
        int total = shelter_service.countByStateCode(normalized_code);

        // Convert shelters to JSON array
        Json::Value shelters_json(Json::arrayValue);
        for (const auto& shelter : shelters) {
            shelters_json.append(shelter.toJson());
        }

        callback(ApiResponse::success(shelters_json, total, page, per_page));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get shelters in state: " + std::string(e.what()),
            {{"endpoint", "/api/states/:code/shelters"}, {"method", "GET"}, {"state_code", code}}
        );
        callback(ApiResponse::serverError("Failed to retrieve shelters in state"));
    }
}

void StatesController::getStateStatistics(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                           const std::string& code) {
    try {
        std::string normalized_code = normalizeStateCode(code);

        // Verify state exists
        auto& state_service = StateService::getInstance();
        auto state = state_service.findByCode(normalized_code);

        if (!state) {
            callback(ApiResponse::notFound("State"));
            return;
        }

        auto& dog_service = DogService::getInstance();
        auto& shelter_service = ShelterService::getInstance();

        // Gather statistics
        Json::Value stats;

        // Basic counts
        stats["state_code"] = normalized_code;
        stats["state_name"] = state->name;
        stats["total_dogs"] = dog_service.countByStateCode(normalized_code);
        stats["total_shelters"] = shelter_service.countByStateCode(normalized_code);

        // Get dogs for detailed stats
        auto dogs = dog_service.findByStateCode(normalized_code);

        // Count by urgency level
        Json::Value urgency_counts;
        int normal_count = 0, medium_count = 0, high_count = 0, critical_count = 0;

        for (const auto& dog : dogs) {
            if (dog.urgency_level == "critical") critical_count++;
            else if (dog.urgency_level == "high") high_count++;
            else if (dog.urgency_level == "medium") medium_count++;
            else normal_count++;
        }

        urgency_counts["normal"] = normal_count;
        urgency_counts["medium"] = medium_count;
        urgency_counts["high"] = high_count;
        urgency_counts["critical"] = critical_count;
        stats["by_urgency"] = urgency_counts;

        // Count by size
        Json::Value size_counts;
        int small_count = 0, medium_size_count = 0, large_count = 0, xlarge_count = 0;

        for (const auto& dog : dogs) {
            if (dog.size == "small") small_count++;
            else if (dog.size == "medium") medium_size_count++;
            else if (dog.size == "large") large_count++;
            else if (dog.size == "xlarge") xlarge_count++;
        }

        size_counts["small"] = small_count;
        size_counts["medium"] = medium_size_count;
        size_counts["large"] = large_count;
        size_counts["xlarge"] = xlarge_count;
        stats["by_size"] = size_counts;

        // Count by age category
        Json::Value age_counts;
        int puppy_count = 0, young_count = 0, adult_count = 0, senior_count = 0;

        for (const auto& dog : dogs) {
            if (dog.age_category == "puppy") puppy_count++;
            else if (dog.age_category == "young") young_count++;
            else if (dog.age_category == "adult") adult_count++;
            else if (dog.age_category == "senior") senior_count++;
        }

        age_counts["puppy"] = puppy_count;
        age_counts["young"] = young_count;
        age_counts["adult"] = adult_count;
        age_counts["senior"] = senior_count;
        stats["by_age_category"] = age_counts;

        // Calculate average wait time (in days)
        if (!dogs.empty()) {
            double total_wait_days = 0;
            auto now = std::chrono::system_clock::now();

            for (const auto& dog : dogs) {
                auto duration = now - dog.intake_date;
                auto days = std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24.0;
                total_wait_days += days;
            }

            stats["average_wait_days"] = total_wait_days / dogs.size();
        } else {
            stats["average_wait_days"] = 0;
        }

        callback(ApiResponse::success(stats));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get state statistics: " + std::string(e.what()),
            {{"endpoint", "/api/states/:code/stats"}, {"method", "GET"}, {"state_code", code}}
        );
        callback(ApiResponse::serverError("Failed to retrieve state statistics"));
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void StatesController::parsePaginationParams(const drogon::HttpRequestPtr& req,
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

std::string StatesController::normalizeStateCode(const std::string& code) {
    std::string normalized = code;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return normalized;
}

} // namespace wtl::core::controllers
