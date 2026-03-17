/**
 * @file SocialPost.h
 * @brief Social media post model for cross-posting to multiple platforms
 *
 * PURPOSE:
 * Defines the SocialPost data model that represents a post to be shared
 * across multiple social media platforms. Tracks per-platform status,
 * post IDs, and aggregated analytics.
 *
 * USAGE:
 * SocialPost post;
 * post.dog_id = "uuid-of-dog";
 * post.primary_text = "Meet Max! He's been waiting 2 years for adoption.";
 * post.platforms = {Platform::FACEBOOK, Platform::INSTAGRAM};
 * auto json = post.toJson();
 *
 * DEPENDENCIES:
 * - Platform.h (Platform enum, PlatformStatus)
 * - jsoncpp (JSON serialization)
 * - pqxx (database row conversion)
 *
 * MODIFICATION GUIDE:
 * - Add new fields to struct and update all conversion methods
 * - Maintain backward compatibility with JSON format
 * - Update database schema when adding persistent fields
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

// Project includes
#include "content/social/Platform.h"

namespace wtl::content::social {

// ============================================================================
// POST TYPE ENUMERATION
// ============================================================================

/**
 * @enum PostType
 * @brief Types of social media posts
 */
enum class PostType {
    ADOPTION_SPOTLIGHT = 0,   ///< Featured dog for adoption
    URGENT_APPEAL,            ///< Urgent - dog at risk
    WAITING_MILESTONE,        ///< Dog reached waiting milestone (1 year, etc.)
    SUCCESS_STORY,            ///< Adoption success story
    FOSTER_NEEDED,            ///< Foster home needed
    EVENT_PROMOTION,          ///< Shelter event
    GENERAL_AWARENESS,        ///< General shelter awareness
    REPOST_TIKTOK             ///< Cross-posting TikTok content to other platforms
};

/**
 * @brief Convert PostType to string
 */
std::string postTypeToString(PostType type);

/**
 * @brief Convert string to PostType
 */
PostType stringToPostType(const std::string& str);

// ============================================================================
// PLATFORM POST INFO
// ============================================================================

/**
 * @struct PlatformPostInfo
 * @brief Status and metadata for a post on a specific platform
 */
struct PlatformPostInfo {
    Platform platform;                    ///< The platform
    PlatformStatus status;                ///< Current status
    std::string platform_post_id;         ///< Post ID on the platform
    std::string platform_url;             ///< URL to the post
    std::string error_message;            ///< Error message if failed
    int retry_count;                      ///< Number of retry attempts
    std::chrono::system_clock::time_point posted_at;     ///< When successfully posted
    std::chrono::system_clock::time_point last_attempt;  ///< Last posting attempt

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     */
    static PlatformPostInfo fromJson(const Json::Value& json);
};

// ============================================================================
// SOCIAL POST
// ============================================================================

/**
 * @struct SocialPost
 * @brief Represents a social media post to be shared across platforms
 */
struct SocialPost {
    // ========================================================================
    // IDENTITY
    // ========================================================================

    std::string id;                       ///< UUID primary key
    std::optional<std::string> dog_id;    ///< FK to dogs table (if dog-related)
    std::optional<std::string> shelter_id; ///< FK to shelters table
    std::string created_by;               ///< User ID who created the post

    // ========================================================================
    // POST TYPE AND CONTENT
    // ========================================================================

    PostType post_type;                   ///< Type of post
    std::string primary_text;             ///< Main post text/caption

    // Platform-specific text overrides
    std::map<Platform, std::string> platform_text;  ///< Custom text per platform

    // ========================================================================
    // MEDIA
    // ========================================================================

    std::vector<std::string> images;      ///< Image URLs
    std::optional<std::string> video_url; ///< Video URL (for TikTok, Reels, etc.)
    std::optional<std::string> thumbnail_url;  ///< Video thumbnail

    // Generated share card
    std::optional<std::string> share_card_url;  ///< Auto-generated share image

    // ========================================================================
    // TARGETING
    // ========================================================================

    std::vector<Platform> platforms;      ///< Target platforms
    std::vector<std::string> hashtags;    ///< Hashtags to include

    // ========================================================================
    // SCHEDULING
    // ========================================================================

    std::optional<std::chrono::system_clock::time_point> scheduled_at;  ///< Scheduled time
    bool is_immediate;                    ///< Post immediately vs schedule

    // ========================================================================
    // STATUS PER PLATFORM
    // ========================================================================

    std::map<Platform, PlatformPostInfo> platform_info;  ///< Per-platform status

    // Overall status
    bool all_posted;                      ///< True if posted to all platforms
    bool any_failed;                      ///< True if any platform failed

    // ========================================================================
    // ANALYTICS
    // ========================================================================

    int total_impressions;                ///< Sum across platforms
    int total_engagements;                ///< Sum of likes, comments, shares
    int total_clicks;                     ///< Link clicks
    bool resulted_in_adoption;            ///< Did this lead to adoption?
    bool resulted_in_foster;              ///< Did this lead to foster placement?

    // ========================================================================
    // METADATA
    // ========================================================================

    Json::Value metadata;                 ///< Additional metadata (flexible)

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Convert to JSON for API responses
     * @return Json::Value The post as JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json The JSON data
     * @return SocialPost The parsed post
     */
    static SocialPost fromJson(const Json::Value& json);

    /**
     * @brief Create from database row
     * @param row The database row
     * @return SocialPost The parsed post
     */
    static SocialPost fromDbRow(const pqxx::row& row);

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Get text formatted for a specific platform
     * @param platform The target platform
     * @return std::string Formatted text with platform constraints applied
     */
    std::string getTextForPlatform(Platform platform) const;

    /**
     * @brief Get hashtags formatted for platform
     * @param platform The target platform
     * @return std::string Hashtags as string (respecting platform limits)
     */
    std::string getHashtagsForPlatform(Platform platform) const;

    /**
     * @brief Check if post is ready for a platform
     * @param platform The platform to check
     * @return bool True if content meets platform requirements
     */
    bool isValidForPlatform(Platform platform) const;

    /**
     * @brief Get overall posting status summary
     * @return Json::Value Summary of status across platforms
     */
    Json::Value getStatusSummary() const;

    /**
     * @brief Update status for a platform
     * @param platform The platform
     * @param status The new status
     * @param post_id The platform post ID (if posted)
     * @param error_msg Error message (if failed)
     */
    void updatePlatformStatus(Platform platform, PlatformStatus status,
                               const std::string& post_id = "",
                               const std::string& error_msg = "");

    /**
     * @brief Add impression/engagement data
     * @param platform The platform
     * @param impressions Number of impressions
     * @param engagements Number of engagements
     * @param clicks Number of clicks
     */
    void addAnalytics(Platform platform, int impressions, int engagements, int clicks);

private:
    /**
     * @brief Truncate text to fit platform limit
     * @param text The text to truncate
     * @param max_length Maximum length
     * @return std::string Truncated text
     */
    static std::string truncateText(const std::string& text, int max_length);

    /**
     * @brief Parse timestamp string to time_point
     */
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);

    /**
     * @brief Format time_point to ISO 8601 string
     */
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);

    /**
     * @brief Parse PostgreSQL array to vector
     */
    static std::vector<std::string> parsePostgresArray(const std::string& array_str);

    /**
     * @brief Format vector to PostgreSQL array
     */
    static std::string formatPostgresArray(const std::vector<std::string>& vec);
};

// ============================================================================
// POST BUILDER
// ============================================================================

/**
 * @class SocialPostBuilder
 * @brief Fluent builder for creating SocialPost instances
 */
class SocialPostBuilder {
public:
    /**
     * @brief Create a builder for a dog adoption post
     * @param dog_id The dog's UUID
     * @return SocialPostBuilder Builder instance
     */
    static SocialPostBuilder forDog(const std::string& dog_id);

    /**
     * @brief Create a builder for a shelter post
     * @param shelter_id The shelter's UUID
     * @return SocialPostBuilder Builder instance
     */
    static SocialPostBuilder forShelter(const std::string& shelter_id);

    // Fluent methods
    SocialPostBuilder& withType(PostType type);
    SocialPostBuilder& withText(const std::string& text);
    SocialPostBuilder& withPlatformText(Platform platform, const std::string& text);
    SocialPostBuilder& withImage(const std::string& image_url);
    SocialPostBuilder& withImages(const std::vector<std::string>& image_urls);
    SocialPostBuilder& withVideo(const std::string& video_url);
    SocialPostBuilder& withShareCard(const std::string& card_url);
    SocialPostBuilder& toplatform(Platform platform);
    SocialPostBuilder& toPlatforms(const std::vector<Platform>& platforms);
    SocialPostBuilder& withHashtag(const std::string& hashtag);
    SocialPostBuilder& withHashtags(const std::vector<std::string>& hashtags);
    SocialPostBuilder& scheduleAt(std::chrono::system_clock::time_point time);
    SocialPostBuilder& postImmediately();
    SocialPostBuilder& createdBy(const std::string& user_id);
    SocialPostBuilder& withMetadata(const Json::Value& metadata);

    /**
     * @brief Build the SocialPost
     * @return SocialPost The built post
     */
    SocialPost build();

    SocialPostBuilder() = default;

private:
    SocialPost post_;
};

} // namespace wtl::content::social
