/**
 * @file NotificationType.h
 * @brief Notification type enumeration for WaitingTheLongest push notification system
 *
 * PURPOSE:
 * Defines all notification types for the platform. Each type has different
 * priority levels and handling requirements. Critical alerts (dogs < 24 hours)
 * bypass quiet hours and are sent to all enabled channels.
 *
 * USAGE:
 * NotificationType type = NotificationType::CRITICAL_ALERT;
 * std::string name = notificationTypeToString(type);
 * int priority = getNotificationPriority(type);
 *
 * DEPENDENCIES:
 * - Standard library (string)
 *
 * MODIFICATION GUIDE:
 * - Add new types at the end of the enum (before COUNT)
 * - Update all helper functions when adding types
 * - Update notification templates for new types
 *
 * @author Agent 14 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <string>

namespace wtl::notifications {

/**
 * @enum NotificationType
 * @brief Types of notifications sent to users
 *
 * PRIORITY LEVELS:
 * - Level 1 (Critical): CRITICAL_ALERT - Bypasses all quiet hours
 * - Level 2 (High): HIGH_URGENCY, FOSTER_NEEDED_URGENT - Important time-sensitive
 * - Level 3 (Medium): PERFECT_MATCH, GOOD_MATCH, DOG_UPDATE - Engagement
 * - Level 4 (Low): FOSTER_FOLLOWUP, SUCCESS_STORY, NEW_BLOG_POST, TIP_OF_THE_DAY - Informational
 */
enum class NotificationType {
    // =========================================================================
    // LEVEL 1 - CRITICAL (Bypass quiet hours)
    // =========================================================================

    CRITICAL_ALERT = 0,     ///< Dog has less than 24 hours - EMERGENCY

    // =========================================================================
    // LEVEL 2 - HIGH URGENCY
    // =========================================================================

    HIGH_URGENCY,           ///< Dog has less than 72 hours
    FOSTER_NEEDED_URGENT,   ///< Urgent need for foster home

    // =========================================================================
    // LEVEL 3 - MEDIUM (Engagement)
    // =========================================================================

    PERFECT_MATCH,          ///< Dog matches user preferences 90%+
    GOOD_MATCH,             ///< Dog matches user preferences 70-89%
    DOG_UPDATE,             ///< Update on a favorited or matched dog

    // =========================================================================
    // LEVEL 4 - LOW (Informational)
    // =========================================================================

    FOSTER_FOLLOWUP,        ///< Follow-up on foster placement
    SUCCESS_STORY,          ///< Dog was adopted - positive news
    NEW_BLOG_POST,          ///< New content published
    TIP_OF_THE_DAY,         ///< Daily tips for pet owners

    // =========================================================================
    // SYSTEM
    // =========================================================================

    SYSTEM_ALERT,           ///< System notifications (account, etc.)

    COUNT                   ///< Number of notification types (for iteration)
};

/**
 * @brief Convert NotificationType to string
 *
 * @param type The notification type
 * @return std::string Human-readable string representation
 *
 * @example
 * auto str = notificationTypeToString(NotificationType::CRITICAL_ALERT);
 * // Returns "critical_alert"
 */
inline std::string notificationTypeToString(NotificationType type) {
    switch (type) {
        case NotificationType::CRITICAL_ALERT:
            return "critical_alert";
        case NotificationType::HIGH_URGENCY:
            return "high_urgency";
        case NotificationType::FOSTER_NEEDED_URGENT:
            return "foster_needed_urgent";
        case NotificationType::PERFECT_MATCH:
            return "perfect_match";
        case NotificationType::GOOD_MATCH:
            return "good_match";
        case NotificationType::DOG_UPDATE:
            return "dog_update";
        case NotificationType::FOSTER_FOLLOWUP:
            return "foster_followup";
        case NotificationType::SUCCESS_STORY:
            return "success_story";
        case NotificationType::NEW_BLOG_POST:
            return "new_blog_post";
        case NotificationType::TIP_OF_THE_DAY:
            return "tip_of_the_day";
        case NotificationType::SYSTEM_ALERT:
            return "system_alert";
        default:
            return "unknown";
    }
}

/**
 * @brief Parse string to NotificationType
 *
 * @param str The string to parse
 * @return NotificationType The parsed type, or SYSTEM_ALERT if unknown
 */
inline NotificationType stringToNotificationType(const std::string& str) {
    if (str == "critical_alert") return NotificationType::CRITICAL_ALERT;
    if (str == "high_urgency") return NotificationType::HIGH_URGENCY;
    if (str == "foster_needed_urgent") return NotificationType::FOSTER_NEEDED_URGENT;
    if (str == "perfect_match") return NotificationType::PERFECT_MATCH;
    if (str == "good_match") return NotificationType::GOOD_MATCH;
    if (str == "dog_update") return NotificationType::DOG_UPDATE;
    if (str == "foster_followup") return NotificationType::FOSTER_FOLLOWUP;
    if (str == "success_story") return NotificationType::SUCCESS_STORY;
    if (str == "new_blog_post") return NotificationType::NEW_BLOG_POST;
    if (str == "tip_of_the_day") return NotificationType::TIP_OF_THE_DAY;
    if (str == "system_alert") return NotificationType::SYSTEM_ALERT;
    return NotificationType::SYSTEM_ALERT; // Default
}

/**
 * @brief Get priority level for notification type (1 = highest)
 *
 * @param type The notification type
 * @return int Priority level (1-4, lower is more urgent)
 */
inline int getNotificationPriority(NotificationType type) {
    switch (type) {
        case NotificationType::CRITICAL_ALERT:
            return 1;
        case NotificationType::HIGH_URGENCY:
        case NotificationType::FOSTER_NEEDED_URGENT:
            return 2;
        case NotificationType::PERFECT_MATCH:
        case NotificationType::GOOD_MATCH:
        case NotificationType::DOG_UPDATE:
            return 3;
        case NotificationType::FOSTER_FOLLOWUP:
        case NotificationType::SUCCESS_STORY:
        case NotificationType::NEW_BLOG_POST:
        case NotificationType::TIP_OF_THE_DAY:
        case NotificationType::SYSTEM_ALERT:
        default:
            return 4;
    }
}

/**
 * @brief Check if notification type bypasses quiet hours
 *
 * @param type The notification type
 * @return true if notification should bypass quiet hours
 */
inline bool bypassesQuietHours(NotificationType type) {
    // Only critical alerts bypass quiet hours - a dog's life is at stake
    return type == NotificationType::CRITICAL_ALERT;
}

/**
 * @brief Get display title for notification type
 *
 * @param type The notification type
 * @return std::string Human-readable title
 */
inline std::string getNotificationTypeTitle(NotificationType type) {
    switch (type) {
        case NotificationType::CRITICAL_ALERT:
            return "URGENT: Dog Needs Help NOW";
        case NotificationType::HIGH_URGENCY:
            return "Time-Sensitive: Dog at Risk";
        case NotificationType::FOSTER_NEEDED_URGENT:
            return "Foster Needed Urgently";
        case NotificationType::PERFECT_MATCH:
            return "Perfect Match Found!";
        case NotificationType::GOOD_MATCH:
            return "Great Match for You";
        case NotificationType::DOG_UPDATE:
            return "Update on a Dog You Follow";
        case NotificationType::FOSTER_FOLLOWUP:
            return "Foster Check-In";
        case NotificationType::SUCCESS_STORY:
            return "Happy Tails - Adoption Success!";
        case NotificationType::NEW_BLOG_POST:
            return "New from WaitingTheLongest";
        case NotificationType::TIP_OF_THE_DAY:
            return "Today's Pet Tip";
        case NotificationType::SYSTEM_ALERT:
            return "WaitingTheLongest";
        default:
            return "WaitingTheLongest";
    }
}

/**
 * @brief Get icon name for notification type
 *
 * @param type The notification type
 * @return std::string Icon identifier
 */
inline std::string getNotificationIcon(NotificationType type) {
    switch (type) {
        case NotificationType::CRITICAL_ALERT:
            return "emergency";
        case NotificationType::HIGH_URGENCY:
            return "warning";
        case NotificationType::FOSTER_NEEDED_URGENT:
            return "home_urgent";
        case NotificationType::PERFECT_MATCH:
            return "star";
        case NotificationType::GOOD_MATCH:
            return "heart";
        case NotificationType::DOG_UPDATE:
            return "info";
        case NotificationType::FOSTER_FOLLOWUP:
            return "calendar";
        case NotificationType::SUCCESS_STORY:
            return "celebration";
        case NotificationType::NEW_BLOG_POST:
            return "article";
        case NotificationType::TIP_OF_THE_DAY:
            return "lightbulb";
        case NotificationType::SYSTEM_ALERT:
        default:
            return "notification";
    }
}

/**
 * @brief Get color code for notification type
 *
 * @param type The notification type
 * @return std::string Hex color code
 */
inline std::string getNotificationColor(NotificationType type) {
    switch (type) {
        case NotificationType::CRITICAL_ALERT:
            return "#FF0000"; // Red - Emergency
        case NotificationType::HIGH_URGENCY:
            return "#FF6600"; // Orange - Urgent
        case NotificationType::FOSTER_NEEDED_URGENT:
            return "#FF9900"; // Amber - Important
        case NotificationType::PERFECT_MATCH:
            return "#00CC00"; // Green - Positive
        case NotificationType::GOOD_MATCH:
            return "#0099FF"; // Blue - Good
        case NotificationType::DOG_UPDATE:
            return "#6699CC"; // Light blue - Info
        case NotificationType::FOSTER_FOLLOWUP:
            return "#9966CC"; // Purple - Foster
        case NotificationType::SUCCESS_STORY:
            return "#FFD700"; // Gold - Celebration
        case NotificationType::NEW_BLOG_POST:
            return "#666666"; // Gray - Content
        case NotificationType::TIP_OF_THE_DAY:
            return "#99CC00"; // Yellow-green - Tips
        case NotificationType::SYSTEM_ALERT:
        default:
            return "#333333"; // Dark gray - System
    }
}

/**
 * @brief Check if notification requires immediate delivery
 *
 * @param type The notification type
 * @return true if notification should be sent immediately
 */
inline bool requiresImmediateDelivery(NotificationType type) {
    switch (type) {
        case NotificationType::CRITICAL_ALERT:
        case NotificationType::HIGH_URGENCY:
        case NotificationType::FOSTER_NEEDED_URGENT:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Get default TTL (Time To Live) in seconds for notification type
 *
 * @param type The notification type
 * @return int TTL in seconds
 */
inline int getNotificationTTL(NotificationType type) {
    switch (type) {
        case NotificationType::CRITICAL_ALERT:
            return 3600;     // 1 hour - critical but dog might be saved
        case NotificationType::HIGH_URGENCY:
            return 86400;    // 24 hours
        case NotificationType::FOSTER_NEEDED_URGENT:
            return 172800;   // 48 hours
        case NotificationType::PERFECT_MATCH:
        case NotificationType::GOOD_MATCH:
            return 604800;   // 7 days
        case NotificationType::DOG_UPDATE:
            return 259200;   // 3 days
        case NotificationType::FOSTER_FOLLOWUP:
            return 604800;   // 7 days
        case NotificationType::SUCCESS_STORY:
        case NotificationType::NEW_BLOG_POST:
        case NotificationType::TIP_OF_THE_DAY:
            return 86400;    // 24 hours
        case NotificationType::SYSTEM_ALERT:
        default:
            return 604800;   // 7 days
    }
}

} // namespace wtl::notifications
