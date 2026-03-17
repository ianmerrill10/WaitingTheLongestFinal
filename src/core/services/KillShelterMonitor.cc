/**
 * @file KillShelterMonitor.cc
 * @brief Implementation of KillShelterMonitor service
 * @see KillShelterMonitor.h for documentation
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/KillShelterMonitor.h"

// Standard library includes
#include <algorithm>

// Project includes
#include "core/services/UrgencyCalculator.h"

// These will be provided by other agents
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

KillShelterMonitor::KillShelterMonitor() {
    last_cache_update_ = std::chrono::system_clock::now() -
                         std::chrono::hours(24);  // Force initial refresh
}

KillShelterMonitor& KillShelterMonitor::getInstance() {
    static KillShelterMonitor instance;
    return instance;
}

// ============================================================================
// SHELTER QUERIES
// ============================================================================

std::vector<models::Shelter> KillShelterMonitor::getKillShelters() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Shelter> shelters;

    // TODO: Implementation when ShelterService is available
    // auto& shelter_service = ShelterService::getInstance();
    // shelters = shelter_service.findKillShelters();

    return shelters;
}

std::vector<models::Shelter> KillShelterMonitor::getKillSheltersByState(
    const std::string& state_code) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Shelter> shelters;

    // TODO: Implementation when services are available
    // auto& shelter_service = ShelterService::getInstance();
    // auto all_in_state = shelter_service.findByStateCode(state_code);
    // for (const auto& shelter : all_in_state) {
    //     if (shelter.is_kill_shelter) {
    //         shelters.push_back(shelter);
    //     }
    // }

    (void)state_code;
    return shelters;
}

std::vector<models::Shelter> KillShelterMonitor::getSheltersWithCriticalDogs() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Shelter> shelters;

    // Refresh cache if needed
    if (!isCacheValid()) {
        // Would call refreshStatuses() but avoiding recursion
        cache_valid_ = false;
    }

    // TODO: Implementation when services are available
    // for (const auto& status : cached_statuses_) {
    //     if (status.critical_count > 0) {
    //         auto& shelter_service = ShelterService::getInstance();
    //         auto shelter = shelter_service.findById(status.shelter_id);
    //         if (shelter) {
    //             shelters.push_back(*shelter);
    //         }
    //     }
    // }

    return shelters;
}

// ============================================================================
// DOG QUERIES - AT RISK
// ============================================================================

std::vector<models::Dog> KillShelterMonitor::getDogsAtRisk(int days_threshold) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Dog> at_risk_dogs;

    // TODO: Implementation when DogService is available
    // auto& dog_service = DogService::getInstance();
    // auto& shelter_service = ShelterService::getInstance();
    //
    // // Get all dogs in kill shelters
    // auto kill_shelters = shelter_service.findKillShelters();
    // for (const auto& shelter : kill_shelters) {
    //     auto dogs = dog_service.findByShelterId(shelter.id);
    //     for (const auto& dog : dogs) {
    //         if (!dog.is_available) continue;
    //
    //         // Check if on euthanasia list
    //         if (dog.is_on_euthanasia_list) {
    //             at_risk_dogs.push_back(dog);
    //             continue;
    //         }
    //
    //         // Check if euthanasia date is within threshold
    //         if (dog.euthanasia_date.has_value()) {
    //             auto& calculator = UrgencyCalculator::getInstance();
    //             int days_remaining = calculateDaysRemaining(dog.euthanasia_date.value());
    //             if (days_remaining <= days_threshold) {
    //                 at_risk_dogs.push_back(dog);
    //             }
    //         }
    //     }
    // }
    //
    // // Sort by urgency
    // sortByUrgency(at_risk_dogs);

    (void)days_threshold;
    return at_risk_dogs;
}

std::vector<models::Dog> KillShelterMonitor::getCriticalDogs() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Dog> critical_dogs;

    // TODO: Implementation when DogService is available
    // auto& dog_service = DogService::getInstance();
    //
    // // Query dogs with urgency_level = 'critical'
    // // Or euthanasia_date within 24 hours
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     auto result = txn.exec(
    //         "SELECT * FROM dogs WHERE "
    //         "urgency_level = 'critical' OR "
    //         "(euthanasia_date IS NOT NULL AND "
    //         " euthanasia_date <= NOW() + INTERVAL '24 hours') "
    //         "ORDER BY euthanasia_date ASC NULLS LAST"
    //     );
    //
    //     for (const auto& row : result) {
    //         critical_dogs.push_back(Dog::fromDbRow(row));
    //     }
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "getCriticalDogs"}});
    // }
    // ConnectionPool::getInstance().release(conn);

    return critical_dogs;
}

std::vector<models::Dog> KillShelterMonitor::getHighUrgencyDogs() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Dog> high_urgency_dogs;

    // TODO: Implementation when DogService is available
    // Query dogs with urgency_level = 'high'
    // Or euthanasia_date between 24-72 hours

    return high_urgency_dogs;
}

std::vector<models::Dog> KillShelterMonitor::getDogsOnEuthanasiaList() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Dog> dogs;

    // TODO: Implementation when DogService is available
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     auto result = txn.exec(
    //         "SELECT * FROM dogs WHERE is_on_euthanasia_list = true "
    //         "ORDER BY euthanasia_date ASC NULLS LAST"
    //     );
    //
    //     for (const auto& row : result) {
    //         dogs.push_back(Dog::fromDbRow(row));
    //     }
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "getDogsOnEuthanasiaList"}});
    // }
    // ConnectionPool::getInstance().release(conn);

    return dogs;
}

std::vector<models::Dog> KillShelterMonitor::getDogsAtRiskByShelter(
    const std::string& shelter_id,
    int days_threshold) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Dog> at_risk_dogs;

    // TODO: Implementation when DogService is available
    (void)shelter_id;
    (void)days_threshold;

    return at_risk_dogs;
}

std::vector<models::Dog> KillShelterMonitor::getDogsAtRiskByState(
    const std::string& state_code,
    int days_threshold) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Dog> at_risk_dogs;

    // TODO: Implementation when DogService is available
    (void)state_code;
    (void)days_threshold;

    return at_risk_dogs;
}

// ============================================================================
// SHELTER STATUS
// ============================================================================

std::vector<ShelterUrgencyStatus> KillShelterMonitor::getShelterStatuses() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check cache validity
    if (!isCacheValid()) {
        // Refresh is needed - but we need to release lock first
        // to avoid deadlock with refreshStatuses()
        cache_valid_ = false;
    }

    if (!cache_valid_) {
        // TODO: Refresh when services are available
        // This would normally call refreshStatuses() in a lock-safe way
    }

    return cached_statuses_;
}

std::optional<ShelterUrgencyStatus> KillShelterMonitor::getShelterStatus(
    const std::string& shelter_id) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Check cached statuses
    for (const auto& status : cached_statuses_) {
        if (status.shelter_id == shelter_id) {
            return status;
        }
    }

    // TODO: If not in cache, calculate fresh
    // auto& shelter_service = ShelterService::getInstance();
    // auto shelter = shelter_service.findById(shelter_id);
    // if (shelter) {
    //     return calculateShelterStatus(*shelter);
    // }

    return std::nullopt;
}

std::vector<ShelterUrgencyStatus> KillShelterMonitor::getMostUrgentShelters(int limit) {
    auto statuses = getShelterStatuses();

    // Sort by urgency score (highest first)
    std::sort(statuses.begin(), statuses.end(),
        [](const ShelterUrgencyStatus& a, const ShelterUrgencyStatus& b) {
            return a.getUrgencyScore() > b.getUrgencyScore();
        });

    // Limit results
    if (static_cast<int>(statuses.size()) > limit) {
        statuses.resize(limit);
    }

    return statuses;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value KillShelterMonitor::getUrgencyStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;

    // Shelter counts
    stats["total_kill_shelters"] = total_kill_shelters_;

    // Dog counts by urgency
    Json::Value urgency;
    urgency["critical"] = total_critical_;
    urgency["high"] = total_high_;
    urgency["medium"] = total_medium_;
    urgency["on_euthanasia_list"] = total_on_list_;
    stats["urgency_counts"] = urgency;

    // State breakdown
    Json::Value by_state(Json::objectValue);
    // TODO: Populate state statistics when services available
    stats["by_state"] = by_state;

    // Top urgent shelters
    Json::Value top_shelters(Json::arrayValue);
    auto urgent_shelters = getMostUrgentShelters(5);
    for (const auto& shelter : urgent_shelters) {
        top_shelters.append(shelter.toJson());
    }
    stats["top_urgent_shelters"] = top_shelters;

    // Timestamp
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    stats["generated_at"] = static_cast<Json::Int64>(now);

    return stats;
}

Json::Value KillShelterMonitor::getStateStatistics(const std::string& state_code) {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    stats["state_code"] = state_code;

    // TODO: Implementation when services available
    // Count shelters and dogs by urgency in this state

    return stats;
}

int KillShelterMonitor::getTotalKillShelters() {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_kill_shelters_;
}

int KillShelterMonitor::getTotalDogsAtRisk(int days_threshold) {
    // Get dogs at risk and count
    auto dogs = getDogsAtRisk(days_threshold);
    return static_cast<int>(dogs.size());
}

int KillShelterMonitor::getTotalCriticalDogs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_critical_;
}

int KillShelterMonitor::getTotalOnEuthanasiaList() {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_on_list_;
}

// ============================================================================
// REFRESH / UPDATE
// ============================================================================

void KillShelterMonitor::refreshStatuses() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Clear existing cache
    cached_statuses_.clear();
    total_critical_ = 0;
    total_high_ = 0;
    total_medium_ = 0;
    total_on_list_ = 0;

    // TODO: Implementation when services available
    // auto& shelter_service = ShelterService::getInstance();
    // auto kill_shelters = shelter_service.findKillShelters();
    //
    // total_kill_shelters_ = kill_shelters.size();
    //
    // for (const auto& shelter : kill_shelters) {
    //     auto status = calculateShelterStatus(shelter);
    //     cached_statuses_.push_back(status);
    //
    //     // Update totals
    //     total_critical_ += status.critical_count;
    //     total_high_ += status.high_count;
    //     total_medium_ += status.medium_count;
    //     total_on_list_ += status.on_euthanasia_list_count;
    // }
    //
    // // Sort by urgency score
    // std::sort(cached_statuses_.begin(), cached_statuses_.end(),
    //     [](const ShelterUrgencyStatus& a, const ShelterUrgencyStatus& b) {
    //         return a.getUrgencyScore() > b.getUrgencyScore();
    //     });

    last_cache_update_ = std::chrono::system_clock::now();
    cache_valid_ = true;
}

void KillShelterMonitor::refreshShelterStatus(const std::string& shelter_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: Implementation when services available
    // auto& shelter_service = ShelterService::getInstance();
    // auto shelter = shelter_service.findById(shelter_id);
    // if (!shelter) return;
    //
    // auto new_status = calculateShelterStatus(*shelter);
    //
    // // Update or add to cache
    // bool found = false;
    // for (auto& status : cached_statuses_) {
    //     if (status.shelter_id == shelter_id) {
    //         // Update totals
    //         total_critical_ -= status.critical_count;
    //         total_high_ -= status.high_count;
    //         total_medium_ -= status.medium_count;
    //         total_on_list_ -= status.on_euthanasia_list_count;
    //
    //         status = new_status;
    //
    //         total_critical_ += status.critical_count;
    //         total_high_ += status.high_count;
    //         total_medium_ += status.medium_count;
    //         total_on_list_ += status.on_euthanasia_list_count;
    //
    //         found = true;
    //         break;
    //     }
    // }
    //
    // if (!found && shelter->is_kill_shelter) {
    //     cached_statuses_.push_back(new_status);
    //     total_critical_ += new_status.critical_count;
    //     total_high_ += new_status.high_count;
    //     total_medium_ += new_status.medium_count;
    //     total_on_list_ += new_status.on_euthanasia_list_count;
    // }

    (void)shelter_id;
}

void KillShelterMonitor::invalidateCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_valid_ = false;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

ShelterUrgencyStatus KillShelterMonitor::calculateShelterStatus(
    const models::Shelter& shelter) {

    ShelterUrgencyStatus status;
    status.shelter_id = shelter.id;
    status.shelter_name = shelter.name;
    status.state_code = shelter.state_code;
    status.city = shelter.city;
    status.is_kill_shelter = shelter.is_kill_shelter;
    status.avg_hold_days = shelter.avg_hold_days;
    status.total_dogs = shelter.dog_count;
    status.available_dogs = shelter.available_count;

    // Initialize counts
    status.critical_count = 0;
    status.high_count = 0;
    status.medium_count = 0;
    status.normal_count = 0;
    status.on_euthanasia_list_count = 0;

    // TODO: Query dogs for this shelter when DogService is available
    // auto& dog_service = DogService::getInstance();
    // auto dogs = dog_service.findByShelterId(shelter.id);
    //
    // std::optional<std::chrono::system_clock::time_point> earliest_euthanasia;
    // std::string earliest_dog_id;
    // std::string earliest_dog_name;
    //
    // for (const auto& dog : dogs) {
    //     if (!dog.is_available) continue;
    //
    //     // Count by urgency level
    //     UrgencyLevel level = stringToUrgency(dog.urgency_level);
    //     switch (level) {
    //         case UrgencyLevel::CRITICAL:
    //             status.critical_count++;
    //             break;
    //         case UrgencyLevel::HIGH:
    //             status.high_count++;
    //             break;
    //         case UrgencyLevel::MEDIUM:
    //             status.medium_count++;
    //             break;
    //         default:
    //             status.normal_count++;
    //             break;
    //     }
    //
    //     // Count euthanasia list
    //     if (dog.is_on_euthanasia_list) {
    //         status.on_euthanasia_list_count++;
    //     }
    //
    //     // Track earliest euthanasia
    //     if (dog.euthanasia_date.has_value()) {
    //         if (!earliest_euthanasia.has_value() ||
    //             dog.euthanasia_date.value() < earliest_euthanasia.value()) {
    //             earliest_euthanasia = dog.euthanasia_date;
    //             earliest_dog_id = dog.id;
    //             earliest_dog_name = dog.name;
    //         }
    //     }
    // }
    //
    // if (earliest_euthanasia.has_value()) {
    //     status.next_euthanasia = earliest_euthanasia;
    //     status.next_euthanasia_dog_id = earliest_dog_id;
    //     status.next_euthanasia_dog_name = earliest_dog_name;
    // }

    status.last_updated = std::chrono::system_clock::now();

    return status;
}

void KillShelterMonitor::sortByUrgency(std::vector<models::Dog>& dogs) {
    std::sort(dogs.begin(), dogs.end(),
        [](const models::Dog& a, const models::Dog& b) {
            // First by urgency level
            UrgencyLevel level_a = stringToUrgency(a.urgency_level);
            UrgencyLevel level_b = stringToUrgency(b.urgency_level);
            int priority_a = getUrgencyPriority(level_a);
            int priority_b = getUrgencyPriority(level_b);

            if (priority_a != priority_b) {
                return priority_a > priority_b;  // Higher priority first
            }

            // Then by euthanasia date (earlier first)
            if (a.euthanasia_date.has_value() && b.euthanasia_date.has_value()) {
                return a.euthanasia_date.value() < b.euthanasia_date.value();
            }

            // Dogs with dates before dogs without
            if (a.euthanasia_date.has_value()) return true;
            if (b.euthanasia_date.has_value()) return false;

            // Finally by intake date (longer stay first)
            return a.intake_date < b.intake_date;
        });
}

bool KillShelterMonitor::isCacheValid() const {
    if (!cache_valid_) return false;

    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::minutes>(
        now - last_cache_update_);

    return age < cache_ttl_;
}

} // namespace wtl::core::services
