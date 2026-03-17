/**
 * @file TransportController.cc
 * @brief Implementation of transport network HTTP endpoints
 * @see TransportController.h
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/controllers/TransportController.h"
#include "core/db/ConnectionPool.h"
#include "core/utils/ApiResponse.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

#include <pqxx/pqxx>

using namespace ::wtl::core::utils;
using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

namespace wtl::core::controllers {

// ============================================================================
// CREATE TRANSPORT REQUEST
// ============================================================================

void TransportController::createRequest(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string dogId = (*json)["dog_id"].asString();
        std::string requesterId = (*json)["requester_id"].asString();
        std::string pickupShelterId = (*json).get("pickup_shelter_id", "").asString();
        std::string pickupAddress = (*json).get("pickup_address", "").asString();
        std::string pickupCity = (*json)["pickup_city"].asString();
        std::string pickupStateCode = (*json)["pickup_state_code"].asString();
        std::string pickupZip = (*json).get("pickup_zip", "").asString();
        std::string destAddress = (*json).get("destination_address", "").asString();
        std::string destCity = (*json)["destination_city"].asString();
        std::string destStateCode = (*json)["destination_state_code"].asString();
        std::string destZip = (*json).get("destination_zip", "").asString();
        int distanceMiles = (*json).get("distance_miles", 0).asInt();
        std::string preferredDate = (*json).get("preferred_date", "").asString();
        bool flexibleDates = (*json).get("flexible_dates", true).asBool();

        if (dogId.empty() || requesterId.empty() || pickupCity.empty() || destCity.empty()) {
            callback(ApiResponse::badRequest("dog_id, requester_id, pickup_city, and destination_city are required"));
            return;
        }

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "create_transport_request",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    auto dbResult = txn.exec_params(
                        "INSERT INTO transport_requests "
                        "(dog_id, requester_id, pickup_shelter_id, pickup_address, "
                        "pickup_city, pickup_state_code, pickup_zip, "
                        "destination_address, destination_city, destination_state_code, destination_zip, "
                        "distance_miles, preferred_date, flexible_dates) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14) "
                        "RETURNING *",
                        dogId, requesterId,
                        pickupShelterId.empty() ? nullptr : pickupShelterId.c_str(),
                        pickupAddress.empty() ? nullptr : pickupAddress.c_str(),
                        pickupCity, pickupStateCode,
                        pickupZip.empty() ? nullptr : pickupZip.c_str(),
                        destAddress.empty() ? nullptr : destAddress.c_str(),
                        destCity, destStateCode,
                        destZip.empty() ? nullptr : destZip.c_str(),
                        distanceMiles,
                        preferredDate.empty() ? nullptr : preferredDate.c_str(),
                        flexibleDates
                    );

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    if (dbResult.empty()) return Json::Value(Json::nullValue);

                    Json::Value request;
                    request["id"] = dbResult[0]["id"].as<std::string>();
                    request["dog_id"] = dbResult[0]["dog_id"].as<std::string>();
                    request["status"] = dbResult[0]["status"].as<std::string>();
                    request["pickup_city"] = dbResult[0]["pickup_city"].as<std::string>();
                    request["destination_city"] = dbResult[0]["destination_city"].as<std::string>();
                    request["distance_miles"] = dbResult[0]["distance_miles"].is_null() ? 0 : dbResult[0]["distance_miles"].as<int>();
                    request["created_at"] = dbResult[0]["created_at"].as<std::string>();
                    return request;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        if (result.isNull()) {
            callback(ApiResponse::serverError("Failed to create transport request"));
        } else {
            callback(ApiResponse::success(result));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error creating transport request: " + std::string(e.what()),
            {{"endpoint", "POST /api/v1/transport/request"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// GET TRANSPORT REQUEST
// ============================================================================

void TransportController::getRequest(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id)
{
    try {
        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "get_transport_request",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    // Get request with dog and shelter info
                    auto dbResult = txn.exec_params(
                        "SELECT tr.*, d.name as dog_name, "
                        "ps.name as pickup_state_name, ds.name as dest_state_name "
                        "FROM transport_requests tr "
                        "LEFT JOIN dogs d ON tr.dog_id = d.id "
                        "LEFT JOIN states ps ON tr.pickup_state_code = ps.code "
                        "LEFT JOIN states ds ON tr.destination_state_code = ds.code "
                        "WHERE tr.id = $1", id
                    );

                    // Get transport legs
                    auto legsResult = txn.exec_params(
                        "SELECT tl.*, tv.name as volunteer_name "
                        "FROM transport_legs tl "
                        "LEFT JOIN transport_volunteers tv ON tl.volunteer_id = tv.id "
                        "WHERE tl.request_id = $1 ORDER BY tl.leg_order",
                        id
                    );

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    if (dbResult.empty()) return Json::Value(Json::nullValue);

                    const auto& row = dbResult[0];
                    Json::Value request;
                    request["id"] = row["id"].as<std::string>();
                    request["dog_id"] = row["dog_id"].as<std::string>();
                    request["dog_name"] = row["dog_name"].is_null() ? "" : row["dog_name"].as<std::string>();
                    request["requester_id"] = row["requester_id"].as<std::string>();
                    request["pickup_address"] = row["pickup_address"].is_null() ? "" : row["pickup_address"].as<std::string>();
                    request["pickup_city"] = row["pickup_city"].as<std::string>();
                    request["pickup_state"] = row["pickup_state_name"].is_null() ? "" : row["pickup_state_name"].as<std::string>();
                    request["destination_address"] = row["destination_address"].is_null() ? "" : row["destination_address"].as<std::string>();
                    request["destination_city"] = row["destination_city"].as<std::string>();
                    request["destination_state"] = row["dest_state_name"].is_null() ? "" : row["dest_state_name"].as<std::string>();
                    request["distance_miles"] = row["distance_miles"].is_null() ? 0 : row["distance_miles"].as<int>();
                    request["preferred_date"] = row["preferred_date"].is_null() ? "" : row["preferred_date"].as<std::string>();
                    request["flexible_dates"] = row["flexible_dates"].is_null() ? true : row["flexible_dates"].as<bool>();
                    request["status"] = row["status"].as<std::string>();
                    request["notes"] = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
                    request["created_at"] = row["created_at"].as<std::string>();

                    // Add legs
                    Json::Value legs(Json::arrayValue);
                    for (const auto& leg : legsResult) {
                        Json::Value legJson;
                        legJson["id"] = leg["id"].as<std::string>();
                        legJson["leg_order"] = leg["leg_order"].as<int>();
                        legJson["volunteer_name"] = leg["volunteer_name"].is_null() ? "" : leg["volunteer_name"].as<std::string>();
                        legJson["pickup_location"] = leg["pickup_location"].is_null() ? "" : leg["pickup_location"].as<std::string>();
                        legJson["dropoff_location"] = leg["dropoff_location"].is_null() ? "" : leg["dropoff_location"].as<std::string>();
                        legJson["distance_miles"] = leg["distance_miles"].is_null() ? 0 : leg["distance_miles"].as<int>();
                        legJson["status"] = leg["status"].as<std::string>();
                        legs.append(legJson);
                    }
                    request["legs"] = legs;

                    return request;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        if (result.isNull()) {
            callback(ApiResponse::notFound("Transport request"));
        } else {
            callback(ApiResponse::success(result));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error getting transport request: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/transport/request/{id}"}, {"id", id}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// LIST OPEN REQUESTS
// ============================================================================

void TransportController::listOpenRequests(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto stateParam = req->getParameter("state_code");
        auto pageStr = req->getParameter("page");
        auto limitStr = req->getParameter("limit");

        int page = pageStr.empty() ? 1 : std::stoi(pageStr);
        int limit = limitStr.empty() ? 20 : std::stoi(limitStr);
        if (limit > 100) limit = 100;
        int offset = (page - 1) * limit;

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "list_open_transport_requests",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    pqxx::result dbResult;
                    pqxx::result countResult;

                    if (stateParam.empty()) {
                        dbResult = txn.exec_params(
                            "SELECT tr.*, d.name as dog_name, d.breed_primary as dog_breed, "
                            "ps.name as pickup_state, ds.name as dest_state "
                            "FROM transport_requests tr "
                            "LEFT JOIN dogs d ON tr.dog_id = d.id "
                            "LEFT JOIN states ps ON tr.pickup_state_code = ps.code "
                            "LEFT JOIN states ds ON tr.destination_state_code = ds.code "
                            "WHERE tr.status = 'pending' "
                            "ORDER BY tr.preferred_date ASC NULLS LAST, tr.created_at ASC "
                            "LIMIT $1 OFFSET $2",
                            limit, offset
                        );
                        countResult = txn.exec(
                            "SELECT COUNT(*) FROM transport_requests WHERE status = 'pending'"
                        );
                    } else {
                        std::string stateCode = stateParam;
                        dbResult = txn.exec_params(
                            "SELECT tr.*, d.name as dog_name, d.breed_primary as dog_breed, "
                            "ps.name as pickup_state, ds.name as dest_state "
                            "FROM transport_requests tr "
                            "LEFT JOIN dogs d ON tr.dog_id = d.id "
                            "LEFT JOIN states ps ON tr.pickup_state_code = ps.code "
                            "LEFT JOIN states ds ON tr.destination_state_code = ds.code "
                            "WHERE tr.status = 'pending' "
                            "AND (tr.pickup_state_code = $1 OR tr.destination_state_code = $1) "
                            "ORDER BY tr.preferred_date ASC NULLS LAST, tr.created_at ASC "
                            "LIMIT $2 OFFSET $3",
                            stateCode, limit, offset
                        );
                        countResult = txn.exec_params(
                            "SELECT COUNT(*) FROM transport_requests "
                            "WHERE status = 'pending' "
                            "AND (pickup_state_code = $1 OR destination_state_code = $1)",
                            stateCode
                        );
                    }

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    int total = countResult.empty() ? 0 : countResult[0][0].as<int>();

                    Json::Value response;
                    Json::Value requests(Json::arrayValue);

                    for (const auto& row : dbResult) {
                        Json::Value r;
                        r["id"] = row["id"].as<std::string>();
                        r["dog_name"] = row["dog_name"].is_null() ? "" : row["dog_name"].as<std::string>();
                        r["dog_breed"] = row["dog_breed"].is_null() ? "" : row["dog_breed"].as<std::string>();
                        r["pickup_city"] = row["pickup_city"].as<std::string>();
                        r["pickup_state"] = row["pickup_state"].is_null() ? "" : row["pickup_state"].as<std::string>();
                        r["destination_city"] = row["destination_city"].as<std::string>();
                        r["destination_state"] = row["dest_state"].is_null() ? "" : row["dest_state"].as<std::string>();
                        r["distance_miles"] = row["distance_miles"].is_null() ? 0 : row["distance_miles"].as<int>();
                        r["preferred_date"] = row["preferred_date"].is_null() ? "" : row["preferred_date"].as<std::string>();
                        r["flexible_dates"] = row["flexible_dates"].is_null() ? true : row["flexible_dates"].as<bool>();
                        r["created_at"] = row["created_at"].as<std::string>();
                        requests.append(r);
                    }

                    response["requests"] = requests;
                    response["total"] = total;
                    response["page"] = page;
                    response["limit"] = limit;
                    return response;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        callback(ApiResponse::success(result));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error listing transport requests: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/transport/requests"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// CLAIM REQUEST
// ============================================================================

void TransportController::claimRequest(
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

        std::string volunteerId = (*json)["volunteer_id"].asString();
        if (volunteerId.empty()) {
            callback(ApiResponse::badRequest("volunteer_id is required"));
            return;
        }

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "claim_transport_request",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    // Verify request is still pending
                    auto check = txn.exec_params(
                        "SELECT status FROM transport_requests WHERE id = $1", id
                    );

                    if (check.empty()) {
                        txn.abort();
                        ConnectionPool::getInstance().release(conn);
                        Json::Value err;
                        err["error"] = "not_found";
                        return err;
                    }

                    if (check[0]["status"].as<std::string>() != "pending") {
                        txn.abort();
                        ConnectionPool::getInstance().release(conn);
                        Json::Value err;
                        err["error"] = "already_claimed";
                        return err;
                    }

                    // Claim the request
                    auto dbResult = txn.exec_params(
                        "UPDATE transport_requests SET status = 'matched', "
                        "volunteer_id = $1 WHERE id = $2 AND status = 'pending' "
                        "RETURNING *",
                        volunteerId, id
                    );

                    // Update volunteer stats
                    txn.exec_params(
                        "UPDATE transport_volunteers SET transports_completed = transports_completed + 1 "
                        "WHERE id = $1",
                        volunteerId
                    );

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    if (dbResult.empty()) {
                        Json::Value err;
                        err["error"] = "claim_failed";
                        return err;
                    }

                    Json::Value result;
                    result["id"] = dbResult[0]["id"].as<std::string>();
                    result["status"] = dbResult[0]["status"].as<std::string>();
                    result["volunteer_id"] = volunteerId;
                    result["claimed"] = true;
                    return result;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        if (result.isMember("error")) {
            std::string error = result["error"].asString();
            if (error == "not_found") {
                callback(ApiResponse::notFound("Transport request"));
            } else if (error == "already_claimed") {
                callback(ApiResponse::badRequest("Transport request has already been claimed"));
            } else {
                callback(ApiResponse::serverError("Failed to claim transport request"));
            }
        } else {
            callback(ApiResponse::success(result));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error claiming transport request: " + std::string(e.what()),
            {{"endpoint", "PUT /api/v1/transport/request/{id}/claim"}, {"id", id}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// REGISTER VOLUNTEER
// ============================================================================

void TransportController::registerVolunteer(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string userId = (*json).get("user_id", "").asString();
        std::string name = (*json)["name"].asString();
        std::string email = (*json)["email"].asString();
        std::string phone = (*json).get("phone", "").asString();
        std::string homeStateCode = (*json).get("home_state_code", "").asString();
        std::string homeZip = (*json).get("home_zip", "").asString();
        int maxDistance = (*json).get("max_distance_miles", 100).asInt();
        bool crossState = (*json).get("willing_to_cross_state", true).asBool();
        std::string vehicleType = (*json).get("vehicle_type", "sedan").asString();
        bool canTransportLarge = (*json).get("can_transport_large", false).asBool();
        bool hasCrate = (*json).get("has_crate", false).asBool();
        bool weekdays = (*json).get("available_weekdays", false).asBool();
        bool weekends = (*json).get("available_weekends", true).asBool();

        if (name.empty() || email.empty()) {
            callback(ApiResponse::badRequest("Name and email are required"));
            return;
        }

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "register_transport_volunteer",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    auto dbResult = txn.exec_params(
                        "INSERT INTO transport_volunteers "
                        "(user_id, name, email, phone, home_state_code, home_zip, "
                        "max_distance_miles, willing_to_cross_state, vehicle_type, "
                        "can_transport_large, has_crate, available_weekdays, available_weekends) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13) "
                        "RETURNING *",
                        userId.empty() ? nullptr : userId.c_str(),
                        name, email,
                        phone.empty() ? nullptr : phone.c_str(),
                        homeStateCode.empty() ? nullptr : homeStateCode.c_str(),
                        homeZip.empty() ? nullptr : homeZip.c_str(),
                        maxDistance, crossState, vehicleType,
                        canTransportLarge, hasCrate, weekdays, weekends
                    );

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    if (dbResult.empty()) return Json::Value(Json::nullValue);

                    Json::Value volunteer;
                    volunteer["id"] = dbResult[0]["id"].as<std::string>();
                    volunteer["name"] = dbResult[0]["name"].as<std::string>();
                    volunteer["email"] = dbResult[0]["email"].as<std::string>();
                    volunteer["max_distance_miles"] = dbResult[0]["max_distance_miles"].is_null() ? 0 : dbResult[0]["max_distance_miles"].as<int>();
                    volunteer["vehicle_type"] = dbResult[0]["vehicle_type"].is_null() ? "" : dbResult[0]["vehicle_type"].as<std::string>();
                    volunteer["is_active"] = dbResult[0]["is_active"].as<bool>();
                    volunteer["created_at"] = dbResult[0]["created_at"].as<std::string>();
                    return volunteer;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        if (result.isNull()) {
            callback(ApiResponse::serverError("Failed to register volunteer"));
        } else {
            callback(ApiResponse::success(result));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error registering volunteer: " + std::string(e.what()),
            {{"endpoint", "POST /api/v1/transport/volunteer"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

} // namespace wtl::core::controllers
