/**
 * @file ResourcesController.h
 * @brief HTTP controller for adoption resources, check-ins, and success stories
 *
 * PURPOSE:
 * Handles HTTP endpoints for the adopter resource library, post-adoption
 * check-in surveys, and success story submission/viewing.
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

class ResourcesController : public drogon::HttpController<ResourcesController> {
public:
    METHOD_LIST_BEGIN
    // Resource library
    ADD_METHOD_TO(ResourcesController::listResources, "/api/v1/resources", drogon::Get);
    ADD_METHOD_TO(ResourcesController::getResource, "/api/v1/resources/{slug}", drogon::Get);

    // Adoption tracking
    ADD_METHOD_TO(ResourcesController::getMyAdoptions, "/api/v1/adoptions/mine", drogon::Get);
    ADD_METHOD_TO(ResourcesController::submitCheckin, "/api/v1/checkins/{adoptionId}", drogon::Post);

    // Success stories
    ADD_METHOD_TO(ResourcesController::listSuccessStories, "/api/v1/success-stories", drogon::Get);
    ADD_METHOD_TO(ResourcesController::submitSuccessStory, "/api/v1/success-stories", drogon::Post);
    METHOD_LIST_END

    // ====================================================================
    // RESOURCE LIBRARY
    // ====================================================================

    void listResources(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getResource(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& slug);

    // ====================================================================
    // ADOPTION TRACKING
    // ====================================================================

    void getMyAdoptions(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void submitCheckin(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       const std::string& adoptionId);

    // ====================================================================
    // SUCCESS STORIES
    // ====================================================================

    void listSuccessStories(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void submitSuccessStory(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace wtl::core::controllers
