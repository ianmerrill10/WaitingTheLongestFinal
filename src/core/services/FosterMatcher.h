/**
 * @file FosterMatcher.h
 * @brief Intelligent matching service for dogs and foster homes
 *
 * PURPOSE:
 * Provides intelligent matching between dogs needing foster care and
 * available foster homes. Uses scoring algorithms to rank matches based
 * on compatibility factors like size, age, special needs, location, and
 * foster experience.
 *
 * USAGE:
 * auto& matcher = FosterMatcher::getInstance();
 * auto matches = matcher.findMatchesForDog("dog-uuid");
 * auto score = matcher.calculateMatch(fosterHome, dog);
 *
 * DEPENDENCIES:
 * - FosterService (foster home data)
 * - DogService (dog data)
 * - ShelterService (shelter location data)
 * - FosterHome, Dog, Shelter models
 *
 * MODIFICATION GUIDE:
 * - To add new scoring factors, add a new private scoring method
 * - Update calculateMatch() to include the new factor
 * - Adjust weights in the scoring section as needed
 *
 * @author Agent 9 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>
#include <vector>

// Project includes
#include "core/models/FosterHome.h"
#include "core/models/Dog.h"
#include "core/models/Shelter.h"

namespace wtl::core::services {

/**
 * @class FosterMatcher
 * @brief Singleton service for matching dogs with compatible foster homes
 *
 * Uses a weighted scoring algorithm to find optimal matches between
 * dogs and foster homes. Considers multiple compatibility factors
 * and provides detailed explanations of match quality.
 */
class FosterMatcher {
public:
    // ========================================================================
    // MATCH RESULT STRUCTURE
    // ========================================================================

    /**
     * @struct MatchScore
     * @brief Represents a match between a dog and foster home with scoring details
     */
    struct MatchScore {
        std::string foster_home_id;              ///< Foster home UUID
        std::string dog_id;                      ///< Dog UUID
        int score;                               ///< Overall score (0-100)
        std::vector<std::string> positive_factors;   ///< Reasons for good match
        std::vector<std::string> negative_factors;   ///< Potential concerns
        std::string recommendation;              ///< Overall recommendation text

        /**
         * @brief Convert match score to JSON
         * @return Json::Value representation
         */
        Json::Value toJson() const;
    };

    // ========================================================================
    // SINGLETON
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the FosterMatcher singleton
     */
    static FosterMatcher& getInstance();

    // Delete copy/move constructors and assignment operators
    FosterMatcher(const FosterMatcher&) = delete;
    FosterMatcher& operator=(const FosterMatcher&) = delete;
    FosterMatcher(FosterMatcher&&) = delete;
    FosterMatcher& operator=(FosterMatcher&&) = delete;

    // ========================================================================
    // MATCHING OPERATIONS
    // ========================================================================

    /**
     * @brief Find compatible foster homes for a specific dog
     * @param dog_id The dog's UUID
     * @param limit Maximum matches to return (default 10)
     * @return Vector of MatchScore sorted by score descending
     *
     * Searches all active foster homes with available capacity and
     * scores them based on compatibility with the specified dog.
     */
    std::vector<MatchScore> findMatchesForDog(const std::string& dog_id,
                                               int limit = 10);

    /**
     * @brief Find compatible dogs for a specific foster home
     * @param foster_home_id The foster home's UUID
     * @param limit Maximum matches to return (default 10)
     * @return Vector of MatchScore sorted by score descending
     *
     * Searches all available dogs and scores them based on
     * compatibility with the foster home's preferences and environment.
     */
    std::vector<MatchScore> findDogsForFoster(const std::string& foster_home_id,
                                               int limit = 10);

    /**
     * @brief Calculate match score between a foster home and dog
     * @param home The foster home
     * @param dog The dog
     * @return MatchScore with detailed scoring breakdown
     *
     * Evaluates compatibility across multiple factors:
     * - Size compatibility
     * - Age compatibility
     * - Special needs handling
     * - Location proximity
     * - Foster experience
     * - Available capacity
     * - Dog urgency level
     */
    MatchScore calculateMatch(const models::FosterHome& home,
                              const models::Dog& dog);

private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR
    // ========================================================================

    FosterMatcher() = default;
    ~FosterMatcher() = default;

    // ========================================================================
    // SCORING FACTORS
    // ========================================================================

    /**
     * @brief Score size compatibility (0-15 points)
     * @param home The foster home
     * @param dog The dog
     * @return Score based on size preference match
     */
    int scoreSizeCompatibility(const models::FosterHome& home,
                               const models::Dog& dog);

    /**
     * @brief Score age compatibility (0-15 points)
     * @param home The foster home
     * @param dog The dog
     * @return Score based on age category preference
     */
    int scoreAgeCompatibility(const models::FosterHome& home,
                              const models::Dog& dog);

    /**
     * @brief Score special needs compatibility (0-20 points)
     * @param home The foster home
     * @param dog The dog
     * @return Score based on medical/behavioral needs handling
     */
    int scoreSpecialNeedsCompatibility(const models::FosterHome& home,
                                        const models::Dog& dog);

    /**
     * @brief Score location proximity (0-15 points)
     * @param home The foster home
     * @param shelter The dog's current shelter
     * @return Score based on distance within transport range
     */
    int scoreLocationProximity(const models::FosterHome& home,
                               const models::Shelter& shelter);

    /**
     * @brief Score foster experience (0-15 points)
     * @param home The foster home
     * @param dog The dog
     * @return Score based on experience with similar dogs
     */
    int scoreExperience(const models::FosterHome& home,
                        const models::Dog& dog);

    /**
     * @brief Score available capacity (0-10 points)
     * @param home The foster home
     * @return Score based on available slots
     */
    int scoreCapacity(const models::FosterHome& home);

    /**
     * @brief Score dog urgency level (0-10 points bonus)
     * @param dog The dog
     * @return Bonus points for urgent dogs
     *
     * Critical and high urgency dogs receive bonus points to
     * prioritize their matching.
     */
    int scoreUrgency(const models::Dog& dog);

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Calculate distance between two geographic points
     * @param lat1 Latitude of point 1
     * @param lon1 Longitude of point 1
     * @param lat2 Latitude of point 2
     * @param lon2 Longitude of point 2
     * @return Distance in miles
     */
    double calculateDistance(double lat1, double lon1, double lat2, double lon2);

    /**
     * @brief Generate recommendation text based on score
     * @param score The overall score
     * @return Recommendation string
     */
    std::string generateRecommendation(int score);

    /**
     * @brief Check if home environment is compatible with dog
     * @param home The foster home
     * @param dog The dog
     * @param factors Output vector for compatibility factors
     * @return true if compatible, false if dealbreaker found
     */
    bool checkEnvironmentCompatibility(const models::FosterHome& home,
                                        const models::Dog& dog,
                                        std::vector<std::string>& factors);
};

} // namespace wtl::core::services
