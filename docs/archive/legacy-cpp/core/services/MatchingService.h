/**
 * ============================================================================
 * PURPOSE
 * ============================================================================
 * MatchingService provides the core matching algorithm for the Lifestyle
 * Matching Quiz. It calculates compatibility scores between user profiles
 * and adoptable dogs, persists profiles to the database, and returns ranked
 * match results. Implements singleton pattern for centralized access.
 *
 * ============================================================================
 * USAGE
 * ============================================================================
 * auto& service = MatchingService::getInstance();
 * LifestyleProfile profile = {...};
 * std::string profile_id = service.saveProfile(profile, user_id);
 * auto matches = service.calculateMatches(profile_id, 20);
 * float score = service.calculateMatchScore(profile, dog);
 *
 * ============================================================================
 * DEPENDENCIES
 * ============================================================================
 * - ConnectionPool (database connection management via pqxx)
 * - Dog model (core/models/Dog.h)
 * - Json::Value (jsoncpp library)
 * - WTL_CAPTURE_ERROR (error handling macro)
 *
 * ============================================================================
 * MODIFICATION GUIDE
 * ============================================================================
 * To adjust matching algorithm weights:
 * - Modify constants in scoreCompatibility() method
 * - Update penalty values in calculateMatchScore()
 * - Add new scoring methods following pattern: score[Category]()
 *
 * To add profile fields:
 * - Add to LifestyleProfile struct
 * - Update saveProfile() SQL INSERT statement
 * - Update getMatchResults() to include new data
 * - Adjust scoring logic if relevant
 *
 * @author WaitingTheLongest Team
 * @date 2025-02-11
 * @version 1.0.0
 * ============================================================================
 */

#ifndef WTL_CORE_SERVICES_MATCHING_SERVICE_H_
#define WTL_CORE_SERVICES_MATCHING_SERVICE_H_

#include <string>
#include <vector>
#include <memory>
#include <json/json.h>

#include "core/models/Dog.h"

namespace wtl {
namespace core {
namespace services {

/**
 * LifestyleProfile struct containing all user quiz responses
 * Maps directly to BUILD_PLAN requirements
 */
struct LifestyleProfile {
  // Living situation
  std::string home_type;              // "apartment" | "house" | "farm"
  bool has_yard = false;
  bool has_fence = false;

  // Family composition
  bool has_children = false;
  std::vector<int> children_ages;     // Ages of children in household
  bool has_other_dogs = false;
  int other_dogs_count = 0;
  bool has_cats = false;

  // Activity & availability
  std::string activity_level;         // "low" | "moderate" | "high" | "very_high"
  int hours_home_daily = 0;           // 0-24
  bool work_from_home = false;
  std::string travel_frequency;       // "rarely" | "monthly" | "frequently"

  // Experience & training
  std::string dog_experience;         // "none" | "some" | "experienced"
  bool willing_to_train = false;

  // Preferences
  std::vector<std::string> preferred_size;      // "small" | "medium" | "large" | "xlarge"
  std::vector<std::string> preferred_age;       // "puppy" | "young" | "adult" | "senior"
  std::string preferred_energy;                 // "low" | "moderate" | "high"
  bool ok_with_special_needs = false;

  // Restrictions
  bool has_breed_restrictions = false;
  std::vector<std::string> restricted_breeds;
  int weight_limit_lbs = 0;
  int max_adoption_fee = 0;
};

/**
 * MatchResult struct containing dog and compatibility score
 */
struct MatchResult {
  ::wtl::core::models::Dog dog;
  float score = 0.0f;
  Json::Value score_breakdown;        // Detailed scoring component breakdown
};

/**
 * ============================================================================
 * MatchingService - Singleton service for lifestyle-to-dog matching
 * ============================================================================
 */
class MatchingService {
 public:
  /**
   * Singleton accessor
   * @return Reference to MatchingService singleton instance
   */
  static MatchingService& getInstance();

  /**
   * Save lifestyle profile to database
   * @param profile The lifestyle profile to save
   * @param user_id The user ID this profile belongs to
   * @return UUID of the saved profile, empty string on error
   */
  std::string saveProfile(const LifestyleProfile& profile,
                         const std::string& user_id);

  /**
   * Calculate matches for a saved profile against all adoptable dogs
   * @param profile_id The ID of the saved lifestyle profile
   * @param limit Maximum number of results to return (default 20)
   * @return Vector of MatchResult ordered by score descending
   */
  std::vector<MatchResult> calculateMatches(const std::string& profile_id,
                                            int limit = 20);

  /**
   * Calculate match score between a lifestyle profile and a dog
   * Core algorithm implementing the matching logic from BUILD_PLAN
   * @param profile The lifestyle profile
   * @param dog The dog to match against
   * @return Score from 0-100, where 100 is perfect match
   */
  float calculateMatchScore(const LifestyleProfile& profile,
                           const ::wtl::core::models::Dog& dog);

  /**
   * Retrieve previously calculated match results for a profile
   * @param profile_id The ID of the lifestyle profile
   * @return Vector of cached MatchResult ordered by score descending
   */
  std::vector<MatchResult> getMatchResults(const std::string& profile_id);

  // Delete copy/move operations for singleton
  MatchingService(const MatchingService&) = delete;
  MatchingService(MatchingService&&) = delete;
  MatchingService& operator=(const MatchingService&) = delete;
  MatchingService& operator=(MatchingService&&) = delete;

 private:
  MatchingService();
  ~MatchingService();

  /**
   * ========== Private Scoring Methods ==========
   */

  /**
   * Score general compatibility factors
   * @param profile User lifestyle profile
   * @param dog The dog being evaluated
   * @return Penalty amount (subtracted from base score)
   */
  float scoreCompatibility_(const LifestyleProfile& profile,
                           const ::wtl::core::models::Dog& dog);

  /**
   * Score living situation compatibility
   * @param profile User lifestyle profile
   * @param dog The dog being evaluated
   * @return Penalty amount
   */
  float scoreLivingSituation_(const LifestyleProfile& profile,
                             const ::wtl::core::models::Dog& dog);

  /**
   * Score energy level match
   * @param profile User lifestyle profile
   * @param dog The dog being evaluated
   * @return Penalty amount
   */
  float scoreEnergy_(const LifestyleProfile& profile,
                    const ::wtl::core::models::Dog& dog);

  /**
   * Score training experience match
   * @param profile User lifestyle profile
   * @param dog The dog being evaluated
   * @return Penalty amount
   */
  float scoreExperience_(const LifestyleProfile& profile,
                        const ::wtl::core::models::Dog& dog);

  /**
   * Score preference matches (size, age, etc.)
   * @param profile User lifestyle profile
   * @param dog The dog being evaluated
   * @return Bonus amount (added to score)
   */
  float scorePreferences_(const LifestyleProfile& profile,
                        const ::wtl::core::models::Dog& dog);

  /**
   * Check if dog is disqualified for this profile
   * Hard disqualifiers return true (score becomes 0)
   * @param profile User lifestyle profile
   * @param dog The dog being evaluated
   * @return True if dog fails hard disqualification criteria
   */
  bool isDisqualified_(const LifestyleProfile& profile,
                      const ::wtl::core::models::Dog& dog);

  static MatchingService* instance_;
};

}  // namespace services
}  // namespace core
}  // namespace wtl

#endif  // WTL_CORE_SERVICES_MATCHING_SERVICE_H_
