/**
 * @file SponsorController.cc
 * @brief Implementation of Sponsor-a-Fee HTTP endpoints
 * @see SponsorController.h
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/controllers/SponsorController.h"
#include "core/services/SponsorshipService.h"
#include "core/utils/ApiResponse.h"
#include "core/debug/ErrorCapture.h"

using namespace ::wtl::core::services;
using namespace ::wtl::core::utils;
using namespace ::wtl::core::debug;

namespace wtl::core::controllers {

// ============================================================================
// SPONSOR MANAGEMENT
// ============================================================================

void SponsorController::createSponsor(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string name = (*json)["name"].asString();
        std::string email = (*json)["email"].asString();
        bool isAnonymous = (*json).get("is_anonymous", false).asBool();
        std::string userId = (*json).get("user_id", "").asString();

        if (name.empty() || email.empty()) {
            callback(ApiResponse::badRequest("Name and email are required"));
            return;
        }

        auto& service = SponsorshipService::getInstance();
        auto sponsor = service.createSponsor(name, email, isAnonymous, userId);

        if (sponsor) {
            callback(ApiResponse::success(sponsor->toJson()));
        } else {
            callback(ApiResponse::serverError("Failed to create sponsor"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error creating sponsor: " + std::string(e.what()),
            {{"endpoint", "POST /api/v1/sponsors"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

void SponsorController::getSponsor(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id)
{
    try {
        auto& service = SponsorshipService::getInstance();
        auto sponsor = service.findSponsorById(id);

        if (sponsor) {
            callback(ApiResponse::success(sponsor->toJson()));
        } else {
            callback(ApiResponse::notFound("Sponsor"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error getting sponsor: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/sponsors/{id}"}, {"id", id}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

void SponsorController::getSponsorSponsorships(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id)
{
    try {
        int limit = 20;
        int offset = 0;

        auto limitParam = req->getParameter("limit");
        auto offsetParam = req->getParameter("offset");
        if (!limitParam.empty()) limit = std::stoi(limitParam);
        if (!offsetParam.empty()) offset = std::stoi(offsetParam);

        auto& service = SponsorshipService::getInstance();
        auto sponsorships = service.findSponsorshipsBySponsor(id, limit, offset);

        Json::Value data(Json::arrayValue);
        for (const auto& s : sponsorships) {
            data.append(s.toJson());
        }

        callback(ApiResponse::success(data, static_cast<int>(sponsorships.size()), 1, limit));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error getting sponsor sponsorships: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/sponsors/{id}/sponsorships"}, {"id", id}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// SPONSORSHIP TRANSACTIONS
// ============================================================================

void SponsorController::sponsorDog(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string sponsorId = (*json)["sponsor_id"].asString();
        std::string dogId = (*json)["dog_id"].asString();
        double amount = (*json)["amount"].asDouble();
        std::string message = (*json).get("message", "").asString();
        std::string paymentId = (*json).get("payment_id", "").asString();

        if (sponsorId.empty() || dogId.empty() || amount <= 0) {
            callback(ApiResponse::badRequest("sponsor_id, dog_id, and positive amount are required"));
            return;
        }

        auto& service = SponsorshipService::getInstance();
        auto sponsorship = service.sponsorDog(sponsorId, dogId, amount, message, paymentId);

        if (sponsorship) {
            callback(ApiResponse::success(sponsorship->toJson()));
        } else {
            callback(ApiResponse::serverError("Failed to create sponsorship"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error sponsoring dog: " + std::string(e.what()),
            {{"endpoint", "POST /api/v1/sponsors/dog"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

void SponsorController::getSponsorshipsForDog(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& dogId)
{
    try {
        auto& service = SponsorshipService::getInstance();
        auto sponsorships = service.findSponsorshipsByDog(dogId);

        Json::Value data(Json::arrayValue);
        for (const auto& s : sponsorships) {
            data.append(s.toJson());
        }

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error getting sponsorships for dog: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/sponsorships/dog/{dogId}"}, {"dog_id", dogId}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

void SponsorController::updatePaymentStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string status = (*json)["status"].asString();
        std::string paymentId = (*json).get("payment_id", "").asString();

        if (status.empty()) {
            callback(ApiResponse::badRequest("Payment status is required"));
            return;
        }

        auto& service = SponsorshipService::getInstance();
        bool updated = service.updatePaymentStatus(id, status, paymentId);

        if (updated) {
            Json::Value data;
            data["updated"] = true;
            data["sponsorship_id"] = id;
            data["payment_status"] = status;
            callback(ApiResponse::success(data));
        } else {
            callback(ApiResponse::notFound("Sponsorship"));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error updating payment status: " + std::string(e.what()),
            {{"endpoint", "PUT /api/v1/sponsorships/{id}/payment"}, {"id", id}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// CORPORATE SPONSORS
// ============================================================================

void SponsorController::getCorporateSponsors(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto& service = SponsorshipService::getInstance();
        auto sponsors = service.getActiveCorporateSponsors();

        Json::Value data(Json::arrayValue);
        for (const auto& cs : sponsors) {
            data.append(cs.toJson());
        }

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error getting corporate sponsors: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/sponsors/corporate"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

void SponsorController::getGlobalStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto& service = SponsorshipService::getInstance();
        auto stats = service.getGlobalSponsorshipStats();
        callback(ApiResponse::success(stats));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error getting global stats: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/sponsors/stats"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

} // namespace wtl::core::controllers
