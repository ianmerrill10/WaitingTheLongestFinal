/**
 * @file TikTokPost.h
 * @brief TikTok post data model with analytics tracking
 *
 * PURPOSE:
 * Defines the TikTokPost data structure representing a scheduled or published
 * TikTok post for dog awareness campaigns. Tracks post content, scheduling,
 * status, and engagement analytics (views, likes, shares, comments).
 *
 * USAGE:
 * TikTokPost post = TikTokPost::fromJson(jsonData);
 * TikTokPost post = TikTokPost::fromDbRow(dbRow);
 * Json::Value json = post.toJson();
 *
 * DEPENDENCIES:
 * - jsoncpp (Json::Value)
 * - pqxx (database row conversion)
 * - chrono (time handling)
 *
 * MODIFICATION GUIDE:
 * - Add new fields to struct and update all conversion methods
 * - Maintain backward compatibility with JSON format
 * - Ensure database column names match in fromDbRow()
 *
 * @author Agent 11 - WaitingTheLongest Build System
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
#include <pqxx/pqxx>

namespace wtl::content::tiktok {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum TikTokPostStatus
 * @brief Status of a TikTok post in the workflow
 */
enum class TikTokPostStatus {
    DRAFT = 0,              ///< Post created but not scheduled
    SCHEDULED,              ///< Post scheduled for future posting
    PENDING_APPROVAL,       ///< Awaiting admin approval before posting
    POSTING,                ///< Currently being posted to TikTok
    PUBLISHED,              ///< Successfully posted to TikTok
    FAILED,                 ///< Posting failed
    DELETED,                ///< Post deleted from TikTok
    EXPIRED                 ///< Scheduled time passed without posting
};

/**
 * @enum TikTokContentType
 * @brief Type of content for the post
 */
enum class TikTokContentType {
    VIDEO,                  ///< Video content
    PHOTO_SLIDESHOW,        ///< Multiple photos as slideshow
    SINGLE_PHOTO            ///< Single photo post
};

// ============================================================================
// ANALYTICS STRUCTURE
// ============================================================================

/**
 * @struct TikTokAnalytics
 * @brief Engagement analytics for a TikTok post
 */
struct TikTokAnalytics {
    uint64_t views = 0;             ///< Total view count
    uint64_t likes = 0;             ///< Total like count
    uint64_t shares = 0;            ///< Total share count
    uint64_t comments = 0;          ///< Total comment count
    uint64_t saves = 0;             ///< Total bookmark/save count
    double engagement_rate = 0.0;   ///< Calculated engagement rate
    uint64_t reach = 0;             ///< Unique accounts reached
    uint64_t profile_visits = 0;    ///< Profile visits from this post

    std::chrono::system_clock::time_point last_updated;  ///< When analytics were last fetched

    /**
     * @brief Convert analytics to JSON
     * @return Json::Value The JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create analytics from JSON
     * @param json The JSON data
     * @return TikTokAnalytics The parsed analytics
     */
    static TikTokAnalytics fromJson(const Json::Value& json);

    /**
     * @brief Calculate engagement rate from metrics
     */
    void calculateEngagementRate();
};

// ============================================================================
// TIKTOK POST STRUCTURE
// ============================================================================

/**
 * @struct TikTokPost
 * @brief Represents a TikTok post for dog awareness
 *
 * Contains all information about a TikTok post including content,
 * scheduling, status, and analytics.
 */
struct TikTokPost {
    // ========================================================================
    // IDENTITY
    // ========================================================================

    std::string id;                         ///< UUID primary key
    std::string dog_id;                     ///< FK to dogs table
    std::string template_type;              ///< Template type used for generation

    // ========================================================================
    // CONTENT
    // ========================================================================

    std::string caption;                    ///< Post caption text
    std::vector<std::string> hashtags;      ///< Hashtags for the post
    std::optional<std::string> music_id;    ///< TikTok music/sound ID
    std::optional<std::string> music_name;  ///< Music/sound display name

    // ========================================================================
    // MEDIA
    // ========================================================================

    TikTokContentType content_type;         ///< Type of content (video/photos)
    std::optional<std::string> video_url;   ///< Video URL if video content
    std::vector<std::string> photo_urls;    ///< Photo URLs if slideshow/single
    std::optional<std::string> thumbnail_url; ///< Thumbnail image URL

    // ========================================================================
    // OVERLAY/STYLING
    // ========================================================================

    std::optional<std::string> overlay_text;      ///< Text overlay for video
    std::optional<std::string> overlay_style;     ///< Overlay styling preset
    bool include_countdown = false;               ///< Include urgency countdown
    bool include_wait_time = false;               ///< Include wait time display

    // ========================================================================
    // SCHEDULING
    // ========================================================================

    std::optional<std::chrono::system_clock::time_point> scheduled_at;  ///< When to post
    std::optional<std::chrono::system_clock::time_point> posted_at;     ///< When actually posted
    bool auto_generated = false;                  ///< Generated automatically by worker

    // ========================================================================
    // STATUS
    // ========================================================================

    TikTokPostStatus status = TikTokPostStatus::DRAFT;  ///< Current post status
    std::optional<std::string> tiktok_post_id;    ///< TikTok's ID after posting
    std::optional<std::string> tiktok_url;        ///< Direct URL to posted TikTok
    std::optional<std::string> error_message;     ///< Error message if failed
    int retry_count = 0;                          ///< Number of posting retries

    // ========================================================================
    // ANALYTICS
    // ========================================================================

    TikTokAnalytics analytics;                    ///< Engagement analytics

    // ========================================================================
    // METADATA
    // ========================================================================

    std::optional<std::string> created_by;        ///< User ID who created
    std::optional<std::string> approved_by;       ///< User ID who approved
    std::string shelter_id;                       ///< Shelter associated with post

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;   ///< Record creation time
    std::chrono::system_clock::time_point updated_at;   ///< Last update time

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create a TikTokPost from JSON data
     *
     * @param json The JSON object containing post data
     * @return TikTokPost The constructed post object
     *
     * @example
     * Json::Value json;
     * json["dog_id"] = "uuid-here";
     * json["caption"] = "Meet Buddy!";
     * TikTokPost post = TikTokPost::fromJson(json);
     */
    static TikTokPost fromJson(const Json::Value& json);

    /**
     * @brief Create a TikTokPost from a database row
     *
     * @param row The pqxx database row
     * @return TikTokPost The constructed post object
     *
     * @example
     * pqxx::result result = txn.exec("SELECT * FROM tiktok_posts WHERE id = $1", id);
     * if (!result.empty()) {
     *     TikTokPost post = TikTokPost::fromDbRow(result[0]);
     * }
     */
    static TikTokPost fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert TikTokPost to JSON for API responses
     *
     * @return Json::Value The JSON representation
     *
     * @example
     * TikTokPost post;
     * Json::Value json = post.toJson();
     * std::cout << json.toStyledString();
     */
    Json::Value toJson() const;

    /**
     * @brief Convert TikTokPost to JSON for database storage
     *
     * @return Json::Value The JSON representation for DB (includes internal fields)
     */
    Json::Value toDbJson() const;

    // ========================================================================
    // UTILITY METHODS
    // ========================================================================

    /**
     * @brief Check if post can be edited
     * @return true if post is in editable status
     */
    bool isEditable() const;

    /**
     * @brief Check if post can be deleted
     * @return true if post can be deleted
     */
    bool isDeletable() const;

    /**
     * @brief Check if post is ready for publishing
     * @return true if all required fields are set
     */
    bool isReadyToPublish() const;

    /**
     * @brief Generate the full caption with hashtags
     * @return std::string Full caption with hashtags appended
     */
    std::string getFullCaption() const;

    /**
     * @brief Get time until scheduled posting
     * @return std::chrono::seconds Time until scheduled (negative if past)
     */
    std::chrono::seconds getTimeUntilScheduled() const;

    // ========================================================================
    // STATIC UTILITY METHODS
    // ========================================================================

    /**
     * @brief Convert TikTokPostStatus to string
     * @param status The status enum value
     * @return std::string String representation
     */
    static std::string statusToString(TikTokPostStatus status);

    /**
     * @brief Convert string to TikTokPostStatus
     * @param str The string to convert
     * @return TikTokPostStatus The enum value
     */
    static TikTokPostStatus stringToStatus(const std::string& str);

    /**
     * @brief Convert TikTokContentType to string
     * @param type The content type enum value
     * @return std::string String representation
     */
    static std::string contentTypeToString(TikTokContentType type);

    /**
     * @brief Convert string to TikTokContentType
     * @param str The string to convert
     * @return TikTokContentType The enum value
     */
    static TikTokContentType stringToContentType(const std::string& str);

    /**
     * @brief Format a time_point to ISO 8601 string
     * @param tp The time point to format
     * @return std::string ISO 8601 formatted string
     */
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);

    /**
     * @brief Format a vector to PostgreSQL array string
     * @param vec The vector to format
     * @return std::string PostgreSQL array format
     */
    static std::string formatPostgresArray(const std::vector<std::string>& vec);

private:
    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Parse a timestamp string to time_point
     * @param timestamp ISO 8601 formatted timestamp string
     * @return time_point The parsed time
     */
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);

    /**
     * @brief Parse a PostgreSQL array string to vector
     * @param array_str The PostgreSQL array string format
     * @return std::vector<std::string> The parsed vector
     */
    static std::vector<std::string> parsePostgresArray(const std::string& array_str);
};

} // namespace wtl::content::tiktok
