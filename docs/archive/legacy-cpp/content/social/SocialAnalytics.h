/**
 * @file SocialAnalytics.h
 * @brief Analytics tracking for social media posts and adoption impact
 *
 * PURPOSE:
 * Provides comprehensive analytics tracking for social media posts across
 * all platforms. Tracks impressions, engagements, reach, and most importantly,
 * whether posts resulted in adoptions or foster placements.
 *
 * USAGE:
 * auto& analytics = SocialAnalytics::getInstance();
 * analytics.recordImpression(post_id, Platform::INSTAGRAM, 1000);
 * auto stats = analytics.getPostStats(post_id);
 * auto adoption_impact = analytics.getAdoptionImpact(time_range);
 *
 * DEPENDENCIES:
 * - Platform.h (Platform enum)
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - jsoncpp (JSON serialization)
 *
 * MODIFICATION GUIDE:
 * - Add new metrics to PlatformAnalytics struct
 * - Update aggregation methods when adding new metrics
 * - Ensure database schema matches new fields
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

// Project includes
#include "content/social/Platform.h"

namespace wtl::content::social {

// ============================================================================
// ANALYTICS STRUCTURES
// ============================================================================

/**
 * @struct PlatformAnalytics
 * @brief Analytics data for a single platform
 */
struct PlatformAnalytics {
    Platform platform;                    ///< The platform

    // Reach metrics
    int impressions;                      ///< Number of times shown
    int reach;                            ///< Unique accounts reached
    int profile_visits;                   ///< Profile visits from post

    // Engagement metrics
    int likes;                            ///< Likes/hearts/reactions
    int comments;                         ///< Comments
    int shares;                           ///< Shares/retweets/reposts
    int saves;                            ///< Saves/bookmarks
    int link_clicks;                      ///< Clicks to website

    // Video metrics (if applicable)
    int video_views;                      ///< Total video views
    int video_watch_time_seconds;         ///< Total watch time
    double video_completion_rate;         ///< Average completion percentage

    // Follower impact
    int new_followers;                    ///< New followers from post

    // Engagement rate (calculated)
    double engagement_rate;               ///< Engagements / Impressions

    // Timestamp
    std::chrono::system_clock::time_point fetched_at;  ///< When data was fetched

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static PlatformAnalytics fromJson(const Json::Value& json);

    /**
     * @brief Create from database row
     */
    static PlatformAnalytics fromDbRow(const pqxx::row& row);

    /**
     * @brief Calculate total engagements
     */
    int getTotalEngagements() const;

    /**
     * @brief Calculate engagement rate
     */
    void calculateEngagementRate();
};

/**
 * @struct PostAnalytics
 * @brief Aggregated analytics for a post across all platforms
 */
struct PostAnalytics {
    std::string post_id;                  ///< The post UUID
    std::optional<std::string> dog_id;    ///< Associated dog (if any)
    std::optional<std::string> shelter_id; ///< Associated shelter (if any)

    // Per-platform analytics
    std::map<Platform, PlatformAnalytics> platform_analytics;

    // Aggregated totals
    int total_impressions;
    int total_reach;
    int total_engagements;
    int total_link_clicks;
    int total_video_views;
    int total_new_followers;

    // Best performing platform
    Platform best_performing_platform;
    double best_engagement_rate;

    // Impact tracking
    bool resulted_in_adoption;
    bool resulted_in_foster;
    bool resulted_in_application;
    int application_count;

    // Cost metrics (if paid promotion)
    double amount_spent;
    double cost_per_engagement;
    double cost_per_click;

    // Timestamps
    std::chrono::system_clock::time_point first_posted_at;
    std::chrono::system_clock::time_point last_updated_at;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from database row
     */
    static PostAnalytics fromDbRow(const pqxx::row& row);

    /**
     * @brief Aggregate analytics from all platforms
     */
    void aggregate();
};

/**
 * @struct AdoptionImpact
 * @brief Tracks the impact of social media on adoptions
 */
struct AdoptionImpact {
    std::string dog_id;                   ///< The dog that was adopted
    std::string dog_name;                 ///< Dog's name for display
    std::string shelter_name;             ///< Shelter name

    // Social media exposure
    std::vector<std::string> post_ids;    ///< Posts about this dog
    int total_social_impressions;
    int total_social_engagements;

    // Timeline
    std::chrono::system_clock::time_point first_social_post;
    std::chrono::system_clock::time_point adoption_date;
    int days_from_first_post;

    // Attribution
    std::optional<std::string> attributed_post_id;  ///< Post that led to adoption
    std::optional<Platform> attributed_platform;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct AnalyticsSummary
 * @brief Summary statistics for a time period
 */
struct AnalyticsSummary {
    // Time period
    std::chrono::system_clock::time_point start_date;
    std::chrono::system_clock::time_point end_date;

    // Post counts
    int total_posts;
    int posts_with_video;
    int posts_with_images;
    std::map<Platform, int> posts_per_platform;

    // Aggregate metrics
    int total_impressions;
    int total_reach;
    int total_engagements;
    int total_link_clicks;
    double average_engagement_rate;

    // Impact metrics
    int adoptions_attributed;
    int fosters_attributed;
    int applications_received;

    // Best content
    std::string top_post_id;
    int top_post_engagements;
    Platform top_platform;

    // Trends
    double impressions_growth_pct;
    double engagements_growth_pct;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

// ============================================================================
// SOCIAL ANALYTICS SERVICE
// ============================================================================

/**
 * @class SocialAnalytics
 * @brief Singleton service for tracking and reporting social media analytics
 */
class SocialAnalytics {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     */
    static SocialAnalytics& getInstance();

    // Prevent copying
    SocialAnalytics(const SocialAnalytics&) = delete;
    SocialAnalytics& operator=(const SocialAnalytics&) = delete;

    // ========================================================================
    // RECORDING ANALYTICS
    // ========================================================================

    /**
     * @brief Record analytics data for a post on a platform
     * @param post_id The post UUID
     * @param analytics The platform analytics data
     */
    void recordAnalytics(const std::string& post_id, const PlatformAnalytics& analytics);

    /**
     * @brief Record an adoption attributed to social media
     * @param dog_id The dog UUID
     * @param post_id The attributed post (optional)
     * @param platform The attributed platform (optional)
     */
    void recordAdoption(const std::string& dog_id,
                        const std::optional<std::string>& post_id = std::nullopt,
                        const std::optional<Platform>& platform = std::nullopt);

    /**
     * @brief Record a foster placement attributed to social media
     * @param dog_id The dog UUID
     * @param post_id The attributed post (optional)
     */
    void recordFoster(const std::string& dog_id,
                      const std::optional<std::string>& post_id = std::nullopt);

    /**
     * @brief Record an adoption application
     * @param dog_id The dog UUID
     * @param source_post_id The post that led to application (optional)
     */
    void recordApplication(const std::string& dog_id,
                           const std::optional<std::string>& source_post_id = std::nullopt);

    // ========================================================================
    // RETRIEVING ANALYTICS
    // ========================================================================

    /**
     * @brief Get analytics for a specific post
     * @param post_id The post UUID
     * @return PostAnalytics Aggregated analytics
     */
    std::optional<PostAnalytics> getPostAnalytics(const std::string& post_id);

    /**
     * @brief Get analytics for a dog's posts
     * @param dog_id The dog UUID
     * @return Vector of post analytics
     */
    std::vector<PostAnalytics> getDogAnalytics(const std::string& dog_id);

    /**
     * @brief Get analytics summary for a time period
     * @param start Start of period
     * @param end End of period
     * @return AnalyticsSummary Summary statistics
     */
    AnalyticsSummary getSummary(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end);

    /**
     * @brief Get analytics by platform
     * @param platform The platform to filter by
     * @param limit Max results
     * @return Vector of post analytics for platform
     */
    std::vector<PostAnalytics> getByPlatform(Platform platform, int limit = 100);

    /**
     * @brief Get top performing posts
     * @param metric Metric to sort by ("engagements", "impressions", "clicks")
     * @param limit Max results
     * @return Vector of top posts
     */
    std::vector<PostAnalytics> getTopPosts(const std::string& metric, int limit = 10);

    // ========================================================================
    // ADOPTION IMPACT
    // ========================================================================

    /**
     * @brief Get adoption impact data
     * @param start Start of period
     * @param end End of period
     * @return Vector of adoption impacts
     */
    std::vector<AdoptionImpact> getAdoptionImpact(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end);

    /**
     * @brief Calculate social media's contribution to adoptions
     * @param start Start of period
     * @param end End of period
     * @return JSON with impact statistics
     */
    Json::Value calculateImpactMetrics(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end);

    // ========================================================================
    // SYNC AND REFRESH
    // ========================================================================

    /**
     * @brief Sync analytics from all platforms for a post
     * @param post_id The post UUID
     * @return bool True if successful
     */
    bool syncPostAnalytics(const std::string& post_id);

    /**
     * @brief Sync analytics for all recent posts
     * @param hours_back How many hours back to sync
     * @return Number of posts synced
     */
    int syncRecentAnalytics(int hours_back = 24);

    /**
     * @brief Refresh aggregated totals for a post
     * @param post_id The post UUID
     */
    void refreshAggregates(const std::string& post_id);

    // ========================================================================
    // OPTIMAL TIMING
    // ========================================================================

    /**
     * @brief Get optimal posting times for a platform
     * @param platform The platform
     * @return JSON with day/hour engagement data
     */
    Json::Value getOptimalPostTimes(Platform platform);

    /**
     * @brief Analyze best content types for a platform
     * @param platform The platform
     * @return JSON with content type performance
     */
    Json::Value analyzeContentPerformance(Platform platform);

private:
    SocialAnalytics() = default;
    ~SocialAnalytics() = default;

    /**
     * @brief Save analytics to database
     */
    void saveToDatabase(const std::string& post_id, const PlatformAnalytics& analytics);

    /**
     * @brief Load analytics from database
     */
    std::optional<PlatformAnalytics> loadFromDatabase(
        const std::string& post_id, Platform platform);

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Cache for recently accessed analytics
    std::map<std::string, PostAnalytics> analytics_cache_;
    std::chrono::system_clock::time_point cache_last_cleared_;
    static constexpr int CACHE_TTL_MINUTES = 5;
};

} // namespace wtl::content::social
