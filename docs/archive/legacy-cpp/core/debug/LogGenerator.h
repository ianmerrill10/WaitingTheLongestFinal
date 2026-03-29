/**
 * @file LogGenerator.h
 * @brief Log export and report generation for WaitingTheLongest application
 *
 * PURPOSE:
 * Generates human-readable and machine-parseable log reports from
 * captured errors. Supports multiple formats including TXT, JSON,
 * and a special AI-friendly format for Claude analysis.
 *
 * USAGE:
 * // Generate a text report
 * LogGenerator::getInstance().generateTxtReport("errors.txt");
 *
 * // Generate AI-friendly report for analysis
 * LogGenerator::getInstance().generateAIReport("ai_report.txt");
 *
 * DEPENDENCIES:
 * - ErrorCapture (error data source)
 * - SelfHealing (circuit breaker state)
 * - Standard library (filesystem, chrono)
 *
 * MODIFICATION GUIDE:
 * - Add new export formats by implementing generate*Report methods
 * - Customize report templates in the implementation file
 * - Add new sections to reports as needed
 *
 * @author Agent 1 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "core/debug/ErrorCapture.h"

namespace wtl::core::debug {

// ============================================================================
// CONFIGURATION STRUCTURES
// ============================================================================

/**
 * @struct LogExportOptions
 * @brief Options for customizing log exports
 */
struct LogExportOptions {
    // Time range (nullopt means no filter)
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;

    // Category filter (empty means all categories)
    std::vector<ErrorCategory> categories;

    // Minimum severity to include
    ErrorSeverity min_severity = ErrorSeverity::DEBUG;

    // Maximum number of errors to include (0 = no limit)
    size_t max_errors = 0;

    // Include stack traces in output
    bool include_stack_traces = false;

    // Include metadata in output
    bool include_metadata = true;

    // Include statistics summary
    bool include_statistics = true;

    // Include circuit breaker states
    bool include_circuit_states = true;

    // Include system information
    bool include_system_info = true;

    // Group errors by category
    bool group_by_category = false;

    // Sort order (newest first by default)
    bool newest_first = true;
};

/**
 * @struct SystemInfo
 * @brief System information included in reports
 */
struct SystemInfo {
    std::string hostname;
    std::string platform;
    std::string app_version;
    std::string environment;
    std::chrono::system_clock::time_point report_time;
    std::chrono::system_clock::time_point app_start_time;
    std::chrono::seconds uptime;

    Json::Value toJson() const;
};

// ============================================================================
// LOG GENERATOR CLASS
// ============================================================================

/**
 * @class LogGenerator
 * @brief Singleton class for generating log reports
 *
 * Generates reports in multiple formats from captured error data.
 * Supports filtering, sorting, and customization via LogExportOptions.
 */
class LogGenerator {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the LogGenerator singleton
     */
    static LogGenerator& getInstance();

    // Delete copy/move constructors
    LogGenerator(const LogGenerator&) = delete;
    LogGenerator& operator=(const LogGenerator&) = delete;
    LogGenerator(LogGenerator&&) = delete;
    LogGenerator& operator=(LogGenerator&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the log generator
     *
     * @param export_path Base directory for export files
     * @param app_version Application version string
     */
    void initialize(const std::string& export_path = "./logs",
                    const std::string& app_version = "1.0.0");

    /**
     * @brief Initialize from Config singleton
     */
    void initializeFromConfig();

    // =========================================================================
    // TEXT REPORT GENERATION
    // =========================================================================

    /**
     * @brief Generate a human-readable text report
     *
     * @param options Export options
     * @return The report as a string
     *
     * Generates a formatted text report suitable for reading by developers.
     */
    std::string generateTxtReport(const LogExportOptions& options = {}) const;

    /**
     * @brief Generate text report and save to file
     *
     * @param file_path Path to output file
     * @param options Export options
     * @return true if successful, false otherwise
     */
    bool generateTxtReport(const std::string& file_path,
                           const LogExportOptions& options = {}) const;

    // =========================================================================
    // JSON REPORT GENERATION
    // =========================================================================

    /**
     * @brief Generate a JSON report
     *
     * @param options Export options
     * @return The report as a JSON object
     *
     * Generates a structured JSON report suitable for parsing.
     */
    Json::Value generateJsonReport(const LogExportOptions& options = {}) const;

    /**
     * @brief Generate JSON report and save to file
     *
     * @param file_path Path to output file
     * @param options Export options
     * @return true if successful, false otherwise
     */
    bool generateJsonReport(const std::string& file_path,
                            const LogExportOptions& options = {}) const;

    // =========================================================================
    // AI REPORT GENERATION
    // =========================================================================

    /**
     * @brief Generate a report formatted for AI analysis (Claude)
     *
     * @param options Export options
     * @return The report as a string
     *
     * Generates a specially formatted report optimized for analysis by
     * Claude or other AI assistants. Includes context and prompts
     * for effective analysis.
     *
     * Format includes:
     * - Executive summary of errors
     * - Errors grouped by category with analysis hints
     * - Pattern detection suggestions
     * - Recommended actions section
     */
    std::string generateAIReport(const LogExportOptions& options = {}) const;

    /**
     * @brief Generate AI report and save to file
     *
     * @param file_path Path to output file
     * @param options Export options
     * @return true if successful, false otherwise
     */
    bool generateAIReport(const std::string& file_path,
                          const LogExportOptions& options = {}) const;

    // =========================================================================
    // EXPORT TO FILE
    // =========================================================================

    /**
     * @brief Export content to a file
     *
     * @param file_path Path to output file
     * @param content Content to write
     * @return true if successful, false otherwise
     *
     * Generic file export function used by all report generators.
     * Creates directories if they don't exist.
     */
    bool exportToFile(const std::string& file_path,
                      const std::string& content) const;

    /**
     * @brief Export JSON to a file
     *
     * @param file_path Path to output file
     * @param json JSON content to write
     * @param pretty_print Whether to format output nicely
     * @return true if successful, false otherwise
     */
    bool exportToFile(const std::string& file_path,
                      const Json::Value& json,
                      bool pretty_print = true) const;

    // =========================================================================
    // QUICK EXPORT METHODS
    // =========================================================================

    /**
     * @brief Quick export of all recent errors
     *
     * @param minutes Number of minutes to look back
     * @return Path to the generated file
     *
     * Generates a timestamped file in the export directory.
     */
    std::string exportRecentErrors(int minutes = 60) const;

    /**
     * @brief Quick export of critical errors only
     *
     * @return Path to the generated file
     */
    std::string exportCriticalErrors() const;

    /**
     * @brief Generate a daily summary report
     *
     * @return Path to the generated file
     */
    std::string generateDailySummary() const;

    // =========================================================================
    // SYSTEM INFORMATION
    // =========================================================================

    /**
     * @brief Get current system information
     * @return SystemInfo struct
     */
    SystemInfo getSystemInfo() const;

    /**
     * @brief Set application start time (call at startup)
     *
     * @param start_time Application start time
     */
    void setAppStartTime(std::chrono::system_clock::time_point start_time);

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Set export directory
     *
     * @param path Directory path for exports
     */
    void setExportPath(const std::string& path);

    /**
     * @brief Get export directory
     * @return Current export directory path
     */
    std::string getExportPath() const;

    /**
     * @brief Set application version
     *
     * @param version Version string
     */
    void setAppVersion(const std::string& version);

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    LogGenerator() = default;
    ~LogGenerator() = default;

    /**
     * @brief Get filtered errors based on options
     *
     * @param options Filter options
     * @return Filtered vector of errors
     */
    std::vector<ErrorContext> getFilteredErrors(
        const LogExportOptions& options) const;

    /**
     * @brief Format a timestamp for display
     *
     * @param tp Time point to format
     * @return Formatted string
     */
    std::string formatTimestamp(
        std::chrono::system_clock::time_point tp) const;

    /**
     * @brief Generate timestamp string for filenames
     * @return Filename-safe timestamp
     */
    std::string generateFileTimestamp() const;

    /**
     * @brief Generate section separator for text reports
     *
     * @param title Section title
     * @param width Line width
     * @return Formatted separator
     */
    std::string generateSectionHeader(const std::string& title,
                                       int width = 80) const;

    /**
     * @brief Generate error summary for AI report
     *
     * @param errors List of errors to summarize
     * @return Summary text
     */
    std::string generateErrorSummary(
        const std::vector<ErrorContext>& errors) const;

    /**
     * @brief Generate pattern analysis hints for AI
     *
     * @param errors List of errors to analyze
     * @return Analysis hints text
     */
    std::string generatePatternHints(
        const std::vector<ErrorContext>& errors) const;

    // Export directory path
    std::string export_path_{"./logs"};

    // Application version
    std::string app_version_{"1.0.0"};

    // Application start time
    std::chrono::system_clock::time_point app_start_time_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::core::debug
