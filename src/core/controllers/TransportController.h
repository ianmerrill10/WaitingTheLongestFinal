/**
 * @file TransportController.h
 * @brief HTTP controller for the transport network
 *
 * PURPOSE:
 * Handles HTTP endpoints for transport request creation, volunteer
 * registration, request claiming, and transport tracking.
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

class TransportController : public drogon::HttpController<TransportController> {
public:
    METHOD_LIST_BEGIN
    // Transport requests
    ADD_METHOD_TO(TransportController::createRequest, "/api/v1/transport/request", drogon::Post);
    ADD_METHOD_TO(TransportController::getRequest, "/api/v1/transport/request/{id}", drogon::Get);
    ADD_METHOD_TO(TransportController::listOpenRequests, "/api/v1/transport/requests", drogon::Get);
    ADD_METHOD_TO(TransportController::claimRequest, "/api/v1/transport/request/{id}/claim", drogon::Put);

    // Volunteer management
    ADD_METHOD_TO(TransportController::registerVolunteer, "/api/v1/transport/volunteer", drogon::Post);
    METHOD_LIST_END

    // ====================================================================
    // TRANSPORT REQUESTS
    // ====================================================================

    void createRequest(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getRequest(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& id);

    void listOpenRequests(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void claimRequest(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      const std::string& id);

    // ====================================================================
    // VOLUNTEER MANAGEMENT
    // ====================================================================

    void registerVolunteer(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace wtl::core::controllers
