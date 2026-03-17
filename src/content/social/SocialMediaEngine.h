/**
 * @file SocialMediaEngine.h
 * @brief Central engine for cross-posting to multiple social media platforms
 *
 * PURPOSE:
 * The SocialMediaEngine is the main orchestrator for social media operations.
 * It handles cross-posting content to multiple platforms, formatting content
 * for each platform's requirements, scheduling posts, and syncing analytics.
 *
 * USAGE:
 * auto& engine = SocialMediaEngine::getInstance();
 * auto post = SocialPostBuilder::forDog(dog_id)
 *     .withText("Meet Max!")
 *     .withImage(image_url)
 *     .toPlatforms({Platform::FACEBOOK, Platform::INSTAGRAM})
 *     .build();
 * auto results = engine.crossPost(post);
 *
 * DEPENDENCIES:
 * - Platform.h (Platform enum)
 * - SocialPost.h (SocialPost model)
 * - SocialAnalytics.h (analytics tracking)
 * - FacebookClient, InstagramClient, TwitterClient (API clients)
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - Add new platforms by implementing their client and adding to crossPost
 * - Extend formatForPlatform for new content types
 * - Add new scheduling strategies as needed
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/social/Platform.h"
#include "content/social/SocialPost.h"
#include "content/social/SocialAnalytics.h"

namespace wtl::content::social {

// ============================================================================
// CROSS-POST RESULT
// ============================================================================

/**
 * @struct CrossPostResult
 * @brief Result of a cross-posting operation
 */
struct CrossPostResult {
    std::string post_id;                  ///< Internal post UUID
    bool overall_success;                 ///< True if at least one platform succeeded
    bool all_succeeded;                   ///< True if all platforms succeeded

    // Per-platform results
    std::map<Platform, bool> platform_success;
    std::map<Platform, std::string> platform_post_ids;
    std::map<Platform, std::string> platform_urls;
    std::map<Platform, std::string> platform_errors;

    // Stats
    int platforms_attempted;
    int platforms_succeeded;
    int platforms_failed;

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @struct OptimalPostTime
 * @brief Recommended posting time for a platform
 */
struct OptimalPostTime {
    Platform platform;
    int day_of_week;                      ///< 0=Sunday, 6=Saturday
    int hour;                             ///< 0-23
    double engagement_score;              ///< Relative engagement score
    std::string reason;                   ///< Why this time is recommended
};

// ============================================================================
// SOCIAL MEDIA ENGINE
// ============================================================================

/**
 * @class SocialMediaEngine
 * @brief Singleton service for social media cross-posting
 */
class SocialMediaEngine {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     */
    static SocialMediaEngine& getInstance();

    // Prevent copying
    SocialMediaEngine(const SocialMediaEngine&) = delete;
    SocialMediaEngine& operator=(const SocialMediaEngine&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize the engine
     */
    void initialize();

    /**
     * @brief Initialize from configuration
     */
    void initializeFromConfig();

    /**
     * @brief Check if engine is initialized
     */
    bool isInitialized() const;

    /**
     * @brief Shutdown the engine and release resources
     */
    void shutdown();

    // ========================================================================
    // CROSS-POSTING
    // ========================================================================

    /**
     * @brief Cross-post content to multiple platforms
     * @param post The post to publish
     * @param platforms Target platforms (uses post.platforms if empty)
     * @return CrossPostResult Results for each platform
     */
    CrossPostResult crossPost(
        const SocialPost& post,
        const std::vector<Platform>& platforms = {});

    /**
     * @brief Cross-post content asynchronously
     * @param post The post to publish
     * @param platforms Target platforms
     * @param callback Callback when complete
     */
    void crossPostAsync(
        const SocialPost& post,
        const std::vector<Platform>& platforms,
        std::function<void(const CrossPostResult&)> callback);

    /**
     * @brief Cross-post a TikTok video to other platforms
     * @param tiktok_url URL of TikTok video
     * @param dog_id Associated dog ID
     * @param platforms Target platforms (default: FB, IG, Twitter)
     * @return CrossPostResult Results
     */
    CrossPostResult crossPostTikTok(
        const std::string& tiktok_url,
        const std::string& dog_id,
        const std::vector<Platform>& platforms = {});

    // ========================================================================
    // CONTENT FORMATTING
    // ========================================================================

    /**
     * @brief Format a post for a specific platform
     * @param post The original post
     * @param platform Target platform
     * @return SocialPost Formatted post for the platform
     */
    SocialPost formatForPlatform(const SocialPost& post, Platform platform) const;

    /**
     * @brief Generate platform-specific caption
     * @param post The post
     * @param platform Target platform
     * @return std::string Formatted caption
     */
    std::string generateCaption(const SocialPost& post, Platform platform) const;

    /**
     * @brief Get recommended hashtags for a dog post
     * @param dog_id The dog ID
     * @param platform Target platform
     * @return std::vector<std::string> Recommended hashtags
     */
    std::vector<std::string> getRecommendedHashtags(
        const std::string& dog_id,
        Platform platform) const;

    // ========================================================================
    // SCHEDULING
    // ========================================================================

    /**
     * @brief Schedule a post for later
     * @param post The post to schedule
     * @param scheduled_time When to post
     * @return std::string Scheduled post ID
     */
    std::string schedulePost(
        const SocialPost& post,
        std::chrono::system_clock::time_point scheduled_time);

    /**
     * @brief Schedule a post for optimal times
     * @param post The post to schedule
     * @return std::map<Platform, std::chrono::system_clock::time_point> Scheduled times per platform
     */
    std::map<Platform, std::chrono::system_clock::time_point> scheduleForOptimalTimes(
        const SocialPost& post);

    /**
     * @brief Cancel a scheduled post
     * @param post_id The scheduled post ID
     * @return bool True if cancelled
     */
    bool cancelScheduledPost(const std::string& post_id);

    /**
     * @brief Get all scheduled posts
     * @return std::vector<SocialPost> Scheduled posts
     */
    std::vector<SocialPost> getScheduledPosts() const;

    /**
     * @brief Process due scheduled posts
     * @return int Number of posts processed
     */
    int processScheduledPosts();

    // ========================================================================
    // OPTIMAL TIMING
    // ========================================================================

    /**
     * @brief Get optimal posting times for a platform
     * @param platform Target platform
     * @return std::vector<OptimalPostTime> Best times to post
     */
    std::vector<OptimalPostTime> getOptimalPostTimes(Platform platform) const;

    /**
     * @brief Get next optimal posting time
     * @param platform Target platform
     * @return std::chrono::system_clock::time_point Next optimal time
     */
    std::chrono::system_clock::time_point getNextOptimalTime(Platform platform) const;

    // ========================================================================
    // ANALYTICS SYNC
    // ========================================================================

    /**
     * @brief Sync analytics for all recent posts
     * @param hours_back Hours to look back
     * @return int Number of posts synced
     */
    int syncAnalytics(int hours_back = 24);

    /**
     * @brief Sync analytics for a specific post
     * @param post_id The post ID
     * @return bool True if synced successfully
     */
    bool syncPostAnalytics(const std::string& post_id);

    // ========================================================================
    // POST MANAGEMENT
    // ========================================================================

    /**
     * @brief Save a post to database
     * @param post The post to save
     * @return std::string The post ID
     */
    std::string savePost(const SocialPost& post);

    /**
     * @brief Get a post by ID
     * @param post_id The post ID
     * @return std::optional<SocialPost> The post if found
     */
    std::optional<SocialPost> getPost(const std::string& post_id) const;

    /**
     * @brief Update a post
     * @param post The post to update
     * @return bool True if updated
     */
    bool updatePost(const SocialPost& post);

    /**
     * @brief Delete a post from all platforms
     * @param post_id The post ID
     * @return bool True if deleted
     */
    bool deletePost(const std::string& post_id);

    /**
     * @brief Get all posts for a dog
     * @param dog_id The dog ID
     * @return std::vector<SocialPost> Posts for the dog
     */
    std::vector<SocialPost> getPostsForDog(const std::string& dog_id) const;

    /**
     * @brief Get recent posts
     * @param limit Max posts to return
     * @return std::vector<SocialPost> Recent posts
     */
    std::vector<SocialPost> getRecentPosts(int limit = 50) const;

    // ========================================================================
    // PLATFORM CONNECTIONS
    // ========================================================================

    /**
     * @brief Get connection for a platform
     * @param user_id User ID
     * @param platform Target platform
     * @return std::optional<PlatformConnection> The connection if exists
     */
    std::optional<PlatformConnection> getConnection(
        const std::string& user_id,
        Platform platform) const;

    /**
     * @brief Save/update a platform connection
     * @param connection The connection to save
     * @return bool True if saved
     */
    bool saveConnection(const PlatformConnection& connection);

    /**
     * @brief Get all connections for a user
     * @param user_id User ID
     * @return std::vector<PlatformConnection> User's connections
     */
    std::vector<PlatformConnection> getUserConnections(
        const std::string& user_id) const;

    /**
     * @brief Refresh expired token
     * @param connection_id Connection ID
     * @return bool True if refreshed
     */
    bool refreshToken(const std::string& connection_id);

    // ========================================================================
    // AUTO-GENERATION
    // ========================================================================

    /**
     * @brief Auto-generate post for urgent dog
     * @param dog_id The urgent dog ID
     * @return std::optional<SocialPost> Generated post
     */
    std::optional<SocialPost> generateUrgentDogPost(const std::string& dog_id);

    /**
     * @brief Auto-generate milestone post (1 year, etc.)
     * @param dog_id The dog ID
     * @param milestone_type Milestone type (e.g., "1_year", "2_years")
     * @return std::optional<SocialPost> Generated post
     */
    std::optional<SocialPost> generateMilestonePost(
        const std::string& dog_id,
        const std::string& milestone_type);

private:
    SocialMediaEngine() = default;
    ~SocialMediaEngine() = default;

    /**
     * @brief Post to a single platform
     */
    std::tuple<bool, std::string, std::string, std::string> postToPlatform(
        const SocialPost& post,
        Platform platform,
        const PlatformConnection& connection);

    /**
     * @brief Get default connection for platform
     */
    std::optional<PlatformConnection> getDefaultConnection(Platform platform) const;

    /**
     * @brief Generate UUID
     */
    std::string generateUUID() const;

    // State
    bool initialized_{false};

    // Default hashtags per post type
    std::map<PostType, std::vector<std::string>> default_hashtags_;

    // Thread safety
    mutable std::shared_mutex mutex_;
};

} // namespace wtl::content::social
