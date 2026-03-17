/**
 * @file AnalyticsEvent.h
 * @brief Analytics event data structure for tracking all platform events
 *
 * PURPOSE:
 * Represents a single analytics event captured on the platform.
 * Events track user behavior, dog interactions, social media engagement,
 * and system metrics. This is the primary data structure for all analytics
 * data flowing through the system.
 *
 * USAGE:
 * AnalyticsEvent event;
 * event.event_type = AnalyticsEventType::DOG_VIEW;
 * event.entity_type = "dog";
 * event.entity_id = "abc-123";
 * event.user_id = "user-456";
 * auto json = event.toJson();
 *
 * DEPENDENCIES:
 * - EventType.h (event type enumeration)
 * - json/json.h (JSON serialization)
 * - pqxx (database row parsing)
 *
 * MODIFICATION GUIDE:
 * - Add new fields to the struct and update toJson/fromJson/fromDbRow
 * - Keep fields in sync with analytics_events database table
 * - Use std::optional for nullable/optional fields
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <json/json.h>
#include <pqxx/pqxx>

#include "analytics/EventType.h"

namespace wtl::analytics {

/**
 * @struct AnalyticsEvent
 * @brief Represents a single analytics event in the system
 *
 * An analytics event captures a specific user action or system event
 * with full context including the entity involved, user information,
 * session data, and additional metadata in JSON format.
 */
struct AnalyticsEvent {
    // =========================================================================
    // IDENTIFICATION
    // =========================================================================

    /** Unique event identifier (UUID) */
    std::string id;

    /** Type of event (see AnalyticsEventType enum) */
    AnalyticsEventType event_type;

    /** Type of entity this event relates to (dog, shelter, user, etc.) */
    std::string entity_type;

    /** ID of the entity this event relates to */
    std::string entity_id;

    // =========================================================================
    // USER CONTEXT
    // =========================================================================

    /** User ID if authenticated (optional) */
    std::optional<std::string> user_id;

    /** Session ID for tracking user journey */
    std::string session_id;

    /** Anonymous visitor ID for non-authenticated users */
    std::optional<std::string> visitor_id;

    // =========================================================================
    // SOURCE TRACKING
    // =========================================================================

    /** Source of the event (web, mobile, api, import, etc.) */
    std::string source;

    /** HTTP referrer if applicable */
    std::optional<std::string> referrer;

    /** UTM campaign if present */
    std::optional<std::string> utm_campaign;

    /** UTM source if present */
    std::optional<std::string> utm_source;

    /** UTM medium if present */
    std::optional<std::string> utm_medium;

    /** Page URL where event occurred */
    std::optional<std::string> page_url;

    // =========================================================================
    // DEVICE CONTEXT
    // =========================================================================

    /** Device type (desktop, mobile, tablet) */
    std::optional<std::string> device_type;

    /** Browser name */
    std::optional<std::string> browser;

    /** Operating system */
    std::optional<std::string> os;

    /** IP address (anonymized/hashed for privacy) */
    std::optional<std::string> ip_hash;

    /** Geographic country code */
    std::optional<std::string> country_code;

    /** Geographic region/state code */
    std::optional<std::string> region_code;

    /** Geographic city */
    std::optional<std::string> city;

    // =========================================================================
    // EVENT DATA
    // =========================================================================

    /** Additional event-specific data as JSON */
    Json::Value data;

    /** Timestamp when event occurred */
    std::chrono::system_clock::time_point timestamp;

    /** Timestamp when event was received by server */
    std::optional<std::chrono::system_clock::time_point> received_at;

    /** Whether event has been processed for aggregation */
    bool is_processed = false;

    // =========================================================================
    // CONSTRUCTORS
    // =========================================================================

    /**
     * @brief Default constructor
     */
    AnalyticsEvent() = default;

    /**
     * @brief Constructor with essential fields
     * @param type Event type
     * @param entity_type Type of entity
     * @param entity_id ID of entity
     */
    AnalyticsEvent(AnalyticsEventType type,
                   const std::string& entity_type,
                   const std::string& entity_id);

    /**
     * @brief Constructor with user context
     * @param type Event type
     * @param entity_type Type of entity
     * @param entity_id ID of entity
     * @param user_id User ID if authenticated
     * @param session_id Session ID
     */
    AnalyticsEvent(AnalyticsEventType type,
                   const std::string& entity_type,
                   const std::string& entity_id,
                   const std::optional<std::string>& user_id,
                   const std::string& session_id);

    // =========================================================================
    // SERIALIZATION
    // =========================================================================

    /**
     * @brief Convert event to JSON representation
     * @return JSON value containing all event data
     */
    Json::Value toJson() const;

    /**
     * @brief Create event from JSON
     * @param json JSON value to parse
     * @return Parsed AnalyticsEvent
     * @throws std::runtime_error if required fields are missing
     */
    static AnalyticsEvent fromJson(const Json::Value& json);

    /**
     * @brief Create event from database row
     * @param row Database row to parse
     * @return Parsed AnalyticsEvent
     */
    static AnalyticsEvent fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert event to JSON string
     * @return JSON string representation
     */
    std::string toJsonString() const;

    // =========================================================================
    // UTILITY METHODS
    // =========================================================================

    /**
     * @brief Get the event category
     * @return Category string (view, search, interaction, etc.)
     */
    std::string getCategory() const;

    /**
     * @brief Check if this is a conversion event
     * @return True if this event represents a conversion
     */
    bool isConversion() const;

    /**
     * @brief Check if this is a social media event
     * @return True if this event is from social media
     */
    bool isSocialEvent() const;

    /**
     * @brief Get social platform if this is a social event
     * @return Platform name or empty string
     */
    std::string getSocialPlatform() const;

    /**
     * @brief Get timestamp as ISO 8601 string
     * @return ISO formatted timestamp
     */
    std::string getTimestampISO() const;

    /**
     * @brief Get timestamp as Unix epoch seconds
     * @return Unix timestamp
     */
    int64_t getTimestampEpoch() const;

    /**
     * @brief Set timestamp to current time
     */
    void setTimestampNow();

    /**
     * @brief Generate a new UUID for this event
     */
    void generateId();

    /**
     * @brief Validate that required fields are present
     * @return True if event is valid
     */
    bool isValid() const;

    /**
     * @brief Get validation errors
     * @return Vector of error messages
     */
    std::vector<std::string> getValidationErrors() const;
};

/**
 * @brief Helper to parse timestamp from various formats
 * @param str Timestamp string (ISO 8601 or Unix epoch)
 * @return Parsed time point
 */
std::chrono::system_clock::time_point parseTimestamp(const std::string& str);

/**
 * @brief Helper to format timestamp as ISO 8601
 * @param tp Time point to format
 * @return ISO 8601 formatted string
 */
std::string formatTimestampISO(const std::chrono::system_clock::time_point& tp);

/**
 * @brief Generate a UUID v4 string
 * @return New UUID string
 */
std::string generateUUID();

} // namespace wtl::analytics
