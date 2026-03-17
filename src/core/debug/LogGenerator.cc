/**
 * @file LogGenerator.cc
 * @brief Implementation of log report generation
 * @see LogGenerator.h for documentation
 */

#include "core/debug/LogGenerator.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

// Standard library includes
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// Platform-specific includes
#ifndef _WIN32
#include <unistd.h>
#endif

namespace wtl::core::debug {

// ============================================================================
// SYSTEM INFO IMPLEMENTATION
// ============================================================================

Json::Value SystemInfo::toJson() const {
    Json::Value json;

    json["hostname"] = hostname;
    json["platform"] = platform;
    json["app_version"] = app_version;
    json["environment"] = environment;

    // Format timestamps
    auto format_time = [](std::chrono::system_clock::time_point tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_buf;
#ifdef _WIN32
        gmtime_s(&tm_buf, &time_t);
#else
        gmtime_r(&time_t, &tm_buf);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    };

    json["report_time"] = format_time(report_time);
    json["app_start_time"] = format_time(app_start_time);
    json["uptime_seconds"] = static_cast<Json::Int64>(uptime.count());

    return json;
}

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

LogGenerator& LogGenerator::getInstance() {
    static LogGenerator instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void LogGenerator::initialize(const std::string& export_path,
                               const std::string& app_version) {
    std::lock_guard<std::mutex> lock(mutex_);

    export_path_ = export_path;
    app_version_ = app_version;
    app_start_time_ = std::chrono::system_clock::now();

    // Create export directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(export_path_, ec);

    if (ec) {
        std::cerr << "[LogGenerator] Warning: Could not create export directory: "
                  << ec.message() << std::endl;
    }

    std::cout << "[LogGenerator] Initialized with export_path=" << export_path_
              << std::endl;
}

void LogGenerator::initializeFromConfig() {
    auto& config = wtl::core::utils::Config::getInstance();

    std::string export_path = config.getString("debug.error_export_path",
                                                "./logs/errors");

    initialize(export_path, "1.0.0");
}

// ============================================================================
// TEXT REPORT GENERATION
// ============================================================================

std::string LogGenerator::generateTxtReport(const LogExportOptions& options) const {
    std::ostringstream report;

    auto errors = getFilteredErrors(options);
    auto sys_info = getSystemInfo();

    // Header
    report << generateSectionHeader("WAITINGTHELONGEST ERROR REPORT");
    report << "\n";

    // System info
    if (options.include_system_info) {
        report << "Report Time:     " << formatTimestamp(sys_info.report_time) << "\n";
        report << "Environment:     " << sys_info.environment << "\n";
        report << "App Version:     " << sys_info.app_version << "\n";
        report << "Platform:        " << sys_info.platform << "\n";
        report << "Uptime:          " << sys_info.uptime.count() << " seconds\n";
        report << "\n";
    }

    // Statistics summary
    if (options.include_statistics) {
        auto stats = ErrorCapture::getInstance().getStats();

        report << generateSectionHeader("STATISTICS");
        report << "\n";
        report << "Total Errors in Report:  " << errors.size() << "\n";
        report << "Total Captured (all):    " << stats["total_captured"].asUInt64() << "\n";
        report << "\n";

        report << "By Severity:\n";
        const auto& by_sev = stats["by_severity"];
        report << "  CRITICAL: " << by_sev["critical"].asInt() << "\n";
        report << "  ERROR:    " << by_sev["error"].asInt() << "\n";
        report << "  WARNING:  " << by_sev["warning"].asInt() << "\n";
        report << "  INFO:     " << by_sev["info"].asInt() << "\n";
        report << "  DEBUG:    " << by_sev["debug"].asInt() << "\n";
        report << "\n";

        report << "By Category:\n";
        const auto& by_cat = stats["by_category"];
        for (const auto& cat : by_cat.getMemberNames()) {
            int count = by_cat[cat].asInt();
            if (count > 0) {
                report << "  " << std::left << std::setw(15) << cat << ": "
                       << count << "\n";
            }
        }
        report << "\n";
    }

    // Circuit breaker states
    if (options.include_circuit_states) {
        auto circuit_states = SelfHealing::getInstance().getAllCircuitStates();

        if (!circuit_states.empty()) {
            report << generateSectionHeader("CIRCUIT BREAKERS");
            report << "\n";

            for (const auto& [name, state] : circuit_states) {
                report << "Circuit: " << name << "\n";
                report << "  State:    " << SelfHealing::stateToString(state.state) << "\n";
                report << "  Failures: " << state.failure_count << "\n";
                report << "\n";
            }
        }
    }

    // Error listing
    report << generateSectionHeader("ERROR DETAILS");
    report << "\n";

    if (errors.empty()) {
        report << "No errors match the specified criteria.\n";
    } else {
        int error_num = 0;
        for (const auto& err : errors) {
            ++error_num;
            report << "--- Error #" << error_num << " ---\n";
            report << "ID:        " << err.id << "\n";
            report << "Time:      " << formatTimestamp(err.timestamp) << "\n";
            report << "Severity:  " << ErrorCapture::severityToString(err.severity) << "\n";
            report << "Category:  " << ErrorCapture::categoryToString(err.category) << "\n";
            report << "Message:   " << err.message << "\n";

            if (!err.file.empty()) {
                report << "Location:  " << err.file << ":" << err.line
                       << " (" << err.function << ")\n";
            }

            if (options.include_metadata && !err.metadata.empty()) {
                report << "Metadata:\n";
                for (const auto& [key, value] : err.metadata) {
                    report << "  " << key << ": " << value << "\n";
                }
            }

            if (options.include_stack_traces && !err.stack_trace.empty()) {
                report << "Stack Trace:\n" << err.stack_trace << "\n";
            }

            report << "\n";
        }
    }

    report << generateSectionHeader("END OF REPORT");

    return report.str();
}

bool LogGenerator::generateTxtReport(const std::string& file_path,
                                      const LogExportOptions& options) const {
    std::string content = generateTxtReport(options);
    return exportToFile(file_path, content);
}

// ============================================================================
// JSON REPORT GENERATION
// ============================================================================

Json::Value LogGenerator::generateJsonReport(const LogExportOptions& options) const {
    Json::Value report;

    auto errors = getFilteredErrors(options);
    auto sys_info = getSystemInfo();

    // Metadata
    report["generated_at"] = formatTimestamp(sys_info.report_time);
    report["generator_version"] = "1.0.0";

    // System info
    if (options.include_system_info) {
        report["system"] = sys_info.toJson();
    }

    // Statistics
    if (options.include_statistics) {
        report["statistics"] = ErrorCapture::getInstance().getStats();
    }

    // Circuit breakers
    if (options.include_circuit_states) {
        auto circuit_states = SelfHealing::getInstance().getAllCircuitStates();

        Json::Value circuits(Json::objectValue);
        for (const auto& [name, state] : circuit_states) {
            circuits[name] = state.toJson();
        }
        report["circuit_breakers"] = circuits;
    }

    // Errors
    Json::Value errors_json(Json::arrayValue);
    for (const auto& err : errors) {
        errors_json.append(err.toJson());
    }
    report["errors"] = errors_json;
    report["error_count"] = static_cast<int>(errors.size());

    return report;
}

bool LogGenerator::generateJsonReport(const std::string& file_path,
                                       const LogExportOptions& options) const {
    Json::Value json = generateJsonReport(options);
    return exportToFile(file_path, json, true);
}

// ============================================================================
// AI REPORT GENERATION
// ============================================================================

std::string LogGenerator::generateAIReport(const LogExportOptions& options) const {
    std::ostringstream report;

    auto errors = getFilteredErrors(options);
    auto sys_info = getSystemInfo();

    // AI-friendly header with context
    report << "# WaitingTheLongest Error Analysis Report\n\n";

    report << "## Context for AI Analysis\n\n";
    report << "This report contains error logs from WaitingTheLongest.com, a dog rescue platform.\n";
    report << "The platform connects shelters with adopters and tracks dogs waiting for adoption.\n";
    report << "Your task is to analyze these errors and identify patterns, root causes, and solutions.\n\n";

    report << "**Environment:** " << sys_info.environment << "\n";
    report << "**Report Time:** " << formatTimestamp(sys_info.report_time) << "\n";
    report << "**Uptime:** " << sys_info.uptime.count() << " seconds\n\n";

    // Executive summary
    report << "## Executive Summary\n\n";
    report << generateErrorSummary(errors);
    report << "\n";

    // Pattern analysis hints
    report << "## Pattern Analysis Hints\n\n";
    report << generatePatternHints(errors);
    report << "\n";

    // Errors by category for easier analysis
    report << "## Errors by Category\n\n";

    std::unordered_map<ErrorCategory, std::vector<ErrorContext>> by_category;
    for (const auto& err : errors) {
        by_category[err.category].push_back(err);
    }

    for (const auto& [category, cat_errors] : by_category) {
        std::string cat_name = ErrorCapture::categoryToString(category);
        report << "### " << cat_name << " (" << cat_errors.size() << " errors)\n\n";

        // Analysis prompt for this category
        switch (category) {
            case ErrorCategory::DATABASE:
                report << "*Analysis focus: Look for connection issues, query timeouts, "
                       << "or schema problems.*\n\n";
                break;
            case ErrorCategory::EXTERNAL_API:
                report << "*Analysis focus: Check for API rate limits, timeout patterns, "
                       << "or integration failures.*\n\n";
                break;
            case ErrorCategory::AUTHENTICATION:
                report << "*Analysis focus: Look for brute force attempts, token issues, "
                       << "or session problems.*\n\n";
                break;
            case ErrorCategory::VALIDATION:
                report << "*Analysis focus: Identify common validation failures and "
                       << "potential UX improvements.*\n\n";
                break;
            default:
                report << "*Analyze these errors for common patterns and root causes.*\n\n";
        }

        // List errors (limited for readability)
        int shown = 0;
        for (const auto& err : cat_errors) {
            if (shown >= 10) {
                report << "... and " << (cat_errors.size() - 10) << " more errors\n";
                break;
            }

            report << "- **" << formatTimestamp(err.timestamp) << "** ["
                   << ErrorCapture::severityToString(err.severity) << "]\n";
            report << "  " << err.message << "\n";

            if (!err.metadata.empty()) {
                report << "  Metadata: ";
                for (const auto& [key, value] : err.metadata) {
                    report << key << "=" << value << " ";
                }
                report << "\n";
            }
            report << "\n";
            ++shown;
        }

        report << "\n";
    }

    // Circuit breaker analysis
    auto circuit_states = SelfHealing::getInstance().getAllCircuitStates();
    if (!circuit_states.empty()) {
        report << "## Circuit Breaker States\n\n";
        report << "*Circuit breakers indicate services that have experienced repeated failures.*\n\n";

        for (const auto& [name, state] : circuit_states) {
            std::string state_str = SelfHealing::stateToString(state.state);
            report << "- **" << name << "**: " << state_str;

            if (state.state == CircuitState::OPEN) {
                report << " (ATTENTION: Service is failing)";
            }
            report << "\n";
            report << "  Failure count: " << state.failure_count << "\n\n";
        }
    }

    // Analysis questions for AI
    report << "## Analysis Questions\n\n";
    report << "Please analyze the above errors and answer:\n\n";
    report << "1. What are the main categories of errors and their frequencies?\n";
    report << "2. Are there any patterns in timing (e.g., errors clustering at certain times)?\n";
    report << "3. Which errors seem related or have a common root cause?\n";
    report << "4. What are the highest priority issues to fix?\n";
    report << "5. Are there any potential security concerns?\n";
    report << "6. What specific code changes would you recommend?\n";
    report << "7. Are there any infrastructure or configuration issues?\n\n";

    // Raw data section
    report << "## Raw Error Data (JSON)\n\n";
    report << "```json\n";

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    Json::Value errors_json(Json::arrayValue);
    for (const auto& err : errors) {
        errors_json.append(err.toJson());
    }
    report << Json::writeString(builder, errors_json);
    report << "\n```\n";

    return report.str();
}

bool LogGenerator::generateAIReport(const std::string& file_path,
                                     const LogExportOptions& options) const {
    std::string content = generateAIReport(options);
    return exportToFile(file_path, content);
}

// ============================================================================
// EXPORT TO FILE
// ============================================================================

bool LogGenerator::exportToFile(const std::string& file_path,
                                 const std::string& content) const {
    try {
        // Create directory if needed
        std::filesystem::path path(file_path);
        if (path.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec) {
                std::cerr << "[LogGenerator] Could not create directory: "
                          << ec.message() << std::endl;
                return false;
            }
        }

        // Write file
        std::ofstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "[LogGenerator] Could not open file for writing: "
                      << file_path << std::endl;
            return false;
        }

        file << content;
        file.close();

        std::cout << "[LogGenerator] Exported report to: " << file_path << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[LogGenerator] Export failed: " << e.what() << std::endl;
        return false;
    }
}

bool LogGenerator::exportToFile(const std::string& file_path,
                                 const Json::Value& json,
                                 bool pretty_print) const {
    Json::StreamWriterBuilder builder;
    if (pretty_print) {
        builder["indentation"] = "  ";
    } else {
        builder["indentation"] = "";
    }

    std::string content = Json::writeString(builder, json);
    return exportToFile(file_path, content);
}

// ============================================================================
// QUICK EXPORT METHODS
// ============================================================================

std::string LogGenerator::exportRecentErrors(int minutes) const {
    std::string filename = export_path_ + "/recent_errors_" +
                           generateFileTimestamp() + ".txt";

    LogExportOptions options;
    options.start_time = std::chrono::system_clock::now() -
                         std::chrono::minutes(minutes);

    generateTxtReport(filename, options);
    return filename;
}

std::string LogGenerator::exportCriticalErrors() const {
    std::string filename = export_path_ + "/critical_errors_" +
                           generateFileTimestamp() + ".txt";

    LogExportOptions options;
    options.min_severity = ErrorSeverity::CRITICAL;

    generateTxtReport(filename, options);
    return filename;
}

std::string LogGenerator::generateDailySummary() const {
    std::string filename = export_path_ + "/daily_summary_" +
                           generateFileTimestamp() + ".json";

    LogExportOptions options;
    options.start_time = std::chrono::system_clock::now() -
                         std::chrono::hours(24);
    options.include_statistics = true;
    options.include_circuit_states = true;

    generateJsonReport(filename, options);
    return filename;
}

// ============================================================================
// SYSTEM INFORMATION
// ============================================================================

SystemInfo LogGenerator::getSystemInfo() const {
    SystemInfo info;

    // Get hostname
#ifdef _WIN32
    char hostname[256];
    DWORD size = sizeof(hostname);
    GetComputerNameA(hostname, &size);
    info.hostname = hostname;
    info.platform = "Windows";
#else
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    info.hostname = hostname;
    #ifdef __linux__
        info.platform = "Linux";
    #elif __APPLE__
        info.platform = "macOS";
    #else
        info.platform = "Unix";
    #endif
#endif

    info.app_version = app_version_;

    // Get environment from config if available
    if (wtl::core::utils::Config::getInstance().isLoaded()) {
        info.environment = wtl::core::utils::Config::getInstance().getEnvironment();
    } else {
        info.environment = "unknown";
    }

    info.report_time = std::chrono::system_clock::now();
    info.app_start_time = app_start_time_;
    info.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        info.report_time - app_start_time_);

    return info;
}

void LogGenerator::setAppStartTime(std::chrono::system_clock::time_point start_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    app_start_time_ = start_time;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void LogGenerator::setExportPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    export_path_ = path;

    // Create directory if needed
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
}

std::string LogGenerator::getExportPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return export_path_;
}

void LogGenerator::setAppVersion(const std::string& version) {
    std::lock_guard<std::mutex> lock(mutex_);
    app_version_ = version;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::vector<ErrorContext> LogGenerator::getFilteredErrors(
    const LogExportOptions& options) const {

    // Build filter function
    auto filter = [&options](const ErrorContext& err) {
        // Severity filter
        if (err.severity < options.min_severity) {
            return false;
        }

        // Time range filter
        if (options.start_time.has_value() &&
            err.timestamp < *options.start_time) {
            return false;
        }
        if (options.end_time.has_value() &&
            err.timestamp > *options.end_time) {
            return false;
        }

        // Category filter
        if (!options.categories.empty()) {
            bool found = false;
            for (const auto& cat : options.categories) {
                if (err.category == cat) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }

        return true;
    };

    auto errors = ErrorCapture::getInstance().query(filter, options.max_errors);

    // Sort if needed
    if (options.newest_first) {
        std::sort(errors.begin(), errors.end(),
                  [](const ErrorContext& a, const ErrorContext& b) {
                      return a.timestamp > b.timestamp;
                  });
    } else {
        std::sort(errors.begin(), errors.end(),
                  [](const ErrorContext& a, const ErrorContext& b) {
                      return a.timestamp < b.timestamp;
                  });
    }

    return errors;
}

std::string LogGenerator::formatTimestamp(
    std::chrono::system_clock::time_point tp) const {

    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string LogGenerator::generateFileTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
    return ss.str();
}

std::string LogGenerator::generateSectionHeader(const std::string& title,
                                                  int width) const {
    std::ostringstream ss;
    std::string line(width, '=');

    ss << line << "\n";
    int padding = (width - static_cast<int>(title.length())) / 2;
    ss << std::string(padding, ' ') << title << "\n";
    ss << line << "\n";

    return ss.str();
}

std::string LogGenerator::generateErrorSummary(
    const std::vector<ErrorContext>& errors) const {

    std::ostringstream summary;

    if (errors.empty()) {
        summary << "No errors found in the specified time range.\n";
        return summary.str();
    }

    // Count by severity
    int critical = 0, error = 0, warning = 0, info = 0;
    for (const auto& err : errors) {
        switch (err.severity) {
            case ErrorSeverity::CRITICAL: ++critical; break;
            case ErrorSeverity::ERROR: ++error; break;
            case ErrorSeverity::WARNING: ++warning; break;
            default: ++info; break;
        }
    }

    summary << "**Total Errors:** " << errors.size() << "\n";
    summary << "- Critical: " << critical << "\n";
    summary << "- Error: " << error << "\n";
    summary << "- Warning: " << warning << "\n";
    summary << "- Info/Debug: " << info << "\n\n";

    if (critical > 0) {
        summary << "**ATTENTION:** There are " << critical
                << " CRITICAL errors requiring immediate attention.\n\n";
    }

    return summary.str();
}

std::string LogGenerator::generatePatternHints(
    const std::vector<ErrorContext>& errors) const {

    std::ostringstream hints;

    if (errors.empty()) {
        return "No errors to analyze.\n";
    }

    // Analyze message patterns
    std::unordered_map<std::string, int> message_prefixes;
    for (const auto& err : errors) {
        // Get first 50 chars of message as a pattern
        std::string prefix = err.message.substr(0, std::min(size_t(50),
                                                             err.message.length()));
        message_prefixes[prefix]++;
    }

    // Find repeated patterns
    std::vector<std::pair<std::string, int>> sorted_patterns(
        message_prefixes.begin(), message_prefixes.end());
    std::sort(sorted_patterns.begin(), sorted_patterns.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    hints << "**Repeated Error Patterns:**\n\n";
    int shown = 0;
    for (const auto& [pattern, count] : sorted_patterns) {
        if (count < 2 || shown >= 5) break;
        hints << "- \"" << pattern << "...\" appears " << count << " times\n";
        ++shown;
    }

    if (shown == 0) {
        hints << "No repeated patterns detected.\n";
    }

    hints << "\n**Suggestions:**\n";
    hints << "- Look for errors that cluster in time (may indicate cascading failures)\n";
    hints << "- Check if database errors correlate with external API errors\n";
    hints << "- Review authentication errors for potential security issues\n";

    return hints.str();
}

} // namespace wtl::core::debug
