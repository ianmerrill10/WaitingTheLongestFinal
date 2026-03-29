/**
 * @file UrgencyCalculator.cc
 * @brief Implementation of UrgencyCalculator service
 * @see UrgencyCalculator.h for documentation
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/UrgencyCalculator.h"

// Standard library includes
#include <algorithm>
#include <ctime>

// Project includes - these will be provided by other agents
// #include "core/models/Dog.h"
// #include "core/models/Shelter.h"
// #include "core/services/DogService.h"
// #include "core/services/ShelterService.h"
// #include "core/db/ConnectionPool.h"
// #include "core/debug/ErrorCapture.h"

namespace wtl::core::services {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

UrgencyCalculator::UrgencyCalculator() {
    // Initialize high-risk breeds list
    // These breeds face higher euthanasia rates due to prejudice, BSL, or housing restrictions
    high_risk_breeds_ = {
        "pit bull",
        "pit bull terrier",
        "american pit bull terrier",
        "american staffordshire terrier",
        "staffordshire bull terrier",
        "pit mix",
        "bully breed",
        "american bulldog",
        "rottweiler",
        "doberman",
        "doberman pinscher",
        "german shepherd",
        "chow chow",
        "akita",
        "wolf hybrid",
        "wolfdog",
        "presa canario",
        "cane corso",
        "dogo argentino",
        "fila brasileiro",
        "mastiff"  // Large breeds often face housing restrictions
    };
}

UrgencyCalculator& UrgencyCalculator::getInstance() {
    static UrgencyCalculator instance;
    return instance;
}

// ============================================================================
// SINGLE DOG CALCULATION
// ============================================================================

UrgencyLevel UrgencyCalculator::calculate(const models::Dog& dog,
                                           const models::Shelter& shelter) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Update statistics
    total_calculations_++;

    // If dog has a known euthanasia date, that's the primary factor
    if (dog.euthanasia_date.has_value()) {
        int days = calculateDaysRemaining(dog.euthanasia_date.value());
        UrgencyLevel level = calculateUrgencyFromDays(days);

        // Update level counters
        switch (level) {
            case UrgencyLevel::CRITICAL:
                critical_count_++;
                break;
            case UrgencyLevel::HIGH:
                high_count_++;
                break;
            case UrgencyLevel::MEDIUM:
                medium_count_++;
                break;
            default:
                normal_count_++;
                break;
        }

        return level;
    }

    // If on euthanasia list but no specific date, treat as HIGH
    if (dog.is_on_euthanasia_list) {
        high_count_++;
        return UrgencyLevel::HIGH;
    }

    // If in a kill shelter, assess based on risk factors
    if (shelter.is_kill_shelter) {
        int risk_score = calculateRiskScore(dog, shelter);

        // High risk score in kill shelter = elevated urgency
        if (risk_score >= 70) {
            high_count_++;
            return UrgencyLevel::HIGH;
        } else if (risk_score >= 50) {
            medium_count_++;
            return UrgencyLevel::MEDIUM;
        }
    }

    // Default to NORMAL
    normal_count_++;
    return UrgencyLevel::NORMAL;
}

UrgencyResult UrgencyCalculator::calculateDetailed(const models::Dog& dog,
                                                    const models::Shelter& shelter) {
    std::lock_guard<std::mutex> lock(mutex_);

    UrgencyResult result;
    result.shelter_name = shelter.name;
    result.is_kill_shelter = shelter.is_kill_shelter;
    result.on_euthanasia_list = dog.is_on_euthanasia_list;

    // Collect all risk factors first
    result.risk_factors = collectRiskFactors(dog, shelter);
    result.risk_score = calculateRiskScore(dog, shelter);

    // Calculate time remaining if euthanasia date is known
    if (dog.euthanasia_date.has_value()) {
        result.euthanasia_date = dog.euthanasia_date;
        result.days_remaining = calculateDaysRemaining(dog.euthanasia_date.value());
        result.hours_remaining = calculateHoursRemaining(dog.euthanasia_date.value());

        // Determine level based on days remaining
        result.level = calculateUrgencyFromDays(result.days_remaining.value());
    } else if (dog.is_on_euthanasia_list) {
        // On list but no specific date
        result.level = UrgencyLevel::HIGH;
        result.risk_factors.push_back("On euthanasia list without specific date");
    } else if (shelter.is_kill_shelter) {
        // Estimate based on shelter's average hold days
        if (shelter.avg_hold_days > 0) {
            // Calculate days since intake
            auto now = std::chrono::system_clock::now();
            auto intake_duration = now - dog.intake_date;
            int days_in_shelter = std::chrono::duration_cast<std::chrono::hours>(
                intake_duration).count() / 24;

            int estimated_days_remaining = shelter.avg_hold_days - days_in_shelter;

            if (estimated_days_remaining <= 0) {
                result.level = UrgencyLevel::HIGH;
                result.days_remaining = 0;
                result.risk_factors.push_back("Exceeded average hold period for this shelter");
            } else if (estimated_days_remaining <= 3) {
                result.level = UrgencyLevel::HIGH;
                result.days_remaining = estimated_days_remaining;
            } else if (estimated_days_remaining <= 7) {
                result.level = UrgencyLevel::MEDIUM;
                result.days_remaining = estimated_days_remaining;
            } else {
                result.level = UrgencyLevel::NORMAL;
                result.days_remaining = estimated_days_remaining;
            }
        } else {
            // No avg_hold_days info, use risk score
            if (result.risk_score >= 70) {
                result.level = UrgencyLevel::HIGH;
            } else if (result.risk_score >= 50) {
                result.level = UrgencyLevel::MEDIUM;
            } else {
                result.level = UrgencyLevel::NORMAL;
            }
        }
    } else {
        // Not a kill shelter, normal urgency
        result.level = UrgencyLevel::NORMAL;
    }

    // Generate recommendation
    result.recommendation = generateRecommendation(
        result.level, result.risk_factors, shelter.is_kill_shelter);

    // Update statistics
    total_calculations_++;
    switch (result.level) {
        case UrgencyLevel::CRITICAL:
            critical_count_++;
            break;
        case UrgencyLevel::HIGH:
            high_count_++;
            break;
        case UrgencyLevel::MEDIUM:
            medium_count_++;
            break;
        default:
            normal_count_++;
            break;
    }

    return result;
}

UrgencyLevel UrgencyCalculator::calculateFromDate(
    const std::chrono::system_clock::time_point& euthanasia_date) {

    int days = calculateDaysRemaining(euthanasia_date);
    return calculateUrgencyFromDays(days);
}

// ============================================================================
// BATCH CALCULATION
// ============================================================================

int UrgencyCalculator::recalculateAll() {
    // This will be implemented with actual database access
    // For now, return 0 as placeholder

    // TODO: Implementation when DogService and ShelterService are available
    // auto& dog_service = DogService::getInstance();
    // auto& shelter_service = ShelterService::getInstance();
    //
    // auto dogs = dog_service.findAll();
    // int changed_count = 0;
    //
    // for (const auto& dog : dogs) {
    //     auto shelter = shelter_service.findById(dog.shelter_id);
    //     if (shelter) {
    //         UrgencyLevel new_level = calculate(dog, *shelter);
    //         UrgencyLevel old_level = stringToUrgency(dog.urgency_level);
    //         if (new_level != old_level) {
    //             updateDogUrgency(dog.id, new_level);
    //             changed_count++;
    //         }
    //     }
    // }
    //
    // return changed_count;

    return 0;
}

int UrgencyCalculator::recalculateForShelter(const std::string& shelter_id) {
    // TODO: Implementation when DogService and ShelterService are available
    // auto& dog_service = DogService::getInstance();
    // auto& shelter_service = ShelterService::getInstance();
    //
    // auto shelter = shelter_service.findById(shelter_id);
    // if (!shelter) return 0;
    //
    // auto dogs = dog_service.findByShelterId(shelter_id);
    // int changed_count = 0;
    //
    // for (const auto& dog : dogs) {
    //     UrgencyLevel new_level = calculate(dog, *shelter);
    //     UrgencyLevel old_level = stringToUrgency(dog.urgency_level);
    //     if (new_level != old_level) {
    //         updateDogUrgency(dog.id, new_level);
    //         changed_count++;
    //     }
    // }
    //
    // return changed_count;

    (void)shelter_id;  // Suppress unused parameter warning
    return 0;
}

int UrgencyCalculator::recalculateBatch(const std::vector<std::string>& dog_ids) {
    // TODO: Implementation when services are available
    (void)dog_ids;
    return 0;
}

// ============================================================================
// RISK ASSESSMENT
// ============================================================================

bool UrgencyCalculator::isHighRiskBreed(const std::string& breed) {
    // Convert to lowercase for comparison
    std::string lower_breed;
    lower_breed.reserve(breed.size());
    for (char c : breed) {
        lower_breed.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))));
    }

    // Check if breed contains any high-risk breed keywords
    for (const auto& risk_breed : high_risk_breeds_) {
        if (lower_breed.find(risk_breed) != std::string::npos) {
            return true;
        }
    }

    // Also check for "black" as black dogs face discrimination
    if (lower_breed.find("black") != std::string::npos) {
        return true;
    }

    return false;
}

bool UrgencyCalculator::isHighRiskAge(int age_months) {
    // Senior dogs (7+ years) face higher euthanasia rates
    if (age_months >= SENIOR_AGE_MONTHS) {
        return true;
    }

    // Very young puppies without mothers also at risk
    if (age_months <= 2) {
        return true;
    }

    return false;
}

bool UrgencyCalculator::isHighRiskSize(const std::string& size) {
    // Convert to lowercase
    std::string lower_size;
    lower_size.reserve(size.size());
    for (char c : size) {
        lower_size.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))));
    }

    // Large and extra-large dogs face housing restrictions and longer stays
    return (lower_size == "large" || lower_size == "xlarge" ||
            lower_size == "extra-large" || lower_size == "xl");
}

int UrgencyCalculator::calculateRiskScore(const models::Dog& dog,
                                           const models::Shelter& shelter) {
    int score = 0;

    // Kill shelter base risk
    if (shelter.is_kill_shelter) {
        score += KILL_SHELTER_WEIGHT;
    }

    // On euthanasia list is highest risk factor
    if (dog.is_on_euthanasia_list) {
        score += EUTHANASIA_LIST_WEIGHT;
    }

    // Breed risk
    if (isHighRiskBreed(dog.breed_primary)) {
        score += HIGH_RISK_BREED_WEIGHT;
    }
    if (dog.breed_secondary.has_value() && isHighRiskBreed(dog.breed_secondary.value())) {
        score += HIGH_RISK_BREED_WEIGHT / 2;  // Secondary breed less weight
    }

    // Age risk
    if (isHighRiskAge(dog.age_months)) {
        score += SENIOR_AGE_WEIGHT;
    }

    // Size risk
    if (isHighRiskSize(dog.size)) {
        score += LARGE_SIZE_WEIGHT;
    }

    // Long stay risk (over 30 days increases euthanasia likelihood)
    auto now = std::chrono::system_clock::now();
    auto duration = now - dog.intake_date;
    int days_in_shelter = std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;

    if (days_in_shelter > 30) {
        score += LONG_STAY_WEIGHT;
    }
    if (days_in_shelter > 60) {
        score += LONG_STAY_WEIGHT;  // Additional weight for very long stays
    }
    if (days_in_shelter > 90) {
        score += LONG_STAY_WEIGHT;  // Even more for 90+ days
    }

    // Cap at 100
    return std::min(score, 100);
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value UrgencyCalculator::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    stats["total_calculations"] = total_calculations_;
    stats["critical_count"] = critical_count_;
    stats["high_count"] = high_count_;
    stats["medium_count"] = medium_count_;
    stats["normal_count"] = normal_count_;

    // Calculate percentages if we have data
    if (total_calculations_ > 0) {
        stats["critical_percentage"] =
            (critical_count_ * 100.0) / total_calculations_;
        stats["high_percentage"] =
            (high_count_ * 100.0) / total_calculations_;
        stats["medium_percentage"] =
            (medium_count_ * 100.0) / total_calculations_;
        stats["normal_percentage"] =
            (normal_count_ * 100.0) / total_calculations_;
    }

    return stats;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

int UrgencyCalculator::calculateDaysRemaining(
    const std::chrono::system_clock::time_point& target_date) {

    auto now = std::chrono::system_clock::now();
    auto duration = target_date - now;
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();

    // Round up to be conservative (partial day = full day)
    return static_cast<int>((hours + 23) / 24);
}

int UrgencyCalculator::calculateHoursRemaining(
    const std::chrono::system_clock::time_point& target_date) {

    auto now = std::chrono::system_clock::now();
    auto duration = target_date - now;
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::hours>(duration).count());
}

std::string UrgencyCalculator::generateRecommendation(
    UrgencyLevel level,
    const std::vector<std::string>& risk_factors,
    bool is_kill_shelter) {

    switch (level) {
        case UrgencyLevel::CRITICAL:
            return "EMERGENCY: This dog has less than 24 hours. Immediate action required. "
                   "Contact shelter NOW for rescue pull or emergency foster. "
                   "Share on all social media platforms immediately.";

        case UrgencyLevel::HIGH:
            if (is_kill_shelter) {
                return "URGENT: This dog is at high risk and needs help within 72 hours. "
                       "Priority foster placement recommended. Contact rescue coordinators. "
                       "Share widely to find foster or adopter.";
            } else {
                return "HIGH PRIORITY: This dog has multiple risk factors. "
                       "Increased promotion and foster outreach recommended.";
            }

        case UrgencyLevel::MEDIUM:
            if (!risk_factors.empty()) {
                return "MODERATE PRIORITY: This dog has some risk factors. "
                       "Enhanced visibility and foster matching recommended. "
                       "Monitor for changes in shelter status.";
            } else {
                return "MODERATE PRIORITY: Regular promotion schedule. "
                       "Include in foster outreach campaigns.";
            }

        case UrgencyLevel::NORMAL:
        default:
            if (is_kill_shelter) {
                return "Standard priority but in kill shelter. "
                       "Regular monitoring recommended. Include in promotion rotation.";
            } else {
                return "Standard priority. Include in regular promotion schedule. "
                       "Continue to monitor wait time.";
            }
    }
}

std::vector<std::string> UrgencyCalculator::collectRiskFactors(
    const models::Dog& dog,
    const models::Shelter& shelter) {

    std::vector<std::string> factors;

    // Shelter factors
    if (shelter.is_kill_shelter) {
        factors.push_back("Located in kill shelter");
    }

    // Euthanasia status
    if (dog.is_on_euthanasia_list) {
        factors.push_back("Currently on euthanasia list");
    }

    if (dog.euthanasia_date.has_value()) {
        int days = calculateDaysRemaining(dog.euthanasia_date.value());
        if (days <= 0) {
            factors.push_back("Euthanasia scheduled within 24 hours");
        } else if (days <= 3) {
            factors.push_back("Euthanasia scheduled within " + std::to_string(days) + " days");
        }
    }

    // Breed factors
    if (isHighRiskBreed(dog.breed_primary)) {
        factors.push_back("High-risk breed: " + dog.breed_primary);
    }

    // Age factors
    if (dog.age_months >= SENIOR_AGE_MONTHS) {
        int years = dog.age_months / 12;
        factors.push_back("Senior dog (" + std::to_string(years) + " years)");
    } else if (dog.age_months <= 2) {
        factors.push_back("Very young puppy requiring special care");
    }

    // Size factors
    if (isHighRiskSize(dog.size)) {
        factors.push_back("Large size may limit housing options");
    }

    // Length of stay
    auto now = std::chrono::system_clock::now();
    auto duration = now - dog.intake_date;
    int days = std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;

    if (days > 90) {
        factors.push_back("Extended stay: " + std::to_string(days) + " days in shelter");
    } else if (days > 60) {
        factors.push_back("Long stay: " + std::to_string(days) + " days in shelter");
    } else if (days > 30) {
        factors.push_back("Over 30 days in shelter");
    }

    // Check for exceeded hold period
    if (shelter.is_kill_shelter && shelter.avg_hold_days > 0 && days > shelter.avg_hold_days) {
        factors.push_back("Exceeded shelter's average hold period");
    }

    return factors;
}

void UrgencyCalculator::updateDogUrgency(const std::string& dog_id,
                                          UrgencyLevel level) {
    // TODO: Implementation when DogService is available
    // auto& dog_service = DogService::getInstance();
    // auto dog = dog_service.findById(dog_id);
    // if (dog) {
    //     dog->urgency_level = urgencyToString(level);
    //     dog_service.update(*dog);
    //
    //     // Emit event for urgency change
    //     emitEvent(EventType::DOG_URGENCY_CHANGED, dog_id, {
    //         {"new_level", urgencyToString(level)},
    //         {"priority", getUrgencyPriority(level)}
    //     }, "urgency_calculator");
    // }

    (void)dog_id;
    (void)level;
}

} // namespace wtl::core::services
