/**
 * @file Event.h
 * @brief Event structure for the EventBus system
 *
 * PURPOSE:
 * Defines the Event struct which carries all event data through the
 * EventBus system. Events are the primary communication mechanism
 * between decoupled components and modules.
 *
 * USAGE:
 * Event event;
 * event.type = EventType::DOG_CREATED;
 * event.entity_id = dog.id;
 * event.entity_type = "dog";
 * event.data["name"] = dog.name;
 * event.timestamp = std::chrono::system_clock::now();
 * event.source = "DogService";
 * EventBus::getInstance().publish(event);
 *
 * DEPENDENCIES:
 * - EventType.h (event type enumeration)
 * - json/json.h (JSON data payload)
 *
 * MODIFICATION GUIDE:
 * - Event struct is designed to be generic; add fields sparingly
 * - Event-specific data should go in the data JSON field
 * - Keep serialization methods in sync with struct fields
 *
 * @author Agent 10 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

// Third-party includes
#include <json/json.h>

// Project includes
#include "EventType.h"

namespace wtl::core {

/**
 * @struct Event
 * @brief Represents an event in the system
 *
 * Events carry information about state changes or actions that have occurred.
 * They are published to the EventBus and delivered to subscribed handlers.
 */
struct Event {
    EventType type;                                    ///< The type of event
    std::string entity_id;                             ///< ID of affected entity (dog_id, user_id, etc.)
    std::string entity_type;                           ///< Type of entity ("dog", "shelter", "user", etc.)
    Json::Value data;                                  ///< Event-specific data payload
    std::chrono::system_clock::time_point timestamp;   ///< When the event occurred
    std::string source;                                ///< Which component fired the event

    /**
     * @brief Convert Event to JSON representation
     *
     * @return Json::Value JSON object containing all event fields
     *
     * @example
     * Event event;
     * // ... populate event ...
     * Json::Value json = event.toJson();
     * // Can now serialize to string, store in database, etc.
     */
    Json::Value toJson() const {
        Json::Value json;
        json["type"] = eventTypeToString(type);
        json["entity_id"] = entity_id;
        json["entity_type"] = entity_type;
        json["data"] = data;

        // Convert timestamp to ISO 8601 string
        auto time_t_stamp = std::chrono::system_clock::to_time_t(timestamp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t_stamp), "%Y-%m-%dT%H:%M:%SZ");
        json["timestamp"] = oss.str();

        json["source"] = source;
        return json;
    }

    /**
     * @brief Create Event from JSON representation
     *
     * @param json JSON object containing event fields
     * @return Event The reconstructed event
     * @throws std::invalid_argument if required fields are missing or invalid
     *
     * @example
     * Json::Value json;
     * // ... parse JSON from string/file ...
     * Event event = Event::fromJson(json);
     */
    static Event fromJson(const Json::Value& json) {
        Event event;

        if (!json.isMember("type")) {
            throw std::invalid_argument("Event JSON missing 'type' field");
        }
        event.type = stringToEventType(json["type"].asString());

        event.entity_id = json.get("entity_id", "").asString();
        event.entity_type = json.get("entity_type", "").asString();
        event.data = json.get("data", Json::Value(Json::objectValue));
        event.source = json.get("source", "unknown").asString();

        // Parse timestamp from ISO 8601 string
        if (json.isMember("timestamp")) {
            std::string ts_str = json["timestamp"].asString();
            std::tm tm = {};
            std::istringstream iss(ts_str);
            iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            if (!iss.fail()) {
                event.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            } else {
                event.timestamp = std::chrono::system_clock::now();
            }
        } else {
            event.timestamp = std::chrono::system_clock::now();
        }

        return event;
    }

    /**
     * @brief Convert Event to human-readable string
     *
     * @return std::string Human-readable representation of the event
     *
     * @example
     * Event event;
     * // ... populate event ...
     * std::cout << event.toString() << std::endl;
     * // Output: [2024-01-28T12:00:00Z] DOG_CREATED entity=abc123 source=DogService
     */
    std::string toString() const {
        std::ostringstream oss;

        // Format timestamp
        auto time_t_stamp = std::chrono::system_clock::to_time_t(timestamp);
        oss << "[" << std::put_time(std::gmtime(&time_t_stamp), "%Y-%m-%dT%H:%M:%SZ") << "] ";

        // Event type
        oss << eventTypeToString(type);

        // Entity info
        if (!entity_id.empty()) {
            oss << " entity=" << entity_id;
            if (!entity_type.empty()) {
                oss << " (" << entity_type << ")";
            }
        }

        // Source
        if (!source.empty()) {
            oss << " source=" << source;
        }

        return oss.str();
    }
};

} // namespace wtl::core
