/**
 * @file CronParser.cc
 * @brief Implementation of CronParser class
 * @see CronParser.h for documentation
 */

#include "CronParser.h"

// Standard library includes
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace wtl::workers::scheduler {

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

namespace {

/**
 * @brief Trim whitespace from string
 */
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

/**
 * @brief Split string by delimiter
 */
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> parts;
    std::istringstream iss(str);
    std::string part;
    while (std::getline(iss, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}

/**
 * @brief Convert string to uppercase
 */
std::string toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

/**
 * @brief Parse integer from string
 */
bool parseInt(const std::string& str, int& out) {
    try {
        size_t pos;
        out = std::stoi(str, &pos);
        return pos == str.length();
    } catch (...) {
        return false;
    }
}

} // anonymous namespace

// ============================================================================
// CONSTRUCTION
// ============================================================================

CronParser::CronParser(const std::string& expression) {
    parse(expression);
}

// ============================================================================
// PARSING
// ============================================================================

bool CronParser::parse(const std::string& expression) {
    parsed_ = ParsedCron{};
    parsed_.original = expression;

    std::string expr = trim(expression);

    // Handle named schedules
    if (!expr.empty() && expr[0] == '@') {
        std::string cron = namedScheduleToCron(expr);
        if (cron.empty()) {
            parsed_.error = "Unknown named schedule: " + expr;
            return false;
        }
        expr = cron;
    }

    // Split into fields
    std::vector<std::string> fields = split(expr, ' ');

    // Remove empty fields
    fields.erase(std::remove_if(fields.begin(), fields.end(),
                                [](const std::string& s) { return s.empty(); }),
                 fields.end());

    if (fields.size() != 5) {
        parsed_.error = "Expected 5 fields, got " + std::to_string(fields.size());
        return false;
    }

    // Parse each field
    parsed_.minute.min_value = 0;
    parsed_.minute.max_value = 59;
    if (!parseField(fields[0], 0, 59, parsed_.minute)) {
        parsed_.error = "Invalid minute field: " + fields[0];
        return false;
    }

    parsed_.hour.min_value = 0;
    parsed_.hour.max_value = 23;
    if (!parseField(fields[1], 0, 23, parsed_.hour)) {
        parsed_.error = "Invalid hour field: " + fields[1];
        return false;
    }

    parsed_.day.min_value = 1;
    parsed_.day.max_value = 31;
    if (!parseField(fields[2], 1, 31, parsed_.day)) {
        parsed_.error = "Invalid day field: " + fields[2];
        return false;
    }

    parsed_.month.min_value = 1;
    parsed_.month.max_value = 12;
    if (!parseField(fields[3], 1, 12, parsed_.month)) {
        parsed_.error = "Invalid month field: " + fields[3];
        return false;
    }

    parsed_.weekday.min_value = 0;
    parsed_.weekday.max_value = 6;
    if (!parseField(fields[4], 0, 6, parsed_.weekday)) {
        parsed_.error = "Invalid weekday field: " + fields[4];
        return false;
    }

    parsed_.valid = true;
    return true;
}

bool CronParser::isValid() const {
    return parsed_.valid;
}

std::string CronParser::getError() const {
    return parsed_.error;
}

std::string CronParser::getExpression() const {
    return parsed_.original;
}

const ParsedCron& CronParser::getParsed() const {
    return parsed_;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool CronParser::validate(const std::string& expression) {
    CronParser parser;
    return parser.parse(expression);
}

bool CronParser::validateWithError(const std::string& expression, std::string& error_out) {
    CronParser parser;
    bool valid = parser.parse(expression);
    error_out = parser.getError();
    return valid;
}

// ============================================================================
// NEXT RUN CALCULATION
// ============================================================================

std::optional<std::chrono::system_clock::time_point> CronParser::getNextRunTime() const {
    return getNextRunTime(std::chrono::system_clock::now());
}

std::optional<std::chrono::system_clock::time_point> CronParser::getNextRunTime(
    std::chrono::system_clock::time_point from) const {

    if (!parsed_.valid) {
        return std::nullopt;
    }

    // Convert to tm structure
    auto from_time = std::chrono::system_clock::to_time_t(from);
    std::tm tm = *std::gmtime(&from_time);

    // Start from the next minute
    tm.tm_sec = 0;
    tm.tm_min++;

    // Normalize the time
    auto normalized = std::mktime(&tm);
    tm = *std::gmtime(&normalized);

    // Iterate to find next valid time (max 4 years ahead)
    int iterations = 0;
    const int max_iterations = 4 * 366 * 24 * 60;  // ~4 years in minutes

    while (iterations < max_iterations) {
        iterations++;

        // Check month
        if (!parsed_.month.matches(tm.tm_mon + 1)) {
            // Move to first valid month
            auto next_month = parsed_.month.getNextValue(tm.tm_mon + 1);
            if (!next_month) {
                // Move to next year
                tm.tm_year++;
                tm.tm_mon = parsed_.month.getFirstValue() - 1;
            } else {
                tm.tm_mon = *next_month - 1;
            }
            tm.tm_mday = 1;
            tm.tm_hour = 0;
            tm.tm_min = 0;

            // Normalize and continue
            normalized = std::mktime(&tm);
            tm = *std::gmtime(&normalized);
            continue;
        }

        // Check day of month
        int days_in_month = getDaysInMonth(tm.tm_year + 1900, tm.tm_mon + 1);
        if (!parsed_.day.matches(tm.tm_mday) || tm.tm_mday > days_in_month) {
            auto next_day = parsed_.day.getNextValue(tm.tm_mday);
            if (!next_day || *next_day > days_in_month) {
                // Move to next month
                tm.tm_mon++;
                tm.tm_mday = 1;
            } else {
                tm.tm_mday = *next_day;
            }
            tm.tm_hour = 0;
            tm.tm_min = 0;

            // Normalize and continue
            normalized = std::mktime(&tm);
            tm = *std::gmtime(&normalized);
            continue;
        }

        // Check weekday (if not "any")
        if (!parsed_.weekday.any) {
            int current_weekday = tm.tm_wday;  // 0=Sunday
            if (!parsed_.weekday.matches(current_weekday)) {
                // Move to next day
                tm.tm_mday++;
                tm.tm_hour = 0;
                tm.tm_min = 0;

                // Normalize and continue
                normalized = std::mktime(&tm);
                tm = *std::gmtime(&normalized);
                continue;
            }
        }

        // Check hour
        if (!parsed_.hour.matches(tm.tm_hour)) {
            auto next_hour = parsed_.hour.getNextValue(tm.tm_hour);
            if (!next_hour) {
                // Move to next day
                tm.tm_mday++;
                tm.tm_hour = parsed_.hour.getFirstValue();
            } else {
                tm.tm_hour = *next_hour;
            }
            tm.tm_min = 0;

            // Normalize and continue
            normalized = std::mktime(&tm);
            tm = *std::gmtime(&normalized);
            continue;
        }

        // Check minute
        if (!parsed_.minute.matches(tm.tm_min)) {
            auto next_minute = parsed_.minute.getNextValue(tm.tm_min);
            if (!next_minute) {
                // Move to next hour
                tm.tm_hour++;
                tm.tm_min = parsed_.minute.getFirstValue();
            } else {
                tm.tm_min = *next_minute;
            }

            // Normalize and continue
            normalized = std::mktime(&tm);
            tm = *std::gmtime(&normalized);
            continue;
        }

        // All fields match - we found the next run time
        #ifdef _WIN32
        auto result_time = _mkgmtime(&tm);
        #else
        auto result_time = timegm(&tm);
        #endif

        return std::chrono::system_clock::from_time_t(result_time);
    }

    // No valid time found within limit
    return std::nullopt;
}

std::vector<std::chrono::system_clock::time_point> CronParser::getNextRunTimes(
    int count,
    std::chrono::system_clock::time_point from) const {

    std::vector<std::chrono::system_clock::time_point> times;

    auto current = from;
    for (int i = 0; i < count; ++i) {
        auto next = getNextRunTime(current);
        if (!next) {
            break;
        }
        times.push_back(*next);
        current = *next + std::chrono::seconds(1);  // Move past this time
    }

    return times;
}

std::optional<std::chrono::system_clock::time_point> CronParser::getNextRunTime(
    const std::string& expression) {
    CronParser parser;
    if (!parser.parse(expression)) {
        return std::nullopt;
    }
    return parser.getNextRunTime();
}

std::optional<std::chrono::system_clock::time_point> CronParser::getNextRunTime(
    const std::string& expression,
    std::chrono::system_clock::time_point from) {
    CronParser parser;
    if (!parser.parse(expression)) {
        return std::nullopt;
    }
    return parser.getNextRunTime(from);
}

// ============================================================================
// MATCHING
// ============================================================================

bool CronParser::matches(std::chrono::system_clock::time_point time) const {
    if (!parsed_.valid) {
        return false;
    }

    auto time_t_val = std::chrono::system_clock::to_time_t(time);
    std::tm tm = *std::gmtime(&time_t_val);

    return parsed_.minute.matches(tm.tm_min) &&
           parsed_.hour.matches(tm.tm_hour) &&
           parsed_.day.matches(tm.tm_mday) &&
           parsed_.month.matches(tm.tm_mon + 1) &&
           parsed_.weekday.matches(tm.tm_wday);
}

bool CronParser::matchesNow() const {
    return matches(std::chrono::system_clock::now());
}

// ============================================================================
// SPECIAL EXPRESSIONS
// ============================================================================

std::string CronParser::namedScheduleToCron(const std::string& name) {
    std::string upper_name = toUpper(name);

    if (upper_name == "@YEARLY" || upper_name == "@ANNUALLY") {
        return "0 0 1 1 *";
    }
    if (upper_name == "@MONTHLY") {
        return "0 0 1 * *";
    }
    if (upper_name == "@WEEKLY") {
        return "0 0 * * 0";
    }
    if (upper_name == "@DAILY" || upper_name == "@MIDNIGHT") {
        return "0 0 * * *";
    }
    if (upper_name == "@HOURLY") {
        return "0 * * * *";
    }

    return "";  // Unknown
}

std::string CronParser::describe() const {
    if (!parsed_.valid) {
        return "Invalid cron expression";
    }

    std::ostringstream oss;

    // Describe minute
    if (parsed_.minute.any) {
        oss << "every minute";
    } else if (parsed_.minute.values.size() == 1) {
        oss << "at minute " << *parsed_.minute.values.begin();
    } else {
        oss << "at minutes ";
        bool first = true;
        for (int v : parsed_.minute.values) {
            if (!first) oss << ",";
            oss << v;
            first = false;
        }
    }

    // Describe hour
    if (!parsed_.hour.any) {
        oss << " of hour";
        if (parsed_.hour.values.size() == 1) {
            oss << " " << *parsed_.hour.values.begin();
        } else {
            oss << "s ";
            bool first = true;
            for (int v : parsed_.hour.values) {
                if (!first) oss << ",";
                oss << v;
                first = false;
            }
        }
    }

    // Describe day
    if (!parsed_.day.any) {
        oss << " on day";
        if (parsed_.day.values.size() == 1) {
            oss << " " << *parsed_.day.values.begin();
        } else {
            oss << "s ";
            bool first = true;
            for (int v : parsed_.day.values) {
                if (!first) oss << ",";
                oss << v;
                first = false;
            }
        }
    }

    // Describe month
    if (!parsed_.month.any) {
        static const char* month_names[] = {
            "", "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        };

        oss << " in";
        for (int v : parsed_.month.values) {
            oss << " " << month_names[v];
        }
    }

    // Describe weekday
    if (!parsed_.weekday.any) {
        static const char* day_names[] = {
            "Sunday", "Monday", "Tuesday", "Wednesday",
            "Thursday", "Friday", "Saturday"
        };

        oss << " on";
        for (int v : parsed_.weekday.values) {
            oss << " " << day_names[v];
        }
    }

    return oss.str();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool CronParser::parseField(const std::string& field_str, int min_val, int max_val,
                            CronField& field) {
    field.values.clear();
    field.any = false;

    std::string trimmed = trim(field_str);
    if (trimmed.empty()) {
        return false;
    }

    // Handle * (any)
    if (trimmed == "*") {
        field.any = true;
        return true;
    }

    // Split by comma
    std::vector<std::string> parts = split(trimmed, ',');

    for (const auto& part : parts) {
        if (!parseFieldPart(trim(part), min_val, max_val, field)) {
            return false;
        }
    }

    return !field.values.empty() || field.any;
}

bool CronParser::parseFieldPart(const std::string& part, int min_val, int max_val,
                                CronField& field) {
    // Handle step values (*/n or n-m/s)
    size_t step_pos = part.find('/');
    int step = 1;

    std::string range_part = part;
    if (step_pos != std::string::npos) {
        std::string step_str = part.substr(step_pos + 1);
        range_part = part.substr(0, step_pos);

        if (!parseInt(step_str, step) || step <= 0) {
            return false;
        }
    }

    // Handle * with step
    if (range_part == "*") {
        for (int v = min_val; v <= max_val; v += step) {
            field.values.insert(v);
        }
        return true;
    }

    // Handle range (n-m)
    size_t range_pos = range_part.find('-');
    if (range_pos != std::string::npos) {
        std::string start_str = range_part.substr(0, range_pos);
        std::string end_str = range_part.substr(range_pos + 1);

        int start, end;

        // Try numeric first
        if (!parseInt(start_str, start)) {
            // Try month/weekday names
            start = monthNameToNumber(start_str);
            if (start < 0) start = weekdayNameToNumber(start_str);
            if (start < 0) return false;
        }

        if (!parseInt(end_str, end)) {
            end = monthNameToNumber(end_str);
            if (end < 0) end = weekdayNameToNumber(end_str);
            if (end < 0) return false;
        }

        if (start < min_val || start > max_val ||
            end < min_val || end > max_val || start > end) {
            return false;
        }

        for (int v = start; v <= end; v += step) {
            field.values.insert(v);
        }

        return true;
    }

    // Single value
    int value;
    if (!parseInt(range_part, value)) {
        // Try month/weekday names
        value = monthNameToNumber(range_part);
        if (value < 0) value = weekdayNameToNumber(range_part);
        if (value < 0) return false;
    }

    if (value < min_val || value > max_val) {
        return false;
    }

    field.values.insert(value);
    return true;
}

int CronParser::monthNameToNumber(const std::string& name) {
    std::string upper = toUpper(name);

    if (upper == "JAN") return 1;
    if (upper == "FEB") return 2;
    if (upper == "MAR") return 3;
    if (upper == "APR") return 4;
    if (upper == "MAY") return 5;
    if (upper == "JUN") return 6;
    if (upper == "JUL") return 7;
    if (upper == "AUG") return 8;
    if (upper == "SEP") return 9;
    if (upper == "OCT") return 10;
    if (upper == "NOV") return 11;
    if (upper == "DEC") return 12;

    return -1;
}

int CronParser::weekdayNameToNumber(const std::string& name) {
    std::string upper = toUpper(name);

    if (upper == "SUN") return 0;
    if (upper == "MON") return 1;
    if (upper == "TUE") return 2;
    if (upper == "WED") return 3;
    if (upper == "THU") return 4;
    if (upper == "FRI") return 5;
    if (upper == "SAT") return 6;

    return -1;
}

int CronParser::getDayOfWeek(std::chrono::system_clock::time_point time) {
    auto time_t_val = std::chrono::system_clock::to_time_t(time);
    std::tm tm = *std::gmtime(&time_t_val);
    return tm.tm_wday;
}

int CronParser::getDaysInMonth(int year, int month) {
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month < 1 || month > 12) return 0;

    if (month == 2) {
        // Check leap year
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }

    return days[month];
}

} // namespace wtl::workers::scheduler
