/**
 * @file ResourcesController.cc
 * @brief Implementation of adoption resources, check-ins, and success stories endpoints
 * @see ResourcesController.h
 *
 * @author Agent 5 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/controllers/ResourcesController.h"
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
// RESOURCE LIBRARY
// ============================================================================

void ResourcesController::listResources(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto category = req->getParameter("category");
        auto forNewAdopters = req->getParameter("for_new_adopters");
        auto pageStr = req->getParameter("page");
        auto limitStr = req->getParameter("limit");

        int page = pageStr.empty() ? 1 : std::stoi(pageStr);
        int limit = limitStr.empty() ? 20 : std::stoi(limitStr);
        if (limit > 100) limit = 100;
        int offset = (page - 1) * limit;

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "list_resources",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    pqxx::result dbResult;
                    pqxx::result countResult;

                    if (!category.empty()) {
                        dbResult = txn.exec_params(
                            "SELECT * FROM resources WHERE is_published = TRUE "
                            "AND category = $1 ORDER BY view_count DESC, created_at DESC "
                            "LIMIT $2 OFFSET $3",
                            category, limit, offset
                        );
                        countResult = txn.exec_params(
                            "SELECT COUNT(*) FROM resources WHERE is_published = TRUE AND category = $1",
                            category
                        );
                    } else if (!forNewAdopters.empty() && forNewAdopters == "true") {
                        dbResult = txn.exec_params(
                            "SELECT * FROM resources WHERE is_published = TRUE "
                            "AND for_new_adopters = TRUE ORDER BY view_count DESC, created_at DESC "
                            "LIMIT $1 OFFSET $2",
                            limit, offset
                        );
                        countResult = txn.exec(
                            "SELECT COUNT(*) FROM resources WHERE is_published = TRUE AND for_new_adopters = TRUE"
                        );
                    } else {
                        dbResult = txn.exec_params(
                            "SELECT * FROM resources WHERE is_published = TRUE "
                            "ORDER BY view_count DESC, created_at DESC "
                            "LIMIT $1 OFFSET $2",
                            limit, offset
                        );
                        countResult = txn.exec(
                            "SELECT COUNT(*) FROM resources WHERE is_published = TRUE"
                        );
                    }

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    int total = countResult.empty() ? 0 : countResult[0][0].as<int>();

                    Json::Value response;
                    Json::Value resources(Json::arrayValue);

                    for (const auto& row : dbResult) {
                        Json::Value resource;
                        resource["id"] = row["id"].as<std::string>();
                        resource["title"] = row["title"].as<std::string>();
                        resource["slug"] = row["slug"].as<std::string>();
                        resource["category"] = row["category"].is_null() ? "" : row["category"].as<std::string>();
                        resource["summary"] = row["summary"].is_null() ? "" : row["summary"].as<std::string>();
                        resource["for_new_adopters"] = row["for_new_adopters"].as<bool>();
                        resource["for_special_needs"] = row["for_special_needs"].as<bool>();
                        resource["featured_image"] = row["featured_image"].is_null() ? "" : row["featured_image"].as<std::string>();
                        resource["view_count"] = row["view_count"].as<int>();
                        resource["created_at"] = row["created_at"].as<std::string>();
                        resources.append(resource);
                    }

                    response["resources"] = resources;
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
            "Error listing resources: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/resources"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

void ResourcesController::getResource(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& slug)
{
    try {
        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "get_resource",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    auto dbResult = txn.exec_params(
                        "SELECT * FROM resources WHERE slug = $1 AND is_published = TRUE",
                        slug
                    );

                    // Increment view count
                    if (!dbResult.empty()) {
                        txn.exec_params(
                            "UPDATE resources SET view_count = view_count + 1 WHERE slug = $1",
                            slug
                        );
                    }

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    if (dbResult.empty()) return Json::Value(Json::nullValue);

                    const auto& row = dbResult[0];
                    Json::Value resource;
                    resource["id"] = row["id"].as<std::string>();
                    resource["title"] = row["title"].as<std::string>();
                    resource["slug"] = row["slug"].as<std::string>();
                    resource["category"] = row["category"].is_null() ? "" : row["category"].as<std::string>();
                    resource["summary"] = row["summary"].is_null() ? "" : row["summary"].as<std::string>();
                    resource["content"] = row["content"].as<std::string>();
                    resource["for_new_adopters"] = row["for_new_adopters"].as<bool>();
                    resource["for_special_needs"] = row["for_special_needs"].as<bool>();
                    resource["featured_image"] = row["featured_image"].is_null() ? "" : row["featured_image"].as<std::string>();
                    resource["video_url"] = row["video_url"].is_null() ? "" : row["video_url"].as<std::string>();
                    resource["meta_description"] = row["meta_description"].is_null() ? "" : row["meta_description"].as<std::string>();
                    resource["view_count"] = row["view_count"].as<int>() + 1;
                    resource["created_at"] = row["created_at"].as<std::string>();
                    resource["updated_at"] = row["updated_at"].as<std::string>();
                    return resource;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        if (result.isNull()) {
            callback(ApiResponse::notFound("Resource"));
        } else {
            callback(ApiResponse::success(result));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error getting resource: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/resources/{slug}"}, {"slug", slug}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// ADOPTION TRACKING
// ============================================================================

void ResourcesController::getMyAdoptions(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto userId = req->getParameter("user_id");
        if (userId.empty()) {
            callback(ApiResponse::badRequest("user_id query parameter is required"));
            return;
        }

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "get_my_adoptions",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    auto dbResult = txn.exec_params(
                        "SELECT a.*, d.name as dog_name, d.breed as dog_breed, "
                        "d.primary_photo_url as dog_photo, "
                        "sh.name as shelter_name "
                        "FROM adoptions a "
                        "LEFT JOIN dogs d ON a.dog_id = d.id "
                        "LEFT JOIN shelters sh ON a.shelter_id = sh.id "
                        "WHERE a.adopter_id = $1 "
                        "ORDER BY a.adoption_date DESC",
                        userId
                    );

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    Json::Value adoptions(Json::arrayValue);
                    for (const auto& row : dbResult) {
                        Json::Value adoption;
                        adoption["id"] = row["id"].as<std::string>();
                        adoption["dog_id"] = row["dog_id"].as<std::string>();
                        adoption["dog_name"] = row["dog_name"].is_null() ? "" : row["dog_name"].as<std::string>();
                        adoption["dog_breed"] = row["dog_breed"].is_null() ? "" : row["dog_breed"].as<std::string>();
                        adoption["dog_photo"] = row["dog_photo"].is_null() ? "" : row["dog_photo"].as<std::string>();
                        adoption["shelter_name"] = row["shelter_name"].is_null() ? "" : row["shelter_name"].as<std::string>();
                        adoption["adoption_date"] = row["adoption_date"].as<std::string>();
                        adoption["found_via"] = row["found_via"].is_null() ? "" : row["found_via"].as<std::string>();
                        adoption["was_sponsored"] = row["was_sponsored"].as<bool>();
                        adoption["checkin_count"] = row["checkin_count"].as<int>();
                        adoption["status"] = row["status"].as<std::string>();
                        adoption["created_at"] = row["created_at"].as<std::string>();
                        adoptions.append(adoption);
                    }

                    return adoptions;

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
            "Error getting user adoptions: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/adoptions/mine"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

void ResourcesController::submitCheckin(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& adoptionId)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string checkinType = (*json)["checkin_type"].asString();
        std::string adjustment = (*json).get("how_is_adjustment", "").asString();
        bool needsHelp = (*json).get("needs_help", false).asBool();
        std::string helpTopic = (*json).get("help_topic", "").asString();
        std::string comments = (*json).get("comments", "").asString();

        if (checkinType.empty()) {
            callback(ApiResponse::badRequest("checkin_type is required (day1, week1, month1, month3, year1)"));
            return;
        }

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "submit_checkin",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    // Verify adoption exists
                    auto adoptionCheck = txn.exec_params(
                        "SELECT id FROM adoptions WHERE id = $1", adoptionId
                    );
                    if (adoptionCheck.empty()) {
                        txn.abort();
                        ConnectionPool::getInstance().release(conn);
                        Json::Value err;
                        err["error"] = "adoption_not_found";
                        return err;
                    }

                    // Insert check-in
                    auto dbResult = txn.exec_params(
                        "INSERT INTO adoption_checkins "
                        "(adoption_id, checkin_type, responded, response_date, "
                        "how_is_adjustment, needs_help, help_topic, comments) "
                        "VALUES ($1, $2, TRUE, NOW(), $3, $4, $5, $6) RETURNING *",
                        adoptionId, checkinType,
                        adjustment.empty() ? nullptr : adjustment.c_str(),
                        needsHelp,
                        helpTopic.empty() ? nullptr : helpTopic.c_str(),
                        comments.empty() ? nullptr : comments.c_str()
                    );

                    // Update adoption check-in tracking
                    txn.exec_params(
                        "UPDATE adoptions SET last_checkin_at = NOW(), "
                        "checkin_count = checkin_count + 1 WHERE id = $1",
                        adoptionId
                    );

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    if (dbResult.empty()) return Json::Value(Json::nullValue);

                    Json::Value checkin;
                    checkin["id"] = dbResult[0]["id"].as<std::string>();
                    checkin["adoption_id"] = dbResult[0]["adoption_id"].as<std::string>();
                    checkin["checkin_type"] = dbResult[0]["checkin_type"].as<std::string>();
                    checkin["how_is_adjustment"] = dbResult[0]["how_is_adjustment"].is_null() ? "" : dbResult[0]["how_is_adjustment"].as<std::string>();
                    checkin["needs_help"] = dbResult[0]["needs_help"].as<bool>();
                    checkin["created_at"] = dbResult[0]["created_at"].as<std::string>();
                    return checkin;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        if (result.isMember("error") && result["error"].asString() == "adoption_not_found") {
            callback(ApiResponse::notFound("Adoption"));
        } else if (result.isNull()) {
            callback(ApiResponse::serverError("Failed to submit check-in"));
        } else {
            callback(ApiResponse::success(result));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error submitting check-in: " + std::string(e.what()),
            {{"endpoint", "POST /api/v1/checkins/{adoptionId}"}, {"adoption_id", adoptionId}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

// ============================================================================
// SUCCESS STORIES
// ============================================================================

void ResourcesController::listSuccessStories(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto featuredParam = req->getParameter("featured");
        auto pageStr = req->getParameter("page");
        auto limitStr = req->getParameter("limit");

        int page = pageStr.empty() ? 1 : std::stoi(pageStr);
        int limit = limitStr.empty() ? 20 : std::stoi(limitStr);
        if (limit > 100) limit = 100;
        int offset = (page - 1) * limit;

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "list_success_stories",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    pqxx::result dbResult;
                    pqxx::result countResult;

                    if (!featuredParam.empty() && featuredParam == "true") {
                        dbResult = txn.exec_params(
                            "SELECT ss.*, d.name as dog_name, d.breed as dog_breed, "
                            "d.primary_photo_url as dog_photo "
                            "FROM success_stories ss "
                            "LEFT JOIN dogs d ON ss.dog_id = d.id "
                            "WHERE ss.status = 'approved' AND ss.featured = TRUE "
                            "ORDER BY ss.approved_at DESC "
                            "LIMIT $1 OFFSET $2",
                            limit, offset
                        );
                        countResult = txn.exec(
                            "SELECT COUNT(*) FROM success_stories "
                            "WHERE status = 'approved' AND featured = TRUE"
                        );
                    } else {
                        dbResult = txn.exec_params(
                            "SELECT ss.*, d.name as dog_name, d.breed as dog_breed, "
                            "d.primary_photo_url as dog_photo "
                            "FROM success_stories ss "
                            "LEFT JOIN dogs d ON ss.dog_id = d.id "
                            "WHERE ss.status = 'approved' "
                            "ORDER BY ss.approved_at DESC "
                            "LIMIT $1 OFFSET $2",
                            limit, offset
                        );
                        countResult = txn.exec(
                            "SELECT COUNT(*) FROM success_stories WHERE status = 'approved'"
                        );
                    }

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    int total = countResult.empty() ? 0 : countResult[0][0].as<int>();

                    Json::Value response;
                    Json::Value stories(Json::arrayValue);

                    for (const auto& row : dbResult) {
                        Json::Value story;
                        story["id"] = row["id"].as<std::string>();
                        story["dog_name"] = row["dog_name"].is_null() ? "" : row["dog_name"].as<std::string>();
                        story["dog_breed"] = row["dog_breed"].is_null() ? "" : row["dog_breed"].as<std::string>();
                        story["dog_photo"] = row["dog_photo"].is_null() ? "" : row["dog_photo"].as<std::string>();
                        story["title"] = row["title"].is_null() ? "" : row["title"].as<std::string>();
                        story["story"] = row["story"].as<std::string>();
                        story["featured"] = row["featured"].as<bool>();
                        story["approved_at"] = row["approved_at"].is_null() ? "" : row["approved_at"].as<std::string>();
                        stories.append(story);
                    }

                    response["stories"] = stories;
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
            "Error listing success stories: " + std::string(e.what()),
            {{"endpoint", "GET /api/v1/success-stories"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

void ResourcesController::submitSuccessStory(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string adoptionId = (*json).get("adoption_id", "").asString();
        std::string userId = (*json)["user_id"].asString();
        std::string dogId = (*json)["dog_id"].asString();
        std::string title = (*json).get("title", "").asString();
        std::string story = (*json)["story"].asString();

        if (userId.empty() || dogId.empty() || story.empty()) {
            callback(ApiResponse::badRequest("user_id, dog_id, and story are required"));
            return;
        }

        auto result = SelfHealing::getInstance().executeOrThrow<Json::Value>(
            "submit_success_story",
            [&]() -> Json::Value {
                auto conn = ConnectionPool::getInstance().acquire();
                try {
                    pqxx::work txn(*conn);

                    auto dbResult = txn.exec_params(
                        "INSERT INTO success_stories "
                        "(adoption_id, user_id, dog_id, title, story) "
                        "VALUES ($1, $2, $3, $4, $5) RETURNING *",
                        adoptionId.empty() ? nullptr : adoptionId.c_str(),
                        userId, dogId,
                        title.empty() ? nullptr : title.c_str(),
                        story
                    );

                    txn.commit();
                    ConnectionPool::getInstance().release(conn);

                    if (dbResult.empty()) return Json::Value(Json::nullValue);

                    Json::Value storyJson;
                    storyJson["id"] = dbResult[0]["id"].as<std::string>();
                    storyJson["dog_id"] = dbResult[0]["dog_id"].as<std::string>();
                    storyJson["title"] = dbResult[0]["title"].is_null() ? "" : dbResult[0]["title"].as<std::string>();
                    storyJson["status"] = dbResult[0]["status"].as<std::string>();
                    storyJson["created_at"] = dbResult[0]["created_at"].as<std::string>();
                    storyJson["message"] = "Your success story has been submitted for review. Thank you!";
                    return storyJson;

                } catch (const std::exception& e) {
                    ConnectionPool::getInstance().release(conn);
                    throw;
                }
            }
        );

        if (result.isNull()) {
            callback(ApiResponse::serverError("Failed to submit success story"));
        } else {
            callback(ApiResponse::success(result));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Error submitting success story: " + std::string(e.what()),
            {{"endpoint", "POST /api/v1/success-stories"}}
        );
        callback(ApiResponse::serverError("Internal server error"));
    }
}

} // namespace wtl::core::controllers
