/**
 * @file TikTokClient.h
 * @brief HTTP client for TikTok API integration with rate limiting and circuit breaker
 *
 * PURPOSE:
 * Provides HTTP client functionality for interacting with the TikTok API.
 * Handles authentication, posting content, fetching analytics, and managing
 * rate limits with circuit breaker pattern for resilience.
 *
 * USAGE:
 * auto& client = TikTokClient::getInstance();
 * client.initialize();
 * auto result = client.post(post_content);
 * auto analytics = client.getPostAnalytics(post_id);
 *
 * DEPENDENCIES:
 * - Drogon HTTP client for HTTP requests
 * - SelfHealing (circuit breaker and retry)
 * - ErrorCapture (error logging)
 * - Config (API configuration)
 *
 * MODIFICATION GUIDE:
 * - Update API endpoints when TikTok API changes
 * - Adjust rate limits based on TikTok API quotas
 * - Add new API methods as TikTok releases features
 *
 * @author Agent 11 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>

// Third-party includes
#include <json/json.h>
#include <drogon/drogon.h>

namespace wtl::content::tiktok {

// Forward declarations
struct TikTokPost;

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum TikTokApiError
 * @brief Error codes for TikTok API operations
 */
enum class TikTokApiError {
    NONE = 0,                   ///< No error
    NETWORK_ERROR,              ///< Network connectivity issue
    AUTH_FAILED,                ///< Authentication failed
    AUTH_EXPIRED,               ///< Token expired
    RATE_LIMITED,               ///< Rate limit exceeded
    QUOTA_EXCEEDED,             ///< Daily quota exceeded
    INVALID_CONTENT,            ///< Content rejected by TikTok
    CONTENT_TOO_LONG,           ///< Caption/content exceeds limits
    MEDIA_ERROR,                ///< Media upload/processing failed
    POST_NOT_FOUND,             ///< Post ID not found
    PERMISSION_DENIED,          ///< Insufficient permissions
    API_ERROR,                  ///< Generic API error
    CIRCUIT_OPEN,               ///< Circuit breaker is open
    TIMEOUT,                    ///< Request timed out
    UNKNOWN                     ///< Unknown error
};

// ============================================================================
// RESULT STRUCTURES
// ============================================================================

/**
 * @struct TikTokApiResult
 * @brief Result of a TikTok API operation
 */
struct TikTokApiResult {
    bool success = false;                   ///< Whether operation succeeded
    TikTokApiError error = TikTokApiError::NONE;  ///< Error code if failed
    std::string error_message;              ///< Error message if failed
    std::optional<std::string> post_id;     ///< TikTok post ID if applicable
    std::optional<std::string> post_url;    ///< TikTok post URL if applicable
    Json::Value data;                       ///< Response data from API
    int http_status = 0;                    ///< HTTP status code
    int retry_count = 0;                    ///< Number of retries attempted
    std::chrono::milliseconds latency{0};   ///< Request latency

    /**
     * @brief Convert result to JSON
     * @return Json::Value The JSON representation
     */
    Json::Value toJson() const;
};

/**
 * @struct TikTokAuthToken
 * @brief TikTok API authentication token
 */
struct TikTokAuthToken {
    std::string access_token;               ///< OAuth access token
    std::string refresh_token;              ///< OAuth refresh token
    std::string open_id;                    ///< TikTok user open ID
    std::string scope;                      ///< Token scopes
    std::chrono::system_clock::time_point expires_at;  ///< Expiration time

    /**
     * @brief Check if token is expired
     * @return true if token is expired or about to expire
     */
    bool isExpired() const;

    /**
     * @brief Check if token needs refresh (expiring soon)
     * @return true if token should be refreshed
     */
    bool needsRefresh() const;

    /**
     * @brief Convert to JSON
     * @return Json::Value The JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json The JSON data
     * @return TikTokAuthToken The parsed token
     */
    static TikTokAuthToken fromJson(const Json::Value& json);
};

/**
 * @struct TikTokRateLimitInfo
 * @brief Rate limit information from TikTok API
 */
struct TikTokRateLimitInfo {
    int limit = 0;                          ///< Total requests allowed
    int remaining = 0;                      ///< Requests remaining
    std::chrono::system_clock::time_point reset_at;  ///< When limit resets
    bool is_limited = false;                ///< Currently rate limited

    /**
     * @brief Check if we should wait before next request
     * @return std::chrono::milliseconds Time to wait (0 if ok)
     */
    std::chrono::milliseconds getWaitTime() const;
};

/**
 * @struct TikTokPostContent
 * @brief Content for posting to TikTok
 */
struct TikTokPostContent {
    std::string caption;                    ///< Post caption
    std::vector<std::string> hashtags;      ///< Hashtags to include
    std::optional<std::string> video_url;   ///< Video URL for upload
    std::vector<std::string> photo_urls;    ///< Photo URLs for slideshow
    std::optional<std::string> music_id;    ///< TikTok sound ID
    bool is_video = true;                   ///< Video or photo content
    std::optional<std::string> disclosure;  ///< Content disclosure
    bool allow_comments = true;             ///< Allow comments on post
    bool allow_duet = true;                 ///< Allow duet
    bool allow_stitch = true;               ///< Allow stitch

    /**
     * @brief Validate content meets TikTok requirements
     * @return std::vector<std::string> List of validation errors
     */
    std::vector<std::string> validate() const;
};

// ============================================================================
// TIKTOK CLIENT CLASS
// ============================================================================

/**
 * @class TikTokClient
 * @brief Singleton HTTP client for TikTok API operations
 *
 * Handles all communication with TikTok API including:
 * - OAuth authentication
 * - Content posting
 * - Analytics fetching
 * - Rate limiting
 * - Circuit breaker for resilience
 */
class TikTokClient {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the TikTokClient singleton
     */
    static TikTokClient& getInstance();

    // Delete copy/move constructors
    TikTokClient(const TikTokClient&) = delete;
    TikTokClient& operator=(const TikTokClient&) = delete;
    TikTokClient(TikTokClient&&) = delete;
    TikTokClient& operator=(TikTokClient&&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize the client
     *
     * @param client_key TikTok API client key
     * @param client_secret TikTok API client secret
     * @param redirect_uri OAuth redirect URI
     * @return true if initialized successfully
     */
    bool initialize(const std::string& client_key,
                   const std::string& client_secret,
                   const std::string& redirect_uri);

    /**
     * @brief Initialize from Config singleton
     * @return true if initialized successfully
     */
    bool initializeFromConfig();

    /**
     * @brief Check if client is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    /**
     * @brief Shutdown the client
     */
    void shutdown();

    // ========================================================================
    // AUTHENTICATION
    // ========================================================================

    /**
     * @brief Get OAuth authorization URL
     *
     * @param state State parameter for CSRF protection
     * @return std::string Authorization URL to redirect user
     */
    std::string getAuthorizationUrl(const std::string& state) const;

    /**
     * @brief Exchange authorization code for tokens
     *
     * @param code Authorization code from OAuth callback
     * @return TikTokApiResult Result with tokens if successful
     */
    TikTokApiResult authenticate(const std::string& code);

    /**
     * @brief Refresh access token
     *
     * @return TikTokApiResult Result with new token if successful
     */
    TikTokApiResult refreshToken();

    /**
     * @brief Set authentication token directly
     *
     * @param token The authentication token
     */
    void setAuthToken(const TikTokAuthToken& token);

    /**
     * @brief Get current authentication token
     *
     * @return std::optional<TikTokAuthToken> Current token if authenticated
     */
    std::optional<TikTokAuthToken> getAuthToken() const;

    /**
     * @brief Check if client is authenticated
     *
     * @return true if authenticated with valid token
     */
    bool isAuthenticated() const;

    /**
     * @brief Revoke current token
     *
     * @return TikTokApiResult Result of revocation
     */
    TikTokApiResult revokeToken();

    // ========================================================================
    // POSTING
    // ========================================================================

    /**
     * @brief Post content to TikTok
     *
     * @param content The content to post
     * @return TikTokApiResult Result with post ID and URL if successful
     *
     * @note Uses TikTok's Content Posting API
     */
    TikTokApiResult post(const TikTokPostContent& content);

    /**
     * @brief Upload video for posting
     *
     * @param video_url URL of video to upload
     * @return TikTokApiResult Result with video ID if successful
     */
    TikTokApiResult uploadVideo(const std::string& video_url);

    /**
     * @brief Upload photos for slideshow
     *
     * @param photo_urls URLs of photos to upload
     * @return TikTokApiResult Result with photo IDs if successful
     */
    TikTokApiResult uploadPhotos(const std::vector<std::string>& photo_urls);

    /**
     * @brief Delete a post from TikTok
     *
     * @param post_id TikTok post ID
     * @return TikTokApiResult Result of deletion
     */
    TikTokApiResult deletePost(const std::string& post_id);

    // ========================================================================
    // ANALYTICS
    // ========================================================================

    /**
     * @brief Get analytics for a specific post
     *
     * @param post_id TikTok post ID
     * @return TikTokApiResult Result with analytics data
     *
     * Analytics include: views, likes, comments, shares, reach
     */
    TikTokApiResult getPostAnalytics(const std::string& post_id);

    /**
     * @brief Get analytics for multiple posts
     *
     * @param post_ids List of TikTok post IDs
     * @return TikTokApiResult Result with analytics for all posts
     */
    TikTokApiResult getMultiPostAnalytics(const std::vector<std::string>& post_ids);

    /**
     * @brief Get account analytics
     *
     * @return TikTokApiResult Result with account-level analytics
     */
    TikTokApiResult getAccountAnalytics();

    // ========================================================================
    // UTILITY
    // ========================================================================

    /**
     * @brief Get current rate limit status
     *
     * @return TikTokRateLimitInfo Current rate limit information
     */
    TikTokRateLimitInfo getRateLimitInfo() const;

    /**
     * @brief Check if circuit breaker is open
     *
     * @return true if circuit breaker is open (API unavailable)
     */
    bool isCircuitOpen() const;

    /**
     * @brief Get client statistics
     *
     * @return Json::Value Statistics (requests, errors, latency, etc.)
     */
    Json::Value getStats() const;

    /**
     * @brief Test API connectivity
     *
     * @return true if API is reachable and responding
     */
    bool testConnectivity();

    /**
     * @brief Convert TikTokApiError to string
     * @param error The error code
     * @return std::string String representation
     */
    static std::string errorToString(TikTokApiError error);

private:
    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================

    TikTokClient() = default;
    ~TikTokClient() = default;

    /**
     * @brief Make an authenticated API request
     *
     * @param method HTTP method
     * @param endpoint API endpoint
     * @param body Request body (optional)
     * @param with_retry Whether to retry on failure
     * @return TikTokApiResult API result
     */
    TikTokApiResult makeRequest(
        drogon::HttpMethod method,
        const std::string& endpoint,
        const Json::Value& body = Json::nullValue,
        bool with_retry = true);

    /**
     * @brief Parse rate limit headers from response
     *
     * @param response HTTP response
     */
    void parseRateLimitHeaders(const drogon::HttpResponsePtr& response);

    /**
     * @brief Parse error from API response
     *
     * @param response HTTP response
     * @return TikTokApiError Parsed error code
     */
    TikTokApiError parseApiError(const drogon::HttpResponsePtr& response);

    /**
     * @brief Wait for rate limit if needed
     */
    void waitForRateLimit();

    /**
     * @brief Record request metrics
     *
     * @param success Whether request succeeded
     * @param latency Request latency
     */
    void recordMetrics(bool success, std::chrono::milliseconds latency);

    /**
     * @brief Ensure token is valid, refresh if needed
     * @return true if token is valid (or was refreshed)
     */
    bool ensureValidToken();

    // API Configuration
    std::string client_key_;
    std::string client_secret_;
    std::string redirect_uri_;
    std::string api_base_url_ = "https://open.tiktokapis.com/v2";

    // Authentication
    std::optional<TikTokAuthToken> auth_token_;
    mutable std::mutex auth_mutex_;

    // Rate limiting
    TikTokRateLimitInfo rate_limit_;
    mutable std::mutex rate_limit_mutex_;

    // Circuit breaker
    std::atomic<bool> circuit_open_{false};
    std::atomic<int> consecutive_failures_{0};
    std::chrono::system_clock::time_point circuit_opened_at_;
    static constexpr int CIRCUIT_FAILURE_THRESHOLD = 5;
    static constexpr std::chrono::seconds CIRCUIT_RESET_TIMEOUT{60};

    // Statistics
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> successful_requests_{0};
    std::atomic<uint64_t> failed_requests_{0};
    std::atomic<uint64_t> rate_limited_requests_{0};
    std::atomic<uint64_t> total_latency_ms_{0};

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};

    // Request timeout
    std::chrono::seconds request_timeout_{30};

    // Retry configuration
    int max_retries_ = 3;
    std::chrono::milliseconds retry_base_delay_{1000};
};

} // namespace wtl::content::tiktok
