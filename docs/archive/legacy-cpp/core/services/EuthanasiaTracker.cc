/**
 * @file EuthanasiaTracker.cc
 * @brief Implementation of EuthanasiaTracker service
 * @see EuthanasiaTracker.h for documentation
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/EuthanasiaTracker.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>

// Project includes
#include "core/services/UrgencyCalculator.h"

// These will be provided by other agents
// #include "core/models/Dog.h"
// #include "core/services/DogService.h"
// #include "core/db/ConnectionPool.h"
// #include "core/debug/ErrorCapture.h"
// #include "core/EventBus.h"

namespace wtl::core::services {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

EuthanasiaTracker::EuthanasiaTracker() {
    // Initialize statistics
}

EuthanasiaTracker& EuthanasiaTracker::getInstance() {
    static EuthanasiaTracker instance;
    return instance;
}

// ============================================================================
// DATE MANAGEMENT
// ============================================================================

void EuthanasiaTracker::setEuthanasiaDate(
    const std::string& dog_id,
    std::chrono::system_clock::time_point date,
    const std::string& reason,
    const std::string& actor) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Create event record
    EuthanasiaEvent event;
    event.id = generateEventId();
    event.dog_id = dog_id;
    event.action = "date_set";
    event.timestamp = std::chrono::system_clock::now();
    event.reason = reason;
    event.new_date = date;
    event.actor = actor;

    // TODO: Get shelter_id from dog record
    // auto& dog_service = DogService::getInstance();
    // auto dog = dog_service.findById(dog_id);
    // if (dog) {
    //     event.shelter_id = dog->shelter_id;
    // }

    // Update the dog record
    updateDogRecord(dog_id, date, true);

    // Log the event
    logEvent(event);

    // Calculate new urgency level and emit event
    auto countdown = calculateCountdown(date);
    emitUrgencyEvent(dog_id, countdown.urgency_level, "euthanasia_date_set");

    // Update statistics
    total_dates_set_++;
}

void EuthanasiaTracker::updateEuthanasiaDate(
    const std::string& dog_id,
    std::chrono::system_clock::time_point new_date,
    const std::string& reason,
    const std::string& actor) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Get current date for history
    auto current_date = getEuthanasiaDate(dog_id);

    // Create event record
    EuthanasiaEvent event;
    event.id = generateEventId();
    event.dog_id = dog_id;
    event.action = "date_changed";
    event.timestamp = std::chrono::system_clock::now();
    event.reason = reason;
    event.old_date = current_date;
    event.new_date = new_date;
    event.actor = actor;

    // Update the dog record
    updateDogRecord(dog_id, new_date, true);

    // Log the event
    logEvent(event);

    // Calculate new urgency level and emit event
    auto countdown = calculateCountdown(new_date);
    emitUrgencyEvent(dog_id, countdown.urgency_level, "euthanasia_date_changed");
}

void EuthanasiaTracker::clearEuthanasiaDate(
    const std::string& dog_id,
    const std::string& reason,
    const std::string& actor) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Get current date for history
    auto current_date = getEuthanasiaDate(dog_id);

    // Create event record
    EuthanasiaEvent event;
    event.id = generateEventId();
    event.dog_id = dog_id;
    event.action = "date_cleared";
    event.timestamp = std::chrono::system_clock::now();
    event.reason = reason;
    event.old_date = current_date;
    event.actor = actor;

    // Update the dog record (clear date and remove from list)
    updateDogRecord(dog_id, std::nullopt, false);

    // Log the event
    logEvent(event);

    // Emit event with NORMAL urgency (no longer at risk)
    emitUrgencyEvent(dog_id, UrgencyLevel::NORMAL, "euthanasia_date_cleared");

    // Update statistics
    total_dates_cleared_++;
}

// ============================================================================
// EUTHANASIA LIST MANAGEMENT
// ============================================================================

void EuthanasiaTracker::addToEuthanasiaList(
    const std::string& dog_id,
    std::optional<std::chrono::system_clock::time_point> scheduled,
    const std::string& reason,
    const std::string& actor) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Create event record
    EuthanasiaEvent event;
    event.id = generateEventId();
    event.dog_id = dog_id;
    event.action = "added";
    event.timestamp = std::chrono::system_clock::now();
    event.reason = reason;
    event.new_date = scheduled;
    event.actor = actor;

    // Update the dog record
    updateDogRecord(dog_id, scheduled, true);

    // Log the event
    logEvent(event);

    // Calculate urgency and emit event
    UrgencyLevel level = UrgencyLevel::HIGH;  // At least HIGH for being on list
    if (scheduled.has_value()) {
        auto countdown = calculateCountdown(scheduled.value());
        level = countdown.urgency_level;
    }
    emitUrgencyEvent(dog_id, level, "added_to_euthanasia_list");

    // Update statistics
    total_list_additions_++;
}

void EuthanasiaTracker::removeFromEuthanasiaList(
    const std::string& dog_id,
    const std::string& reason,
    const std::string& actor) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Get current date for history
    auto current_date = getEuthanasiaDate(dog_id);

    // Create event record
    EuthanasiaEvent event;
    event.id = generateEventId();
    event.dog_id = dog_id;
    event.action = "removed";
    event.timestamp = std::chrono::system_clock::now();
    event.reason = reason;
    event.old_date = current_date;
    event.actor = actor;

    // Update the dog record
    updateDogRecord(dog_id, std::nullopt, false);

    // Log the event
    logEvent(event);

    // Emit event with updated urgency
    emitUrgencyEvent(dog_id, UrgencyLevel::NORMAL, "removed_from_euthanasia_list");

    // Update statistics
    total_list_removals_++;
}

int EuthanasiaTracker::processEuthanasiaListUpdate(
    const std::string& shelter_id,
    const std::vector<std::string>& dog_ids,
    std::optional<std::chrono::system_clock::time_point> scheduled_date,
    const std::string& actor) {

    std::lock_guard<std::mutex> lock(mutex_);

    int changes = 0;

    // TODO: Implementation when DogService is available
    // Get current list for this shelter
    // auto current_list = getEuthanasiaList(shelter_id);
    //
    // // Find dogs to add (in new list but not in current)
    // for (const auto& dog_id : dog_ids) {
    //     bool found = false;
    //     for (const auto& dog : current_list) {
    //         if (dog.id == dog_id) {
    //             found = true;
    //             break;
    //         }
    //     }
    //     if (!found) {
    //         // Need to add - but unlock first to avoid deadlock
    //         // addToEuthanasiaList(dog_id, scheduled_date, "List sync", actor);
    //         changes++;
    //     }
    // }
    //
    // // Find dogs to remove (in current but not in new)
    // for (const auto& dog : current_list) {
    //     bool found = std::find(dog_ids.begin(), dog_ids.end(), dog.id) != dog_ids.end();
    //     if (!found) {
    //         // Need to remove
    //         // removeFromEuthanasiaList(dog.id, "Removed from shelter list", actor);
    //         changes++;
    //     }
    // }

    // Emit event for list update
    // emitEvent(EventType::EUTHANASIA_LIST_UPDATED, shelter_id, {
    //     {"dog_count", static_cast<int>(dog_ids.size())},
    //     {"changes", changes}
    // }, "euthanasia_tracker");

    (void)shelter_id;
    (void)dog_ids;
    (void)scheduled_date;
    (void)actor;

    return changes;
}

// ============================================================================
// QUERY METHODS
// ============================================================================

std::vector<models::Dog> EuthanasiaTracker::getEuthanasiaList(
    const std::string& shelter_id) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<models::Dog> dogs;

    // TODO: Implementation when DogService is available
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     auto result = txn.exec_params(
    //         "SELECT * FROM dogs WHERE shelter_id = $1 AND is_on_euthanasia_list = true "
    //         "ORDER BY euthanasia_date ASC NULLS LAST",
    //         shelter_id
    //     );
    //
    //     for (const auto& row : result) {
    //         dogs.push_back(Dog::fromDbRow(row));
    //     }
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "getEuthanasiaList"}, {"shelter_id", shelter_id}});
    // }
    // ConnectionPool::getInstance().release(conn);

    (void)shelter_id;
    return dogs;
}

std::optional<std::chrono::system_clock::time_point>
EuthanasiaTracker::getEuthanasiaDate(const std::string& dog_id) {

    // Note: Not locking here as this may be called from locked context
    // TODO: Implementation when DogService is available
    // auto& dog_service = DogService::getInstance();
    // auto dog = dog_service.findById(dog_id);
    // if (dog && dog->euthanasia_date.has_value()) {
    //     return dog->euthanasia_date;
    // }

    (void)dog_id;
    return std::nullopt;
}

bool EuthanasiaTracker::isOnEuthanasiaList(const std::string& dog_id) {
    // TODO: Implementation when DogService is available
    // auto& dog_service = DogService::getInstance();
    // auto dog = dog_service.findById(dog_id);
    // if (dog) {
    //     return dog->is_on_euthanasia_list;
    // }

    (void)dog_id;
    return false;
}

// ============================================================================
// COUNTDOWN
// ============================================================================

std::optional<EuthanasiaCountdown> EuthanasiaTracker::getCountdown(
    const std::string& dog_id) {

    auto euthanasia_date = getEuthanasiaDate(dog_id);
    if (!euthanasia_date.has_value()) {
        return std::nullopt;
    }

    return calculateCountdown(euthanasia_date.value());
}

EuthanasiaCountdown EuthanasiaTracker::calculateCountdown(
    const std::chrono::system_clock::time_point& euthanasia_date) {

    EuthanasiaCountdown countdown;

    auto now = std::chrono::system_clock::now();
    auto duration = euthanasia_date - now;

    // Handle past dates (shouldn't happen but be safe)
    if (duration.count() < 0) {
        countdown.days = 0;
        countdown.hours = 0;
        countdown.minutes = 0;
        countdown.seconds = 0;
        countdown.total_seconds = 0;
        countdown.is_critical = true;
        countdown.urgency_level = UrgencyLevel::CRITICAL;
        countdown.formatted = "00:00:00:00";
        return countdown;
    }

    // Calculate components
    auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    countdown.total_seconds = total_seconds;

    countdown.days = static_cast<int>(total_seconds / 86400);
    int remaining = static_cast<int>(total_seconds % 86400);
    countdown.hours = remaining / 3600;
    remaining = remaining % 3600;
    countdown.minutes = remaining / 60;
    countdown.seconds = remaining % 60;

    // Determine urgency level
    if (countdown.days == 0 && countdown.hours < 24) {
        countdown.is_critical = true;
        countdown.urgency_level = UrgencyLevel::CRITICAL;
    } else if (countdown.days <= 3) {
        countdown.is_critical = false;
        countdown.urgency_level = UrgencyLevel::HIGH;
    } else if (countdown.days <= 7) {
        countdown.is_critical = false;
        countdown.urgency_level = UrgencyLevel::MEDIUM;
    } else {
        countdown.is_critical = false;
        countdown.urgency_level = UrgencyLevel::NORMAL;
    }

    // Format string
    countdown.formatted = formatCountdown(
        countdown.days, countdown.hours, countdown.minutes, countdown.seconds);

    return countdown;
}

std::vector<std::pair<std::string, EuthanasiaCountdown>>
EuthanasiaTracker::getAllCountdowns() {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::pair<std::string, EuthanasiaCountdown>> result;

    // TODO: Implementation when DogService is available
    // Query all dogs with euthanasia dates
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     auto query_result = txn.exec(
    //         "SELECT id, euthanasia_date FROM dogs "
    //         "WHERE euthanasia_date IS NOT NULL "
    //         "ORDER BY euthanasia_date ASC"
    //     );
    //
    //     for (const auto& row : query_result) {
    //         std::string dog_id = row["id"].as<std::string>();
    //         // Parse timestamp and calculate countdown
    //         auto countdown = calculateCountdown(/* parsed date */);
    //         result.push_back({dog_id, countdown});
    //     }
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "getAllCountdowns"}});
    // }
    // ConnectionPool::getInstance().release(conn);

    return result;
}

// ============================================================================
// HISTORY
// ============================================================================

std::vector<EuthanasiaEvent> EuthanasiaTracker::getHistory(const std::string& dog_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<EuthanasiaEvent> events;

    // TODO: Implementation when database is available
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     auto result = txn.exec_params(
    //         "SELECT * FROM euthanasia_events WHERE dog_id = $1 "
    //         "ORDER BY timestamp DESC",
    //         dog_id
    //     );
    //
    //     for (const auto& row : result) {
    //         events.push_back(EuthanasiaEvent::fromDbRow(row));
    //     }
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "getHistory"}, {"dog_id", dog_id}});
    // }
    // ConnectionPool::getInstance().release(conn);

    (void)dog_id;
    return events;
}

std::vector<EuthanasiaEvent> EuthanasiaTracker::getShelterHistory(
    const std::string& shelter_id, int limit) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<EuthanasiaEvent> events;

    // TODO: Implementation when database is available
    (void)shelter_id;
    (void)limit;

    return events;
}

std::vector<EuthanasiaEvent> EuthanasiaTracker::getRecentEvents(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<EuthanasiaEvent> events;

    // TODO: Implementation when database is available
    (void)limit;

    return events;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value EuthanasiaTracker::getStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    stats["total_dates_set"] = total_dates_set_;
    stats["total_dates_cleared"] = total_dates_cleared_;
    stats["total_list_additions"] = total_list_additions_;
    stats["total_list_removals"] = total_list_removals_;

    // TODO: Add more statistics when database is available
    // - Current dogs on euthanasia lists
    // - Dogs by urgency level
    // - Average time from list to resolution

    return stats;
}

Json::Value EuthanasiaTracker::getShelterStatistics(const std::string& shelter_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    stats["shelter_id"] = shelter_id;

    // TODO: Implementation when database is available

    return stats;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

void EuthanasiaTracker::logEvent(const EuthanasiaEvent& event) {
    // TODO: Implementation when database is available
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     txn.exec_params(
    //         "INSERT INTO euthanasia_events "
    //         "(id, dog_id, shelter_id, action, timestamp, reason, old_date, new_date, actor) "
    //         "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
    //         event.id, event.dog_id, event.shelter_id, event.action,
    //         /* timestamp */, event.reason,
    //         /* old_date */, /* new_date */, event.actor
    //     );
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "logEvent"}, {"event_id", event.id}});
    // }
    // ConnectionPool::getInstance().release(conn);

    (void)event;
}

std::string EuthanasiaTracker::generateEventId() {
    // Generate a simple UUID-like string
    // TODO: Use proper UUID generation when available

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    const char* hex = "0123456789abcdef";
    std::string uuid;
    uuid.reserve(36);

    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else {
            uuid += hex[dis(gen)];
        }
    }

    return uuid;
}

void EuthanasiaTracker::updateDogRecord(
    const std::string& dog_id,
    std::optional<std::chrono::system_clock::time_point> euthanasia_date,
    bool is_on_list) {

    // TODO: Implementation when DogService is available
    // auto& dog_service = DogService::getInstance();
    // auto dog = dog_service.findById(dog_id);
    // if (dog) {
    //     dog->euthanasia_date = euthanasia_date;
    //     dog->is_on_euthanasia_list = is_on_list;
    //
    //     // Calculate and update urgency level
    //     if (euthanasia_date.has_value()) {
    //         auto countdown = calculateCountdown(euthanasia_date.value());
    //         dog->urgency_level = urgencyToString(countdown.urgency_level);
    //     } else if (is_on_list) {
    //         dog->urgency_level = urgencyToString(UrgencyLevel::HIGH);
    //     } else {
    //         dog->urgency_level = urgencyToString(UrgencyLevel::NORMAL);
    //     }
    //
    //     dog_service.update(*dog);
    // }

    (void)dog_id;
    (void)euthanasia_date;
    (void)is_on_list;
}

void EuthanasiaTracker::emitUrgencyEvent(
    const std::string& dog_id,
    UrgencyLevel new_level,
    const std::string& action) {

    // TODO: Implementation when EventBus is available
    // Json::Value data;
    // data["dog_id"] = dog_id;
    // data["new_level"] = urgencyToString(new_level);
    // data["priority"] = getUrgencyPriority(new_level);
    // data["action"] = action;
    //
    // if (isCritical(new_level)) {
    //     emitEvent(EventType::DOG_BECAME_CRITICAL, dog_id, data, "euthanasia_tracker");
    // } else {
    //     emitEvent(EventType::DOG_URGENCY_CHANGED, dog_id, data, "euthanasia_tracker");
    // }

    (void)dog_id;
    (void)new_level;
    (void)action;
}

std::string EuthanasiaTracker::formatCountdown(
    int days, int hours, int minutes, int seconds) {

    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << days << ":"
       << std::setw(2) << hours << ":"
       << std::setw(2) << minutes << ":"
       << std::setw(2) << seconds;
    return ss.str();
}

} // namespace wtl::core::services
