/**
 * @file CronParser.h
 * @brief Cron expression parser for scheduling
 *
 * PURPOSE:
 * Parses cron expressions and calculates next execution times.
 * Supports standard 5-field cron syntax (minute, hour, day, month, weekday).
 *
 * USAGE:
 * CronParser parser;
 * // parse("0 0,6,12,18 * * *") for every 6 hours
 * if (parser.parse("30 8 * * 1-5")) {
 *     auto next = parser.getNextRunTime();
 * }
 *
 * CRON FORMAT:
 * minute hour day month weekday
 * - minute: 0-59
 * - hour: 0-23
 * - day: 1-31
 * - month: 1-12 (or JAN-DEC)
 * - weekday: 0-6 (0=Sunday, or SUN-SAT)
 *
 * SPECIAL CHARACTERS:
 * - *: any value
 * - ,: value list separator (1,2,3)
 * - -: range (1-5)
 * - /: step values (e.g. 0,15,30,45 = every 15)
 *
 * DEPENDENCIES:
 * - Standard library
 *
 * MODIFICATION GUIDE:
 * - Add support for 6-field cron (with seconds)
 * - Add support for year field
 * - Add named schedules (@hourly, @daily, etc.)
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace wtl::workers::scheduler {

/**
 * @struct CronField
 * @brief Represents a single field in a cron expression
 */
struct CronField {
    std::set<int> values;     ///< Valid values for this field
    int min_value{0};         ///< Minimum valid value
    int max_value{59};        ///< Maximum valid value
    bool any{false};          ///< Whether * was specified

    /**
     * @brief Check if a value matches this field
     * @param value The value to check
     * @return true if value matches
     */
    bool matches(int value) const {
        if (any) return true;
        return values.find(value) != values.end();
    }

    /**
     * @brief Get the next valid value >= start
     * @param start Starting value
     * @return Next valid value, or nullopt if none
     */
    std::optional<int> getNextValue(int start) const {
        if (any) {
            return start <= max_value ? std::optional<int>(start) : std::nullopt;
        }

        auto it = values.lower_bound(start);
        if (it != values.end()) {
            return *it;
        }

        return std::nullopt;
    }

    /**
     * @brief Get the first valid value
     * @return First valid value
     */
    int getFirstValue() const {
        if (any) return min_value;
        return values.empty() ? min_value : *values.begin();
    }
};

/**
 * @struct ParsedCron
 * @brief Parsed cron expression
 */
struct ParsedCron {
    CronField minute;         ///< Minute field (0-59)
    CronField hour;           ///< Hour field (0-23)
    CronField day;            ///< Day of month field (1-31)
    CronField month;          ///< Month field (1-12)
    CronField weekday;        ///< Day of week field (0-6, Sunday=0)
    bool valid{false};        ///< Whether parsing succeeded
    std::string error;        ///< Error message if parsing failed
    std::string original;     ///< Original expression
};

/**
 * @class CronParser
 * @brief Parses cron expressions and calculates next run times
 *
 * Thread-safe utility class for working with cron schedules.
 */
class CronParser {
public:
    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Default constructor
     */
    CronParser() = default;

    /**
     * @brief Construct with expression
     * @param expression Cron expression to parse
     */
    explicit CronParser(const std::string& expression);

    // =========================================================================
    // PARSING
    // =========================================================================

    /**
     * @brief Parse a cron expression
     *
     * @param expression Cron expression (5 fields)
     * @return true if parsing succeeded
     *
     * @example
     * parser.parse("0 0,6,12,18 * * *"); // Every 6 hours
     * parser.parse("30 8 * * 1-5");      // 8:30 AM weekdays
     */
    bool parse(const std::string& expression);

    /**
     * @brief Check if the parser has a valid expression
     * @return true if valid
     */
    bool isValid() const;

    /**
     * @brief Get the parse error message
     * @return Error message, or empty if valid
     */
    std::string getError() const;

    /**
     * @brief Get the original expression
     * @return Original expression string
     */
    std::string getExpression() const;

    /**
     * @brief Get the parsed cron structure
     * @return ParsedCron structure
     */
    const ParsedCron& getParsed() const;

    // =========================================================================
    // VALIDATION
    // =========================================================================

    /**
     * @brief Validate a cron expression
     *
     * @param expression Cron expression to validate
     * @return true if expression is valid
     *
     * Static method for quick validation without creating an instance.
     */
    static bool validate(const std::string& expression);

    /**
     * @brief Validate with error message
     *
     * @param expression Cron expression to validate
     * @param error_out Output parameter for error message
     * @return true if expression is valid
     */
    static bool validateWithError(const std::string& expression, std::string& error_out);

    // =========================================================================
    // NEXT RUN CALCULATION
    // =========================================================================

    /**
     * @brief Get the next run time from now
     *
     * @return Next run time, or nullopt if no valid next time
     */
    std::optional<std::chrono::system_clock::time_point> getNextRunTime() const;

    /**
     * @brief Get the next run time from a specific time
     *
     * @param from Starting time
     * @return Next run time, or nullopt if no valid next time
     */
    std::optional<std::chrono::system_clock::time_point> getNextRunTime(
        std::chrono::system_clock::time_point from) const;

    /**
     * @brief Get the next N run times
     *
     * @param count Number of run times to calculate
     * @param from Starting time (default: now)
     * @return Vector of next run times
     */
    std::vector<std::chrono::system_clock::time_point> getNextRunTimes(
        int count,
        std::chrono::system_clock::time_point from =
            std::chrono::system_clock::now()) const;

    /**
     * @brief Static method to get next run time for an expression
     *
     * @param expression Cron expression
     * @return Next run time, or nullopt if invalid
     */
    static std::optional<std::chrono::system_clock::time_point> getNextRunTime(
        const std::string& expression);

    /**
     * @brief Static method to get next run time from a specific time
     *
     * @param expression Cron expression
     * @param from Starting time
     * @return Next run time, or nullopt if invalid
     */
    static std::optional<std::chrono::system_clock::time_point> getNextRunTime(
        const std::string& expression,
        std::chrono::system_clock::time_point from);

    // =========================================================================
    // MATCHING
    // =========================================================================

    /**
     * @brief Check if a time point matches the cron expression
     *
     * @param time Time to check
     * @return true if time matches
     */
    bool matches(std::chrono::system_clock::time_point time) const;

    /**
     * @brief Check if the current time matches
     * @return true if now matches
     */
    bool matchesNow() const;

    // =========================================================================
    // SPECIAL EXPRESSIONS
    // =========================================================================

    /**
     * @brief Parse a named schedule expression
     *
     * @param name Named schedule (@yearly, @monthly, @weekly, @daily, @hourly)
     * @return Cron expression, or empty if invalid
     *
     * Supported names:
     * - @yearly / @annually: 0 0 1 1 *
     * - @monthly: 0 0 1 * *
     * - @weekly: 0 0 * * 0
     * - @daily / @midnight: 0 0 * * *
     * - @hourly: 0 * * * *
     */
    static std::string namedScheduleToCron(const std::string& name);

    /**
     * @brief Create a human-readable description of the schedule
     * @return Description string
     */
    std::string describe() const;

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    /**
     * @brief Parse a single field
     *
     * @param field_str Field string
     * @param min_val Minimum valid value
     * @param max_val Maximum valid value
     * @param field Output field
     * @return true if parsing succeeded
     */
    bool parseField(const std::string& field_str, int min_val, int max_val,
                    CronField& field);

    /**
     * @brief Parse a value or range
     *
     * @param part Part of field (may be value, range, or step)
     * @param min_val Minimum valid value
     * @param max_val Maximum valid value
     * @param field Output field
     * @return true if parsing succeeded
     */
    bool parseFieldPart(const std::string& part, int min_val, int max_val,
                        CronField& field);

    /**
     * @brief Convert month name to number
     * @param name Month name (JAN, FEB, etc.)
     * @return Month number (1-12), or -1 if invalid
     */
    static int monthNameToNumber(const std::string& name);

    /**
     * @brief Convert weekday name to number
     * @param name Weekday name (SUN, MON, etc.)
     * @return Weekday number (0-6), or -1 if invalid
     */
    static int weekdayNameToNumber(const std::string& name);

    /**
     * @brief Get day of week for a time point (0=Sunday)
     * @param time Time point
     * @return Day of week
     */
    static int getDayOfWeek(std::chrono::system_clock::time_point time);

    /**
     * @brief Get days in month
     * @param year Year
     * @param month Month (1-12)
     * @return Number of days
     */
    static int getDaysInMonth(int year, int month);

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    ParsedCron parsed_;
};

} // namespace wtl::workers::scheduler
