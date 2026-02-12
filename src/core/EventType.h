/**
 * @file EventType.h
 * @brief Event type enumeration for the EventBus system
 *
 * PURPOSE:
 * Defines all event types used throughout the WaitingTheLongest platform.
 * Events are the primary mechanism for decoupled communication between
 * modules and core components.
 *
 * USAGE:
 * #include "core/EventType.h"
 * auto type = wtl::core::EventType::DOG_CREATED;
 * std::string name = wtl::core::eventTypeToString(type);
 *
 * DEPENDENCIES:
 * - None (standalone header)
 *
 * MODIFICATION GUIDE:
 * - To add new event types, add to the enum and update both conversion functions
 * - Keep events grouped by domain (Dog, Shelter, User, etc.)
 * - Use UPPER_SNAKE_CASE for enum values
 *
 * @author Agent 10 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>
#include <stdexcept>

namespace wtl::core {

/**
 * @enum EventType
 * @brief All event types supported by the EventBus
 *
 * Events are organized by domain:
 * - DOG_* : Events related to dog entities
 * - SHELTER_* : Events related to shelter entities
 * - USER_* : Events related to user accounts
 * - FOSTER_* : Events related to foster system
 * - ADOPTION_* : Events related to adoption process
 * - SYNC_* : Events related to data synchronization
 * - MODULE_* : Events related to module lifecycle
 */
enum class EventType {
    // ========================================================================
    // DOG EVENTS
    // ========================================================================
    DOG_CREATED,           ///< New dog added to the system
    DOG_UPDATED,           ///< Dog information updated
    DOG_DELETED,           ///< Dog removed from the system
    DOG_STATUS_CHANGED,    ///< Dog status changed (adoptable, pending, etc.)
    DOG_URGENCY_CHANGED,   ///< Dog urgency level changed
    DOG_BECAME_CRITICAL,   ///< Dog urgency reached critical level
    DOG_ADOPTED,           ///< Dog has been adopted

    // ========================================================================
    // SHELTER EVENTS
    // ========================================================================
    SHELTER_CREATED,       ///< New shelter added to the system
    SHELTER_UPDATED,       ///< Shelter information updated
    SHELTER_DELETED,       ///< Shelter removed from the system

    // ========================================================================
    // USER EVENTS
    // ========================================================================
    USER_REGISTERED,       ///< New user registered
    USER_LOGIN,            ///< User logged in
    USER_LOGOUT,           ///< User logged out
    USER_UPDATED,          ///< User profile updated

    // ========================================================================
    // FOSTER EVENTS
    // ========================================================================
    FOSTER_APPLICATION_SUBMITTED,  ///< Foster application submitted
    FOSTER_APPROVED,               ///< Foster application approved
    FOSTER_REJECTED,               ///< Foster application rejected
    FOSTER_PLACEMENT_STARTED,      ///< Dog placed with foster
    FOSTER_PLACEMENT_ENDED,        ///< Foster placement ended

    // ========================================================================
    // ADOPTION EVENTS (for future expansion)
    // ========================================================================
    ADOPTION_APPLICATION_SUBMITTED, ///< Adoption application submitted
    ADOPTION_COMPLETED,             ///< Adoption finalized

    // ========================================================================
    // URGENCY EVENTS
    // ========================================================================
    EUTHANASIA_LIST_UPDATED,   ///< Euthanasia list has been updated
    CRITICAL_ALERT_TRIGGERED,  ///< Critical alert triggered for urgent dogs

    // ========================================================================
    // SYSTEM EVENTS
    // ========================================================================
    SYNC_STARTED,          ///< Data synchronization started
    SYNC_COMPLETED,        ///< Data synchronization completed successfully
    SYNC_FAILED,           ///< Data synchronization failed
    ERROR_OCCURRED,        ///< General error occurred
    HEALTH_CHECK_FAILED,   ///< Health check failed for a component

    // ========================================================================
    // MODULE EVENTS
    // ========================================================================
    MODULE_LOADED,         ///< Module successfully loaded
    MODULE_UNLOADED,       ///< Module unloaded
    MODULE_ERROR,          ///< Error occurred in a module

    // ========================================================================
    // CONTENT EVENTS
    // ========================================================================
    CONTENT_GENERATED,     ///< Content was generated (blog post, social post, etc.)
    CONTENT_PUBLISHED,     ///< Content was published to a platform

    // ========================================================================
    // NOTIFICATION EVENTS
    // ========================================================================
    NOTIFICATION_SENT,     ///< Notification was sent to a user

    // ========================================================================
    // GENERIC / CUSTOM EVENTS
    // ========================================================================
    CUSTOM                 ///< Custom event for module-specific signaling
};

/**
 * @brief Convert EventType to string representation
 *
 * @param type The event type to convert
 * @return std::string The string name of the event type
 *
 * @example
 * auto str = eventTypeToString(EventType::DOG_CREATED);
 * // str == "DOG_CREATED"
 */
inline std::string eventTypeToString(EventType type) {
    switch (type) {
        // Dog events
        case EventType::DOG_CREATED:           return "DOG_CREATED";
        case EventType::DOG_UPDATED:           return "DOG_UPDATED";
        case EventType::DOG_DELETED:           return "DOG_DELETED";
        case EventType::DOG_STATUS_CHANGED:    return "DOG_STATUS_CHANGED";
        case EventType::DOG_URGENCY_CHANGED:   return "DOG_URGENCY_CHANGED";
        case EventType::DOG_BECAME_CRITICAL:   return "DOG_BECAME_CRITICAL";
        case EventType::DOG_ADOPTED:           return "DOG_ADOPTED";

        // Shelter events
        case EventType::SHELTER_CREATED:       return "SHELTER_CREATED";
        case EventType::SHELTER_UPDATED:       return "SHELTER_UPDATED";
        case EventType::SHELTER_DELETED:       return "SHELTER_DELETED";

        // User events
        case EventType::USER_REGISTERED:       return "USER_REGISTERED";
        case EventType::USER_LOGIN:            return "USER_LOGIN";
        case EventType::USER_LOGOUT:           return "USER_LOGOUT";
        case EventType::USER_UPDATED:          return "USER_UPDATED";

        // Foster events
        case EventType::FOSTER_APPLICATION_SUBMITTED: return "FOSTER_APPLICATION_SUBMITTED";
        case EventType::FOSTER_APPROVED:              return "FOSTER_APPROVED";
        case EventType::FOSTER_REJECTED:              return "FOSTER_REJECTED";
        case EventType::FOSTER_PLACEMENT_STARTED:     return "FOSTER_PLACEMENT_STARTED";
        case EventType::FOSTER_PLACEMENT_ENDED:       return "FOSTER_PLACEMENT_ENDED";

        // Adoption events
        case EventType::ADOPTION_APPLICATION_SUBMITTED: return "ADOPTION_APPLICATION_SUBMITTED";
        case EventType::ADOPTION_COMPLETED:             return "ADOPTION_COMPLETED";

        // Urgency events
        case EventType::EUTHANASIA_LIST_UPDATED:   return "EUTHANASIA_LIST_UPDATED";
        case EventType::CRITICAL_ALERT_TRIGGERED:  return "CRITICAL_ALERT_TRIGGERED";

        // System events
        case EventType::SYNC_STARTED:          return "SYNC_STARTED";
        case EventType::SYNC_COMPLETED:        return "SYNC_COMPLETED";
        case EventType::SYNC_FAILED:           return "SYNC_FAILED";
        case EventType::ERROR_OCCURRED:        return "ERROR_OCCURRED";
        case EventType::HEALTH_CHECK_FAILED:   return "HEALTH_CHECK_FAILED";

        // Module events
        case EventType::MODULE_LOADED:         return "MODULE_LOADED";
        case EventType::MODULE_UNLOADED:       return "MODULE_UNLOADED";
        case EventType::MODULE_ERROR:          return "MODULE_ERROR";

        // Content events
        case EventType::CONTENT_GENERATED:     return "CONTENT_GENERATED";
        case EventType::CONTENT_PUBLISHED:     return "CONTENT_PUBLISHED";

        // Notification events
        case EventType::NOTIFICATION_SENT:     return "NOTIFICATION_SENT";

        // Custom events
        case EventType::CUSTOM:                return "CUSTOM";

        default:
            return "UNKNOWN_EVENT";
    }
}

/**
 * @brief Convert string to EventType
 *
 * @param str The string representation of the event type
 * @return EventType The corresponding event type enum value
 * @throws std::invalid_argument if the string is not a valid event type
 *
 * @example
 * auto type = stringToEventType("DOG_CREATED");
 * // type == EventType::DOG_CREATED
 */
inline EventType stringToEventType(const std::string& str) {
    // Dog events
    if (str == "DOG_CREATED")           return EventType::DOG_CREATED;
    if (str == "DOG_UPDATED")           return EventType::DOG_UPDATED;
    if (str == "DOG_DELETED")           return EventType::DOG_DELETED;
    if (str == "DOG_STATUS_CHANGED")    return EventType::DOG_STATUS_CHANGED;
    if (str == "DOG_URGENCY_CHANGED")   return EventType::DOG_URGENCY_CHANGED;
    if (str == "DOG_BECAME_CRITICAL")   return EventType::DOG_BECAME_CRITICAL;
    if (str == "DOG_ADOPTED")           return EventType::DOG_ADOPTED;

    // Shelter events
    if (str == "SHELTER_CREATED")       return EventType::SHELTER_CREATED;
    if (str == "SHELTER_UPDATED")       return EventType::SHELTER_UPDATED;
    if (str == "SHELTER_DELETED")       return EventType::SHELTER_DELETED;

    // User events
    if (str == "USER_REGISTERED")       return EventType::USER_REGISTERED;
    if (str == "USER_LOGIN")            return EventType::USER_LOGIN;
    if (str == "USER_LOGOUT")           return EventType::USER_LOGOUT;
    if (str == "USER_UPDATED")          return EventType::USER_UPDATED;

    // Foster events
    if (str == "FOSTER_APPLICATION_SUBMITTED") return EventType::FOSTER_APPLICATION_SUBMITTED;
    if (str == "FOSTER_APPROVED")              return EventType::FOSTER_APPROVED;
    if (str == "FOSTER_REJECTED")              return EventType::FOSTER_REJECTED;
    if (str == "FOSTER_PLACEMENT_STARTED")     return EventType::FOSTER_PLACEMENT_STARTED;
    if (str == "FOSTER_PLACEMENT_ENDED")       return EventType::FOSTER_PLACEMENT_ENDED;

    // Adoption events
    if (str == "ADOPTION_APPLICATION_SUBMITTED") return EventType::ADOPTION_APPLICATION_SUBMITTED;
    if (str == "ADOPTION_COMPLETED")             return EventType::ADOPTION_COMPLETED;

    // Urgency events
    if (str == "EUTHANASIA_LIST_UPDATED")   return EventType::EUTHANASIA_LIST_UPDATED;
    if (str == "CRITICAL_ALERT_TRIGGERED")  return EventType::CRITICAL_ALERT_TRIGGERED;

    // System events
    if (str == "SYNC_STARTED")          return EventType::SYNC_STARTED;
    if (str == "SYNC_COMPLETED")        return EventType::SYNC_COMPLETED;
    if (str == "SYNC_FAILED")           return EventType::SYNC_FAILED;
    if (str == "ERROR_OCCURRED")        return EventType::ERROR_OCCURRED;
    if (str == "HEALTH_CHECK_FAILED")   return EventType::HEALTH_CHECK_FAILED;

    // Module events
    if (str == "MODULE_LOADED")         return EventType::MODULE_LOADED;
    if (str == "MODULE_UNLOADED")       return EventType::MODULE_UNLOADED;
    if (str == "MODULE_ERROR")          return EventType::MODULE_ERROR;

    // Content events
    if (str == "CONTENT_GENERATED")     return EventType::CONTENT_GENERATED;
    if (str == "CONTENT_PUBLISHED")     return EventType::CONTENT_PUBLISHED;

    // Notification events
    if (str == "NOTIFICATION_SENT")     return EventType::NOTIFICATION_SENT;

    // Custom events
    if (str == "CUSTOM")                return EventType::CUSTOM;

    throw std::invalid_argument("Unknown event type: " + str);
}

} // namespace wtl::core
