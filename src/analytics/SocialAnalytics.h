/**
 * @file SocialAnalytics.h
 * @brief Social media analytics tracking and reporting
 *
 * PURPOSE:
 * Tracks performance of dog content across social media platforms including
 * TikTok, Instagram, Facebook, and Twitter. Measures engagement, correlates
 * social media views with adoptions, and identifies best-performing content.
 *
 * USAGE:
 * auto& analytics = SocialAnalytics::getInstance();
 * analytics.recordSocialView("tiktok", content_id, dog_id);
 * auto stats = analytics.getPlatformStats("tiktok", range);
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - DashboardStats (data structures)
 *
 * MODIFICATION GUIDE:
 * - Add new platforms to SocialPlatform enum
 * - Update recording methods for new engagement types
 * - Add new metrics as social platforms evolve
 *
 * @author Agent 17 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <json/json.h>
#include <pqxx/pqxx>

#include "analytics/DashboardStats.h"

namespace wtl::analytics {

/**
 * @enum SocialPlatform
 * @brief Supported social media platforms
 */
enum class SocialPlatform {
    TIKTOK,
    INSTAGRAM,
    FACEBOOK,
    TWITTER,
    YOUTUBE,
    UNKNOWN
};

/**
 * @brief Convert SocialPlatform to string
 */
inline std::string socialPlatformToString(SocialPlatform platform) {
    switch (platform) {
        case SocialPlatform::TIKTOK: return "tiktok";
        case SocialPlatform::INSTAGRAM: return "instagram";
        case SocialPlatform::FACEBOOK: return "facebook";
        case SocialPlatform::TWITTER: return "twitter";
        case SocialPlatform::YOUTUBE: return "youtube";
        default: return "unknown";
    }
}

/**
 * @brief Parse SocialPlatform from string
 */
inline SocialPlatform socialPlatformFromString(const std::string& str) {
    if (str == "tiktok") return SocialPlatform::TIKTOK;
    if (str == "instagram") return SocialPlatform::INSTAGRAM;
    if (str == "facebook") return SocialPlatform::FACEBOOK;
    if (str == "twitter" || str == "x") return SocialPlatform::TWITTER;
    if (str == "youtube") return SocialPlatform::YOUTUBE;
    return SocialPlatform::UNKNOWN;
}

/**
 * @struct SocialContent
 * @brief Represents a piece of social media content
 */
struct SocialContent {
    std::string id;
    SocialPlatform platform;
    std::string platform_content_id;    // ID on the platform (TikTok video ID, etc.)
    std::string dog_id;                 // Associated dog
    std::string content_type;           // video, image, story, reel, etc.
    std::string url;
    std::optional<std::string> caption;
    std::chrono::system_clock::time_point posted_at;
    std::chrono::system_clock::time_point created_at;

    Json::Value toJson() const;
    static SocialContent fromDbRow(const pqxx::row& row);
};

/**
 * @struct ContentMetrics
 * @brief Metrics for a specific piece of content
 */
struct ContentMetrics {
    std::string content_id;
    SocialPlatform platform;

    // Engagement metrics
    int views = 0;
    int likes = 0;
    int comments = 0;
    int shares = 0;
    int saves = 0;
    int follows = 0;

    // Calculated metrics
    double engagement_rate = 0;         // (likes + comments + shares) / views * 100
    double share_rate = 0;              // shares / views * 100
    double save_rate = 0;               // saves / views * 100

    // Platform-specific metrics
    int watch_time_seconds = 0;         // TikTok/YouTube
    int average_watch_percent = 0;      // How much of video watched
    int profile_visits = 0;
    int link_clicks = 0;

    // Conversion metrics
    int site_visits = 0;                // Visits to our site from this content
    int dog_views = 0;                  // Views of the associated dog on our site
    int applications = 0;               // Applications resulting from this content
    int adoptions = 0;                  // Adoptions attributed to this content

    Json::Value toJson() const;
    static ContentMetrics fromDbRow(const pqxx::row& row);
};

/**
 * @struct PlatformStats
 * @brief Aggregate statistics for a social platform
 */
struct PlatformStats {
    SocialPlatform platform;

    // Totals
    int total_content = 0;
    int total_views = 0;
    int total_likes = 0;
    int total_comments = 0;
    int total_shares = 0;
    int total_saves = 0;

    // Averages
    double avg_views_per_content = 0;
    double avg_engagement_rate = 0;
    double avg_share_rate = 0;

    // Best performers
    std::string top_content_id;
    int top_content_views = 0;
    std::string most_engaging_content_id;
    double highest_engagement_rate = 0;

    // Conversions
    int total_site_visits = 0;
    int total_dog_views = 0;
    int total_applications = 0;
    int total_adoptions = 0;
    double adoption_conversion_rate = 0;  // adoptions / views * 100

    // Trend
    double views_week_over_week = 0;
    double engagement_week_over_week = 0;

    Json::Value toJson() const;
};

/**
 * @struct ContentPerformanceRanking
 * @brief Ranked content by performance
 */
struct ContentPerformanceRanking {
    std::string content_id;
    std::string platform;
    std::string dog_id;
    std::string dog_name;
    int views = 0;
    double engagement_rate = 0;
    int adoptions = 0;
    double score = 0;  // Composite score

    Json::Value toJson() const;
};

/**
 * @struct SocialOverview
 * @brief Overview of all social media performance
 */
struct SocialOverview {
    // Cross-platform totals
    int total_views = 0;
    int total_engagement = 0;  // likes + comments + shares
    int total_content_pieces = 0;
    int total_attributed_adoptions = 0;

    // By platform breakdown
    std::map<SocialPlatform, PlatformStats> by_platform;

    // Top content across all platforms
    std::vector<ContentPerformanceRanking> top_by_views;
    std::vector<ContentPerformanceRanking> top_by_engagement;
    std::vector<ContentPerformanceRanking> top_by_adoptions;

    // Best platform
    SocialPlatform best_platform_by_views;
    SocialPlatform best_platform_by_engagement;
    SocialPlatform best_platform_by_adoptions;

    // Trends
    std::vector<TrendData> daily_views;
    std::vector<TrendData> daily_engagement;

    Json::Value toJson() const;
};

/**
 * @class SocialAnalytics
 * @brief Singleton service for social media analytics
 *
 * Tracks, aggregates, and reports on social media performance across
 * all platforms. Correlates social media engagement with actual adoptions
 * to identify the most effective content strategies.
 */
class SocialAnalytics {
public:
    /**
     * @brief Get singleton instance
     */
    static SocialAnalytics& getInstance();

    // Prevent copying
    SocialAnalytics(const SocialAnalytics&) = delete;
    SocialAnalytics& operator=(const SocialAnalytics&) = delete;

    // =========================================================================
    // CONTENT MANAGEMENT
    // =========================================================================

    /**
     * @brief Register new social media content
     * @param platform The social platform
     * @param platform_content_id The ID on that platform
     * @param dog_id Associated dog ID
     * @param content_type Type of content (video, image, etc.)
     * @param url URL to the content
     * @return The created content ID
     */
    std::string registerContent(
        SocialPlatform platform,
        const std::string& platform_content_id,
        const std::string& dog_id,
        const std::string& content_type,
        const std::string& url);

    /**
     * @brief Get content by ID
     * @param content_id Our internal content ID
     * @return Content if found
     */
    std::optional<SocialContent> getContent(const std::string& content_id);

    /**
     * @brief Get content by platform content ID
     * @param platform The platform
     * @param platform_content_id The ID on that platform
     * @return Content if found
     */
    std::optional<SocialContent> getContentByPlatformId(
        SocialPlatform platform,
        const std::string& platform_content_id);

    /**
     * @brief Get all content for a dog
     * @param dog_id The dog ID
     * @return Vector of content
     */
    std::vector<SocialContent> getContentByDog(const std::string& dog_id);

    // =========================================================================
    // RECORDING METRICS
    // =========================================================================

    /**
     * @brief Record a view on social content
     * @param content_id Our content ID or platform content ID
     * @param platform The platform
     */
    void recordView(const std::string& content_id, SocialPlatform platform);

    /**
     * @brief Record a like on social content
     */
    void recordLike(const std::string& content_id, SocialPlatform platform);

    /**
     * @brief Record a comment on social content
     */
    void recordComment(const std::string& content_id, SocialPlatform platform);

    /**
     * @brief Record a share on social content
     */
    void recordShare(const std::string& content_id, SocialPlatform platform);

    /**
     * @brief Record a save/bookmark on social content
     */
    void recordSave(const std::string& content_id, SocialPlatform platform);

    /**
     * @brief Record a follow resulting from content
     */
    void recordFollow(const std::string& content_id, SocialPlatform platform);

    /**
     * @brief Update metrics in bulk from platform API
     * @param content_id Our content ID
     * @param metrics The metrics to update
     */
    void updateMetrics(const std::string& content_id, const ContentMetrics& metrics);

    /**
     * @brief Record a referral visit to our site from social
     * @param content_id The content that led to the visit
     * @param session_id The user's session ID
     */
    void recordSiteVisit(const std::string& content_id, const std::string& session_id);

    /**
     * @brief Record that a social referral viewed a dog
     * @param content_id The content that led to the view
     * @param dog_id The dog that was viewed
     */
    void recordDogView(const std::string& content_id, const std::string& dog_id);

    /**
     * @brief Record an application attributed to social content
     * @param content_id The content that led to the application
     * @param dog_id The dog applied for
     */
    void recordApplication(const std::string& content_id, const std::string& dog_id);

    /**
     * @brief Record an adoption attributed to social content
     * @param content_id The content that led to the adoption
     * @param dog_id The dog that was adopted
     */
    void recordAdoption(const std::string& content_id, const std::string& dog_id);

    // =========================================================================
    // QUERYING METRICS
    // =========================================================================

    /**
     * @brief Get metrics for specific content
     * @param content_id The content ID
     * @return Metrics for the content
     */
    ContentMetrics getContentMetrics(const std::string& content_id);

    /**
     * @brief Get platform statistics
     * @param platform The platform to query
     * @param range Date range
     * @return Platform statistics
     */
    PlatformStats getPlatformStats(SocialPlatform platform, const DateRange& range);

    /**
     * @brief Get platform statistics as string
     * @param platform Platform name as string
     * @param range Date range
     * @return Platform statistics
     */
    PlatformStats getPlatformStats(const std::string& platform, const DateRange& range);

    /**
     * @brief Get overview of all social media performance
     * @param range Date range
     * @return Social overview
     */
    SocialOverview getSocialOverview(const DateRange& range);

    /**
     * @brief Get top content by views
     * @param range Date range
     * @param limit Maximum results
     * @return Vector of top content rankings
     */
    std::vector<ContentPerformanceRanking> getTopByViews(const DateRange& range, int limit = 10);

    /**
     * @brief Get top content by engagement rate
     * @param range Date range
     * @param limit Maximum results
     * @return Vector of top content rankings
     */
    std::vector<ContentPerformanceRanking> getTopByEngagement(const DateRange& range, int limit = 10);

    /**
     * @brief Get content that led to most adoptions
     * @param range Date range
     * @param limit Maximum results
     * @return Vector of top content rankings
     */
    std::vector<ContentPerformanceRanking> getTopByAdoptions(const DateRange& range, int limit = 10);

    /**
     * @brief Get social performance for a specific dog
     * @param dog_id The dog ID
     * @param range Date range
     * @return Content metrics aggregated for the dog
     */
    ContentMetrics getDogSocialMetrics(const std::string& dog_id, const DateRange& range);

    // =========================================================================
    // CORRELATION ANALYSIS
    // =========================================================================

    /**
     * @brief Calculate correlation between social views and adoptions
     * @param platform Platform to analyze
     * @param range Date range
     * @return Correlation coefficient (-1 to 1)
     */
    double getViewsToAdoptionsCorrelation(SocialPlatform platform, const DateRange& range);

    /**
     * @brief Get average time from first social view to adoption
     * @param platform Platform to analyze
     * @param range Date range
     * @return Average days from first view to adoption
     */
    double getAverageViewToAdoptionTime(SocialPlatform platform, const DateRange& range);

    /**
     * @brief Identify content characteristics that lead to adoptions
     * @param range Date range
     * @return JSON with analysis of successful content traits
     */
    Json::Value analyzeSuccessfulContent(const DateRange& range);

    // =========================================================================
    // SYNC FROM PLATFORMS
    // =========================================================================

    /**
     * @brief Sync metrics from TikTok API
     * @return Number of content pieces updated
     */
    int syncTikTokMetrics();

    /**
     * @brief Sync metrics from Instagram API
     * @return Number of content pieces updated
     */
    int syncInstagramMetrics();

    /**
     * @brief Sync all platform metrics
     * @return Total number of content pieces updated
     */
    int syncAllPlatforms();

private:
    /**
     * @brief Private constructor for singleton
     */
    SocialAnalytics();

    /**
     * @brief Increment a metric for content
     */
    void incrementMetric(const std::string& content_id, const std::string& metric);

    /**
     * @brief Get or create content ID from platform ID
     */
    std::string resolveContentId(const std::string& id, SocialPlatform platform);

    /**
     * @brief Calculate engagement rate
     */
    double calculateEngagementRate(int views, int likes, int comments, int shares);

    /** Mutex for thread safety */
    std::mutex mutex_;
};

} // namespace wtl::analytics
