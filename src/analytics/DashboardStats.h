/**
 * @file DashboardStats.h
 * @brief Dashboard statistics structures for admin analytics dashboard
 *
 * PURPOSE:
 * Provides data structures for the analytics dashboard that displays
 * real-time statistics, trends, and performance metrics for the
 * WaitingTheLongest.com platform. Used by administrators to monitor
 * platform health and dog adoption success rates.
 *
 * USAGE:
 * auto& service = AnalyticsService::getInstance();
 * DashboardStats stats = service.getDashboardStats(DateRange::lastWeek());
 * Json::Value json = stats.toJson();
 *
 * DEPENDENCIES:
 * - json/json.h (JSON serialization)
 * - pqxx (database row parsing)
 *
 * MODIFICATION GUIDE:
 * - Add new metric fields as needed
 * - Update toJson/fromDbRow methods when adding fields
 * - Keep nested structs for logical grouping
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include <json/json.h>
#include <pqxx/pqxx>

namespace wtl::analytics {

/**
 * @struct DateRange
 * @brief Date range for querying analytics
 */
struct DateRange {
    std::chrono::system_clock::time_point start;
    std::chrono::system_clock::time_point end;

    /** Create range for last N days */
    static DateRange lastDays(int days);

    /** Create range for last week */
    static DateRange lastWeek();

    /** Create range for last month */
    static DateRange lastMonth();

    /** Create range for last year */
    static DateRange lastYear();

    /** Create range for today */
    static DateRange today();

    /** Create range for yesterday */
    static DateRange yesterday();

    /** Create custom range */
    static DateRange custom(const std::chrono::system_clock::time_point& start,
                            const std::chrono::system_clock::time_point& end);

    /** Parse from query params (start_date, end_date) */
    static DateRange fromParams(const std::string& start_date, const std::string& end_date);

    /** Get start as ISO string */
    std::string getStartISO() const;

    /** Get end as ISO string */
    std::string getEndISO() const;

    /** Get duration in days */
    int getDurationDays() const;

    /** Convert to JSON */
    Json::Value toJson() const;
};

/**
 * @struct RealTimeStats
 * @brief Current real-time statistics
 */
struct RealTimeStats {
    int active_dogs = 0;              // Dogs currently available
    int urgent_dogs = 0;              // Dogs in urgent status
    int critical_dogs = 0;            // Dogs in critical status (< 24 hours)
    int active_users_today = 0;       // Unique users today
    int active_sessions = 0;          // Currently active sessions
    int pending_applications = 0;     // Pending foster/adoption apps
    int dogs_fostered_today = 0;      // Dogs fostered today
    int dogs_adopted_today = 0;       // Dogs adopted today

    Json::Value toJson() const;
    static RealTimeStats fromDbRow(const pqxx::row& row);
};

/**
 * @struct EngagementStats
 * @brief User engagement metrics for a period
 */
struct EngagementStats {
    int total_page_views = 0;         // Total page views
    int unique_visitors = 0;          // Unique visitors
    int dog_views = 0;                // Individual dog profile views
    int searches = 0;                 // Number of searches performed
    int favorites_added = 0;          // Dogs added to favorites
    int shares = 0;                   // Content shares
    double avg_session_duration = 0;  // Average session duration in seconds
    double bounce_rate = 0;           // Bounce rate percentage
    int pages_per_session = 0;        // Average pages per session

    Json::Value toJson() const;
    static EngagementStats fromDbRow(const pqxx::row& row);
};

/**
 * @struct ConversionStats
 * @brief Conversion and outcome metrics
 */
struct ConversionStats {
    int foster_applications = 0;      // Foster applications submitted
    int foster_approved = 0;          // Foster applications approved
    int foster_started = 0;           // Foster placements started
    int adoption_applications = 0;    // Adoption applications submitted
    int adoptions_completed = 0;      // Completed adoptions
    int dogs_rescued = 0;             // Dogs rescued/pulled
    double view_to_application = 0;   // View to application rate %
    double application_to_adoption = 0; // Application to adoption rate %
    double foster_to_adoption = 0;    // Foster to adoption rate %

    Json::Value toJson() const;
    static ConversionStats fromDbRow(const pqxx::row& row);
};

/**
 * @struct TrendData
 * @brief Time series data point for trends
 */
struct TrendData {
    std::string period;               // Time period label (date, week, etc.)
    int value = 0;                    // Metric value
    double change_percent = 0;        // Change from previous period

    Json::Value toJson() const;
};

/**
 * @struct TrendStats
 * @brief Trend statistics over time
 */
struct TrendStats {
    std::vector<TrendData> dog_views;
    std::vector<TrendData> adoptions;
    std::vector<TrendData> fosters;
    std::vector<TrendData> new_users;
    std::vector<TrendData> urgent_dogs;

    // Week-over-week changes
    double views_wow_change = 0;      // Views week-over-week change %
    double adoptions_wow_change = 0;  // Adoptions week-over-week change %
    double fosters_wow_change = 0;    // Fosters week-over-week change %

    // Month-over-month changes
    double views_mom_change = 0;      // Views month-over-month change %
    double adoptions_mom_change = 0;  // Adoptions month-over-month change %
    double fosters_mom_change = 0;    // Fosters month-over-month change %

    Json::Value toJson() const;
};

/**
 * @struct TopPerformer
 * @brief Top performing entity (dog, shelter, content)
 */
struct TopPerformer {
    std::string id;
    std::string name;
    std::string type;                 // dog, shelter, content
    int metric_value = 0;
    std::string metric_name;          // views, shares, adoptions, etc.
    std::string thumbnail_url;

    Json::Value toJson() const;
};

/**
 * @struct TopPerformersStats
 * @brief Lists of top performing entities
 */
struct TopPerformersStats {
    std::vector<TopPerformer> most_viewed_dogs;
    std::vector<TopPerformer> most_shared_dogs;
    std::vector<TopPerformer> most_favorited_dogs;
    std::vector<TopPerformer> top_shelters_by_adoptions;
    std::vector<TopPerformer> top_content_by_engagement;
    std::vector<TopPerformer> fastest_adopted_dogs;

    Json::Value toJson() const;
};

/**
 * @struct GeographicStats
 * @brief Statistics broken down by geography
 */
struct GeographicStats {
    std::map<std::string, int> dogs_by_state;
    std::map<std::string, int> adoptions_by_state;
    std::map<std::string, int> users_by_state;
    std::map<std::string, int> urgent_dogs_by_state;

    Json::Value toJson() const;
};

/**
 * @struct SocialMediaSummary
 * @brief Summary of social media performance
 */
struct SocialMediaSummary {
    int total_social_views = 0;
    int total_social_shares = 0;
    int total_social_engagement = 0;
    int social_referrals = 0;
    int social_attributed_adoptions = 0;
    std::string top_platform;
    std::string best_performing_content_id;

    Json::Value toJson() const;
};

/**
 * @struct ImpactSummary
 * @brief Summary of life-saving impact
 */
struct ImpactSummary {
    int dogs_saved_from_euthanasia = 0;
    int total_adoptions = 0;
    int total_fosters = 0;
    int total_rescues = 0;
    int total_transports = 0;
    double avg_time_to_adoption_days = 0;
    double avg_time_improvement_days = 0; // Improvement vs. average
    int lives_saved_this_month = 0;
    int lives_saved_total = 0;

    Json::Value toJson() const;
};

/**
 * @struct DashboardStats
 * @brief Complete dashboard statistics
 *
 * Aggregates all statistics needed for the admin analytics dashboard.
 * Includes real-time stats, engagement, conversions, trends, top performers,
 * geographic breakdown, social media summary, and life-saving impact.
 */
struct DashboardStats {
    // Time context
    DateRange date_range;
    std::chrono::system_clock::time_point generated_at;

    // Statistics sections
    RealTimeStats real_time;
    EngagementStats engagement;
    ConversionStats conversions;
    TrendStats trends;
    TopPerformersStats top_performers;
    GeographicStats geographic;
    SocialMediaSummary social;
    ImpactSummary impact;

    /**
     * @brief Convert to JSON representation
     * @return Complete JSON object with all stats
     */
    Json::Value toJson() const;

    /**
     * @brief Get summary metrics for quick display
     * @return JSON with key metrics only
     */
    Json::Value toSummaryJson() const;
};

/**
 * @struct DogStats
 * @brief Statistics for a specific dog
 */
struct DogStats {
    std::string dog_id;
    std::string dog_name;

    // View metrics
    int total_views = 0;
    int unique_viewers = 0;
    int photo_views = 0;
    int video_plays = 0;

    // Engagement metrics
    int favorites_count = 0;
    int shares_count = 0;
    int contact_clicks = 0;
    int direction_clicks = 0;

    // Social metrics
    int social_views = 0;
    int social_shares = 0;
    int social_engagement = 0;

    // Conversion metrics
    int application_starts = 0;
    int applications_submitted = 0;
    bool is_adopted = false;
    bool is_fostered = false;

    // Time metrics
    int days_listed = 0;
    std::string first_view_date;
    std::string last_view_date;

    // Trend data
    std::vector<TrendData> daily_views;

    Json::Value toJson() const;
    static DogStats fromDbRow(const pqxx::row& row);
};

/**
 * @struct ShelterStats
 * @brief Statistics for a specific shelter
 */
struct ShelterStats {
    std::string shelter_id;
    std::string shelter_name;

    // Dog metrics
    int active_dogs = 0;
    int urgent_dogs = 0;
    int total_dogs_listed = 0;

    // Outcome metrics
    int adoptions = 0;
    int fosters = 0;
    int rescues = 0;
    int returns = 0;

    // Performance metrics
    double avg_time_to_adoption = 0;
    double adoption_rate = 0;
    double foster_rate = 0;

    // Engagement metrics
    int page_views = 0;
    int dog_views = 0;
    int applications = 0;

    // Trend data
    std::vector<TrendData> weekly_adoptions;
    std::vector<TrendData> weekly_intakes;

    Json::Value toJson() const;
    static ShelterStats fromDbRow(const pqxx::row& row);
};

} // namespace wtl::analytics
