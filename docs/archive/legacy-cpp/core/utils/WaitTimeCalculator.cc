/**
 * @file WaitTimeCalculator.cc
 * @brief Implementation of WaitTimeCalculator
 * @see WaitTimeCalculator.h for documentation
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/utils/WaitTimeCalculator.h"

#include <iomanip>
#include <sstream>
#include <cmath>

namespace wtl::core::utils {

// ============================================================================
// WaitTimeComponents METHODS
// ============================================================================

double WaitTimeComponents::totalDays() const {
    // Approximate calculation: 365.25 days per year, 30.44 days per month
    return years * 365.25 + months * 30.44 + days +
           hours / 24.0 + minutes / 1440.0 + seconds / 86400.0;
}

int64_t WaitTimeComponents::totalSeconds() const {
    // More precise calculation using seconds
    int64_t total = 0;
    total += static_cast<int64_t>(years) * 365 * 24 * 3600;  // Approximate year
    total += static_cast<int64_t>(months) * 30 * 24 * 3600;  // Approximate month
    total += static_cast<int64_t>(days) * 24 * 3600;
    total += static_cast<int64_t>(hours) * 3600;
    total += static_cast<int64_t>(minutes) * 60;
    total += seconds;
    return total;
}

// ============================================================================
// CountdownComponents METHODS
// ============================================================================

int64_t CountdownComponents::totalSeconds() const {
    int64_t total = 0;
    total += static_cast<int64_t>(days) * 24 * 3600;
    total += static_cast<int64_t>(hours) * 3600;
    total += static_cast<int64_t>(minutes) * 60;
    total += seconds;
    return total;
}

// ============================================================================
// WAIT TIME CALCULATION
// ============================================================================

WaitTimeComponents WaitTimeCalculator::calculate(
    const std::chrono::system_clock::time_point& intake_date) {
    return calculate(intake_date, std::chrono::system_clock::now());
}

WaitTimeComponents WaitTimeCalculator::calculate(
    const std::chrono::system_clock::time_point& start_date,
    const std::chrono::system_clock::time_point& end_date) {

    WaitTimeComponents result;

    // Handle case where end is before start (negative duration)
    if (end_date < start_date) {
        result.years = 0;
        result.months = 0;
        result.days = 0;
        result.hours = 0;
        result.minutes = 0;
        result.seconds = 0;
        result.formatted = "00:00:00:00:00:00";
        return result;
    }

    // Calculate duration in seconds
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        end_date - start_date);
    int64_t total_seconds = duration.count();

    // Extract components
    // Note: This is an approximation. For exact calendar calculations,
    // we would need to iterate through actual months.

    const int64_t SECONDS_PER_MINUTE = 60;
    const int64_t SECONDS_PER_HOUR = 3600;
    const int64_t SECONDS_PER_DAY = 86400;
    const int64_t SECONDS_PER_MONTH = 30 * SECONDS_PER_DAY;  // Approximate
    const int64_t SECONDS_PER_YEAR = 365 * SECONDS_PER_DAY;  // Approximate

    result.years = static_cast<int>(total_seconds / SECONDS_PER_YEAR);
    total_seconds %= SECONDS_PER_YEAR;

    result.months = static_cast<int>(total_seconds / SECONDS_PER_MONTH);
    total_seconds %= SECONDS_PER_MONTH;

    result.days = static_cast<int>(total_seconds / SECONDS_PER_DAY);
    total_seconds %= SECONDS_PER_DAY;

    result.hours = static_cast<int>(total_seconds / SECONDS_PER_HOUR);
    total_seconds %= SECONDS_PER_HOUR;

    result.minutes = static_cast<int>(total_seconds / SECONDS_PER_MINUTE);
    total_seconds %= SECONDS_PER_MINUTE;

    result.seconds = static_cast<int>(total_seconds);

    // Format the string
    result.formatted = formatWaitTime(result);

    return result;
}

std::string WaitTimeCalculator::formatWaitTime(const WaitTimeComponents& components) {
    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << components.years << ":"
       << std::setw(2) << components.months << ":"
       << std::setw(2) << components.days << ":"
       << std::setw(2) << components.hours << ":"
       << std::setw(2) << components.minutes << ":"
       << std::setw(2) << components.seconds;
    return ss.str();
}

// ============================================================================
// COUNTDOWN CALCULATION
// ============================================================================

std::optional<CountdownComponents> WaitTimeCalculator::calculateCountdown(
    const std::chrono::system_clock::time_point& euthanasia_date) {
    return calculateCountdown(euthanasia_date, std::chrono::system_clock::now());
}

std::optional<CountdownComponents> WaitTimeCalculator::calculateCountdown(
    const std::chrono::system_clock::time_point& target_date,
    const std::chrono::system_clock::time_point& from_date) {

    // If target is in the past, return nullopt
    if (target_date <= from_date) {
        return std::nullopt;
    }

    CountdownComponents result;

    // Calculate duration in seconds
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        target_date - from_date);
    int64_t total_seconds = duration.count();

    const int64_t SECONDS_PER_MINUTE = 60;
    const int64_t SECONDS_PER_HOUR = 3600;
    const int64_t SECONDS_PER_DAY = 86400;

    result.days = static_cast<int>(total_seconds / SECONDS_PER_DAY);
    total_seconds %= SECONDS_PER_DAY;

    result.hours = static_cast<int>(total_seconds / SECONDS_PER_HOUR);
    total_seconds %= SECONDS_PER_HOUR;

    result.minutes = static_cast<int>(total_seconds / SECONDS_PER_MINUTE);
    total_seconds %= SECONDS_PER_MINUTE;

    result.seconds = static_cast<int>(total_seconds);

    // Determine if critical (< 24 hours)
    result.is_critical = (result.days == 0 && result.hours < 24);
    // More precisely: total time < 24 hours
    result.is_critical = (result.totalSeconds() < CRITICAL_THRESHOLD_HOURS * 3600);

    // Format the string
    result.formatted = formatCountdown(result);

    return result;
}

std::string WaitTimeCalculator::formatCountdown(const CountdownComponents& components) {
    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << components.days << ":"
       << std::setw(2) << components.hours << ":"
       << std::setw(2) << components.minutes << ":"
       << std::setw(2) << components.seconds;
    return ss.str();
}

// ============================================================================
// URGENCY HELPERS
// ============================================================================

std::string WaitTimeCalculator::determineUrgencyLevel(const CountdownComponents& countdown) {
    int64_t total_hours = countdown.totalSeconds() / 3600;

    if (total_hours < CRITICAL_THRESHOLD_HOURS) {
        return "critical";
    } else if (total_hours < HIGH_THRESHOLD_HOURS) {
        return "high";
    } else if (total_hours < MEDIUM_THRESHOLD_HOURS) {
        return "medium";
    } else {
        return "normal";
    }
}

std::string WaitTimeCalculator::determineUrgencyFromWaitTime(
    const WaitTimeComponents& wait_time,
    bool is_kill_shelter) {

    if (!is_kill_shelter) {
        // Non-kill shelters: longer waits increase visibility but not urgency
        if (wait_time.totalDays() > 365) {
            return "medium";  // Over a year - needs attention
        }
        return "normal";
    }

    // Kill shelters: wait time directly correlates with danger
    double total_days = wait_time.totalDays();

    if (total_days >= KILL_SHELTER_CRITICAL_DAYS) {
        return "critical";
    } else if (total_days >= KILL_SHELTER_HIGH_DAYS) {
        return "high";
    } else if (total_days >= 3) {
        return "medium";
    } else {
        return "normal";
    }
}

} // namespace wtl::core::utils
