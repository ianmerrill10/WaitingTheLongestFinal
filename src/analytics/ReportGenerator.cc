/**
 * @file ReportGenerator.cc
 * @brief Implementation of ReportGenerator
 * @see ReportGenerator.h for documentation
 */

#include "analytics/ReportGenerator.h"

#include <fstream>
#include <iomanip>
#include <sstream>

#include "analytics/AnalyticsEvent.h"
#include "analytics/AnalyticsService.h"
#include "analytics/ImpactTracker.h"
#include "analytics/SocialAnalytics.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::analytics {

using namespace ::wtl::core::debug;

// ============================================================================
// REPORT
// ============================================================================

Json::Value Report::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["type"] = type;
    json["title"] = title;
    json["date_range"] = date_range.toJson();
    json["generated_at"] = formatTimestampISO(generated_at);

    Json::Value sections_arr(Json::arrayValue);
    for (const auto& section : sections) {
        Json::Value sec_json;
        sec_json["title"] = section.title;
        sec_json["description"] = section.description;
        sec_json["data"] = section.data;
        sections_arr.append(sec_json);
    }
    json["sections"] = sections_arr;

    return json;
}

std::string Report::toCsv() const {
    std::ostringstream ss;

    // Header
    ss << "Report: " << title << "\n";
    ss << "Type: " << type << "\n";
    ss << "Period: " << date_range.getStartISO().substr(0, 10) << " to "
       << date_range.getEndISO().substr(0, 10) << "\n";
    ss << "Generated: " << formatTimestampISO(generated_at) << "\n\n";

    for (const auto& section : sections) {
        ss << "\n" << section.title << "\n";
        ss << std::string(section.title.length(), '=') << "\n";

        if (!section.csv_headers.empty()) {
            for (size_t i = 0; i < section.csv_headers.size(); i++) {
                if (i > 0) ss << ",";
                ss << "\"" << section.csv_headers[i] << "\"";
            }
            ss << "\n";

            for (const auto& row : section.csv_rows) {
                for (size_t i = 0; i < row.size(); i++) {
                    if (i > 0) ss << ",";
                    ss << "\"" << row[i] << "\"";
                }
                ss << "\n";
            }
        } else {
            // Flatten JSON data to CSV-like format
            for (const auto& key : section.data.getMemberNames()) {
                ss << key << "," << section.data[key].asString() << "\n";
            }
        }
    }

    return ss.str();
}

std::string Report::toText() const {
    std::ostringstream ss;

    ss << "========================================\n";
    ss << title << "\n";
    ss << "========================================\n\n";

    ss << "Report ID: " << id << "\n";
    ss << "Type: " << type << "\n";
    ss << "Period: " << date_range.getStartISO().substr(0, 10) << " to "
       << date_range.getEndISO().substr(0, 10) << "\n";
    ss << "Generated: " << formatTimestampISO(generated_at) << "\n";
    ss << "\n";

    for (const auto& section : sections) {
        ss << "\n" << section.title << "\n";
        ss << std::string(section.title.length(), '-') << "\n";

        if (!section.description.empty()) {
            ss << section.description << "\n\n";
        }

        // Output data in readable format
        for (const auto& key : section.data.getMemberNames()) {
            const auto& value = section.data[key];
            ss << "  " << key << ": ";

            if (value.isInt()) {
                ss << value.asInt();
            } else if (value.isDouble()) {
                ss << std::fixed << std::setprecision(2) << value.asDouble();
            } else {
                ss << value.asString();
            }
            ss << "\n";
        }
    }

    return ss.str();
}

// ============================================================================
// SINGLETON
// ============================================================================

ReportGenerator& ReportGenerator::getInstance() {
    static ReportGenerator instance;
    return instance;
}

ReportGenerator::ReportGenerator() {
}

// ============================================================================
// REPORT GENERATION
// ============================================================================

std::string ReportGenerator::generateReport(const std::string& type,
                                             const DateRange& range,
                                             const std::string& format)
{
    Report report;

    if (type == "daily") {
        report = generateDailySummary(range);
    } else if (type == "weekly") {
        report = generateWeeklyImpact(range);
    } else if (type == "monthly") {
        report = generateMonthlyAdoption(range);
    } else if (type == "social") {
        report = generateSocialPerformance(range);
    } else {
        report = generateDailySummary(range);
    }

    if (format == "csv") {
        return exportToCsv(report);
    } else if (format == "text") {
        return exportToText(report);
    } else {
        return exportToJson(report, true);
    }
}

Report ReportGenerator::generateDailySummary(const DateRange& range) {
    Report report;
    report.id = generateReportId();
    report.type = "daily";
    report.title = "Daily Analytics Summary";
    report.date_range = range;
    report.generated_at = std::chrono::system_clock::now();

    auto& service = AnalyticsService::getInstance();

    // Engagement section
    auto engagement = service.getEngagementStats(range);
    report.sections.push_back(createEngagementSection(engagement));

    // Conversion section
    auto conversions = service.getConversionStats(range);
    report.sections.push_back(createConversionSection(conversions));

    // Real-time stats
    auto real_time = service.getRealTimeStats();
    ReportSection rt_section;
    rt_section.title = "Current Status";
    rt_section.description = "Real-time platform metrics";
    rt_section.data["active_dogs"] = real_time.active_dogs;
    rt_section.data["urgent_dogs"] = real_time.urgent_dogs;
    rt_section.data["critical_dogs"] = real_time.critical_dogs;
    rt_section.data["pending_applications"] = real_time.pending_applications;
    report.sections.push_back(rt_section);

    // Top viewed dogs
    auto top_dogs = service.getMostViewedDogs(range, 10);
    report.sections.push_back(createTopPerformersSection(top_dogs, "Most Viewed Dogs"));

    return report;
}

Report ReportGenerator::generateWeeklyImpact(const DateRange& range) {
    Report report;
    report.id = generateReportId();
    report.type = "weekly";
    report.title = "Weekly Impact Report";
    report.date_range = range;
    report.generated_at = std::chrono::system_clock::now();

    auto& impact = ImpactTracker::getInstance();

    // Impact summary
    auto summary = impact.getImpactSummary(range);
    report.sections.push_back(createImpactSection(summary));

    // Detailed metrics
    auto metrics = impact.getImpactMetrics(range);
    ReportSection metrics_section;
    metrics_section.title = "Detailed Impact Metrics";
    metrics_section.data["total_lives_saved"] = metrics.total_lives_saved;
    metrics_section.data["adoptions"] = metrics.adoptions;
    metrics_section.data["foster_placements"] = metrics.foster_placements;
    metrics_section.data["foster_to_adoptions"] = metrics.foster_to_adoptions;
    metrics_section.data["rescue_pulls"] = metrics.rescue_pulls;
    metrics_section.data["urgent_dogs_saved"] = metrics.urgent_dogs_saved;
    metrics_section.data["critical_dogs_saved"] = metrics.critical_dogs_saved;
    metrics_section.data["euthanasia_list_saves"] = metrics.euthanasia_list_saves;
    metrics_section.data["avg_days_in_shelter"] = metrics.avg_days_in_shelter;
    report.sections.push_back(metrics_section);

    // Attribution report
    auto attribution = impact.getAttributionReport(range);
    ReportSection attr_section;
    attr_section.title = "Attribution Analysis";
    attr_section.description = "What sources led to adoptions";
    attr_section.data["top_source"] = attribution.top_source;
    attr_section.data["top_campaign"] = attribution.top_campaign;
    attr_section.data["top_content"] = attribution.top_content;
    attr_section.data["total_attributed"] = attribution.total_attributed;
    attr_section.data["total_unattributed"] = attribution.total_unattributed;

    attr_section.csv_headers = {"Source", "Total Saves", "Adoptions", "Fosters", "Conversion Rate"};
    for (const auto& src : attribution.sources) {
        attr_section.csv_rows.push_back({
            src.source,
            std::to_string(src.total_saves),
            std::to_string(src.adoptions),
            std::to_string(src.fosters),
            std::to_string(static_cast<int>(src.conversion_rate)) + "%"
        });
    }
    report.sections.push_back(attr_section);

    return report;
}

Report ReportGenerator::generateMonthlyAdoption(const DateRange& range) {
    Report report;
    report.id = generateReportId();
    report.type = "monthly";
    report.title = "Monthly Adoption Report";
    report.date_range = range;
    report.generated_at = std::chrono::system_clock::now();

    auto& service = AnalyticsService::getInstance();

    // Overall metrics
    auto conversions = service.getConversionStats(range);
    report.sections.push_back(createConversionSection(conversions));

    // Impact metrics
    auto impact = ImpactTracker::getInstance().getImpactSummary(range);
    report.sections.push_back(createImpactSection(impact));

    // Top shelters by adoptions
    auto top_shelters = service.getTopSheltersByAdoptions(range, 10);
    report.sections.push_back(createTopPerformersSection(top_shelters, "Top Shelters by Adoptions"));

    // Adoption time distribution
    auto time_dist = ImpactTracker::getInstance().getAdoptionTimeDistribution(range);
    ReportSection time_section;
    time_section.title = "Time to Adoption Distribution";
    time_section.description = "How long dogs waited before adoption";
    for (const auto& [bucket, count] : time_dist) {
        time_section.data[bucket] = count;
    }
    time_section.csv_headers = {"Time Range", "Adoptions"};
    for (const auto& [bucket, count] : time_dist) {
        time_section.csv_rows.push_back({bucket, std::to_string(count)});
    }
    report.sections.push_back(time_section);

    return report;
}

Report ReportGenerator::generateSocialPerformance(const DateRange& range) {
    Report report;
    report.id = generateReportId();
    report.type = "social";
    report.title = "Social Media Performance Report";
    report.date_range = range;
    report.generated_at = std::chrono::system_clock::now();

    auto& social = SocialAnalytics::getInstance();
    auto overview = social.getSocialOverview(range);

    // Overview section
    ReportSection overview_section;
    overview_section.title = "Overall Social Performance";
    overview_section.data["total_views"] = overview.total_views;
    overview_section.data["total_engagement"] = overview.total_engagement;
    overview_section.data["total_content_pieces"] = overview.total_content_pieces;
    overview_section.data["total_attributed_adoptions"] = overview.total_attributed_adoptions;
    overview_section.data["best_platform_by_views"] = socialPlatformToString(overview.best_platform_by_views);
    overview_section.data["best_platform_by_engagement"] = socialPlatformToString(overview.best_platform_by_engagement);
    overview_section.data["best_platform_by_adoptions"] = socialPlatformToString(overview.best_platform_by_adoptions);
    report.sections.push_back(overview_section);

    // Platform breakdown
    ReportSection platform_section;
    platform_section.title = "Performance by Platform";
    platform_section.csv_headers = {"Platform", "Views", "Likes", "Comments", "Shares", "Engagement Rate", "Adoptions"};

    for (const auto& [platform, stats] : overview.by_platform) {
        std::string platform_name = socialPlatformToString(platform);
        platform_section.csv_rows.push_back({
            platform_name,
            std::to_string(stats.total_views),
            std::to_string(stats.total_likes),
            std::to_string(stats.total_comments),
            std::to_string(stats.total_shares),
            std::to_string(static_cast<int>(stats.avg_engagement_rate)) + "%",
            std::to_string(stats.total_adoptions)
        });

        platform_section.data[platform_name] = stats.toJson();
    }
    report.sections.push_back(platform_section);

    // Top content
    ReportSection top_content_section;
    top_content_section.title = "Top Performing Content";
    top_content_section.csv_headers = {"Content ID", "Platform", "Dog", "Views", "Engagement Rate", "Adoptions"};

    for (const auto& content : overview.top_by_views) {
        top_content_section.csv_rows.push_back({
            content.content_id,
            content.platform,
            content.dog_name,
            std::to_string(content.views),
            std::to_string(static_cast<int>(content.engagement_rate)) + "%",
            std::to_string(content.adoptions)
        });
    }
    report.sections.push_back(top_content_section);

    return report;
}

Report ReportGenerator::generateShelterReport(const std::string& shelter_id, const DateRange& range) {
    Report report;
    report.id = generateReportId();
    report.type = "shelter";
    report.title = shelter_id.empty() ? "All Shelters Report" : "Shelter Performance Report";
    report.date_range = range;
    report.generated_at = std::chrono::system_clock::now();

    auto& service = AnalyticsService::getInstance();

    if (!shelter_id.empty()) {
        auto stats = service.getShelterStats(shelter_id, range);

        ReportSection overview_section;
        overview_section.title = "Shelter Overview: " + stats.shelter_name;
        overview_section.data["active_dogs"] = stats.active_dogs;
        overview_section.data["urgent_dogs"] = stats.urgent_dogs;
        overview_section.data["total_dogs_listed"] = stats.total_dogs_listed;
        overview_section.data["adoptions"] = stats.adoptions;
        overview_section.data["fosters"] = stats.fosters;
        overview_section.data["adoption_rate"] = stats.adoption_rate;
        overview_section.data["foster_rate"] = stats.foster_rate;
        overview_section.data["page_views"] = stats.page_views;
        overview_section.data["dog_views"] = stats.dog_views;
        overview_section.data["applications"] = stats.applications;
        report.sections.push_back(overview_section);
    }

    // Top shelters
    auto top_shelters = service.getTopSheltersByAdoptions(range, 20);
    report.sections.push_back(createTopPerformersSection(top_shelters, "Top Shelters"));

    return report;
}

Report ReportGenerator::generateTopDogsReport(const DateRange& range,
                                               const std::string& metric,
                                               int limit)
{
    Report report;
    report.id = generateReportId();
    report.type = "top_dogs";
    report.title = "Top Dogs Report by " + metric;
    report.date_range = range;
    report.generated_at = std::chrono::system_clock::now();

    auto& service = AnalyticsService::getInstance();

    std::vector<TopPerformer> top_dogs;
    if (metric == "views") {
        top_dogs = service.getMostViewedDogs(range, limit);
    } else if (metric == "shares") {
        top_dogs = service.getMostSharedDogs(range, limit);
    } else {
        top_dogs = service.getMostViewedDogs(range, limit);
    }

    report.sections.push_back(createTopPerformersSection(top_dogs, "Top Dogs by " + metric));

    return report;
}

// ============================================================================
// EXPORT METHODS
// ============================================================================

std::string ReportGenerator::exportToJson(const Report& report, bool pretty) {
    Json::StreamWriterBuilder builder;
    if (pretty) {
        builder["indentation"] = "  ";
    } else {
        builder["indentation"] = "";
    }
    return Json::writeString(builder, report.toJson());
}

std::string ReportGenerator::exportToCsv(const Report& report) {
    return report.toCsv();
}

std::string ReportGenerator::exportToText(const Report& report) {
    return report.toText();
}

bool ReportGenerator::saveToFile(const Report& report,
                                  const std::string& filename,
                                  const std::string& format)
{
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        if (format == "csv") {
            file << exportToCsv(report);
        } else if (format == "text") {
            file << exportToText(report);
        } else {
            file << exportToJson(report, true);
        }

        file.close();
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::FILE_IO,
            "Failed to save report to file: " + std::string(e.what()),
            {{"filename", filename}}
        );
        return false;
    }
}

// ============================================================================
// SCHEDULED REPORTS
// ============================================================================

std::vector<std::string> ReportGenerator::getAvailableReportTypes() const {
    return {"daily", "weekly", "monthly", "social", "shelter", "top_dogs"};
}

Json::Value ReportGenerator::getReportMetadata(const std::string& type) const {
    Json::Value metadata;
    metadata["type"] = type;

    if (type == "daily") {
        metadata["title"] = "Daily Analytics Summary";
        metadata["description"] = "Summary of daily engagement, conversions, and top performers";
        metadata["recommended_range"] = "yesterday";
    } else if (type == "weekly") {
        metadata["title"] = "Weekly Impact Report";
        metadata["description"] = "Weekly impact metrics including lives saved and attribution";
        metadata["recommended_range"] = "last_week";
    } else if (type == "monthly") {
        metadata["title"] = "Monthly Adoption Report";
        metadata["description"] = "Monthly adoption metrics and shelter performance";
        metadata["recommended_range"] = "last_month";
    } else if (type == "social") {
        metadata["title"] = "Social Media Performance";
        metadata["description"] = "Social media engagement and adoption attribution";
        metadata["recommended_range"] = "last_week";
    }

    metadata["formats"] = Json::Value(Json::arrayValue);
    metadata["formats"].append("json");
    metadata["formats"].append("csv");
    metadata["formats"].append("text");

    return metadata;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::string ReportGenerator::generateReportId() {
    return generateUUID();
}

ReportSection ReportGenerator::createEngagementSection(const EngagementStats& stats) {
    ReportSection section;
    section.title = "Engagement Metrics";
    section.description = "User engagement with the platform";

    section.data["total_page_views"] = stats.total_page_views;
    section.data["unique_visitors"] = stats.unique_visitors;
    section.data["dog_views"] = stats.dog_views;
    section.data["searches"] = stats.searches;
    section.data["favorites_added"] = stats.favorites_added;
    section.data["shares"] = stats.shares;

    section.csv_headers = {"Metric", "Value"};
    section.csv_rows = {
        {"Total Page Views", std::to_string(stats.total_page_views)},
        {"Unique Visitors", std::to_string(stats.unique_visitors)},
        {"Dog Profile Views", std::to_string(stats.dog_views)},
        {"Searches", std::to_string(stats.searches)},
        {"Favorites Added", std::to_string(stats.favorites_added)},
        {"Shares", std::to_string(stats.shares)}
    };

    return section;
}

ReportSection ReportGenerator::createConversionSection(const ConversionStats& stats) {
    ReportSection section;
    section.title = "Conversion Metrics";
    section.description = "Applications and adoptions";

    section.data["foster_applications"] = stats.foster_applications;
    section.data["foster_approved"] = stats.foster_approved;
    section.data["foster_started"] = stats.foster_started;
    section.data["adoption_applications"] = stats.adoption_applications;
    section.data["adoptions_completed"] = stats.adoptions_completed;
    section.data["dogs_rescued"] = stats.dogs_rescued;
    section.data["view_to_application_rate"] = stats.view_to_application;
    section.data["application_to_adoption_rate"] = stats.application_to_adoption;

    section.csv_headers = {"Metric", "Value"};
    section.csv_rows = {
        {"Foster Applications", std::to_string(stats.foster_applications)},
        {"Foster Approved", std::to_string(stats.foster_approved)},
        {"Foster Started", std::to_string(stats.foster_started)},
        {"Adoption Applications", std::to_string(stats.adoption_applications)},
        {"Adoptions Completed", std::to_string(stats.adoptions_completed)},
        {"Dogs Rescued", std::to_string(stats.dogs_rescued)},
        {"View to Application Rate", std::to_string(static_cast<int>(stats.view_to_application)) + "%"},
        {"Application to Adoption Rate", std::to_string(static_cast<int>(stats.application_to_adoption)) + "%"}
    };

    return section;
}

ReportSection ReportGenerator::createImpactSection(const ImpactSummary& summary) {
    ReportSection section;
    section.title = "Life-Saving Impact";
    section.description = "Lives saved through the platform";

    section.data["dogs_saved_from_euthanasia"] = summary.dogs_saved_from_euthanasia;
    section.data["total_adoptions"] = summary.total_adoptions;
    section.data["total_fosters"] = summary.total_fosters;
    section.data["total_rescues"] = summary.total_rescues;
    section.data["total_transports"] = summary.total_transports;
    section.data["lives_saved_this_month"] = summary.lives_saved_this_month;
    section.data["lives_saved_total"] = summary.lives_saved_total;
    section.data["avg_time_to_adoption_days"] = summary.avg_time_to_adoption_days;

    section.csv_headers = {"Metric", "Value"};
    section.csv_rows = {
        {"Dogs Saved from Euthanasia", std::to_string(summary.dogs_saved_from_euthanasia)},
        {"Total Adoptions", std::to_string(summary.total_adoptions)},
        {"Total Fosters", std::to_string(summary.total_fosters)},
        {"Total Rescues", std::to_string(summary.total_rescues)},
        {"Lives Saved This Period", std::to_string(summary.lives_saved_this_month)},
        {"Total Lives Saved (All Time)", std::to_string(summary.lives_saved_total)},
        {"Avg Days to Adoption", std::to_string(static_cast<int>(summary.avg_time_to_adoption_days))}
    };

    return section;
}

ReportSection ReportGenerator::createTopPerformersSection(const std::vector<TopPerformer>& performers,
                                                           const std::string& title)
{
    ReportSection section;
    section.title = title;

    section.csv_headers = {"Rank", "Name", "Type", "Metric", "Value"};

    int rank = 1;
    for (const auto& p : performers) {
        section.csv_rows.push_back({
            std::to_string(rank),
            p.name,
            p.type,
            p.metric_name,
            std::to_string(p.metric_value)
        });

        Json::Value performer_json;
        performer_json["rank"] = rank;
        performer_json["id"] = p.id;
        performer_json["name"] = p.name;
        performer_json["type"] = p.type;
        performer_json["metric_name"] = p.metric_name;
        performer_json["metric_value"] = p.metric_value;

        section.data[std::to_string(rank)] = performer_json;
        rank++;
    }

    return section;
}

} // namespace wtl::analytics
