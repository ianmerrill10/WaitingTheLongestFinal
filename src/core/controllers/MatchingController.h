/**
 * ============================================================================
 * PURPOSE
 * ============================================================================
 * MatchingController handles HTTP requests for the Lifestyle Matching Quiz.
 * Provides REST API endpoints for quiz submission, match calculation, and
 * result retrieval. Acts as the primary interface between frontend clients
 * and the matching service backend.
 *
 * ============================================================================
 * USAGE
 * ============================================================================
 * POST /api/v1/matching/quiz
 *   Request body: JSON quiz answers
 *   Response: { success: true, data: { profile_id: "..." }, meta: {...} }
 *
 * GET /api/v1/matching/results?profile_id={id}
 *   Response: { success: true, data: [MatchResult, ...], meta: {...} }
 *
 * GET /api/v1/matching/results?profile_id={id}&dog_id={id}
 *   Response: { success: true, data: MatchResult, meta: {...} }
 *
 * ============================================================================
 * DEPENDENCIES
 * ============================================================================
 * - Drogon web framework (drogon/HttpController.h)
 * - MatchingService (core/services/MatchingService.h)
 * - Dog model (core/models/Dog.h)
 * - Json::Value (jsoncpp)
 * - WTL_CAPTURE_ERROR (error handling macro)
 *
 * ============================================================================
 * MODIFICATION GUIDE
 * ============================================================================
 * To add new endpoints:
 * 1. Create handler method with @GetMapping/@PostMapping decorator
 * 2. Call MatchingService methods to perform business logic
 * 3. Format response with standard JSON structure
 * 4. Add error handling with WTL_CAPTURE_ERROR
 *
 * To modify request/response format:
 * - Update JSON parsing in quiz submission handler
 * - Modify response construction methods
 * - Update API documentation and tests
 *
 * @author WaitingTheLongest Team
 * @date 2025-02-11
 * @version 1.0.0
 * ============================================================================
 */

#ifndef WTL_CORE_CONTROLLERS_MATCHING_CONTROLLER_H_
#define WTL_CORE_CONTROLLERS_MATCHING_CONTROLLER_H_

#include <drogon/HttpController.h>
#include <memory>
#include <json/json.h>

#include "core/services/MatchingService.h"

namespace wtl {
namespace core {
namespace controllers {

/**
 * ============================================================================
 * MatchingController - HTTP API handler for matching quiz
 * ============================================================================
 */
class MatchingController : public drogon::HttpController<MatchingController> {
 public:
  MatchingController();
  ~MatchingController();

  /**
   * POST /api/v1/matching/quiz
   * Submit quiz answers and calculate initial matches
   *
   * Request body (JSON):
   * {
   *   "home_type": "house",
   *   "has_yard": true,
   *   "has_fence": true,
   *   "has_children": false,
   *   "children_ages": [],
   *   "has_other_dogs": true,
   *   "other_dogs_count": 1,
   *   "has_cats": false,
   *   "activity_level": "high",
   *   "hours_home_daily": 8,
   *   "work_from_home": false,
   *   "travel_frequency": "monthly",
   *   "dog_experience": "experienced",
   *   "willing_to_train": true,
   *   "preferred_size": ["medium", "large"],
   *   "preferred_age": ["adult", "senior"],
   *   "preferred_energy": "high",
   *   "ok_with_special_needs": true,
   *   "has_breed_restrictions": false,
   *   "restricted_breeds": [],
   *   "weight_limit_lbs": 100,
   *   "max_adoption_fee": 500,
   *   "user_id": "user_123"
   * }
   *
   * Response (200 OK):
   * {
   *   "success": true,
   *   "data": {
   *     "profile_id": "550e8400-e29b-41d4-a716-446655440000",
   *     "matches_count": 20
   *   },
   *   "meta": {
   *     "timestamp": "2025-02-11T10:30:00Z",
   *     "version": "1.0"
   *   }
   * }
   *
   * @param req HTTP request object
   * @param callback Async response callback
   */
  void submitQuiz(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

  /**
   * GET /api/v1/matching/results?profile_id={id}
   * Retrieve all match results for a profile
   *
   * Query parameters:
   *   profile_id (required): UUID of the lifestyle profile
   *   limit (optional): Maximum results to return (default: 20)
   *
   * Response (200 OK):
   * {
   *   "success": true,
   *   "data": [
   *     {
   *       "dog": { id, name, breed, ... },
   *       "score": 92.5,
   *       "score_breakdown": { ... }
   *     },
   *     ...
   *   ],
   *   "meta": {
   *     "timestamp": "2025-02-11T10:30:00Z",
   *     "count": 20,
   *     "version": "1.0"
   *   }
   * }
   *
   * @param req HTTP request object
   * @param callback Async response callback
   */
  void getMatches(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

  /**
   * GET /api/v1/matching/results?profile_id={id}&dog_id={id}
   * Retrieve single match detail
   *
   * Query parameters:
   *   profile_id (required): UUID of the lifestyle profile
   *   dog_id (required): UUID of the dog
   *
   * Response (200 OK):
   * {
   *   "success": true,
   *   "data": {
   *     "dog": { id, name, breed, ... },
   *     "score": 92.5,
   *     "score_breakdown": {
   *       "compatibility": 0,
   *       "living_situation": 0,
   *       "energy_level": 10,
   *       "experience": 0,
   *       "preferences": 10
   *     }
   *   },
   *   "meta": {
   *     "timestamp": "2025-02-11T10:30:00Z",
   *     "version": "1.0"
   *   }
   * }
   *
   * @param req HTTP request object
   * @param callback Async response callback
   */
  void getMatchDetail(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

  // Define route mappings
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(MatchingController::submitQuiz, "/api/v1/matching/quiz", drogon::Post);
  ADD_METHOD_TO(MatchingController::getMatches, "/api/v1/matching/results", drogon::Get);
  ADD_METHOD_TO(MatchingController::getMatchDetail, "/api/v1/matching/results", drogon::Get);
  METHOD_LIST_END

 private:
  /**
   * Parse quiz JSON request body into LifestyleProfile
   * @param json_obj Request JSON object
   * @return Populated LifestyleProfile, or empty on parse error
   */
  ::wtl::core::services::LifestyleProfile parseQuizJson_(const Json::Value& json_obj);

  /**
   * Build standard success response JSON
   * @param data Response data
   * @param meta Optional metadata
   * @return JSON response object
   */
  Json::Value buildSuccessResponse_(const Json::Value& data,
                                    const Json::Value& meta = Json::Value());

  /**
   * Build standard error response JSON
   * @param error_message Error description
   * @param error_code HTTP error code (400, 500, etc.)
   * @return JSON response object
   */
  Json::Value buildErrorResponse_(const std::string& error_message,
                                  int error_code);

  /**
   * Build metadata object for responses
   * @return Metadata JSON object with timestamp, version, etc.
   */
  Json::Value buildMetadata_();

  /**
   * Serialize MatchResult to JSON
   * @param match_result The result to serialize
   * @return JSON representation
   */
  Json::Value matchResultToJson_(const ::wtl::core::services::MatchResult& match_result);
};

}  // namespace controllers
}  // namespace core
}  // namespace wtl

#endif  // WTL_CORE_CONTROLLERS_MATCHING_CONTROLLER_H_
