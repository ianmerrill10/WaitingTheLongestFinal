/**
 * ============================================================================
 * PURPOSE
 * ============================================================================
 * MatchingController implementation handles HTTP requests for the Lifestyle
 * Matching Quiz API endpoints. Parses JSON request bodies, orchestrates calls
 * to MatchingService for business logic, and formats responses according to
 * the standard API response format.
 *
 * ============================================================================
 * USAGE
 * ============================================================================
 * See MatchingController.h for endpoint documentation and request/response
 * formats.
 *
 * ============================================================================
 * DEPENDENCIES
 * ============================================================================
 * - Drogon web framework
 * - MatchingService (core/services/MatchingService.h)
 * - Dog model (core/models/Dog.h)
 * - jsoncpp for JSON parsing/generation
 * - Error handling utilities (WTL_CAPTURE_ERROR macro)
 *
 * ============================================================================
 * MODIFICATION GUIDE
 * ============================================================================
 * To modify request parsing:
 * - Update parseQuizJson_() method to handle additional fields
 * - Validate required fields and return errors for invalid data
 * - Update corresponding struct in MatchingService
 *
 * To change response format:
 * - Modify buildSuccessResponse_(), buildErrorResponse_()
 * - Update metadata structure in buildMetadata_()
 * - Ensure backward compatibility if possible
 *
 * @author WaitingTheLongest Team
 * @date 2025-02-11
 * @version 1.0.0
 * ============================================================================
 */

#include "core/controllers/MatchingController.h"

#include <chrono>
#include <iomanip>
#include <sstream>

#include "core/models/Dog.h"
#include "core/services/MatchingService.h"
#include "core/debug/ErrorCapture.h"

namespace wtl {
namespace core {
namespace controllers {

/**
 * ============================================================================
 * CONSTRUCTOR / DESTRUCTOR
 * ============================================================================
 */

MatchingController::MatchingController() {
  // Initialize controller
}

MatchingController::~MatchingController() {
  // Cleanup
}

/**
 * ============================================================================
 * PUBLIC ENDPOINTS
 * ============================================================================
 */

void MatchingController::submitQuiz(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
  try {
    // Parse request body
    auto json_obj = req->getJsonObject();
    if (!json_obj) {
      auto response = drogon::HttpResponse::newHttpJsonResponse(
          buildErrorResponse_("Invalid JSON request body", 400));
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
      return;
    }

    // Check required fields
    if (!json_obj->isMember("user_id")) {
      auto response = drogon::HttpResponse::newHttpJsonResponse(
          buildErrorResponse_("Missing required field: user_id", 400));
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
      return;
    }

    std::string user_id = (*json_obj)["user_id"].asString();

    // Parse quiz responses into profile
    ::wtl::core::services::LifestyleProfile profile = parseQuizJson_(*json_obj);

    // Validate parsed profile
    if (profile.home_type.empty()) {
      auto response = drogon::HttpResponse::newHttpJsonResponse(
          buildErrorResponse_("Invalid quiz data: missing home_type", 400));
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
      return;
    }

    // Save profile to database
    auto& service = ::wtl::core::services::MatchingService::getInstance();
    std::string profile_id = service.saveProfile(profile, user_id);

    if (profile_id.empty()) {
      WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC, "Failed to save profile for user: " + user_id, {});
      auto response = drogon::HttpResponse::newHttpJsonResponse(
          buildErrorResponse_("Failed to save profile", 500));
      response->setStatusCode(drogon::k500InternalServerError);
      callback(response);
      return;
    }

    // Calculate initial matches
    auto matches = service.calculateMatches(profile_id, 20);

    // Build response
    Json::Value response_data;
    response_data["profile_id"] = profile_id;
    response_data["matches_count"] = static_cast<int>(matches.size());

    auto response = drogon::HttpResponse::newHttpJsonResponse(
        buildSuccessResponse_(response_data, buildMetadata_()));
    response->setStatusCode(drogon::k200OK);
    callback(response);

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC, std::string("submitQuiz Exception: ") + e.what(), {});
    auto response = drogon::HttpResponse::newHttpJsonResponse(
        buildErrorResponse_("Internal server error", 500));
    response->setStatusCode(drogon::k500InternalServerError);
    callback(response);
  }
}

void MatchingController::getMatches(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
  try {
    // Check if this is a single match detail request
    std::string dog_id_param = req->getParameter("dog_id");
    if (!dog_id_param.empty()) {
      getMatchDetail(req, std::move(callback));
      return;
    }

    // Get profile_id from query parameters
    std::string profile_id = req->getParameter("profile_id");
    if (profile_id.empty()) {
      auto response = drogon::HttpResponse::newHttpJsonResponse(
          buildErrorResponse_("Missing required parameter: profile_id", 400));
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
      return;
    }

    // Get optional limit parameter
    int limit = 20;
    std::string limit_param = req->getParameter("limit");
    if (!limit_param.empty()) {
      try {
        limit = std::stoi(limit_param);
        limit = std::max(1, std::min(limit, 100));  // Clamp to 1-100
      } catch (const std::exception&) {
        limit = 20;  // Default on parse error
      }
    }

    // Retrieve matches from service
    auto& service = ::wtl::core::services::MatchingService::getInstance();
    auto matches = service.getMatchResults(profile_id);

    if (matches.empty()) {
      WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC, "No matches found for profile: " + profile_id, {});
      auto response = drogon::HttpResponse::newHttpJsonResponse(
          buildErrorResponse_("Profile not found or no matches available", 404));
      response->setStatusCode(drogon::k404NotFound);
      callback(response);
      return;
    }

    // Build response data
    Json::Value response_data(Json::arrayValue);
    for (size_t i = 0; i < matches.size() && i < static_cast<size_t>(limit); ++i) {
      response_data.append(matchResultToJson_(matches[i]));
    }

    // Build metadata
    Json::Value meta = buildMetadata_();
    meta["count"] = static_cast<int>(response_data.size());

    auto response = drogon::HttpResponse::newHttpJsonResponse(
        buildSuccessResponse_(response_data, meta));
    response->setStatusCode(drogon::k200OK);
    callback(response);

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC, std::string("getMatches Exception: ") + e.what(), {});
    auto response = drogon::HttpResponse::newHttpJsonResponse(
        buildErrorResponse_("Internal server error", 500));
    response->setStatusCode(drogon::k500InternalServerError);
    callback(response);
  }
}

void MatchingController::getMatchDetail(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
  try {
    // Get query parameters
    std::string profile_id = req->getParameter("profile_id");
    std::string dog_id = req->getParameter("dog_id");

    if (profile_id.empty() || dog_id.empty()) {
      auto response = drogon::HttpResponse::newHttpJsonResponse(
          buildErrorResponse_("Missing required parameters: profile_id, dog_id", 400));
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
      return;
    }

    // Retrieve all matches for profile
    auto& service = ::wtl::core::services::MatchingService::getInstance();
    auto matches = service.getMatchResults(profile_id);

    // Find the specific dog match
    ::wtl::core::services::MatchResult* found_match = nullptr;
    for (auto& match : matches) {
      if (match.dog.id == dog_id) {
        found_match = &match;
        break;
      }
    }

    if (!found_match) {
      auto response = drogon::HttpResponse::newHttpJsonResponse(
          buildErrorResponse_("Match not found", 404));
      response->setStatusCode(drogon::k404NotFound);
      callback(response);
      return;
    }

    // Build response
    Json::Value response_data = matchResultToJson_(*found_match);

    auto response = drogon::HttpResponse::newHttpJsonResponse(
        buildSuccessResponse_(response_data, buildMetadata_()));
    response->setStatusCode(drogon::k200OK);
    callback(response);

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC, std::string("getMatchDetail Exception: ") + e.what(), {});
    auto response = drogon::HttpResponse::newHttpJsonResponse(
        buildErrorResponse_("Internal server error", 500));
    response->setStatusCode(drogon::k500InternalServerError);
    callback(response);
  }
}

/**
 * ============================================================================
 * PRIVATE HELPER METHODS
 * ============================================================================
 */

::wtl::core::services::LifestyleProfile MatchingController::parseQuizJson_(
    const Json::Value& json_obj) {
  ::wtl::core::services::LifestyleProfile profile;

  try {
    // Living situation
    if (json_obj.isMember("home_type")) {
      profile.home_type = json_obj["home_type"].asString();
    }
    if (json_obj.isMember("has_yard")) {
      profile.has_yard = json_obj["has_yard"].asBool();
    }
    if (json_obj.isMember("has_fence")) {
      profile.has_fence = json_obj["has_fence"].asBool();
    }

    // Family composition
    if (json_obj.isMember("has_children")) {
      profile.has_children = json_obj["has_children"].asBool();
    }
    if (json_obj.isMember("children_ages") && json_obj["children_ages"].isArray()) {
      for (const auto& age : json_obj["children_ages"]) {
        profile.children_ages.push_back(age.asInt());
      }
    }
    if (json_obj.isMember("has_other_dogs")) {
      profile.has_other_dogs = json_obj["has_other_dogs"].asBool();
    }
    if (json_obj.isMember("other_dogs_count")) {
      profile.other_dogs_count = json_obj["other_dogs_count"].asInt();
    }
    if (json_obj.isMember("has_cats")) {
      profile.has_cats = json_obj["has_cats"].asBool();
    }

    // Activity & availability
    if (json_obj.isMember("activity_level")) {
      profile.activity_level = json_obj["activity_level"].asString();
    }
    if (json_obj.isMember("hours_home_daily")) {
      profile.hours_home_daily = json_obj["hours_home_daily"].asInt();
    }
    if (json_obj.isMember("work_from_home")) {
      profile.work_from_home = json_obj["work_from_home"].asBool();
    }
    if (json_obj.isMember("travel_frequency")) {
      profile.travel_frequency = json_obj["travel_frequency"].asString();
    }

    // Experience & training
    if (json_obj.isMember("dog_experience")) {
      profile.dog_experience = json_obj["dog_experience"].asString();
    }
    if (json_obj.isMember("willing_to_train")) {
      profile.willing_to_train = json_obj["willing_to_train"].asBool();
    }

    // Preferences
    if (json_obj.isMember("preferred_size") && json_obj["preferred_size"].isArray()) {
      for (const auto& size : json_obj["preferred_size"]) {
        profile.preferred_size.push_back(size.asString());
      }
    }
    if (json_obj.isMember("preferred_age") && json_obj["preferred_age"].isArray()) {
      for (const auto& age : json_obj["preferred_age"]) {
        profile.preferred_age.push_back(age.asString());
      }
    }
    if (json_obj.isMember("preferred_energy")) {
      profile.preferred_energy = json_obj["preferred_energy"].asString();
    }
    if (json_obj.isMember("ok_with_special_needs")) {
      profile.ok_with_special_needs = json_obj["ok_with_special_needs"].asBool();
    }

    // Restrictions
    if (json_obj.isMember("has_breed_restrictions")) {
      profile.has_breed_restrictions = json_obj["has_breed_restrictions"].asBool();
    }
    if (json_obj.isMember("restricted_breeds") && json_obj["restricted_breeds"].isArray()) {
      for (const auto& breed : json_obj["restricted_breeds"]) {
        profile.restricted_breeds.push_back(breed.asString());
      }
    }
    if (json_obj.isMember("weight_limit_lbs")) {
      profile.weight_limit_lbs = json_obj["weight_limit_lbs"].asInt();
    }
    if (json_obj.isMember("max_adoption_fee")) {
      profile.max_adoption_fee = json_obj["max_adoption_fee"].asInt();
    }

    return profile;

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(ErrorCategory::VALIDATION, std::string("parseQuizJson_ Exception: ") + e.what(), {});
    return ::wtl::core::services::LifestyleProfile();
  }
}

Json::Value MatchingController::buildSuccessResponse_(
    const Json::Value& data,
    const Json::Value& meta) {
  Json::Value response;
  response["success"] = true;
  response["data"] = data;
  response["meta"] = meta;
  return response;
}

Json::Value MatchingController::buildErrorResponse_(
    const std::string& error_message,
    int error_code) {
  Json::Value response;
  response["success"] = false;
  response["error"]["message"] = error_message;
  response["error"]["code"] = error_code;
  response["meta"] = buildMetadata_();
  return response;
}

Json::Value MatchingController::buildMetadata_() {
  Json::Value meta;

  // Add timestamp in ISO 8601 format
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
  meta["timestamp"] = ss.str();

  // Add version
  meta["version"] = "1.0";

  return meta;
}

Json::Value MatchingController::matchResultToJson_(
    const ::wtl::core::services::MatchResult& match_result) {
  Json::Value result;

  // Serialize dog object
  Json::Value dog_json;
  dog_json["id"] = match_result.dog.id;
  dog_json["name"] = match_result.dog.name;
  dog_json["breed"] = match_result.dog.breed_primary;
  dog_json["size"] = match_result.dog.size;
  dog_json["age_category"] = match_result.dog.age_category;
  dog_json["weight"] = match_result.dog.weight_lbs.value_or(0.0);
  dog_json["energy_level"] = match_result.dog.energy_level;
  dog_json["good_with_children"] = match_result.dog.good_with_kids.value_or(false);
  dog_json["good_with_dogs"] = match_result.dog.good_with_dogs.value_or(false);
  dog_json["good_with_cats"] = match_result.dog.good_with_cats.value_or(false);
  dog_json["apartment_suitable"] = false;  // No direct field; derived from size
  dog_json["special_needs"] = match_result.dog.has_special_needs;
  dog_json["adoption_fee"] = match_result.dog.adoption_fee.value_or(0.0);

  result["dog"] = dog_json;
  result["score"] = match_result.score;
  result["score_breakdown"] = match_result.score_breakdown;

  return result;
}

}  // namespace controllers
}  // namespace core
}  // namespace wtl
