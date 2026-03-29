/**
 * @file UrgencyCalculator.h
 * @brief Service for calculating dog urgency levels based on shelter and dog data
 *
 * PURPOSE:
 * Calculates and manages urgency levels for dogs in shelters, particularly kill shelters.
 * This is the brain of the urgency engine - it determines which dogs need immediate help.
 * Every calculation could be the difference between a dog being saved or not.
 *
 * URGENCY FACTORS:
 * - Time until scheduled euthanasia (primary factor)
 * - Shelter type (kill shelter vs rescue)
 * - Dog characteristics (breed, age, size affect adoption likelihood)
 * - Shelter capacity and intake rate
 * - Historical data on similar dogs
 *
 * USAGE:
 * auto& calculator = UrgencyCalculator::getInstance();
 * UrgencyLevel level = calculator.calculate(dog, shelter);
 * auto result = calculator.calculateDetailed(dog, shelter);
 *
 * DEPENDENCIES:
 * - Dog model (Agent 3)
 * - Shelter model (Agent 3)
 * - DogService, ShelterService (Agent 3)
 * - ConnectionPool (Agent 1)
 * - ErrorCapture (Agent 1)
 *
 * MODIFICATION GUIDE:
 * - To add new risk factors: Update calculateRiskScore() and RiskFactors struct
 * - To change urgency thresholds: Update calculateFromDaysRemaining()
 * - All changes should be logged for audit purposes
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "core/services/UrgencyLevel.h"
#include "core/models/Dog.h"
#include "core/models/Shelter.h"

namespace wtl::core::services {

/**
 * @struct UrgencyResult
 * @brief Detailed result of urgency calculation with all contributing factors
 *
 * Provides comprehensive information about why a dog has a particular urgency level,
 * enabling informed decision-making by rescue coordinators and shelter staff.
 */
struct UrgencyResult {
    /// The calculated urgency level
    UrgencyLevel level;

    /// Days remaining until euthanasia (if known)
    std::optional<int> days_remaining;

    /// Hours remaining (more precise for critical cases)
    std::optional<int> hours_remaining;

    /// Scheduled euthanasia date/time (if known)
    std::optional<std::chrono::system_clock::time_point> euthanasia_date;

    /// Risk factors contributing to urgency
    std::vector<std::string> risk_factors;

    /// Calculated risk score (0-100, higher = more at risk)
    int risk_score;

    /// Actionable recommendation for this dog
    std::string recommendation;

    /// Whether this dog is on an active euthanasia list
    bool on_euthanasia_list;

    /// Shelter information
    std::string shelter_name;
    bool is_kill_shelter;

    /**
     * @brief Convert result to JSON for API responses
     */
    Json::Value toJson() const {
        Json::Value json;
        json["level"] = urgencyToString(level);
        json["priority"] = getUrgencyPriority(level);
        json["color"] = getUrgencyColor(level);

        if (days_remaining.has_value()) {
            json["days_remaining"] = days_remaining.value();
        }
        if (hours_remaining.has_value()) {
            json["hours_remaining"] = hours_remaining.value();
        }
        if (euthanasia_date.has_value()) {
            auto time_t_val = std::chrono::system_clock::to_time_t(euthanasia_date.value());
            json["euthanasia_date"] = static_cast<Json::Int64>(time_t_val);
        }

        Json::Value factors_json(Json::arrayValue);
        for (const auto& factor : risk_factors) {
            factors_json.append(factor);
        }
        json["risk_factors"] = factors_json;

        json["risk_score"] = risk_score;
        json["recommendation"] = recommendation;
        json["on_euthanasia_list"] = on_euthanasia_list;
        json["shelter_name"] = shelter_name;
        json["is_kill_shelter"] = is_kill_shelter;

        return json;
    }
};

/**
 * @class UrgencyCalculator
 * @brief Singleton service for calculating dog urgency levels
 *
 * Thread-safe singleton that handles all urgency calculations. Uses multiple
 * factors to determine how urgently a dog needs rescue, with special attention
 * to dogs in kill shelters with scheduled euthanasia dates.
 */
class UrgencyCalculator {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to the UrgencyCalculator instance
     */
    static UrgencyCalculator& getInstance();

    // Prevent copying and assignment
    UrgencyCalculator(const UrgencyCalculator&) = delete;
    UrgencyCalculator& operator=(const UrgencyCalculator&) = delete;

    // ========================================================================
    // SINGLE DOG CALCULATION
    // ========================================================================

    /**
     * @brief Calculate urgency level for a single dog
     *
     * Quick calculation that returns just the urgency level.
     * Use calculateDetailed() if you need full risk analysis.
     *
     * @param dog The dog to evaluate
     * @param shelter The shelter where the dog is located
     * @return UrgencyLevel The calculated urgency level
     *
     * @example
     * auto level = calculator.calculate(dog, shelter);
     * if (isCritical(level)) {
     *     alertService.triggerCriticalAlert(dog);
     * }
     */
    UrgencyLevel calculate(const models::Dog& dog, const models::Shelter& shelter);

    /**
     * @brief Calculate urgency with full detailed analysis
     *
     * Performs comprehensive risk assessment including all contributing factors,
     * recommendations, and time calculations.
     *
     * @param dog The dog to evaluate
     * @param shelter The shelter where the dog is located
     * @return UrgencyResult Complete urgency analysis
     */
    UrgencyResult calculateDetailed(const models::Dog& dog, const models::Shelter& shelter);

    /**
     * @brief Calculate urgency from just the euthanasia date
     *
     * Simplified calculation when only the date is known.
     *
     * @param euthanasia_date The scheduled euthanasia date
     * @return UrgencyLevel The calculated urgency level
     */
    UrgencyLevel calculateFromDate(
        const std::chrono::system_clock::time_point& euthanasia_date);

    // ========================================================================
    // BATCH CALCULATION
    // ========================================================================

    /**
     * @brief Recalculate urgency levels for all dogs
     *
     * Called periodically by UrgencyWorker to ensure all urgency levels
     * are up-to-date. Updates are batched for performance.
     *
     * @return int Number of dogs whose urgency level changed
     */
    int recalculateAll();

    /**
     * @brief Recalculate urgency for all dogs at a specific shelter
     *
     * Use when shelter information changes or after receiving new
     * euthanasia list data.
     *
     * @param shelter_id UUID of the shelter
     * @return int Number of dogs whose urgency level changed
     */
    int recalculateForShelter(const std::string& shelter_id);

    /**
     * @brief Recalculate urgency for a batch of dog IDs
     *
     * @param dog_ids Vector of dog UUIDs to recalculate
     * @return int Number of dogs whose urgency level changed
     */
    int recalculateBatch(const std::vector<std::string>& dog_ids);

    // ========================================================================
    // RISK ASSESSMENT
    // ========================================================================

    /**
     * @brief Check if a breed is considered high-risk for euthanasia
     *
     * Certain breeds (pit bulls, black dogs, etc.) face higher euthanasia
     * rates due to prejudice and breed-specific legislation.
     *
     * @param breed The breed to check
     * @return bool True if breed is high-risk
     */
    bool isHighRiskBreed(const std::string& breed);

    /**
     * @brief Check if age puts dog at higher risk
     *
     * Senior dogs (7+ years) and very young puppies face higher risk.
     *
     * @param age_months Dog's age in months
     * @return bool True if age is a risk factor
     */
    bool isHighRiskAge(int age_months);

    /**
     * @brief Check if size affects adoption likelihood
     *
     * Large and extra-large dogs face longer wait times and higher
     * euthanasia rates in many shelters.
     *
     * @param size Size category: "small", "medium", "large", "xlarge"
     * @return bool True if size is a risk factor
     */
    bool isHighRiskSize(const std::string& size);

    /**
     * @brief Calculate overall risk score for a dog
     *
     * Combines all risk factors into a single score (0-100).
     * Higher scores indicate dogs more likely to be euthanized.
     *
     * @param dog The dog to evaluate
     * @param shelter The shelter where the dog is located
     * @return int Risk score (0-100)
     */
    int calculateRiskScore(const models::Dog& dog, const models::Shelter& shelter);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get urgency calculation statistics
     *
     * @return Json::Value Statistics including total calculations, distribution, etc.
     */
    Json::Value getStatistics() const;

private:
    // Private constructor for singleton
    UrgencyCalculator();
    ~UrgencyCalculator() = default;

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================

    /**
     * @brief Calculate days remaining until a given date
     */
    int calculateDaysRemaining(const std::chrono::system_clock::time_point& target_date);

    /**
     * @brief Calculate hours remaining until a given date
     */
    int calculateHoursRemaining(const std::chrono::system_clock::time_point& target_date);

    /**
     * @brief Generate recommendation string based on urgency and factors
     */
    std::string generateRecommendation(UrgencyLevel level,
                                        const std::vector<std::string>& risk_factors,
                                        bool is_kill_shelter);

    /**
     * @brief Collect all risk factors for a dog
     */
    std::vector<std::string> collectRiskFactors(const models::Dog& dog,
                                                 const models::Shelter& shelter);

    /**
     * @brief Update dog record with new urgency level
     */
    void updateDogUrgency(const std::string& dog_id, UrgencyLevel level);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    /// Mutex for thread-safe operations
    mutable std::mutex mutex_;

    /// Statistics tracking
    mutable int total_calculations_ = 0;
    mutable int critical_count_ = 0;
    mutable int high_count_ = 0;
    mutable int medium_count_ = 0;
    mutable int normal_count_ = 0;

    /// High-risk breed list (loaded on initialization)
    std::vector<std::string> high_risk_breeds_;

    /// Age thresholds (in months)
    static constexpr int SENIOR_AGE_MONTHS = 84;  // 7 years
    static constexpr int PUPPY_AGE_MONTHS = 6;

    /// Risk score weights
    static constexpr int KILL_SHELTER_WEIGHT = 20;
    static constexpr int EUTHANASIA_LIST_WEIGHT = 30;
    static constexpr int HIGH_RISK_BREED_WEIGHT = 15;
    static constexpr int SENIOR_AGE_WEIGHT = 15;
    static constexpr int LARGE_SIZE_WEIGHT = 10;
    static constexpr int LONG_STAY_WEIGHT = 10;  // > 30 days
};

} // namespace wtl::core::services
