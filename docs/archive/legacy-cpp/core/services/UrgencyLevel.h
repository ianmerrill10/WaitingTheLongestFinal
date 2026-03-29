/**
 * @file UrgencyLevel.h
 * @brief Urgency level enumeration and helper functions for dog euthanasia tracking
 *
 * PURPOSE:
 * Defines the urgency levels used to classify dogs based on their proximity to
 * scheduled euthanasia. This is the core component of the life-saving urgency engine.
 * Dogs in kill shelters have limited time - this enum helps prioritize rescue efforts.
 *
 * URGENCY LEVELS:
 * - NORMAL: No known euthanasia date, or > 7 days if known
 * - MEDIUM: 4-7 days until scheduled euthanasia
 * - HIGH: 1-3 days (72 hours) until scheduled euthanasia
 * - CRITICAL: < 24 hours until scheduled euthanasia - EMERGENCY
 *
 * USAGE:
 * UrgencyLevel level = UrgencyLevel::CRITICAL;
 * std::string str = urgencyToString(level);  // "critical"
 * bool urgent = isUrgent(level);  // true
 * int priority = getUrgencyPriority(level);  // 3 (highest)
 *
 * DEPENDENCIES:
 * - None (standalone header)
 *
 * MODIFICATION GUIDE:
 * - Adding new urgency levels: Update enum, all helper functions, and priority mapping
 * - Changing thresholds: Update calculateUrgencyFromDays() documentation
 *
 * @author Agent 8 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>
#include <stdexcept>

namespace wtl::core::services {

/**
 * @enum UrgencyLevel
 * @brief Classification of dog urgency based on time until euthanasia
 *
 * Used throughout the platform to prioritize dogs, trigger alerts,
 * and coordinate rescue efforts. Higher urgency = more immediate action needed.
 */
enum class UrgencyLevel {
    NORMAL,     ///< No known euthanasia date, or > 7 days remaining
    MEDIUM,     ///< 4-7 days until scheduled euthanasia
    HIGH,       ///< 1-3 days (72 hours) until scheduled euthanasia
    CRITICAL    ///< < 24 hours until scheduled euthanasia - EMERGENCY
};

// ============================================================================
// STRING CONVERSION FUNCTIONS
// ============================================================================

/**
 * @brief Convert UrgencyLevel enum to lowercase string representation
 *
 * @param level The urgency level to convert
 * @return std::string Lowercase string: "normal", "medium", "high", or "critical"
 *
 * @example
 * std::string str = urgencyToString(UrgencyLevel::CRITICAL);  // "critical"
 */
inline std::string urgencyToString(UrgencyLevel level) {
    switch (level) {
        case UrgencyLevel::NORMAL:
            return "normal";
        case UrgencyLevel::MEDIUM:
            return "medium";
        case UrgencyLevel::HIGH:
            return "high";
        case UrgencyLevel::CRITICAL:
            return "critical";
        default:
            return "unknown";
    }
}

/**
 * @brief Convert string to UrgencyLevel enum
 *
 * @param str The string to convert (case-insensitive)
 * @return UrgencyLevel The corresponding urgency level
 * @throws std::invalid_argument if string doesn't match any level
 *
 * @example
 * UrgencyLevel level = stringToUrgency("critical");  // UrgencyLevel::CRITICAL
 */
inline UrgencyLevel stringToUrgency(const std::string& str) {
    // Convert to lowercase for comparison
    std::string lower_str;
    lower_str.reserve(str.size());
    for (char c : str) {
        lower_str.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (lower_str == "normal") {
        return UrgencyLevel::NORMAL;
    } else if (lower_str == "medium") {
        return UrgencyLevel::MEDIUM;
    } else if (lower_str == "high") {
        return UrgencyLevel::HIGH;
    } else if (lower_str == "critical") {
        return UrgencyLevel::CRITICAL;
    } else {
        throw std::invalid_argument("Invalid urgency level string: " + str);
    }
}

// ============================================================================
// CALCULATION FUNCTIONS
// ============================================================================

/**
 * @brief Calculate urgency level based on days remaining until euthanasia
 *
 * Thresholds (based on rescue coordination research):
 * - 0 days (< 24 hours): CRITICAL - Emergency protocols activated
 * - 1-3 days: HIGH - Urgent action needed, alerts triggered
 * - 4-7 days: MEDIUM - Time to coordinate rescue/foster
 * - > 7 days or unknown: NORMAL - Standard listing priority
 *
 * @param days_remaining Number of days until scheduled euthanasia
 *                       Use -1 to indicate no known euthanasia date
 * @return UrgencyLevel The calculated urgency level
 *
 * @example
 * UrgencyLevel level = calculateUrgencyFromDays(2);   // UrgencyLevel::HIGH
 * UrgencyLevel level = calculateUrgencyFromDays(0);   // UrgencyLevel::CRITICAL
 * UrgencyLevel level = calculateUrgencyFromDays(-1);  // UrgencyLevel::NORMAL
 */
inline UrgencyLevel calculateUrgencyFromDays(int days_remaining) {
    // No known euthanasia date
    if (days_remaining < 0) {
        return UrgencyLevel::NORMAL;
    }

    // CRITICAL: Less than 24 hours (0 days)
    // Emergency protocols: blast notifications, immediate TikTok, rescue network alerts
    if (days_remaining == 0) {
        return UrgencyLevel::CRITICAL;
    }

    // HIGH: 1-3 days (up to 72 hours)
    // Urgent action: targeted notifications, priority listings, foster outreach
    if (days_remaining <= 3) {
        return UrgencyLevel::HIGH;
    }

    // MEDIUM: 4-7 days
    // Time to coordinate: rescue pulls, transport arrangements, foster matching
    if (days_remaining <= 7) {
        return UrgencyLevel::MEDIUM;
    }

    // NORMAL: More than 7 days
    // Standard listing priority, regular promotion schedule
    return UrgencyLevel::NORMAL;
}

// ============================================================================
// QUERY FUNCTIONS
// ============================================================================

/**
 * @brief Check if urgency level is CRITICAL
 *
 * CRITICAL means < 24 hours to euthanasia - requires immediate action.
 * Used to trigger emergency protocols: blast notifications, immediate social posts,
 * rescue network alerts, and priority placement on all listings.
 *
 * @param level The urgency level to check
 * @return bool True if level is CRITICAL
 *
 * @example
 * if (isCritical(dog.urgency_level)) {
 *     alertService.triggerCriticalAlert(dog);
 * }
 */
inline bool isCritical(UrgencyLevel level) {
    return level == UrgencyLevel::CRITICAL;
}

/**
 * @brief Check if urgency level is HIGH or CRITICAL (requires urgent action)
 *
 * "Urgent" dogs have 72 hours or less until scheduled euthanasia.
 * These dogs require prioritized rescue efforts and enhanced visibility.
 *
 * @param level The urgency level to check
 * @return bool True if level is HIGH or CRITICAL
 *
 * @example
 * if (isUrgent(dog.urgency_level)) {
 *     // Prioritize in search results, trigger notifications
 *     notificationService.sendUrgentAlerts(dog);
 * }
 */
inline bool isUrgent(UrgencyLevel level) {
    return level == UrgencyLevel::HIGH || level == UrgencyLevel::CRITICAL;
}

/**
 * @brief Get numeric priority for sorting (higher = more urgent)
 *
 * Priority mapping:
 * - CRITICAL: 3 (highest priority - immediate action)
 * - HIGH: 2 (urgent - within 72 hours)
 * - MEDIUM: 1 (moderate - within 7 days)
 * - NORMAL: 0 (standard - no known deadline)
 *
 * @param level The urgency level
 * @return int Priority value (0-3, higher = more urgent)
 *
 * @example
 * // Sort dogs by urgency (most urgent first)
 * std::sort(dogs.begin(), dogs.end(), [](const Dog& a, const Dog& b) {
 *     return getUrgencyPriority(a.urgency_level) > getUrgencyPriority(b.urgency_level);
 * });
 */
inline int getUrgencyPriority(UrgencyLevel level) {
    switch (level) {
        case UrgencyLevel::CRITICAL:
            return 3;
        case UrgencyLevel::HIGH:
            return 2;
        case UrgencyLevel::MEDIUM:
            return 1;
        case UrgencyLevel::NORMAL:
        default:
            return 0;
    }
}

/**
 * @brief Get display color for UI representation
 *
 * Color coding for visual urgency indication:
 * - CRITICAL: #FF0000 (red) - flashing/pulsing recommended
 * - HIGH: #FF6600 (orange)
 * - MEDIUM: #FFCC00 (yellow)
 * - NORMAL: #00CC00 (green)
 *
 * @param level The urgency level
 * @return std::string Hex color code
 */
inline std::string getUrgencyColor(UrgencyLevel level) {
    switch (level) {
        case UrgencyLevel::CRITICAL:
            return "#FF0000";  // Red - emergency
        case UrgencyLevel::HIGH:
            return "#FF6600";  // Orange - urgent
        case UrgencyLevel::MEDIUM:
            return "#FFCC00";  // Yellow - moderate
        case UrgencyLevel::NORMAL:
        default:
            return "#00CC00";  // Green - normal
    }
}

/**
 * @brief Get human-readable description of urgency level
 *
 * @param level The urgency level
 * @return std::string Description suitable for display to users
 */
inline std::string getUrgencyDescription(UrgencyLevel level) {
    switch (level) {
        case UrgencyLevel::CRITICAL:
            return "CRITICAL - Less than 24 hours until scheduled euthanasia";
        case UrgencyLevel::HIGH:
            return "HIGH - 1-3 days until scheduled euthanasia";
        case UrgencyLevel::MEDIUM:
            return "MEDIUM - 4-7 days until scheduled euthanasia";
        case UrgencyLevel::NORMAL:
        default:
            return "NORMAL - No imminent euthanasia date";
    }
}

} // namespace wtl::core::services
