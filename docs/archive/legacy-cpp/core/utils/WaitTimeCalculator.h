/**
 * @file WaitTimeCalculator.h
 * @brief Utility for calculating wait times and countdowns for dogs
 *
 * PURPOSE:
 * Provides calculation utilities for determining how long a dog has been
 * waiting for adoption (wait time) and how long until euthanasia (countdown).
 * These calculations are critical for the core mission of the platform.
 *
 * USAGE:
 * auto wait_time = WaitTimeCalculator::calculate(intake_date);
 * std::string formatted = WaitTimeCalculator::formatWaitTime(wait_time);
 *
 * auto countdown = WaitTimeCalculator::calculateCountdown(euthanasia_date);
 * std::string formatted = WaitTimeCalculator::formatCountdown(*countdown);
 *
 * DEPENDENCIES:
 * - chrono (time handling)
 * - string
 *
 * MODIFICATION GUIDE:
 * - Formatting must remain consistent: "YY:MM:DD:HH:MM:SS" for wait time
 * - Countdown format: "DD:HH:MM:SS"
 * - All calculations use UTC time
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>

namespace wtl::core::utils {

/**
 * @struct WaitTimeComponents
 * @brief Represents the components of a dog's wait time
 *
 * Contains individual time components and a formatted string
 * for display on LED counters and UI.
 */
struct WaitTimeComponents {
    int years;                               ///< Years waiting
    int months;                              ///< Months (0-11)
    int days;                                ///< Days (0-30)
    int hours;                               ///< Hours (0-23)
    int minutes;                             ///< Minutes (0-59)
    int seconds;                             ///< Seconds (0-59)
    std::string formatted;                   ///< "YY:MM:DD:HH:MM:SS" format

    /**
     * @brief Get total wait time in days (for comparisons)
     * @return double Total days as decimal
     */
    double totalDays() const;

    /**
     * @brief Get total wait time in seconds (for precise comparisons)
     * @return int64_t Total seconds
     */
    int64_t totalSeconds() const;
};

/**
 * @struct CountdownComponents
 * @brief Represents the countdown to a critical event (like euthanasia)
 *
 * Contains time remaining and urgency flags for UI display.
 */
struct CountdownComponents {
    int days;                                ///< Days remaining
    int hours;                               ///< Hours (0-23)
    int minutes;                             ///< Minutes (0-59)
    int seconds;                             ///< Seconds (0-59)
    std::string formatted;                   ///< "DD:HH:MM:SS" format
    bool is_critical;                        ///< True if less than 24 hours

    /**
     * @brief Get total remaining time in seconds
     * @return int64_t Total seconds remaining
     */
    int64_t totalSeconds() const;
};

/**
 * @class WaitTimeCalculator
 * @brief Static utility class for wait time calculations
 *
 * All methods are static - no instance needed.
 */
class WaitTimeCalculator {
public:
    // ========================================================================
    // WAIT TIME CALCULATION
    // ========================================================================

    /**
     * @brief Calculate wait time from intake date to now
     *
     * @param intake_date When the dog entered the shelter
     * @return WaitTimeComponents The calculated wait time
     *
     * @example
     * auto wait_time = WaitTimeCalculator::calculate(dog.intake_date);
     * std::cout << wait_time.formatted << std::endl; // "02:03:15:08:45:32"
     */
    static WaitTimeComponents calculate(
        const std::chrono::system_clock::time_point& intake_date);

    /**
     * @brief Calculate wait time between two dates
     *
     * @param start_date Start of the wait period
     * @param end_date End of the wait period
     * @return WaitTimeComponents The calculated wait time
     */
    static WaitTimeComponents calculate(
        const std::chrono::system_clock::time_point& start_date,
        const std::chrono::system_clock::time_point& end_date);

    /**
     * @brief Format wait time components to string
     *
     * @param components The wait time to format
     * @return std::string Formatted as "YY:MM:DD:HH:MM:SS"
     *
     * @example
     * std::string formatted = WaitTimeCalculator::formatWaitTime(components);
     * // Result: "02:03:15:08:45:32" (2 years, 3 months, 15 days, etc.)
     */
    static std::string formatWaitTime(const WaitTimeComponents& components);

    // ========================================================================
    // COUNTDOWN CALCULATION
    // ========================================================================

    /**
     * @brief Calculate countdown to euthanasia date
     *
     * @param euthanasia_date The scheduled euthanasia date
     * @return std::optional<CountdownComponents> Countdown if in future, nullopt if past
     *
     * @example
     * auto countdown = WaitTimeCalculator::calculateCountdown(dog.euthanasia_date);
     * if (countdown) {
     *     std::cout << countdown->formatted << std::endl; // "02:14:30:00"
     *     if (countdown->is_critical) {
     *         // Show urgent warning
     *     }
     * }
     */
    static std::optional<CountdownComponents> calculateCountdown(
        const std::chrono::system_clock::time_point& euthanasia_date);

    /**
     * @brief Calculate countdown from custom start time
     *
     * @param target_date The target date
     * @param from_date The start date for countdown
     * @return std::optional<CountdownComponents> Countdown if target is after from, nullopt otherwise
     */
    static std::optional<CountdownComponents> calculateCountdown(
        const std::chrono::system_clock::time_point& target_date,
        const std::chrono::system_clock::time_point& from_date);

    /**
     * @brief Format countdown components to string
     *
     * @param components The countdown to format
     * @return std::string Formatted as "DD:HH:MM:SS"
     *
     * @example
     * std::string formatted = WaitTimeCalculator::formatCountdown(components);
     * // Result: "02:14:30:00" (2 days, 14 hours, 30 minutes, 0 seconds)
     */
    static std::string formatCountdown(const CountdownComponents& components);

    // ========================================================================
    // URGENCY HELPERS
    // ========================================================================

    /**
     * @brief Determine urgency level based on countdown
     *
     * @param countdown The countdown components
     * @return std::string "normal", "medium", "high", or "critical"
     *
     * Thresholds:
     * - critical: < 24 hours
     * - high: < 3 days
     * - medium: < 7 days
     * - normal: >= 7 days
     */
    static std::string determineUrgencyLevel(const CountdownComponents& countdown);

    /**
     * @brief Determine urgency level based on wait time
     *
     * @param wait_time The wait time components
     * @param is_kill_shelter Whether the dog is in a kill shelter
     * @return std::string "normal", "medium", "high", or "critical"
     *
     * For kill shelters, longer wait times increase urgency.
     */
    static std::string determineUrgencyFromWaitTime(
        const WaitTimeComponents& wait_time,
        bool is_kill_shelter);

private:
    // Constants for urgency thresholds (in hours)
    static constexpr int CRITICAL_THRESHOLD_HOURS = 24;
    static constexpr int HIGH_THRESHOLD_HOURS = 72;     // 3 days
    static constexpr int MEDIUM_THRESHOLD_HOURS = 168;  // 7 days

    // Constants for wait time urgency (in days)
    static constexpr int KILL_SHELTER_HIGH_DAYS = 7;
    static constexpr int KILL_SHELTER_CRITICAL_DAYS = 10;
};

} // namespace wtl::core::utils
