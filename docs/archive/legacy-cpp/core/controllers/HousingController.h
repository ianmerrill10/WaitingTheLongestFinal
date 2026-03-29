/**
 * @file HousingController.h
 * @brief HTTP controller for pet-friendly housing directory
 *
 * PURPOSE:
 * Handles HTTP endpoints for searching, viewing, and submitting
 * pet-friendly housing listings. Users can search by location,
 * property type, weight limit, and breed restrictions.
 *
 * USAGE:
 * Registered automatically by Drogon framework.
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ApiResponse (response formatting)
 * - SelfHealing (resilient execution)
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

namespace wtl::core::controllers {

class HousingController : public drogon::HttpController<HousingController> {
public:
    METHOD_LIST_BEGIN
    // Public read endpoints
    ADD_METHOD_TO(HousingController::searchHousing, "/api/v1/housing", drogon::Get);
    ADD_METHOD_TO(HousingController::getHousingDetail, "/api/v1/housing/{id}", drogon::Get);

    // User submissions
    ADD_METHOD_TO(HousingController::submitHousing, "/api/v1/housing/submit", drogon::Post);
    METHOD_LIST_END

    void searchHousing(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getHousingDetail(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          const std::string& id);

    void submitHousing(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace wtl::core::controllers
