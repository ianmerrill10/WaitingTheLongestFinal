/**
 * @file ReportGenerator.h
 * @brief Report generation for analytics data
 *
 * PURPOSE:
 * Generates various reports from analytics data including:
 * - Daily summary reports
 * - Weekly impact reports
 * - Monthly adoption reports
 * - Social performance reports
 * Exports to JSON and CSV formats.
 *
 * USAGE:
 * auto& generator = ReportGenerator::getInstance();
 * std::string report = generator.generateDailyReport(DateRange::yesterday());
 * generator.exportToCsv("report.csv", report_data);
 *
 * DEPENDENCIES:
 * - AnalyticsService (data queries)
 * - ImpactTracker (impact data)
 * - SocialAnalytics (social data)
 *
 * MODIFICATION GUIDE:
 * - Add new report types by creating new generate methods
 * - Update export formats as needed
 * - Keep reports efficient for large datasets
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <json/json.h>

#include "analytics/DashboardStats.h"

namespace wtl::analytics {

/**
 * @struct ReportSection
 * @brief A section of a report with title and data
 */
struct ReportSection {
    std::string title;
    std::string description;
    Json::Value data;
    std::vector<std::string> csv_headers;
    std::vector<std::vector<std::string>> csv_rows;
};

/**
 * @struct Report
 * @brief A complete report with metadata and sections
 */
struct Report {
    std::string id;
    std::string type;                 // daily, weekly, monthly, social, impact
    std::string title;
    DateRange date_range;
    std::chrono::system_clock::time_point generated_at;
    std::vector<ReportSection> sections;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Export to CSV string
     * @return CSV formatted string
     */
    std::string toCsv() const;

    /**
     * @brief Export to plain text
     * @return Plain text formatted report
     */
    std::string toText() const;
};

/**
 * @class ReportGenerator
 * @brief Singleton service for generating analytics reports
 *
 * Generates formatted reports from analytics data for various
 * time periods and metrics. Supports JSON and CSV export formats.
 */
class ReportGenerator {
public:
    /**
     * @brief Get singleton instance
     */
    static ReportGenerator& getInstance();

    // Prevent copying
    ReportGenerator(const ReportGenerator&) = delete;
    ReportGenerator& operator=(const ReportGenerator&) = delete;

    // =========================================================================
    // REPORT GENERATION
    // =========================================================================

    /**
     * @brief Generate a report by type
     * @param type Report type (daily, weekly, monthly, social, impact)
     * @param range Date range for the report
     * @param format Output format (json, csv, text)
     * @return Formatted report string
     */
    std::string generateReport(const std::string& type,
                               const DateRange& range,
                               const std::string& format = "json");

    /**
     * @brief Generate daily summary report
     * @param range Date range (typically yesterday)
     * @return Report object
     */
    Report generateDailySummary(const DateRange& range);

    /**
     * @brief Generate weekly impact report
     * @param range Date range (typically last 7 days)
     * @return Report object
     */
    Report generateWeeklyImpact(const DateRange& range);

    /**
     * @brief Generate monthly adoption report
     * @param range Date range (typically last 30 days)
     * @return Report object
     */
    Report generateMonthlyAdoption(const DateRange& range);

    /**
     * @brief Generate social performance report
     * @param range Date range
     * @return Report object
     */
    Report generateSocialPerformance(const DateRange& range);

    /**
     * @brief Generate shelter performance report
     * @param shelter_id The shelter ID (optional, all if empty)
     * @param range Date range
     * @return Report object
     */
    Report generateShelterReport(const std::string& shelter_id, const DateRange& range);

    /**
     * @brief Generate top dogs report
     * @param range Date range
     * @param metric Metric to rank by (views, shares, applications)
     * @param limit Number of dogs to include
     * @return Report object
     */
    Report generateTopDogsReport(const DateRange& range,
                                  const std::string& metric = "views",
                                  int limit = 20);

    // =========================================================================
    // EXPORT METHODS
    // =========================================================================

    /**
     * @brief Export report to JSON string
     * @param report The report to export
     * @param pretty Pretty print JSON
     * @return JSON string
     */
    std::string exportToJson(const Report& report, bool pretty = true);

    /**
     * @brief Export report to CSV string
     * @param report The report to export
     * @return CSV string
     */
    std::string exportToCsv(const Report& report);

    /**
     * @brief Export report to plain text
     * @param report The report to export
     * @return Plain text string
     */
    std::string exportToText(const Report& report);

    /**
     * @brief Save report to file
     * @param report The report to save
     * @param filename Output filename
     * @param format Output format (json, csv, text)
     * @return True if saved successfully
     */
    bool saveToFile(const Report& report,
                    const std::string& filename,
                    const std::string& format = "json");

    // =========================================================================
    // SCHEDULED REPORTS
    // =========================================================================

    /**
     * @brief Get list of available report types
     * @return Vector of report type names
     */
    std::vector<std::string> getAvailableReportTypes() const;

    /**
     * @brief Get report metadata (without generating full report)
     * @param type Report type
     * @return JSON with report metadata
     */
    Json::Value getReportMetadata(const std::string& type) const;

private:
    /**
     * @brief Private constructor for singleton
     */
    ReportGenerator();

    /**
     * @brief Generate a new report ID
     * @return UUID string
     */
    std::string generateReportId();

    /**
     * @brief Create a report section for engagement metrics
     * @param stats Engagement stats
     * @return Report section
     */
    ReportSection createEngagementSection(const EngagementStats& stats);

    /**
     * @brief Create a report section for conversion metrics
     * @param stats Conversion stats
     * @return Report section
     */
    ReportSection createConversionSection(const ConversionStats& stats);

    /**
     * @brief Create a report section for impact metrics
     * @param summary Impact summary
     * @return Report section
     */
    ReportSection createImpactSection(const ImpactSummary& summary);

    /**
     * @brief Create a report section for top performers
     * @param performers Vector of top performers
     * @param title Section title
     * @return Report section
     */
    ReportSection createTopPerformersSection(const std::vector<TopPerformer>& performers,
                                              const std::string& title);

    /** Mutex for thread safety */
    std::mutex mutex_;
};

} // namespace wtl::analytics
