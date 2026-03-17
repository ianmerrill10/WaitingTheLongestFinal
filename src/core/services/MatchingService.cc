/**
 * ============================================================================
 * PURPOSE
 * ============================================================================
 * MatchingService implementation provides the complete matching algorithm
 * for the Lifestyle Matching Quiz. Persists user profiles to database,
 * calculates compatibility scores between profiles and dogs, and stores
 * match results for retrieval. Implements all scoring logic specified in
 * the BUILD_PLAN matching algorithm.
 *
 * ============================================================================
 * USAGE
 * ============================================================================
 * See MatchingService.h for usage examples and API documentation.
 *
 * ============================================================================
 * DEPENDENCIES
 * ============================================================================
 * - ConnectionPool (database operations via pqxx)
 * - PostgreSQL with libpqxx (parameterized queries)
 * - jsoncpp (JSON serialization)
 * - Dog model (core/models/Dog.h)
 * - Error handling utilities (WTL_CAPTURE_ERROR macro)
 *
 * ============================================================================
 * MODIFICATION GUIDE
 * ============================================================================
 * Algorithm constants are defined as static const at class level.
 * To adjust scoring weights:
 * 1. Modify penalty/bonus values in calculateMatchScore()
 * 2. Update component methods (scoreEnergy_, scoreLivingSituation_, etc.)
 * 3. Test with known dog/profile pairs
 * 4. Update documentation to reflect changes
 *
 * @author WaitingTheLongest Team
 * @date 2025-02-11
 * @version 1.0.0
 * ============================================================================
 */

#include "core/services/MatchingService.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

#include <pqxx/pqxx>

#include "core/db/ConnectionPool.h"
#include "core/models/Dog.h"
#include "core/debug/ErrorCapture.h"

namespace wtl {
namespace core {
namespace services {

// Alias for the Dog type used throughout this file
using Dog = ::wtl::core::models::Dog;

// Static singleton instance
MatchingService* MatchingService::instance_ = nullptr;

/**
 * ============================================================================
 * CONSTRUCTOR / SINGLETON
 * ============================================================================
 */

MatchingService::MatchingService() {
  // Initialize service, connection pool already exists globally
}

MatchingService::~MatchingService() {
  // Cleanup if needed
}

MatchingService& MatchingService::getInstance() {
  if (instance_ == nullptr) {
    instance_ = new MatchingService();
  }
  return *instance_;
}

/**
 * ============================================================================
 * PUBLIC METHODS
 * ============================================================================
 */

std::string MatchingService::saveProfile(const LifestyleProfile& profile,
                                         const std::string& user_id) {
  try {
    // Get database connection
    auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

    try {
      pqxx::work txn(*conn);

      // Generate UUID via PostgreSQL
      std::string profile_id =
          txn.exec("SELECT gen_random_uuid()::text")[0][0].as<std::string>();

      // Serialize preferences to JSON
      Json::Value prefs;
      for (const auto& size : profile.preferred_size) {
        prefs["preferred_size"].append(size);
      }
      for (const auto& age : profile.preferred_age) {
        prefs["preferred_age"].append(age);
      }
      for (const auto& age : profile.children_ages) {
        prefs["children_ages"].append(age);
      }
      for (const auto& breed : profile.restricted_breeds) {
        prefs["restricted_breeds"].append(breed);
      }

      Json::StreamWriterBuilder writer;
      std::string prefs_json = Json::writeString(writer, prefs);

      // Format children_ages as PostgreSQL array
      std::string children_ages_arr = "{";
      for (size_t i = 0; i < profile.children_ages.size(); ++i) {
        if (i > 0) children_ages_arr += ",";
        children_ages_arr += std::to_string(profile.children_ages[i]);
      }
      children_ages_arr += "}";

      // Format preferred_size as PostgreSQL array
      std::string preferred_size_arr = "{";
      for (size_t i = 0; i < profile.preferred_size.size(); ++i) {
        if (i > 0) preferred_size_arr += ",";
        preferred_size_arr += "\"" + profile.preferred_size[i] + "\"";
      }
      preferred_size_arr += "}";

      // Format preferred_age as PostgreSQL array
      std::string preferred_age_arr = "{";
      for (size_t i = 0; i < profile.preferred_age.size(); ++i) {
        if (i > 0) preferred_age_arr += ",";
        preferred_age_arr += "\"" + profile.preferred_age[i] + "\"";
      }
      preferred_age_arr += "}";

      // Format restricted_breeds as PostgreSQL array
      std::string restricted_breeds_arr = "{";
      for (size_t i = 0; i < profile.restricted_breeds.size(); ++i) {
        if (i > 0) restricted_breeds_arr += ",";
        restricted_breeds_arr += "\"" + profile.restricted_breeds[i] + "\"";
      }
      restricted_breeds_arr += "}";

      // INSERT profile into database
      txn.exec_params(
        R"(
          INSERT INTO lifestyle_profiles (
            id, user_id, home_type, has_yard, has_fence, has_children,
            children_ages, has_other_dogs, other_dogs_count, has_cats,
            activity_level, hours_home_daily, work_from_home,
            travel_frequency, dog_experience, willing_to_train,
            preferred_size, preferred_age, preferred_energy,
            ok_with_special_needs, has_breed_restrictions,
            restricted_breeds, weight_limit_lbs, max_adoption_fee,
            created_at
          ) VALUES (
            $1, $2, $3, $4, $5, $6,
            $7, $8, $9, $10,
            $11, $12, $13,
            $14, $15, $16,
            $17, $18, $19,
            $20, $21,
            $22, $23, $24,
            NOW()
          )
        )",
        profile_id,
        user_id,
        profile.home_type,
        profile.has_yard,
        profile.has_fence,
        profile.has_children,
        children_ages_arr,
        profile.has_other_dogs,
        profile.other_dogs_count,
        profile.has_cats,
        profile.activity_level,
        profile.hours_home_daily,
        profile.work_from_home,
        profile.travel_frequency,
        profile.dog_experience,
        profile.willing_to_train,
        preferred_size_arr,
        preferred_age_arr,
        profile.preferred_energy,
        profile.ok_with_special_needs,
        profile.has_breed_restrictions,
        restricted_breeds_arr,
        profile.weight_limit_lbs,
        profile.max_adoption_fee
      );

      txn.commit();
      ::wtl::core::db::ConnectionPool::getInstance().release(conn);
      return profile_id;

    } catch (const std::exception& e) {
      ::wtl::core::db::ConnectionPool::getInstance().release(conn);
      WTL_CAPTURE_ERROR(
        ::wtl::core::debug::ErrorCategory::DATABASE,
        "Failed to save profile: " + std::string(e.what()),
        {{"user_id", user_id}, {"operation", "saveProfile"}}
      );
      return "";
    }

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::DATABASE,
      "Failed to acquire connection for saveProfile: " + std::string(e.what()),
      {{"user_id", user_id}}
    );
    return "";
  }
}

std::vector<MatchResult> MatchingService::calculateMatches(
    const std::string& profile_id,
    int limit) {
  std::vector<MatchResult> results;

  try {
    // Get database connection
    auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

    try {
      pqxx::work txn(*conn);

      // Retrieve the profile from database
      pqxx::result profile_result = txn.exec_params(
        "SELECT * FROM lifestyle_profiles WHERE id = $1",
        profile_id
      );

      if (profile_result.empty()) {
        txn.commit();
        ::wtl::core::db::ConnectionPool::getInstance().release(conn);
        WTL_CAPTURE_ERROR(
          ::wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
          "Profile not found: " + profile_id,
          {{"profile_id", profile_id}}
        );
        return results;
      }

      // Reconstruct profile from database row
      const auto& row = profile_result[0];
      LifestyleProfile profile;
      profile.home_type = row["home_type"].is_null() ? "" : row["home_type"].as<std::string>();
      profile.has_yard = !row["has_yard"].is_null() && row["has_yard"].as<bool>();
      profile.has_fence = !row["has_fence"].is_null() && row["has_fence"].as<bool>();
      profile.has_children = !row["has_children"].is_null() && row["has_children"].as<bool>();
      profile.has_other_dogs = !row["has_other_dogs"].is_null() && row["has_other_dogs"].as<bool>();
      profile.other_dogs_count = row["other_dogs_count"].is_null() ? 0 : row["other_dogs_count"].as<int>();
      profile.has_cats = !row["has_cats"].is_null() && row["has_cats"].as<bool>();
      profile.activity_level = row["activity_level"].is_null() ? "" : row["activity_level"].as<std::string>();
      profile.hours_home_daily = row["hours_home_daily"].is_null() ? 0 : row["hours_home_daily"].as<int>();
      profile.work_from_home = !row["work_from_home"].is_null() && row["work_from_home"].as<bool>();
      profile.travel_frequency = row["travel_frequency"].is_null() ? "" : row["travel_frequency"].as<std::string>();
      profile.dog_experience = row["dog_experience"].is_null() ? "" : row["dog_experience"].as<std::string>();
      profile.willing_to_train = !row["willing_to_train"].is_null() && row["willing_to_train"].as<bool>();
      profile.preferred_energy = row["preferred_energy"].is_null() ? "" : row["preferred_energy"].as<std::string>();
      profile.ok_with_special_needs = !row["ok_with_special_needs"].is_null() && row["ok_with_special_needs"].as<bool>();
      profile.has_breed_restrictions = !row["has_breed_restrictions"].is_null() && row["has_breed_restrictions"].as<bool>();
      profile.weight_limit_lbs = row["weight_limit_lbs"].is_null() ? 0 : row["weight_limit_lbs"].as<int>();
      profile.max_adoption_fee = row["max_adoption_fee"].is_null() ? 0 : row["max_adoption_fee"].as<int>();

      // Parse PostgreSQL arrays for preferred_size, preferred_age, restricted_breeds
      // pqxx returns arrays as strings like {val1,val2,val3}
      auto parseTextArray = [](const std::string& arr_str) -> std::vector<std::string> {
        std::vector<std::string> result;
        if (arr_str.size() < 2 || arr_str.front() != '{' || arr_str.back() != '}') {
          return result;
        }
        std::string inner = arr_str.substr(1, arr_str.size() - 2);
        if (inner.empty()) return result;

        std::stringstream ss(inner);
        std::string item;
        while (std::getline(ss, item, ',')) {
          // Remove surrounding quotes if present
          if (item.size() >= 2 && item.front() == '"' && item.back() == '"') {
            item = item.substr(1, item.size() - 2);
          }
          if (!item.empty()) {
            result.push_back(item);
          }
        }
        return result;
      };

      if (!row["preferred_size"].is_null()) {
        profile.preferred_size = parseTextArray(row["preferred_size"].as<std::string>());
      }
      if (!row["preferred_age"].is_null()) {
        profile.preferred_age = parseTextArray(row["preferred_age"].as<std::string>());
      }
      if (!row["restricted_breeds"].is_null()) {
        profile.restricted_breeds = parseTextArray(row["restricted_breeds"].as<std::string>());
      }

      // Parse children_ages array (integer array)
      if (!row["children_ages"].is_null()) {
        std::string ages_str = row["children_ages"].as<std::string>();
        if (ages_str.size() >= 2 && ages_str.front() == '{' && ages_str.back() == '}') {
          std::string inner = ages_str.substr(1, ages_str.size() - 2);
          if (!inner.empty()) {
            std::stringstream ss(inner);
            std::string item;
            while (std::getline(ss, item, ',')) {
              try {
                profile.children_ages.push_back(std::stoi(item));
              } catch (...) {
                // Skip invalid entries
              }
            }
          }
        }
      }

      // Fetch all adoptable dogs
      pqxx::result dogs_result = txn.exec(
        "SELECT * FROM dogs WHERE status = 'available' ORDER BY id ASC"
      );

      // Score each dog and collect results
      for (const auto& dog_row : dogs_result) {
        Dog dog = Dog::fromDbRow(dog_row);
        float score = calculateMatchScore(profile, dog);

        MatchResult match;
        match.dog = dog;
        match.score = score;
        match.score_breakdown = Json::Value(Json::objectValue);

        results.push_back(match);
      }

      // Sort by score descending
      std::sort(results.begin(), results.end(),
                [](const MatchResult& a, const MatchResult& b) {
                  return a.score > b.score;
                });

      // Limit results
      if (results.size() > static_cast<size_t>(limit)) {
        results.resize(limit);
      }

      // Persist match results to database
      // First delete old scores for this profile
      txn.exec_params(
        "DELETE FROM match_scores WHERE profile_id = $1",
        profile_id
      );

      // Insert new scores
      for (const auto& match : results) {
        Json::StreamWriterBuilder writer;
        std::string breakdown_json = Json::writeString(writer, match.score_breakdown);

        txn.exec_params(
          R"(
            INSERT INTO match_scores (profile_id, dog_id, score, score_breakdown, calculated_at)
            VALUES ($1, $2, $3, $4::jsonb, NOW())
            ON CONFLICT (profile_id, dog_id) DO UPDATE
            SET score = EXCLUDED.score,
                score_breakdown = EXCLUDED.score_breakdown,
                calculated_at = NOW()
          )",
          profile_id,
          match.dog.id,
          static_cast<double>(match.score),
          breakdown_json
        );
      }

      txn.commit();
      ::wtl::core::db::ConnectionPool::getInstance().release(conn);
      return results;

    } catch (const std::exception& e) {
      ::wtl::core::db::ConnectionPool::getInstance().release(conn);
      WTL_CAPTURE_ERROR(
        ::wtl::core::debug::ErrorCategory::DATABASE,
        "Failed to calculate matches: " + std::string(e.what()),
        {{"profile_id", profile_id}, {"operation", "calculateMatches"}}
      );
      return results;
    }

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::DATABASE,
      "Failed to acquire connection for calculateMatches: " + std::string(e.what()),
      {{"profile_id", profile_id}}
    );
    return results;
  }
}

float MatchingService::calculateMatchScore(const LifestyleProfile& profile,
                                          const Dog& dog) {
  try {
    // Check hard disqualifiers first
    if (isDisqualified_(profile, dog)) {
      return 0.0f;
    }

    // Start with base score
    float score = 100.0f;

    // Apply penalties and bonuses
    score -= scoreCompatibility_(profile, dog);
    score -= scoreLivingSituation_(profile, dog);
    score -= scoreEnergy_(profile, dog);
    score -= scoreExperience_(profile, dog);
    score += scorePreferences_(profile, dog);

    // Clamp to 0-100
    score = std::max(0.0f, std::min(100.0f, score));

    return score;

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
      "Failed to calculate match score: " + std::string(e.what()),
      {{"dog_id", dog.id}}
    );
    return 0.0f;
  }
}

std::vector<MatchResult> MatchingService::getMatchResults(
    const std::string& profile_id) {
  std::vector<MatchResult> results;

  try {
    auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

    try {
      pqxx::work txn(*conn);

      pqxx::result db_result = txn.exec_params(
        R"(
          SELECT d.*, m.score
          FROM match_scores m
          JOIN dogs d ON m.dog_id = d.id
          WHERE m.profile_id = $1
          ORDER BY m.score DESC
        )",
        profile_id
      );

      txn.commit();
      ::wtl::core::db::ConnectionPool::getInstance().release(conn);

      for (const auto& row : db_result) {
        MatchResult match;
        match.dog = Dog::fromDbRow(row);
        // The score column is the last column in the SELECT
        match.score = row["score"].is_null() ? 0.0f : row["score"].as<float>();
        match.score_breakdown = Json::Value(Json::objectValue);

        results.push_back(match);
      }

      return results;

    } catch (const std::exception& e) {
      ::wtl::core::db::ConnectionPool::getInstance().release(conn);
      WTL_CAPTURE_ERROR(
        ::wtl::core::debug::ErrorCategory::DATABASE,
        "Failed to get match results: " + std::string(e.what()),
        {{"profile_id", profile_id}, {"operation", "getMatchResults"}}
      );
      return results;
    }

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::DATABASE,
      "Failed to acquire connection for getMatchResults: " + std::string(e.what()),
      {{"profile_id", profile_id}}
    );
    return results;
  }
}

/**
 * ============================================================================
 * PRIVATE SCORING METHODS
 * ============================================================================
 */

bool MatchingService::isDisqualified_(const LifestyleProfile& profile,
                                      const Dog& dog) {
  try {
    // Hard disqualifier: breed restrictions
    if (profile.has_breed_restrictions) {
      for (const auto& restricted : profile.restricted_breeds) {
        if (dog.breed_primary == restricted) {
          return true;
        }
        // Check secondary breed if present
        if (dog.breed_secondary.has_value() &&
            dog.breed_secondary.value().find(restricted) != std::string::npos) {
          return true;
        }
      }
    }

    // Hard disqualifier: weight limit (only check if both values are available)
    if (profile.weight_limit_lbs > 0 && dog.weight_lbs.has_value()) {
      if (dog.weight_lbs.value() > static_cast<double>(profile.weight_limit_lbs)) {
        return true;
      }
    }

    return false;

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
      "Exception in isDisqualified_: " + std::string(e.what()),
      {{"dog_id", dog.id}}
    );
    return true;  // Default to disqualified on error
  }
}

float MatchingService::scoreCompatibility_(const LifestyleProfile& profile,
                                          const Dog& dog) {
  float penalty = 0.0f;

  try {
    // Children compatibility: -50 if has_children and not good_with_kids
    // good_with_kids is a nullable bool; treat null as unknown (no penalty)
    if (profile.has_children && dog.good_with_kids.has_value() && !dog.good_with_kids.value()) {
      penalty += 50.0f;
    }

    // Dog compatibility: -40 if has_other_dogs and not good_with_dogs
    if (profile.has_other_dogs && dog.good_with_dogs.has_value() && !dog.good_with_dogs.value()) {
      penalty += 40.0f;
    }

    // Cat compatibility: -40 if has_cats and not good_with_cats
    if (profile.has_cats && dog.good_with_cats.has_value() && !dog.good_with_cats.value()) {
      penalty += 40.0f;
    }

    return penalty;

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
      "Exception in scoreCompatibility_: " + std::string(e.what()),
      {{"dog_id", dog.id}}
    );
    return 0.0f;
  }
}

float MatchingService::scoreLivingSituation_(const LifestyleProfile& profile,
                                            const Dog& dog) {
  float penalty = 0.0f;

  try {
    // Apartment: -30 if apartment and dog is large or xlarge size
    // The Dog model does not have an apartment_suitable field, so we
    // approximate by checking size (large/xlarge dogs are harder in apartments)
    if (profile.home_type == "apartment") {
      if (dog.size == "large" || dog.size == "xlarge") {
        penalty += 30.0f;
      }
    }

    // Yard: -20 if no yard and dog has high energy level
    if (!profile.has_yard) {
      if (dog.energy_level == "high") {
        penalty += 20.0f;
      }
    }

    return penalty;

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
      "Exception in scoreLivingSituation_: " + std::string(e.what()),
      {{"dog_id", dog.id}}
    );
    return 0.0f;
  }
}

float MatchingService::scoreEnergy_(const LifestyleProfile& profile,
                                   const Dog& dog) {
  float penalty = 0.0f;

  try {
    // Convert activity level and energy to numeric scale for comparison
    // Note: DB energy_level uses "low", "medium", "high"
    // Profile activity_level uses "low", "moderate", "high", "very_high"
    static const std::map<std::string, int> energy_scale = {
        {"low", 0},
        {"medium", 1},
        {"moderate", 1},
        {"high", 2},
        {"very_high", 3}
    };

    auto it_profile = energy_scale.find(profile.activity_level);
    auto it_dog = energy_scale.find(dog.energy_level);

    if (it_profile != energy_scale.end() && it_dog != energy_scale.end()) {
      int diff = std::abs(it_profile->second - it_dog->second);
      penalty += diff * 10.0f;  // -10 per level difference
    }

    return penalty;

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
      "Exception in scoreEnergy_: " + std::string(e.what()),
      {{"dog_id", dog.id}}
    );
    return 0.0f;
  }
}

float MatchingService::scoreExperience_(const LifestyleProfile& profile,
                                       const Dog& dog) {
  float penalty = 0.0f;

  try {
    // Experience: -25 if no experience and dog has special needs
    if (profile.dog_experience == "none" && dog.has_special_needs) {
      penalty += 25.0f;
    }

    return penalty;

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
      "Exception in scoreExperience_: " + std::string(e.what()),
      {{"dog_id", dog.id}}
    );
    return 0.0f;
  }
}

float MatchingService::scorePreferences_(const LifestyleProfile& profile,
                                        const Dog& dog) {
  float bonus = 0.0f;

  try {
    // Size preference bonus: +5
    for (const auto& size : profile.preferred_size) {
      if (dog.size == size) {
        bonus += 5.0f;
        break;
      }
    }

    // Age preference bonus: +5
    for (const auto& age : profile.preferred_age) {
      if (dog.age_category == age) {
        bonus += 5.0f;
        break;
      }
    }

    return bonus;

  } catch (const std::exception& e) {
    WTL_CAPTURE_ERROR(
      ::wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
      "Exception in scorePreferences_: " + std::string(e.what()),
      {{"dog_id", dog.id}}
    );
    return 0.0f;
  }
}

}  // namespace services
}  // namespace core
}  // namespace wtl
