/**
 * @file FosterMatcher.cc
 * @brief Implementation of FosterMatcher
 * @see FosterMatcher.h for documentation
 */

#include "core/services/FosterMatcher.h"

#include "core/services/FosterService.h"
#include "core/services/DogService.h"
#include "core/services/ShelterService.h"
#include "core/debug/ErrorCapture.h"

#include <algorithm>
#include <cmath>

namespace wtl::core::services {

// ============================================================================
// CONSTANTS
// ============================================================================

namespace {
    // Score weights (total base: 90 points + up to 10 urgency bonus)
    constexpr int SIZE_WEIGHT = 15;
    constexpr int AGE_WEIGHT = 15;
    constexpr int SPECIAL_NEEDS_WEIGHT = 20;
    constexpr int LOCATION_WEIGHT = 15;
    constexpr int EXPERIENCE_WEIGHT = 15;
    constexpr int CAPACITY_WEIGHT = 10;
    constexpr int URGENCY_BONUS = 10;

    // Earth's radius in miles for distance calculations
    constexpr double EARTH_RADIUS_MILES = 3958.8;

    // Pi constant
    constexpr double PI = 3.14159265358979323846;
}

// ============================================================================
// MATCH SCORE JSON CONVERSION
// ============================================================================

Json::Value FosterMatcher::MatchScore::toJson() const {
    Json::Value json;

    json["foster_home_id"] = foster_home_id;
    json["dog_id"] = dog_id;
    json["score"] = score;
    json["recommendation"] = recommendation;

    Json::Value positive(Json::arrayValue);
    for (const auto& factor : positive_factors) {
        positive.append(factor);
    }
    json["positive_factors"] = positive;

    Json::Value negative(Json::arrayValue);
    for (const auto& factor : negative_factors) {
        negative.append(factor);
    }
    json["negative_factors"] = negative;

    return json;
}

// ============================================================================
// SINGLETON
// ============================================================================

FosterMatcher& FosterMatcher::getInstance() {
    static FosterMatcher instance;
    return instance;
}

// ============================================================================
// MATCHING OPERATIONS
// ============================================================================

std::vector<FosterMatcher::MatchScore> FosterMatcher::findMatchesForDog(
    const std::string& dog_id,
    int limit) {

    std::vector<MatchScore> matches;

    try {
        // Get the dog
        auto dog_opt = DogService::getInstance().findById(dog_id);
        if (!dog_opt.has_value()) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::VALIDATION,
                "Dog not found for matching",
                {{"dog_id", dog_id}}
            );
            return matches;
        }

        const auto& dog = dog_opt.value();

        // Get all active foster homes with capacity
        auto foster_homes = FosterService::getInstance().findActive();

        // Calculate scores for each foster home
        for (const auto& home : foster_homes) {
            // Skip homes without capacity
            if (!home.hasCapacity()) {
                continue;
            }

            auto score = calculateMatch(home, dog);

            // Only include matches with score > 0 (no dealbreakers)
            if (score.score > 0) {
                matches.push_back(score);
            }
        }

        // Sort by score descending
        std::sort(matches.begin(), matches.end(),
            [](const MatchScore& a, const MatchScore& b) {
                return a.score > b.score;
            });

        // Limit results
        if (static_cast<int>(matches.size()) > limit) {
            matches.resize(limit);
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::BUSINESS_LOGIC,
            "Failed to find matches for dog: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return matches;
}

std::vector<FosterMatcher::MatchScore> FosterMatcher::findDogsForFoster(
    const std::string& foster_home_id,
    int limit) {

    std::vector<MatchScore> matches;

    try {
        // Get the foster home
        auto home_opt = FosterService::getInstance().findById(foster_home_id);
        if (!home_opt.has_value()) {
            WTL_CAPTURE_ERROR(
                ErrorCategory::VALIDATION,
                "Foster home not found for matching",
                {{"foster_home_id", foster_home_id}}
            );
            return matches;
        }

        const auto& home = home_opt.value();

        // Check if home has capacity
        if (!home.hasCapacity()) {
            return matches;
        }

        // Get all available dogs
        // Note: In production, this would use a more targeted query
        auto dogs = DogService::getInstance().findAll(500, 0);

        // Calculate scores for each dog
        for (const auto& dog : dogs) {
            // Skip dogs not available for foster
            if (dog.status != "adoptable" || !dog.is_available) {
                continue;
            }

            auto score = calculateMatch(home, dog);

            // Only include matches with score > 0 (no dealbreakers)
            if (score.score > 0) {
                matches.push_back(score);
            }
        }

        // Sort by score descending
        std::sort(matches.begin(), matches.end(),
            [](const MatchScore& a, const MatchScore& b) {
                return a.score > b.score;
            });

        // Limit results
        if (static_cast<int>(matches.size()) > limit) {
            matches.resize(limit);
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::BUSINESS_LOGIC,
            "Failed to find dogs for foster: " + std::string(e.what()),
            {{"foster_home_id", foster_home_id}}
        );
    }

    return matches;
}

FosterMatcher::MatchScore FosterMatcher::calculateMatch(
    const models::FosterHome& home,
    const models::Dog& dog) {

    MatchScore result;
    result.foster_home_id = home.id;
    result.dog_id = dog.id;
    result.score = 0;

    // Check for dealbreakers first
    std::vector<std::string> compat_factors;
    if (!checkEnvironmentCompatibility(home, dog, compat_factors)) {
        result.score = 0;
        result.negative_factors = compat_factors;
        result.recommendation = "Not recommended - incompatible environment";
        return result;
    }

    // Calculate individual scores
    int size_score = scoreSizeCompatibility(home, dog);
    int age_score = scoreAgeCompatibility(home, dog);
    int special_needs_score = scoreSpecialNeedsCompatibility(home, dog);

    // Get shelter for location scoring
    int location_score = 0;
    auto shelter_opt = ShelterService::getInstance().findById(dog.shelter_id);
    if (shelter_opt.has_value()) {
        location_score = scoreLocationProximity(home, shelter_opt.value());
    }

    int experience_score = scoreExperience(home, dog);
    int capacity_score = scoreCapacity(home);
    int urgency_score = scoreUrgency(dog);

    // Calculate total score
    result.score = size_score + age_score + special_needs_score +
                   location_score + experience_score + capacity_score + urgency_score;

    // Collect positive factors
    if (size_score >= SIZE_WEIGHT * 0.8) {
        result.positive_factors.push_back("Preferred size match");
    }
    if (age_score >= AGE_WEIGHT * 0.8) {
        result.positive_factors.push_back("Age preference match");
    }
    if (special_needs_score >= SPECIAL_NEEDS_WEIGHT * 0.8) {
        result.positive_factors.push_back("Can handle special needs");
    }
    if (location_score >= LOCATION_WEIGHT * 0.8) {
        result.positive_factors.push_back("Nearby location");
    }
    if (experience_score >= EXPERIENCE_WEIGHT * 0.8) {
        result.positive_factors.push_back("Experienced foster");
    }
    if (capacity_score == CAPACITY_WEIGHT) {
        result.positive_factors.push_back("Full capacity available");
    }
    if (urgency_score > 0) {
        result.positive_factors.push_back("Urgent dog prioritized");
    }

    // Add environment compatibility factors
    for (const auto& factor : compat_factors) {
        result.positive_factors.push_back(factor);
    }

    // Collect negative factors (low scores)
    if (size_score < SIZE_WEIGHT * 0.5) {
        result.negative_factors.push_back("Size not in preference range");
    }
    if (age_score < AGE_WEIGHT * 0.5) {
        result.negative_factors.push_back("Age category not preferred");
    }
    if (location_score < LOCATION_WEIGHT * 0.5) {
        result.negative_factors.push_back("May be outside transport range");
    }
    if (experience_score < EXPERIENCE_WEIGHT * 0.3) {
        result.negative_factors.push_back("New foster - may need support");
    }

    // Generate recommendation
    result.recommendation = generateRecommendation(result.score);

    return result;
}

// ============================================================================
// SCORING FACTORS
// ============================================================================

int FosterMatcher::scoreSizeCompatibility(
    const models::FosterHome& home,
    const models::Dog& dog) {

    // If no size preference, full points
    if (home.preferred_sizes.empty()) {
        return SIZE_WEIGHT;
    }

    // Check if dog's size is in preferred sizes
    for (const auto& size : home.preferred_sizes) {
        if (size == dog.size) {
            return SIZE_WEIGHT;
        }
    }

    // Size not in preference - partial points
    return SIZE_WEIGHT / 3;
}

int FosterMatcher::scoreAgeCompatibility(
    const models::FosterHome& home,
    const models::Dog& dog) {

    bool is_puppy = dog.age_category == "puppy";
    bool is_senior = dog.age_category == "senior";

    // Check puppy compatibility
    if (is_puppy) {
        if (home.ok_with_puppies) {
            return AGE_WEIGHT;
        }
        return 0;  // Dealbreaker
    }

    // Check senior compatibility
    if (is_senior) {
        if (home.ok_with_seniors) {
            return AGE_WEIGHT;
        }
        return 0;  // Dealbreaker
    }

    // Adult or young - always compatible
    return AGE_WEIGHT;
}

int FosterMatcher::scoreSpecialNeedsCompatibility(
    const models::FosterHome& home,
    const models::Dog& dog) {

    int score = SPECIAL_NEEDS_WEIGHT;

    // Check for medical needs in tags
    bool has_medical = false;
    bool has_behavioral = false;

    for (const auto& tag : dog.tags) {
        if (tag == "medical_needs" || tag == "special_medical") {
            has_medical = true;
        }
        if (tag == "behavioral_issues" || tag == "needs_training") {
            has_behavioral = true;
        }
    }

    // Adjust score based on needs and willingness
    if (has_medical && !home.ok_with_medical) {
        score -= SPECIAL_NEEDS_WEIGHT / 2;
    } else if (has_medical && home.ok_with_medical) {
        score += 5;  // Bonus for willing medical foster
    }

    if (has_behavioral && !home.ok_with_behavioral) {
        score -= SPECIAL_NEEDS_WEIGHT / 2;
    } else if (has_behavioral && home.ok_with_behavioral) {
        score += 5;  // Bonus for willing behavioral foster
    }

    // Cap score at max
    return std::min(score, SPECIAL_NEEDS_WEIGHT);
}

int FosterMatcher::scoreLocationProximity(
    const models::FosterHome& home,
    const models::Shelter& shelter) {

    // If no coordinates, use ZIP code prefix matching
    if (!home.latitude.has_value() || !home.longitude.has_value() ||
        !shelter.latitude.has_value() || !shelter.longitude.has_value()) {

        // Simple ZIP prefix match
        if (home.zip_code.substr(0, 3) == shelter.zip_code.substr(0, 3)) {
            return LOCATION_WEIGHT;
        }
        return LOCATION_WEIGHT / 2;
    }

    // Calculate actual distance
    double distance = calculateDistance(
        home.latitude.value(), home.longitude.value(),
        shelter.latitude.value(), shelter.longitude.value()
    );

    // Score based on distance vs. max transport
    if (distance <= home.max_transport_miles * 0.25) {
        return LOCATION_WEIGHT;  // Very close
    } else if (distance <= home.max_transport_miles * 0.5) {
        return LOCATION_WEIGHT * 0.8;
    } else if (distance <= home.max_transport_miles * 0.75) {
        return LOCATION_WEIGHT * 0.6;
    } else if (distance <= home.max_transport_miles) {
        return LOCATION_WEIGHT * 0.4;
    }

    // Beyond transport range - still some points if close
    return LOCATION_WEIGHT * 0.2;
}

int FosterMatcher::scoreExperience(
    const models::FosterHome& home,
    const models::Dog& dog) {

    int fosters = home.fosters_completed;

    // Experienced fosters get higher scores
    if (fosters >= 10) {
        return EXPERIENCE_WEIGHT;  // Very experienced
    } else if (fosters >= 5) {
        return EXPERIENCE_WEIGHT * 0.8;
    } else if (fosters >= 2) {
        return EXPERIENCE_WEIGHT * 0.6;
    } else if (fosters >= 1) {
        return EXPERIENCE_WEIGHT * 0.4;
    }

    // New foster - base points
    // Challenging dogs (medical/behavioral) reduce score for new fosters
    bool is_challenging = false;
    for (const auto& tag : dog.tags) {
        if (tag == "medical_needs" || tag == "behavioral_issues") {
            is_challenging = true;
            break;
        }
    }

    if (is_challenging) {
        return EXPERIENCE_WEIGHT * 0.2;  // May want experienced foster
    }

    return EXPERIENCE_WEIGHT * 0.3;
}

int FosterMatcher::scoreCapacity(const models::FosterHome& home) {
    int available = home.max_dogs - home.current_dogs;

    if (available >= 2) {
        return CAPACITY_WEIGHT;  // Plenty of room
    } else if (available == 1) {
        return CAPACITY_WEIGHT * 0.8;
    }

    // No capacity - shouldn't happen if we're filtering
    return 0;
}

int FosterMatcher::scoreUrgency(const models::Dog& dog) {
    // Boost urgent dogs to prioritize matching
    if (dog.urgency_level == "critical") {
        return URGENCY_BONUS;
    } else if (dog.urgency_level == "high") {
        return URGENCY_BONUS * 0.7;
    } else if (dog.urgency_level == "medium") {
        return URGENCY_BONUS * 0.4;
    }

    // Normal urgency - no bonus
    return 0;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

double FosterMatcher::calculateDistance(
    double lat1, double lon1,
    double lat2, double lon2) {

    // Convert to radians
    double lat1_rad = lat1 * PI / 180.0;
    double lon1_rad = lon1 * PI / 180.0;
    double lat2_rad = lat2 * PI / 180.0;
    double lon2_rad = lon2 * PI / 180.0;

    // Haversine formula
    double dlat = lat2_rad - lat1_rad;
    double dlon = lon2_rad - lon1_rad;

    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1_rad) * std::cos(lat2_rad) *
               std::sin(dlon / 2) * std::sin(dlon / 2);

    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return EARTH_RADIUS_MILES * c;
}

std::string FosterMatcher::generateRecommendation(int score) {
    if (score >= 85) {
        return "Excellent match - highly recommended";
    } else if (score >= 70) {
        return "Good match - recommended";
    } else if (score >= 55) {
        return "Moderate match - consider with discussion";
    } else if (score >= 40) {
        return "Possible match - some concerns to address";
    } else if (score > 0) {
        return "Low compatibility - may not be ideal";
    }

    return "Not recommended";
}

bool FosterMatcher::checkEnvironmentCompatibility(
    const models::FosterHome& home,
    const models::Dog& dog,
    std::vector<std::string>& factors) {

    bool compatible = true;

    // Check dog's compatibility with other animals
    for (const auto& tag : dog.tags) {
        // Check other dogs compatibility
        if (home.has_other_dogs && tag == "no_other_dogs") {
            factors.push_back("Dog cannot be with other dogs - home has dogs");
            compatible = false;
        }

        // Check cat compatibility
        if (home.has_cats && tag == "no_cats") {
            factors.push_back("Dog cannot be with cats - home has cats");
            compatible = false;
        }

        // Check children compatibility
        if (home.has_children && tag == "no_children") {
            factors.push_back("Dog not suitable with children - home has children");
            compatible = false;
        }

        // Positive factors
        if (home.has_other_dogs && tag == "good_with_dogs") {
            factors.push_back("Dog is good with other dogs");
        }
        if (home.has_cats && tag == "good_with_cats") {
            factors.push_back("Dog is good with cats");
        }
        if (home.has_children && tag == "good_with_kids") {
            factors.push_back("Dog is good with children");
        }
    }

    // Check if dog needs yard
    bool needs_yard = false;
    for (const auto& tag : dog.tags) {
        if (tag == "needs_yard" || tag == "high_energy") {
            needs_yard = true;
            break;
        }
    }

    if (needs_yard && !home.has_yard) {
        factors.push_back("Dog may need yard - home doesn't have one");
        // Not a dealbreaker, but noted
    } else if (home.has_yard) {
        factors.push_back("Home has yard available");
    }

    return compatible;
}

} // namespace wtl::core::services
