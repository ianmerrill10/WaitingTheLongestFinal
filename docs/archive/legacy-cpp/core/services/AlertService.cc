/**
 * @file AlertService.cc
 * @brief Implementation of AlertService
 * @see AlertService.h for documentation
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/AlertService.h"

// Standard library includes
#include <algorithm>
#include <random>
#include <sstream>

// Project includes
#include "core/services/EuthanasiaTracker.h"
#include "core/models/Dog.h"
#include "core/models/Shelter.h"

namespace wtl::core::services {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

AlertService::AlertService() {
    // Initialize
}

AlertService& AlertService::getInstance() {
    static AlertService instance;
    return instance;
}

// ============================================================================
// TRIGGER ALERTS
// ============================================================================

UrgencyAlert AlertService::triggerCriticalAlert(
    const models::Dog& dog,
    const models::Shelter& shelter) {

    std::lock_guard<std::mutex> lock(mutex_);

    UrgencyAlert alert;
    alert.id = generateAlertId();
    alert.dog_id = dog.id;
    alert.dog_name = dog.name;
    alert.shelter_id = shelter.id;
    alert.shelter_name = shelter.name;
    alert.state_code = shelter.state_code;

    alert.alert_type = AlertType::CRITICAL;
    alert.urgency_level = UrgencyLevel::CRITICAL;

    // Calculate time remaining
    auto [hours, days] = calculateTimeRemaining(dog.euthanasia_date);
    alert.hours_remaining = hours;
    alert.days_remaining = days;

    alert.message = createAlertMessage(AlertType::CRITICAL, dog, shelter, hours);
    alert.action_url = "/dogs/" + dog.id;

    alert.created_at = std::chrono::system_clock::now();
    alert.acknowledged = false;
    alert.resolved = false;

    // Save to database
    saveAlert(alert);

    // Broadcast to connected clients
    broadcastAlert(alert);

    // Emit event
    emitAlertEvent(alert);

    // Invoke handlers
    invokeHandlers(alert);

    // Update statistics
    total_alerts_created_++;
    total_critical_alerts_++;

    // Invalidate cache
    cache_valid_ = false;

    return alert;
}

UrgencyAlert AlertService::triggerHighUrgencyAlert(
    const models::Dog& dog,
    const models::Shelter& shelter) {

    std::lock_guard<std::mutex> lock(mutex_);

    UrgencyAlert alert;
    alert.id = generateAlertId();
    alert.dog_id = dog.id;
    alert.dog_name = dog.name;
    alert.shelter_id = shelter.id;
    alert.shelter_name = shelter.name;
    alert.state_code = shelter.state_code;

    alert.alert_type = AlertType::HIGH_URGENCY;
    alert.urgency_level = UrgencyLevel::HIGH;

    auto [hours, days] = calculateTimeRemaining(dog.euthanasia_date);
    alert.hours_remaining = hours;
    alert.days_remaining = days;

    alert.message = createAlertMessage(AlertType::HIGH_URGENCY, dog, shelter, hours);
    alert.action_url = "/dogs/" + dog.id;

    alert.created_at = std::chrono::system_clock::now();
    alert.acknowledged = false;
    alert.resolved = false;

    // Save to database
    saveAlert(alert);

    // Broadcast to connected clients
    broadcastAlert(alert);

    // Emit event
    emitAlertEvent(alert);

    // Invoke handlers
    invokeHandlers(alert);

    // Update statistics
    total_alerts_created_++;

    // Invalidate cache
    cache_valid_ = false;

    return alert;
}

std::vector<UrgencyAlert> AlertService::triggerNewEuthanasiaListAlert(
    const models::Shelter& shelter,
    const std::vector<models::Dog>& dogs) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<UrgencyAlert> alerts;

    // Create individual alerts for critical dogs
    for (const auto& dog : dogs) {
        UrgencyLevel level = stringToUrgency(dog.urgency_level);

        if (isCritical(level)) {
            // Unlock temporarily to avoid deadlock when calling triggerCriticalAlert
            // In production, this would be handled differently
            // For now, create inline
            UrgencyAlert alert;
            alert.id = generateAlertId();
            alert.dog_id = dog.id;
            alert.dog_name = dog.name;
            alert.shelter_id = shelter.id;
            alert.shelter_name = shelter.name;
            alert.state_code = shelter.state_code;
            alert.alert_type = AlertType::EUTHANASIA_LIST;
            alert.urgency_level = UrgencyLevel::CRITICAL;

            auto [hours, days] = calculateTimeRemaining(dog.euthanasia_date);
            alert.hours_remaining = hours;
            alert.days_remaining = days;

            alert.message = "NEW: " + dog.name + " added to euthanasia list at " +
                           shelter.name + ". Emergency rescue needed!";
            alert.action_url = "/dogs/" + dog.id;
            alert.created_at = std::chrono::system_clock::now();
            alert.acknowledged = false;
            alert.resolved = false;

            saveAlert(alert);
            broadcastAlert(alert);
            emitAlertEvent(alert);
            invokeHandlers(alert);

            alerts.push_back(alert);
            total_alerts_created_++;
            total_critical_alerts_++;
        }
    }

    // Invalidate cache
    cache_valid_ = false;

    // Emit overall list update event
    // TODO: emitEvent(EventType::EUTHANASIA_LIST_UPDATED, shelter.id, {...});

    return alerts;
}

UrgencyAlert AlertService::triggerDateChangedAlert(
    const models::Dog& dog,
    const models::Shelter& shelter,
    const std::chrono::system_clock::time_point& old_date,
    const std::chrono::system_clock::time_point& new_date) {

    std::lock_guard<std::mutex> lock(mutex_);

    UrgencyAlert alert;
    alert.id = generateAlertId();
    alert.dog_id = dog.id;
    alert.dog_name = dog.name;
    alert.shelter_id = shelter.id;
    alert.shelter_name = shelter.name;
    alert.state_code = shelter.state_code;

    alert.alert_type = AlertType::DATE_CHANGED;

    // Calculate new urgency
    auto& tracker = EuthanasiaTracker::getInstance();
    auto countdown = tracker.calculateCountdown(new_date);
    alert.urgency_level = countdown.urgency_level;
    alert.hours_remaining = countdown.days * 24 + countdown.hours;
    alert.days_remaining = countdown.days;

    // Calculate how much sooner
    auto diff = old_date - new_date;
    auto hours_sooner = std::chrono::duration_cast<std::chrono::hours>(diff).count();

    std::ostringstream msg;
    msg << "URGENT: " << dog.name << "'s euthanasia date moved " << hours_sooner
        << " hours sooner at " << shelter.name << ". Now only "
        << countdown.formatted << " remaining!";
    alert.message = msg.str();

    alert.action_url = "/dogs/" + dog.id;
    alert.created_at = std::chrono::system_clock::now();
    alert.acknowledged = false;
    alert.resolved = false;

    saveAlert(alert);
    broadcastAlert(alert);
    emitAlertEvent(alert);
    invokeHandlers(alert);

    total_alerts_created_++;
    if (isCritical(alert.urgency_level)) {
        total_critical_alerts_++;
    }

    cache_valid_ = false;

    return alert;
}

UrgencyAlert AlertService::triggerRescuedAlert(
    const models::Dog& dog,
    const models::Shelter& shelter,
    const std::string& outcome) {

    std::lock_guard<std::mutex> lock(mutex_);

    UrgencyAlert alert;
    alert.id = generateAlertId();
    alert.dog_id = dog.id;
    alert.dog_name = dog.name;
    alert.shelter_id = shelter.id;
    alert.shelter_name = shelter.name;
    alert.state_code = shelter.state_code;

    alert.alert_type = AlertType::RESCUED;
    alert.urgency_level = UrgencyLevel::NORMAL;  // Good news!

    std::ostringstream msg;
    if (outcome == "adopted") {
        msg << "HAPPY NEWS: " << dog.name << " has been ADOPTED from "
            << shelter.name << "! Another life saved!";
    } else if (outcome == "fostered") {
        msg << "GREAT NEWS: " << dog.name << " is going to a FOSTER HOME from "
            << shelter.name << "! Safe for now!";
    } else {
        msg << "RESCUE UPDATE: " << dog.name << " has been pulled by a rescue from "
            << shelter.name << "!";
    }
    alert.message = msg.str();

    alert.action_url = "/dogs/" + dog.id;
    alert.created_at = std::chrono::system_clock::now();
    alert.acknowledged = true;  // Auto-acknowledged
    alert.resolved = true;
    alert.resolution = outcome;
    alert.resolved_at = std::chrono::system_clock::now();

    saveAlert(alert);
    broadcastAlert(alert);  // Share the good news!
    emitAlertEvent(alert);
    invokeHandlers(alert);

    total_alerts_created_++;

    // Update outcome stats
    if (outcome == "adopted") {
        total_adopted_++;
    } else if (outcome == "fostered") {
        total_fostered_++;
    } else {
        total_rescued_++;
    }

    // Resolve any active alerts for this dog
    resolveAlertsForDog(dog.id, outcome);

    return alert;
}

// ============================================================================
// QUERY ALERTS
// ============================================================================

std::vector<UrgencyAlert> AlertService::getActiveAlerts() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Return from cache if valid
    if (cache_valid_) {
        return active_alerts_cache_;
    }

    std::vector<UrgencyAlert> alerts;

    // TODO: Implementation when database is available
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     auto result = txn.exec(
    //         "SELECT * FROM urgency_alerts WHERE resolved = false "
    //         "ORDER BY urgency_level DESC, created_at ASC"
    //     );
    //
    //     for (const auto& row : result) {
    //         alerts.push_back(UrgencyAlert::fromDbRow(row));
    //     }
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "getActiveAlerts"}});
    // }
    // ConnectionPool::getInstance().release(conn);

    // Update cache
    active_alerts_cache_ = alerts;
    cache_valid_ = true;

    return alerts;
}

std::vector<UrgencyAlert> AlertService::getAlertsForShelter(
    const std::string& shelter_id) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<UrgencyAlert> alerts;

    // TODO: Implementation when database is available
    (void)shelter_id;

    return alerts;
}

std::vector<UrgencyAlert> AlertService::getAlertsForState(
    const std::string& state_code) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<UrgencyAlert> alerts;

    // TODO: Implementation when database is available
    (void)state_code;

    return alerts;
}

std::optional<UrgencyAlert> AlertService::getAlertById(const std::string& alert_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: Implementation when database is available
    (void)alert_id;

    return std::nullopt;
}

std::vector<UrgencyAlert> AlertService::getAlertsForDog(const std::string& dog_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<UrgencyAlert> alerts;

    // TODO: Implementation when database is available
    (void)dog_id;

    return alerts;
}

std::vector<UrgencyAlert> AlertService::getCriticalAlerts() {
    auto all_active = getActiveAlerts();

    std::vector<UrgencyAlert> critical;
    for (const auto& alert : all_active) {
        if (alert.alert_type == AlertType::CRITICAL ||
            alert.urgency_level == UrgencyLevel::CRITICAL) {
            critical.push_back(alert);
        }
    }

    return critical;
}

// ============================================================================
// ALERT MANAGEMENT
// ============================================================================

void AlertService::acknowledgeAlert(const std::string& alert_id,
                                     const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: Implementation when database is available
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     txn.exec_params(
    //         "UPDATE urgency_alerts SET acknowledged = true, "
    //         "acknowledged_at = NOW(), acknowledged_by = $2 "
    //         "WHERE id = $1",
    //         alert_id, user_id.empty() ? std::optional<std::string>() : user_id
    //     );
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "acknowledgeAlert"}, {"alert_id", alert_id}});
    // }
    // ConnectionPool::getInstance().release(conn);

    cache_valid_ = false;

    (void)alert_id;
    (void)user_id;
}

void AlertService::resolveAlert(const std::string& alert_id,
                                 const std::string& resolution,
                                 const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: Implementation when database is available
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     txn.exec_params(
    //         "UPDATE urgency_alerts SET resolved = true, "
    //         "resolved_at = NOW(), resolution = $2, resolved_by = $3 "
    //         "WHERE id = $1",
    //         alert_id, resolution, user_id.empty() ? std::optional<std::string>() : user_id
    //     );
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "resolveAlert"}, {"alert_id", alert_id}});
    // }
    // ConnectionPool::getInstance().release(conn);

    total_resolved_++;
    cache_valid_ = false;

    (void)alert_id;
    (void)resolution;
    (void)user_id;
}

void AlertService::resolveAlertsForDog(const std::string& dog_id,
                                        const std::string& resolution) {
    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: Implementation when database is available
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     txn.exec_params(
    //         "UPDATE urgency_alerts SET resolved = true, "
    //         "resolved_at = NOW(), resolution = $2 "
    //         "WHERE dog_id = $1 AND resolved = false",
    //         dog_id, resolution
    //     );
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "resolveAlertsForDog"}, {"dog_id", dog_id}});
    // }
    // ConnectionPool::getInstance().release(conn);

    cache_valid_ = false;

    (void)dog_id;
    (void)resolution;
}

// ============================================================================
// EVENT INTEGRATION
// ============================================================================

void AlertService::emitUrgencyEvent(const models::Dog& dog, UrgencyLevel new_level) {
    // TODO: Implementation when EventBus is available
    // Json::Value data;
    // data["dog_id"] = dog.id;
    // data["dog_name"] = dog.name;
    // data["new_level"] = urgencyToString(new_level);
    // data["priority"] = getUrgencyPriority(new_level);
    //
    // if (isCritical(new_level)) {
    //     emitEvent(EventType::DOG_BECAME_CRITICAL, dog.id, data, "alert_service");
    // } else {
    //     emitEvent(EventType::DOG_URGENCY_CHANGED, dog.id, data, "alert_service");
    // }

    (void)dog;
    (void)new_level;
}

// ============================================================================
// ALERT HANDLERS
// ============================================================================

std::string AlertService::registerCriticalHandler(AlertHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string handler_id = generateAlertId();
    critical_handlers_.push_back({handler_id, handler});
    return handler_id;
}

std::string AlertService::registerAlertHandler(AlertHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string handler_id = generateAlertId();
    all_handlers_.push_back({handler_id, handler});
    return handler_id;
}

void AlertService::unregisterHandler(const std::string& handler_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove from critical handlers
    critical_handlers_.erase(
        std::remove_if(critical_handlers_.begin(), critical_handlers_.end(),
            [&](const auto& pair) { return pair.first == handler_id; }),
        critical_handlers_.end());

    // Remove from all handlers
    all_handlers_.erase(
        std::remove_if(all_handlers_.begin(), all_handlers_.end(),
            [&](const auto& pair) { return pair.first == handler_id; }),
        all_handlers_.end());
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value AlertService::getStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    stats["total_alerts_created"] = total_alerts_created_;
    stats["total_critical_alerts"] = total_critical_alerts_;
    stats["total_resolved"] = total_resolved_;

    Json::Value outcomes;
    outcomes["adopted"] = total_adopted_;
    outcomes["fostered"] = total_fostered_;
    outcomes["rescued"] = total_rescued_;
    stats["outcomes"] = outcomes;

    // Count active alerts
    int active_count = 0;
    int active_critical = 0;
    for (const auto& alert : active_alerts_cache_) {
        if (alert.isActive()) {
            active_count++;
            if (isCritical(alert.urgency_level)) {
                active_critical++;
            }
        }
    }
    stats["active_alerts"] = active_count;
    stats["active_critical"] = active_critical;

    return stats;
}

Json::Value AlertService::getShelterStatistics(const std::string& shelter_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    stats["shelter_id"] = shelter_id;

    // TODO: Implementation when database is available

    return stats;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

std::string AlertService::generateAlertId() {
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

std::string AlertService::createAlertMessage(
    AlertType type,
    const models::Dog& dog,
    const models::Shelter& shelter,
    std::optional<int> hours_remaining) {

    std::ostringstream msg;

    switch (type) {
        case AlertType::CRITICAL:
            msg << "EMERGENCY: " << dog.name << " at " << shelter.name
                << " has only ";
            if (hours_remaining.has_value()) {
                msg << hours_remaining.value() << " hours";
            } else {
                msg << "< 24 hours";
            }
            msg << " until scheduled euthanasia. Immediate action required!";
            break;

        case AlertType::HIGH_URGENCY:
            msg << "URGENT: " << dog.name << " at " << shelter.name
                << " needs help within ";
            if (hours_remaining.has_value() && hours_remaining.value() > 24) {
                msg << (hours_remaining.value() / 24) << " days";
            } else if (hours_remaining.has_value()) {
                msg << hours_remaining.value() << " hours";
            } else {
                msg << "72 hours";
            }
            msg << ". Foster or adoption needed!";
            break;

        case AlertType::EUTHANASIA_LIST:
            msg << dog.name << " has been added to the euthanasia list at "
                << shelter.name << ". Rescue coordination needed immediately.";
            break;

        case AlertType::SHELTER_CRITICAL:
            msg << "SHELTER ALERT: " << shelter.name
                << " has multiple dogs facing euthanasia. Emergency help needed!";
            break;

        case AlertType::DATE_CHANGED:
            msg << "DATE CHANGED: " << dog.name << "'s euthanasia date at "
                << shelter.name << " has been moved sooner. Urgency increased!";
            break;

        case AlertType::RESCUED:
            msg << "SAVED: " << dog.name << " from " << shelter.name
                << " has found safety!";
            break;
    }

    return msg.str();
}

void AlertService::saveAlert(const UrgencyAlert& alert) {
    // TODO: Implementation when database is available
    // auto conn = ConnectionPool::getInstance().acquire();
    // try {
    //     pqxx::work txn(*conn);
    //     txn.exec_params(
    //         "INSERT INTO urgency_alerts "
    //         "(id, dog_id, dog_name, shelter_id, shelter_name, state_code, "
    //         " alert_type, urgency_level, message, action_url, "
    //         " hours_remaining, days_remaining, created_at, "
    //         " acknowledged, resolved) "
    //         "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15)",
    //         alert.id, alert.dog_id, alert.dog_name, alert.shelter_id,
    //         alert.shelter_name, alert.state_code,
    //         alertTypeToString(alert.alert_type), urgencyToString(alert.urgency_level),
    //         alert.message, alert.action_url,
    //         alert.hours_remaining, alert.days_remaining,
    //         /* timestamp */, alert.acknowledged, alert.resolved
    //     );
    //     txn.commit();
    // } catch (const std::exception& e) {
    //     WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, e.what(),
    //         {{"operation", "saveAlert"}, {"alert_id", alert.id}});
    // }
    // ConnectionPool::getInstance().release(conn);

    (void)alert;
}

void AlertService::broadcastAlert(const UrgencyAlert& alert) {
    // TODO: Implementation when BroadcastService is available
    // auto& broadcast = BroadcastService::getInstance();
    //
    // Json::Value payload;
    // payload["type"] = "URGENT_ALERT";
    // payload["alert"] = alert.toJson();
    //
    // if (isCritical(alert.urgency_level)) {
    //     // Broadcast to all connected clients for critical alerts
    //     broadcast.broadcastToAll(payload);
    // } else {
    //     // Broadcast to clients in the same state
    //     broadcast.broadcastToState(alert.state_code, payload);
    // }

    (void)alert;
}

void AlertService::emitAlertEvent(const UrgencyAlert& alert) {
    // TODO: Implementation when EventBus is available
    // Json::Value data = alert.toJson();
    //
    // EventType event_type = EventType::DOG_URGENCY_CHANGED;
    // if (isCritical(alert.urgency_level)) {
    //     event_type = EventType::DOG_BECAME_CRITICAL;
    // }
    //
    // emitEvent(event_type, alert.dog_id, data, "alert_service");

    (void)alert;
}

void AlertService::invokeHandlers(const UrgencyAlert& alert) {
    // Invoke all handlers
    for (const auto& [id, handler] : all_handlers_) {
        try {
            handler(alert);
        } catch (...) {
            // Log but don't fail on handler errors
            // WTL_CAPTURE_ERROR(ErrorCategory::INTERNAL, "Handler failed",
            //     {{"handler_id", id}, {"alert_id", alert.id}});
        }
    }

    // Invoke critical handlers if applicable
    if (isCritical(alert.urgency_level)) {
        for (const auto& [id, handler] : critical_handlers_) {
            try {
                handler(alert);
            } catch (...) {
                // Log but don't fail
            }
        }
    }
}

std::pair<std::optional<int>, std::optional<int>>
AlertService::calculateTimeRemaining(
    const std::optional<std::chrono::system_clock::time_point>& euthanasia_date) {

    if (!euthanasia_date.has_value()) {
        return {std::nullopt, std::nullopt};
    }

    auto now = std::chrono::system_clock::now();
    auto duration = euthanasia_date.value() - now;

    auto total_hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();
    int days = static_cast<int>(total_hours / 24);
    int hours = static_cast<int>(total_hours);

    return {hours, days};
}

} // namespace wtl::core::services
