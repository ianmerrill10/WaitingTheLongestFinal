/**
 * @file InstagramClient.h
 * @brief Instagram Graph API client for posting and analytics
 *
 * PURPOSE:
 * Provides a complete Instagram Graph API client for:
 * - Posting photos, carousels, and Reels to Instagram Business accounts
 * - Fetching post analytics and insights
 * - Managing OAuth authentication (via Facebook Graph API)
 * - Handling rate limiting and container-based publishing
 *
 * USAGE:
 * auto& client = InstagramClient::getInstance();
 * client.initialize(app_id, app_secret);
 * auto result = client.postPhoto(ig_user_id, access_token, image_url, caption);
 *
 * DEPENDENCIES:
 * - Platform.h (PlatformConnection)
 * - SocialPost.h (SocialPost)
 * - FacebookClient.h (shared authentication)
 * - drogon (HTTP client)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - Instagram uses Facebook Graph API, so API version matches Facebook
 * - Container-based publishing required for all media
 * - Reels have specific requirements (9:16 aspect ratio, etc.)
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/social/Platform.h"
#include "content/social/SocialAnalytics.h"
#include "content/social/SocialPost.h"

namespace wtl::content::social::clients {

// Bring parent namespace types into scope
using wtl::content::social::Platform;
using wtl::content::social::PlatformConnection;
using wtl::content::social::PlatformAnalytics;
using wtl::content::social::SocialPost;

// ============================================================================
// INSTAGRAM API RESULT STRUCTURES
// ============================================================================

/**
 * @struct InstagramPostResult
 * @brief Result of posting to Instagram
 */
struct InstagramPostResult {
    bool success;                         ///< Whether post succeeded
    std::string media_id;                 ///< Instagram media ID
    std::string permalink;                ///< URL to the post
    std::string error_code;               ///< Error code if failed
    std::string error_message;            ///< Error message if failed
    bool is_rate_limited;                 ///< Whether rate limited
    std::chrono::seconds retry_after;     ///< How long to wait if rate limited

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct InstagramInsights
 * @brief Insights/analytics data from Instagram
 */
struct InstagramInsights {
    std::string media_id;

    // Reach metrics
    int impressions;                      ///< Total impressions
    int reach;                            ///< Unique accounts reached

    // Engagement metrics
    int likes;                            ///< Total likes
    int comments;                         ///< Total comments
    int saves;                            ///< Times saved
    int shares;                           ///< Times shared (Reels)

    // Video metrics (for Reels and videos)
    int video_views;                      ///< Video views
    int plays;                            ///< Plays (Reels)

    // Story metrics (if story)
    int exits;                            ///< Story exits
    int replies;                          ///< Story replies
    int taps_forward;                     ///< Story taps forward
    int taps_back;                        ///< Story taps back

    // Engagement rate
    double engagement_rate;

    // Timestamp
    std::chrono::system_clock::time_point fetched_at;

    /**
     * @brief Convert to PlatformAnalytics
     */
    PlatformAnalytics toPlatformAnalytics() const;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct InstagramContainer
 * @brief Container for Instagram media (used in carousel and publishing)
 */
struct InstagramContainer {
    std::string container_id;             ///< Container creation ID
    std::string status;                   ///< FINISHED, IN_PROGRESS, ERROR
    std::string error_message;            ///< Error if status is ERROR
};

// ============================================================================
// INSTAGRAM POST DATA
// ============================================================================

/**
 * @struct InstagramPostData
 * @brief Data for creating an Instagram post
 */
struct InstagramPostData {
    std::string caption;                  ///< Post caption (max 2200 chars)
    std::string media_type;               ///< IMAGE, VIDEO, CAROUSEL_ALBUM, REELS

    // Media
    std::optional<std::string> image_url; ///< Image URL (for IMAGE type)
    std::optional<std::string> video_url; ///< Video URL (for VIDEO/REELS type)
    std::vector<std::string> carousel_image_urls;  ///< For CAROUSEL_ALBUM
    std::vector<std::string> carousel_video_urls;  ///< For mixed carousel

    // Video-specific
    std::optional<std::string> thumb_offset;  ///< Thumbnail offset in ms
    std::optional<std::string> cover_url;     ///< Cover image URL (Reels)

    // Location
    std::optional<std::string> location_id;   ///< Instagram location ID

    // User tags
    std::vector<std::pair<std::string, std::pair<double, double>>> user_tags;  ///< username, (x, y)

    // Product tags (for shopping posts)
    std::vector<std::pair<std::string, std::pair<double, double>>> product_tags;

    /**
     * @brief Create from SocialPost
     */
    static InstagramPostData fromSocialPost(const SocialPost& post);
};

// ============================================================================
// INSTAGRAM CLIENT
// ============================================================================

/**
 * @class InstagramClient
 * @brief Singleton client for Instagram Graph API
 */
class InstagramClient {
public:
    // API version (same as Facebook)
    static constexpr const char* API_VERSION = "v18.0";
    static constexpr const char* API_BASE_URL = "https://graph.facebook.com";

    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     */
    static InstagramClient& getInstance();

    // Prevent copying
    InstagramClient(const InstagramClient&) = delete;
    InstagramClient& operator=(const InstagramClient&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize the client
     * @param app_id Facebook App ID
     * @param app_secret Facebook App Secret
     */
    void initialize(const std::string& app_id, const std::string& app_secret);

    /**
     * @brief Initialize from configuration
     */
    void initializeFromConfig();

    /**
     * @brief Check if client is initialized
     */
    bool isInitialized() const;

    // ========================================================================
    // ACCOUNT MANAGEMENT
    // ========================================================================

    /**
     * @brief Get Instagram Business Account ID from Facebook Page
     * @param page_id Facebook Page ID
     * @param access_token Page access token
     * @return Instagram Business Account ID
     */
    std::optional<std::string> getInstagramAccountId(
        const std::string& page_id,
        const std::string& access_token);

    /**
     * @brief Get account info
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @return Account info as JSON
     */
    std::optional<Json::Value> getAccountInfo(
        const std::string& ig_user_id,
        const std::string& access_token);

    // ========================================================================
    // POSTING - PHOTOS
    // ========================================================================

    /**
     * @brief Post a photo to Instagram
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param image_url URL of image to post
     * @param caption Caption for the photo
     * @return Result with media ID or error
     */
    InstagramPostResult postPhoto(
        const std::string& ig_user_id,
        const std::string& access_token,
        const std::string& image_url,
        const std::string& caption);

    // ========================================================================
    // POSTING - CAROUSEL
    // ========================================================================

    /**
     * @brief Post a carousel (multiple images) to Instagram
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param media_urls URLs of images/videos (max 10)
     * @param caption Caption for the carousel
     * @return Result with media ID or error
     */
    InstagramPostResult postCarousel(
        const std::string& ig_user_id,
        const std::string& access_token,
        const std::vector<std::string>& media_urls,
        const std::string& caption);

    // ========================================================================
    // POSTING - REELS
    // ========================================================================

    /**
     * @brief Post a Reel to Instagram
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param video_url URL of video (9:16 aspect ratio, up to 90 seconds)
     * @param caption Caption for the Reel
     * @param cover_url Optional cover image URL
     * @return Result with media ID or error
     */
    InstagramPostResult postReel(
        const std::string& ig_user_id,
        const std::string& access_token,
        const std::string& video_url,
        const std::string& caption,
        const std::optional<std::string>& cover_url = std::nullopt);

    // ========================================================================
    // POSTING - VIDEO
    // ========================================================================

    /**
     * @brief Post a video to Instagram feed
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param video_url URL of video
     * @param caption Caption for the video
     * @return Result with media ID or error
     */
    InstagramPostResult postVideo(
        const std::string& ig_user_id,
        const std::string& access_token,
        const std::string& video_url,
        const std::string& caption);

    // ========================================================================
    // POSTING - GENERAL
    // ========================================================================

    /**
     * @brief Post using InstagramPostData
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param data Post data
     * @return Result with media ID or error
     */
    InstagramPostResult post(
        const std::string& ig_user_id,
        const std::string& access_token,
        const InstagramPostData& data);

    /**
     * @brief Post async (for videos which take longer)
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param data Post data
     * @param callback Callback when complete
     */
    void postAsync(
        const std::string& ig_user_id,
        const std::string& access_token,
        const InstagramPostData& data,
        std::function<void(const InstagramPostResult&)> callback);

    // ========================================================================
    // CONTAINER MANAGEMENT
    // ========================================================================

    /**
     * @brief Create a media container (first step of publishing)
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param media_type IMAGE, VIDEO, CAROUSEL_ALBUM, or REELS
     * @param media_url URL of media
     * @param caption Caption
     * @param additional_params Extra parameters
     * @return Container info
     */
    std::optional<InstagramContainer> createContainer(
        const std::string& ig_user_id,
        const std::string& access_token,
        const std::string& media_type,
        const std::optional<std::string>& media_url,
        const std::string& caption,
        const std::map<std::string, std::string>& additional_params = {});

    /**
     * @brief Check container status
     * @param container_id Container ID
     * @param access_token Access token
     * @return Container status
     */
    std::optional<InstagramContainer> getContainerStatus(
        const std::string& container_id,
        const std::string& access_token);

    /**
     * @brief Publish a ready container
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param container_id Container to publish
     * @return Published media ID
     */
    std::optional<std::string> publishContainer(
        const std::string& ig_user_id,
        const std::string& access_token,
        const std::string& container_id);

    /**
     * @brief Wait for container to be ready (poll status)
     * @param container_id Container ID
     * @param access_token Access token
     * @param timeout_seconds Max wait time
     * @return True if container is ready
     */
    bool waitForContainer(
        const std::string& container_id,
        const std::string& access_token,
        int timeout_seconds = 120);

    // ========================================================================
    // ANALYTICS
    // ========================================================================

    /**
     * @brief Get insights for a media post
     * @param media_id Instagram media ID
     * @param access_token Access token
     * @return Insights data
     */
    std::optional<InstagramInsights> getInsights(
        const std::string& media_id,
        const std::string& access_token);

    /**
     * @brief Get account insights summary
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param since Start date
     * @param until End date
     * @return Account insights as JSON
     */
    std::optional<Json::Value> getAccountInsights(
        const std::string& ig_user_id,
        const std::string& access_token,
        std::chrono::system_clock::time_point since,
        std::chrono::system_clock::time_point until);

    /**
     * @brief Get media list for account
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @param limit Max results
     * @return Media list as JSON
     */
    std::optional<Json::Value> getMediaList(
        const std::string& ig_user_id,
        const std::string& access_token,
        int limit = 25);

    // ========================================================================
    // RATE LIMITING
    // ========================================================================

    /**
     * @brief Check if we're rate limited
     * @param ig_user_id Instagram user ID
     * @return True if rate limited
     */
    bool isRateLimited(const std::string& ig_user_id) const;

    /**
     * @brief Get time until rate limit resets
     * @param ig_user_id Instagram user ID
     * @return Seconds until can post again
     */
    std::chrono::seconds getTimeUntilReset(const std::string& ig_user_id) const;

    /**
     * @brief Get content publishing limit status
     * @param ig_user_id Instagram user ID
     * @param access_token Access token
     * @return JSON with limit info
     */
    std::optional<Json::Value> getContentPublishingLimit(
        const std::string& ig_user_id,
        const std::string& access_token);

private:
    InstagramClient() = default;
    ~InstagramClient() = default;

    /**
     * @brief Make a Graph API GET request
     */
    std::optional<Json::Value> graphGet(
        const std::string& endpoint,
        const std::string& access_token,
        const std::map<std::string, std::string>& params = {});

    /**
     * @brief Make a Graph API POST request
     */
    std::optional<Json::Value> graphPost(
        const std::string& endpoint,
        const std::string& access_token,
        const std::map<std::string, std::string>& params);

    /**
     * @brief Parse error from API response
     */
    InstagramPostResult parseError(const Json::Value& response);

    /**
     * @brief Build full API URL
     */
    std::string buildUrl(const std::string& endpoint) const;

    /**
     * @brief Create carousel child containers
     */
    std::vector<std::string> createCarouselChildren(
        const std::string& ig_user_id,
        const std::string& access_token,
        const std::vector<std::string>& media_urls);

    // Configuration
    std::string app_id_;
    std::string app_secret_;
    bool initialized_{false};

    // Rate limiting tracking per account
    struct RateLimitInfo {
        std::chrono::system_clock::time_point reset_at;
        int remaining_posts{25};  // Instagram limit: 25 posts per 24 hours
    };
    std::map<std::string, RateLimitInfo> rate_limits_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::content::social::clients
