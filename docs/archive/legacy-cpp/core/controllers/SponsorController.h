/**
 * @file SponsorController.h
 * @brief HTTP controller for the Sponsor-a-Fee program
 *
 * PURPOSE:
 * Handles HTTP endpoints for sponsor creation, fee sponsorship,
 * corporate sponsors, and sponsorship statistics.
 *
 * USAGE:
 * Registered automatically by Drogon framework.
 *
 * DEPENDENCIES:
 * - SponsorshipService (business logic)
 * - ApiResponse (response formatting)
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

namespace wtl::core::controllers {

class SponsorController : public drogon::HttpController<SponsorController> {
public:
    METHOD_LIST_BEGIN
    // Sponsor management
    ADD_METHOD_TO(SponsorController::createSponsor, "/api/v1/sponsors", drogon::Post);
    ADD_METHOD_TO(SponsorController::getSponsor, "/api/v1/sponsors/{id}", drogon::Get);
    ADD_METHOD_TO(SponsorController::getSponsorSponsorships, "/api/v1/sponsors/{id}/sponsorships", drogon::Get);

    // Sponsorship transactions
    ADD_METHOD_TO(SponsorController::sponsorDog, "/api/v1/sponsors/dog", drogon::Post);
    ADD_METHOD_TO(SponsorController::getSponsorshipsForDog, "/api/v1/sponsorships/dog/{dogId}", drogon::Get);
    ADD_METHOD_TO(SponsorController::updatePaymentStatus, "/api/v1/sponsorships/{id}/payment", drogon::Put);

    // Corporate sponsors
    ADD_METHOD_TO(SponsorController::getCorporateSponsors, "/api/v1/sponsors/corporate", drogon::Get);

    // Statistics
    ADD_METHOD_TO(SponsorController::getGlobalStats, "/api/v1/sponsors/stats", drogon::Get);
    METHOD_LIST_END

    // ====================================================================
    // SPONSOR MANAGEMENT
    // ====================================================================

    void createSponsor(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getSponsor(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& id);

    void getSponsorSponsorships(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                 const std::string& id);

    // ====================================================================
    // SPONSORSHIP TRANSACTIONS
    // ====================================================================

    void sponsorDog(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getSponsorshipsForDog(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                const std::string& dogId);

    void updatePaymentStatus(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                              const std::string& id);

    // ====================================================================
    // CORPORATE SPONSORS
    // ====================================================================

    void getCorporateSponsors(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // ====================================================================
    // STATISTICS
    // ====================================================================

    void getGlobalStats(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace wtl::core::controllers
