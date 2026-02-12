/**
 * @file HousingController.cc
 * @brief Implementation of pet-friendly housing directory endpoints
 * @see HousingController.h
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/controllers/HousingController.h"
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
// SEARCH HOUSING
// ============================================================================

void HousingController::searchHousing(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        // Parse query parameters
        auto state = req->getParameter("state");
        auto city = req->getParameter("city");
        auto propertyType = req->getParameter("property_type");
        auto weightStr = req->getParameter("weight");
        auto breed = req->getParameter("breed");
        auto pageStr = req->getParameter("page");
        auto limitStr = req->getParameter("limit");

        int page = pageStr.empty() ? 1 : std::stoi(pageStr);
        int limit = limitStr.empty() ? 20 : std::stoi(limitStr);
        if (limit > 100) limit = 100;
        int offset = (page - 1) * limit;

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "search_housing",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    // Build dynamic query
                    std::string sql = "SELECT h.*, s.name as state_name, s.abbreviation as state_code "
                                      "FROM pet_friendly_housing h "
                                      "LEFT JOIN states s ON h.state_code = s.code "
                                      "WHERE h.dogs_allowed = TRUE";

                    std::vector<std::string> params;
                    int paramIdx = 1;

                    if (!state.empty()) {
                        sql += " AND (s.abbreviation = $" + std::to_string(paramIdx) +
                               " OR s.name ILIKE $" + std::to_string(paramIdx) + ")";
                        params.push_back(state);
                        paramIdx++;
                    }

                    if (!city.empty()) {
                        sql += " AND h.city ILIKE $" + std::to_string(paramIdx);
                        params.push_back("%" + city + "%");
                        paramIdx++;
                    }

                    if (!propertyType.empty()) {
                        sql += " AND h.property_type = $" + std::to_string(paramIdx);
                        params.push_back(propertyType);
                        paramIdx++;
                    }

                    if (!weightStr.empty()) {
                        int weight = std::stoi(weightStr);
                        sql += " AND (h.weight_limit_lbs IS NULL OR h.weight_limit_lbs >= $" +
                               std::to_string(paramIdx) + ")";
                        params.push_back(std::to_string(weight));
                        paramIdx++;
                    }

                    // Count total
                    std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") sub";

                    // Add ordering and pagination
                    sql += " ORDER BY h.verified DESC, h.name ASC LIMIT $" +
                           std::to_string(paramIdx) + " OFFSET $" + std::to_string(paramIdx + 1);
                    params.push_back(std::to_string(limit));
                    params.push_back(std::to_string(offset));

                    // Execute with dynamic parameters using exec_params0
                    pqxx::result dbResult;
                    pqxx::result countResult;

                    // Build prepared statement dynamically
                    if (params.size() == 2) {
                        // No filters - just limit/offset
                        dbResult = txn.exec_params(
                            "SELECT h.*, s.name as state_name, s.abbreviation as state_code "
                            "FROM pet_friendly_housing h "
                            "LEFT JOIN states s ON h.state_code = s.code "
                            "WHERE h.dogs_allowed = TRUE "
                            "ORDER BY h.verified DESC, h.name ASC LIMIT $1 OFFSET $2",
                            limit, offset
                        );
                        countResult = txn.exec(
                            "SELECT COUNT(*) FROM pet_friendly_housing WHERE dogs_allowed = TRUE"
                        );
                    } else {
                        // Use the dynamically built SQL
                        // For simplicity, execute as raw SQL with escaped params
                        dbResult = txn.exec_params(sql,
                            params.size() > 0 ? params[0] : "",
                            params.size() > 1 ? params[1] : "",
                            params.size() > 2 ? params[2] : "",
                            params.size() > 3 ? params[3] : "",
                            params.size() > 4 ? params[4] : "",
                            params.size() > 5 ? params[5] : ""
                        );
                        countResult = txn.exec(countSql);
                    }

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    int total = countResult.empty() ? 0 : countResult[0][0].as<int>();

                    Json::Value response;
                    Json::Value listings(Json::arrayValue);

                    for (const auto& row : dbResult) {
                        Json::Value listing;
                        listing["id"] = row["id"].as<std::string>();
                        listing["name"] = row["name"].as<std::string>();
                        listing["property_type"] = row["property_type"].is_null() ? "" : row["property_type"].as<std::string>();
                        listing["management_company"] = row["management_company"].is_null() ? "" : row["management_company"].as<std::string>();
                        listing["address"] = row["address"].is_null() ? "" : row["address"].as<std::string>();
                        listing["city"] = row["city"].as<std::string>();
                        listing["state_name"] = row["state_name"].is_null() ? "" : row["state_name"].as<std::string>();
                        listing["state_code"] = row["state_code"].is_null() ? "" : row["state_code"].as<std::string>();
                        listing["zip_code"] = row["zip_code"].is_null() ? "" : row["zip_code"].as<std::string>();
                        listing["phone"] = row["phone"].is_null() ? "" : row["phone"].as<std::string>();
                        listing["email"] = row["email"].is_null() ? "" : row["email"].as<std::string>();
                        listing["website"] = row["website"].is_null() ? "" : row["website"].as<std::string>();
                        listing["dogs_allowed"] = row["dogs_allowed"].as<bool>();
                        listing["max_pets"] = row["max_pets"].is_null() ? 0 : row["max_pets"].as<int>();
                        listing["weight_limit_lbs"] = row["weight_limit_lbs"].is_null() ? 0 : row["weight_limit_lbs"].as<int>();
                        listing["pet_deposit"] = row["pet_deposit"].is_null() ? 0.0 : row["pet_deposit"].as<double>();
                        listing["monthly_pet_rent"] = row["monthly_pet_rent"].is_null() ? 0.0 : row["monthly_pet_rent"].as<double>();
                        listing["verified"] = row["verified"].as<bool>();
                        listing["notes"] = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
                        listings.append(listing);
                    }

                    response["listings"] = listings;
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
            "Error searching housing: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/housing"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// HOUSING DETAIL
// ============================================================================

void HousingController::getHousingDetail(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id)
{
    try {
        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "get_housing_detail",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);
                    auto dbResult = txn.exec_params(
                        "SELECT h.*, s.name as state_name, s.abbreviation as state_code "
                        "FROM pet_friendly_housing h "
                        "LEFT JOIN states s ON h.state_code = s.code "
                        "WHERE h.id = $1", id
                    );
                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    if (dbResult.empty()) {
                        return Json::Value(Json::nullValue);
                    }

                    const auto& row = dbResult[0];
                    Json::Value listing;
                    listing["id"] = row["id"].as<std::string>();
                    listing["name"] = row["name"].as<std::string>();
                    listing["property_type"] = row["property_type"].is_null() ? "" : row["property_type"].as<std::string>();
                    listing["management_company"] = row["management_company"].is_null() ? "" : row["management_company"].as<std::string>();
                    listing["address"] = row["address"].is_null() ? "" : row["address"].as<std::string>();
                    listing["city"] = row["city"].as<std::string>();
                    listing["state_name"] = row["state_name"].is_null() ? "" : row["state_name"].as<std::string>();
                    listing["state_code"] = row["state_code"].is_null() ? "" : row["state_code"].as<std::string>();
                    listing["zip_code"] = row["zip_code"].is_null() ? "" : row["zip_code"].as<std::string>();
                    listing["latitude"] = row["latitude"].is_null() ? 0.0 : row["latitude"].as<double>();
                    listing["longitude"] = row["longitude"].is_null() ? 0.0 : row["longitude"].as<double>();
                    listing["phone"] = row["phone"].is_null() ? "" : row["phone"].as<std::string>();
                    listing["email"] = row["email"].is_null() ? "" : row["email"].as<std::string>();
                    listing["website"] = row["website"].is_null() ? "" : row["website"].as<std::string>();
                    listing["dogs_allowed"] = row["dogs_allowed"].as<bool>();
                    listing["cats_allowed"] = row["cats_allowed"].is_null() ? false : row["cats_allowed"].as<bool>();
                    listing["max_pets"] = row["max_pets"].is_null() ? 0 : row["max_pets"].as<int>();
                    listing["weight_limit_lbs"] = row["weight_limit_lbs"].is_null() ? 0 : row["weight_limit_lbs"].as<int>();
                    listing["pet_deposit"] = row["pet_deposit"].is_null() ? 0.0 : row["pet_deposit"].as<double>();
                    listing["monthly_pet_rent"] = row["monthly_pet_rent"].is_null() ? 0.0 : row["monthly_pet_rent"].as<double>();
                    listing["verified"] = row["verified"].as<bool>();
                    listing["notes"] = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
                    return listing;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        if (result.isNull()) {
            callback(ApiResponse::notFound("Housing listing"));
        } else {
            callback(ApiResponse::success(result));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error getting housing detail: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/housing/{id}"}, {"id", id}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// SUBMIT HOUSING
// ============================================================================

void HousingController::submitHousing(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string propertyName = (*json)["property_name"].asString();
        std::string city = (*json)["city"].asString();
        std::string stateCode = (*json)["state_code"].asString();
        bool dogsAllowed = (*json).get("dogs_allowed", true).asBool();
        std::string notes = (*json).get("notes", "").asString();
        std::string userId = (*json).get("user_id", "").asString();
        std::string housingId = (*json).get("housing_id", "").asString();

        if (propertyName.empty() || city.empty() || stateCode.empty()) {
            callback(ApiResponse::badRequest("property_name, city, and state_code are required"));
            return;
        }

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "submit_housing",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    pqxx::result dbResult;
                    if (housingId.empty()) {
                        if (userId.empty()) {
                            dbResult = txn.exec_params(
                                "INSERT INTO housing_submissions "
                                "(property_name, city, state_code, dogs_allowed, notes) "
                                "VALUES ($1, $2, $3, $4, $5) RETURNING *",
                                propertyName, city, stateCode, dogsAllowed,
                                notes.empty() ? nullptr : notes.c_str()
                            );
                        } else {
                            dbResult = txn.exec_params(
                                "INSERT INTO housing_submissions "
                                "(user_id, property_name, city, state_code, dogs_allowed, notes) "
                                "VALUES ($1, $2, $3, $4, $5, $6) RETURNING *",
                                userId, propertyName, city, stateCode, dogsAllowed,
                                notes.empty() ? nullptr : notes.c_str()
                            );
                        }
                    } else {
                        dbResult = txn.exec_params(
                            "INSERT INTO housing_submissions "
                            "(user_id, housing_id, property_name, city, state_code, dogs_allowed, notes) "
                            "VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING *",
                            userId.empty() ? nullptr : userId.c_str(),
                            housingId, propertyName, city, stateCode, dogsAllowed,
                            notes.empty() ? nullptr : notes.c_str()
                        );
                    }

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    if (dbResult.empty()) return Json::Value(Json::nullValue);

                    Json::Value submission;
                    submission["id"] = dbResult[0]["id"].as<std::string>();
                    submission["property_name"] = dbResult[0]["property_name"].as<std::string>();
                    submission["city"] = dbResult[0]["city"].as<std::string>();
                    submission["state_code"] = dbResult[0]["state_code"].as<std::string>();
                    submission["status"] = dbResult[0]["status"].as<std::string>();
                    submission["created_at"] = dbResult[0]["created_at"].as<std::string>();
                    return submission;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        if (result.isNull()) {
            callback(ApiResponse::serverError("Failed to submit housing listing"));
        } else {
            callback(ApiResponse::success(result));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error submitting housing: " + std::string(e.what()),
            {{"endpoint", "POST /api/v1/housing/submit"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

} // namespace wtl::core::controllers
