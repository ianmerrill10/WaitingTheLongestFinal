/**
 * @file TwitterClient.h
 * @brief Twitter/X API v2 client for posting and analytics
 *
 * PURPOSE:
 * Provides a complete Twitter API v2 client for:
 * - Posting tweets with text, images, and video
 * - Creating tweet threads
 * - Fetching tweet metrics and analytics
 * - Managing OAuth 2.0 authentication
 * - Handling rate limiting
 *
 * USAGE:
 * auto& client = TwitterClient::getInstance();
 * client.initialize(api_key, api_secret, bearer_token);
 * auto result = client.postTweet(access_token, "Check out this dog!");
 *
 * DEPENDENCIES:
 * - Platform.h (PlatformConnection)
 * - SocialPost.h (SocialPost)
 * - drogon (HTTP client)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - Twitter API v2 uses OAuth 2.0 with PKCE
 * - Media upload uses v1.1 endpoint
 * - Rate limits are strict; implement proper backoff
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

// Import types from parent namespace
using wtl::content::social::SocialPost;
using wtl::content::social::Platform;
using wtl::content::social::PlatformConnection;

// ============================================================================
// TWITTER API RESULT STRUCTURES
// ============================================================================

/**
 * @struct TwitterPostResult
 * @brief Result of posting to Twitter
 */
struct TwitterPostResult {
    bool success;                         ///< Whether post succeeded
    std::string tweet_id;                 ///< Twitter tweet ID
    std::string tweet_url;                ///< URL to the tweet
    std::string error_type;               ///< Error type (title)
    std::string error_detail;             ///< Error detail message
    bool is_rate_limited;                 ///< Whether rate limited
    std::chrono::seconds retry_after;     ///< How long to wait if rate limited

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct TwitterMetrics
 * @brief Public and non-public metrics from Twitter
 */
struct TwitterMetrics {
    std::string tweet_id;

    // Public metrics (available to everyone)
    int retweet_count;                    ///< Number of retweets
    int reply_count;                      ///< Number of replies
    int like_count;                       ///< Number of likes
    int quote_count;                      ///< Number of quote tweets
    int bookmark_count;                   ///< Number of bookmarks

    // Non-public metrics (only available to tweet author)
    int impression_count;                 ///< Number of impressions
    int url_link_clicks;                  ///< Link clicks
    int user_profile_clicks;              ///< Profile clicks from tweet

    // Video metrics (if video tweet)
    int video_views;                      ///< Video view count

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
 * @struct TwitterMediaUpload
 * @brief Result of media upload to Twitter
 */
struct TwitterMediaUpload {
    bool success;
    std::string media_id_string;          ///< Media ID for attaching to tweet
    std::string media_key;                ///< Media key (v2 API)
    std::string error_message;
    int processing_status;                ///< 0=pending, 1=in_progress, 2=failed, 3=succeeded
};

// ============================================================================
// TWITTER POST DATA
// ============================================================================

/**
 * @struct TwitterPostData
 * @brief Data for creating a Twitter post
 */
struct TwitterPostData {
    std::string text;                     ///< Tweet text (max 280 chars, or 25000 for Twitter Blue)

    // Media
    std::vector<std::string> media_ids;   ///< Pre-uploaded media IDs
    std::vector<std::string> image_urls;  ///< Image URLs to upload
    std::optional<std::string> video_url; ///< Video URL to upload

    // Reply/Quote
    std::optional<std::string> reply_to_tweet_id;  ///< Tweet ID to reply to
    std::optional<std::string> quote_tweet_id;     ///< Tweet ID to quote

    // Poll (optional)
    std::vector<std::string> poll_options;         ///< Poll options (2-4)
    int poll_duration_minutes = 1440;              ///< Poll duration (default 24 hours)

    // Settings
    std::optional<std::string> reply_settings;     ///< "everyone", "mentionedUsers", "following"

    /**
     * @brief Create from SocialPost
     */
    static TwitterPostData fromSocialPost(const SocialPost& post);
};

// ============================================================================
// TWITTER CLIENT
// ============================================================================

/**
 * @class TwitterClient
 * @brief Singleton client for Twitter API v2
 */
class TwitterClient {
public:
    // API endpoints
    static constexpr const char* API_BASE_URL = "https://api.twitter.com";
    static constexpr const char* UPLOAD_URL = "https://upload.twitter.com";

    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     */
    static TwitterClient& getInstance();

    // Prevent copying
    TwitterClient(const TwitterClient&) = delete;
    TwitterClient& operator=(const TwitterClient&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize the client with API credentials
     * @param api_key API Key (Consumer Key)
     * @param api_secret API Secret (Consumer Secret)
     * @param bearer_token App-only Bearer Token (for read operations)
     */
    void initialize(
        const std::string& api_key,
        const std::string& api_secret,
        const std::string& bearer_token);

    /**
     * @brief Initialize from configuration
     */
    void initializeFromConfig();

    /**
     * @brief Check if client is initialized
     */
    bool isInitialized() const;

    // ========================================================================
    // AUTHENTICATION (OAuth 2.0 with PKCE)
    // ========================================================================

    /**
     * @brief Generate OAuth 2.0 authorization URL
     * @param redirect_uri Callback URL after auth
     * @param scopes Requested scopes (tweet.read, tweet.write, users.read, etc.)
     * @param state State parameter for CSRF protection
     * @param code_challenge PKCE code challenge
     * @return Authorization URL for user to visit
     */
    std::string getAuthorizationUrl(
        const std::string& redirect_uri,
        const std::vector<std::string>& scopes,
        const std::string& state,
        const std::string& code_challenge);

    /**
     * @brief Exchange authorization code for access token
     * @param code Authorization code from callback
     * @param redirect_uri Same redirect URI used in auth URL
     * @param code_verifier PKCE code verifier
     * @return Access token and refresh token
     */
    std::optional<std::tuple<std::string, std::string, std::chrono::seconds>> exchangeCodeForToken(
        const std::string& code,
        const std::string& redirect_uri,
        const std::string& code_verifier);

    /**
     * @brief Refresh an access token
     * @param refresh_token Current refresh token
     * @return New access token, refresh token, and expiration
     */
    std::optional<std::tuple<std::string, std::string, std::chrono::seconds>> refreshAccessToken(
        const std::string& refresh_token);

    /**
     * @brief Revoke an access token
     * @param token Token to revoke
     * @return True if revoked successfully
     */
    bool revokeToken(const std::string& token);

    // ========================================================================
    // POSTING - TWEETS
    // ========================================================================

    /**
     * @brief Post a tweet
     * @param access_token User access token
     * @param text Tweet text
     * @return Result with tweet ID or error
     */
    TwitterPostResult postTweet(
        const std::string& access_token,
        const std::string& text);

    /**
     * @brief Post a tweet with media
     * @param access_token User access token
     * @param text Tweet text
     * @param media_ids Pre-uploaded media IDs
     * @return Result with tweet ID or error
     */
    TwitterPostResult postTweetWithMedia(
        const std::string& access_token,
        const std::string& text,
        const std::vector<std::string>& media_ids);

    /**
     * @brief Post a tweet using TwitterPostData
     * @param access_token User access token
     * @param data Post data
     * @return Result with tweet ID or error
     */
    TwitterPostResult post(
        const std::string& access_token,
        const TwitterPostData& data);

    // ========================================================================
    // POSTING - THREADS
    // ========================================================================

    /**
     * @brief Post a thread (multiple connected tweets)
     * @param access_token User access token
     * @param tweets Vector of tweet texts
     * @return Vector of results for each tweet
     */
    std::vector<TwitterPostResult> postThread(
        const std::string& access_token,
        const std::vector<std::string>& tweets);

    /**
     * @brief Post a thread with media
     * @param access_token User access token
     * @param tweets_data Vector of tweet data
     * @return Vector of results for each tweet
     */
    std::vector<TwitterPostResult> postThread(
        const std::string& access_token,
        const std::vector<TwitterPostData>& tweets_data);

    // ========================================================================
    // MEDIA UPLOAD
    // ========================================================================

    /**
     * @brief Upload an image for use in tweet
     * @param access_token User access token
     * @param image_url URL of image to upload
     * @param alt_text Alt text for accessibility
     * @return Upload result with media_id
     */
    TwitterMediaUpload uploadImage(
        const std::string& access_token,
        const std::string& image_url,
        const std::string& alt_text = "");

    /**
     * @brief Upload a video for use in tweet (chunked upload)
     * @param access_token User access token
     * @param video_url URL of video to upload
     * @return Upload result with media_id
     */
    TwitterMediaUpload uploadVideo(
        const std::string& access_token,
        const std::string& video_url);

    /**
     * @brief Check status of video upload
     * @param access_token User access token
     * @param media_id Media ID to check
     * @return Upload status
     */
    TwitterMediaUpload checkUploadStatus(
        const std::string& access_token,
        const std::string& media_id);

    /**
     * @brief Wait for video processing to complete
     * @param access_token User access token
     * @param media_id Media ID to wait for
     * @param timeout_seconds Max wait time
     * @return True if processing succeeded
     */
    bool waitForVideoProcessing(
        const std::string& access_token,
        const std::string& media_id,
        int timeout_seconds = 300);

    // ========================================================================
    // ANALYTICS
    // ========================================================================

    /**
     * @brief Get metrics for a tweet
     * @param tweet_id Tweet ID
     * @param access_token User access token (for non-public metrics)
     * @return Tweet metrics
     */
    std::optional<TwitterMetrics> getMetrics(
        const std::string& tweet_id,
        const std::string& access_token);

    /**
     * @brief Get metrics for multiple tweets
     * @param tweet_ids Vector of tweet IDs
     * @param access_token User access token
     * @return Map of tweet ID to metrics
     */
    std::map<std::string, TwitterMetrics> getBatchMetrics(
        const std::vector<std::string>& tweet_ids,
        const std::string& access_token);

    // ========================================================================
    // TWEET MANAGEMENT
    // ========================================================================

    /**
     * @brief Delete a tweet
     * @param tweet_id Tweet ID to delete
     * @param access_token User access token
     * @return True if deleted successfully
     */
    bool deleteTweet(
        const std::string& tweet_id,
        const std::string& access_token);

    /**
     * @brief Get tweet details
     * @param tweet_id Tweet ID
     * @param access_token Access token (can use bearer for public data)
     * @return Tweet details as JSON
     */
    std::optional<Json::Value> getTweet(
        const std::string& tweet_id,
        const std::string& access_token);

    /**
     * @brief Get authenticated user info
     * @param access_token User access token
     * @return User info as JSON
     */
    std::optional<Json::Value> getAuthenticatedUser(
        const std::string& access_token);

    // ========================================================================
    // RATE LIMITING
    // ========================================================================

    /**
     * @brief Check if we're rate limited for an endpoint
     * @param endpoint Endpoint name
     * @return True if rate limited
     */
    bool isRateLimited(const std::string& endpoint) const;

    /**
     * @brief Get time until rate limit resets
     * @param endpoint Endpoint name
     * @return Seconds until can call again
     */
    std::chrono::seconds getTimeUntilReset(const std::string& endpoint) const;

    /**
     * @brief Get remaining calls for an endpoint
     * @param endpoint Endpoint name
     * @return Remaining calls in current window
     */
    int getRemainingCalls(const std::string& endpoint) const;

private:
    TwitterClient() = default;
    ~TwitterClient() = default;

    /**
     * @brief Make a v2 API GET request
     */
    std::optional<Json::Value> apiGet(
        const std::string& endpoint,
        const std::string& access_token,
        const std::map<std::string, std::string>& params = {},
        bool use_bearer = false);

    /**
     * @brief Make a v2 API POST request
     */
    std::optional<Json::Value> apiPost(
        const std::string& endpoint,
        const std::string& access_token,
        const Json::Value& body);

    /**
     * @brief Make a v2 API DELETE request
     */
    bool apiDelete(
        const std::string& endpoint,
        const std::string& access_token);

    /**
     * @brief Make a media upload request (v1.1 API)
     */
    std::optional<Json::Value> mediaUpload(
        const std::string& endpoint,
        const std::string& access_token,
        const std::map<std::string, std::string>& params);

    /**
     * @brief Generate OAuth 1.0a signature for media uploads
     */
    std::string generateOAuth1Signature(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& params,
        const std::string& access_token,
        const std::string& access_token_secret);

    /**
     * @brief Update rate limit tracking from response headers
     */
    void updateRateLimits(
        const std::string& endpoint,
        const std::map<std::string, std::string>& headers);

    /**
     * @brief Parse error from API response
     */
    TwitterPostResult parseError(const Json::Value& response);

    /**
     * @brief Generate PKCE code challenge from verifier
     */
    static std::string generateCodeChallenge(const std::string& verifier);

    /**
     * @brief Generate random string for PKCE verifier
     */
    static std::string generateRandomString(int length);

    // Configuration
    std::string api_key_;
    std::string api_secret_;
    std::string bearer_token_;
    bool initialized_{false};

    // Rate limiting tracking
    struct RateLimitInfo {
        std::chrono::system_clock::time_point reset_at;
        int remaining{15};
        int limit{15};
    };
    std::map<std::string, RateLimitInfo> rate_limits_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::content::social::clients
