/**
 * @file FacebookClient.h
 * @brief Facebook Graph API client for posting and analytics
 *
 * PURPOSE:
 * Provides a complete Facebook Graph API client for:
 * - Posting to Facebook Pages (photos, videos, carousels)
 * - Fetching post analytics and insights
 * - Managing OAuth authentication
 * - Handling rate limiting
 *
 * USAGE:
 * auto& client = FacebookClient::getInstance();
 * client.initialize(app_id, app_secret);
 * auto result = client.post(page_id, access_token, post_data);
 *
 * DEPENDENCIES:
 * - Platform.h (PlatformConnection)
 * - SocialPost.h (SocialPost)
 * - drogon (HTTP client)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - Update API version in API_VERSION constant
 * - Add new post types by extending PostData struct
 * - Handle new Graph API fields in response parsing
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
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
// FACEBOOK API RESULT STRUCTURES
// ============================================================================

/**
 * @struct FacebookPostResult
 * @brief Result of posting to Facebook
 */
struct FacebookPostResult {
    bool success;                         ///< Whether post succeeded
    std::string post_id;                  ///< Facebook post ID (page_id_post_id format)
    std::string permalink;                ///< URL to the post
    std::string error_code;               ///< Error code if failed
    std::string error_message;            ///< Error message if failed
    int error_subcode;                    ///< Facebook error subcode
    bool is_rate_limited;                 ///< Whether rate limited
    std::chrono::seconds retry_after;     ///< How long to wait if rate limited

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct FacebookInsights
 * @brief Insights/analytics data from Facebook
 */
struct FacebookInsights {
    std::string post_id;

    // Reach metrics
    int post_impressions;                 ///< Total impressions
    int post_impressions_unique;          ///< Unique users reached
    int post_impressions_organic;         ///< Organic impressions
    int post_impressions_paid;            ///< Paid impressions

    // Engagement metrics
    int post_engaged_users;               ///< Users who engaged
    int post_clicks;                      ///< Total clicks
    int post_clicks_unique;               ///< Unique clickers

    // Reactions breakdown
    int reactions_like;
    int reactions_love;
    int reactions_wow;
    int reactions_haha;
    int reactions_sorry;
    int reactions_anger;
    int total_reactions;

    // Other engagements
    int comments;
    int shares;

    // Video metrics (if video post)
    int video_views;                      ///< 3-second views
    int video_views_10s;                  ///< 10-second views
    int video_avg_time_watched;           ///< Average watch time in seconds

    // Timestamps
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
 * @struct FacebookMediaUpload
 * @brief Result of media upload to Facebook
 */
struct FacebookMediaUpload {
    bool success;
    std::string media_fbid;               ///< Facebook media ID
    std::string error_message;
};

// ============================================================================
// FACEBOOK POST DATA
// ============================================================================

/**
 * @struct FacebookPostData
 * @brief Data for creating a Facebook post
 */
struct FacebookPostData {
    std::string message;                  ///< Post text/caption
    std::string link;                     ///< Link to share (optional)

    // Media
    std::vector<std::string> image_urls;  ///< Image URLs for carousel
    std::optional<std::string> video_url; ///< Video URL for video post

    // Scheduling
    std::optional<std::chrono::system_clock::time_point> scheduled_publish_time;
    bool published = true;                ///< False for draft/unpublished

    // Targeting (for Page posts)
    std::optional<std::string> targeting; ///< JSON targeting spec

    // Call to action
    std::optional<std::string> call_to_action_type;  ///< "LEARN_MORE", "ADOPT_NOW", etc.
    std::optional<std::string> call_to_action_link;

    /**
     * @brief Create from SocialPost
     */
    static FacebookPostData fromSocialPost(const SocialPost& post);
};

// ============================================================================
// FACEBOOK CLIENT
// ============================================================================

/**
 * @class FacebookClient
 * @brief Singleton client for Facebook Graph API
 */
class FacebookClient {
public:
    // API version
    static constexpr const char* API_VERSION = "v18.0";
    static constexpr const char* API_BASE_URL = "https://graph.facebook.com";

    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     */
    static FacebookClient& getInstance();

    // Prevent copying
    FacebookClient(const FacebookClient&) = delete;
    FacebookClient& operator=(const FacebookClient&) = delete;

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
    // AUTHENTICATION
    // ========================================================================

    /**
     * @brief Generate OAuth login URL
     * @param redirect_uri Callback URL after auth
     * @param scopes Requested permissions
     * @param state State parameter for CSRF protection
     * @return OAuth URL for user to visit
     */
    std::string getLoginUrl(
        const std::string& redirect_uri,
        const std::vector<std::string>& scopes,
        const std::string& state);

    /**
     * @brief Exchange auth code for access token
     * @param code Authorization code from OAuth callback
     * @param redirect_uri Same redirect_uri used in login URL
     * @return Access token and expiration
     */
    std::optional<std::pair<std::string, std::chrono::seconds>> exchangeCodeForToken(
        const std::string& code,
        const std::string& redirect_uri);

    /**
     * @brief Exchange short-lived token for long-lived token
     * @param short_token Short-lived access token
     * @return Long-lived token (60-day expiration)
     */
    std::optional<std::pair<std::string, std::chrono::seconds>> exchangeForLongLivedToken(
        const std::string& short_token);

    /**
     * @brief Refresh a long-lived Page token
     * @param page_id Page ID
     * @param user_token User access token
     * @return Page access token (never-expiring for Pages)
     */
    std::optional<std::string> getPageAccessToken(
        const std::string& page_id,
        const std::string& user_token);

    /**
     * @brief Validate an access token
     * @param token Token to validate
     * @return Token info if valid
     */
    std::optional<Json::Value> validateToken(const std::string& token);

    // ========================================================================
    // POSTING
    // ========================================================================

    /**
     * @brief Post to a Facebook Page
     * @param page_id Page ID to post to
     * @param access_token Page access token
     * @param data Post data
     * @return Result with post ID or error
     */
    FacebookPostResult post(
        const std::string& page_id,
        const std::string& access_token,
        const FacebookPostData& data);

    /**
     * @brief Post a photo to a Page
     * @param page_id Page ID
     * @param access_token Page access token
     * @param image_url URL of image to post
     * @param caption Caption for the photo
     * @return Result with post ID or error
     */
    FacebookPostResult postPhoto(
        const std::string& page_id,
        const std::string& access_token,
        const std::string& image_url,
        const std::string& caption);

    /**
     * @brief Post multiple photos as a carousel
     * @param page_id Page ID
     * @param access_token Page access token
     * @param image_urls URLs of images
     * @param caption Caption for the carousel
     * @return Result with post ID or error
     */
    FacebookPostResult postCarousel(
        const std::string& page_id,
        const std::string& access_token,
        const std::vector<std::string>& image_urls,
        const std::string& caption);

    /**
     * @brief Post a video to a Page
     * @param page_id Page ID
     * @param access_token Page access token
     * @param video_url URL of video
     * @param description Description for the video
     * @return Result with post ID or error
     */
    FacebookPostResult postVideo(
        const std::string& page_id,
        const std::string& access_token,
        const std::string& video_url,
        const std::string& description);

    /**
     * @brief Post async (for large videos)
     * @param page_id Page ID
     * @param access_token Page access token
     * @param data Post data
     * @param callback Callback when complete
     */
    void postAsync(
        const std::string& page_id,
        const std::string& access_token,
        const FacebookPostData& data,
        std::function<void(const FacebookPostResult&)> callback);

    // ========================================================================
    // ANALYTICS
    // ========================================================================

    /**
     * @brief Get insights for a post
     * @param post_id Facebook post ID
     * @param access_token Page access token
     * @return Insights data
     */
    std::optional<FacebookInsights> getInsights(
        const std::string& post_id,
        const std::string& access_token);

    /**
     * @brief Get engagement metrics for a post
     * @param post_id Facebook post ID
     * @param access_token Page access token
     * @return JSON with reactions, comments, shares counts
     */
    std::optional<Json::Value> getEngagement(
        const std::string& post_id,
        const std::string& access_token);

    /**
     * @brief Get Page insights summary
     * @param page_id Page ID
     * @param access_token Page access token
     * @param since Start date
     * @param until End date
     * @return Page insights data
     */
    std::optional<Json::Value> getPageInsights(
        const std::string& page_id,
        const std::string& access_token,
        std::chrono::system_clock::time_point since,
        std::chrono::system_clock::time_point until);

    // ========================================================================
    // POST MANAGEMENT
    // ========================================================================

    /**
     * @brief Delete a post
     * @param post_id Post ID to delete
     * @param access_token Page access token
     * @return True if deleted successfully
     */
    bool deletePost(
        const std::string& post_id,
        const std::string& access_token);

    /**
     * @brief Update a post's message
     * @param post_id Post ID
     * @param access_token Page access token
     * @param new_message New message text
     * @return True if updated successfully
     */
    bool updatePost(
        const std::string& post_id,
        const std::string& access_token,
        const std::string& new_message);

    /**
     * @brief Get post details
     * @param post_id Post ID
     * @param access_token Access token
     * @return Post details as JSON
     */
    std::optional<Json::Value> getPost(
        const std::string& post_id,
        const std::string& access_token);

    // ========================================================================
    // RATE LIMITING
    // ========================================================================

    /**
     * @brief Check if we're currently rate limited
     * @param page_id Page ID
     * @return True if rate limited
     */
    bool isRateLimited(const std::string& page_id) const;

    /**
     * @brief Get time until rate limit resets
     * @param page_id Page ID
     * @return Seconds until can post again
     */
    std::chrono::seconds getTimeUntilReset(const std::string& page_id) const;

    /**
     * @brief Get remaining API calls for today
     * @return Approximate remaining calls
     */
    int getRemainingCalls() const;

    // ========================================================================
    // MEDIA UPLOAD
    // ========================================================================

    /**
     * @brief Upload an image for later use in post
     * @param page_id Page ID
     * @param access_token Page access token
     * @param image_url URL of image to upload
     * @param published Whether to publish immediately
     * @return Upload result with media_fbid
     */
    FacebookMediaUpload uploadPhoto(
        const std::string& page_id,
        const std::string& access_token,
        const std::string& image_url,
        bool published = false);

private:
    FacebookClient() = default;
    ~FacebookClient() = default;

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
     * @brief Make a Graph API DELETE request
     */
    bool graphDelete(
        const std::string& endpoint,
        const std::string& access_token);

    /**
     * @brief Handle rate limit response
     */
    void handleRateLimit(const std::string& page_id, const Json::Value& response);

    /**
     * @brief Parse error from API response
     */
    FacebookPostResult parseError(const Json::Value& response);

    /**
     * @brief Build full API URL
     */
    std::string buildUrl(const std::string& endpoint) const;

    // Configuration
    std::string app_id_;
    std::string app_secret_;
    bool initialized_{false};

    // Rate limiting tracking per page
    struct RateLimitInfo {
        std::chrono::system_clock::time_point reset_at;
        int remaining_calls{200};
    };
    std::map<std::string, RateLimitInfo> rate_limits_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::content::social::clients
