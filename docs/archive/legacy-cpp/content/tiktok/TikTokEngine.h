/**
 * @file TikTokEngine.h
 * @brief TikTok Automation Engine for dog awareness campaigns
 *
 * PURPOSE:
 * Central engine for generating, scheduling, and posting TikTok content
 * for shelter dogs. Integrates with templates, the TikTok API client,
 * and the database to automate social media presence for dog adoption.
 *
 * USAGE:
 * auto& engine = TikTokEngine::getInstance();
 * engine.initialize();
 *
 * // Generate post for a dog
 * Dog dog = dogService.findById(dog_id);
 * auto post = engine.generatePost(dog, TikTokTemplateType::URGENT_COUNTDOWN);
 *
 * // Schedule or post immediately
 * engine.schedulePost(post);
 * engine.postNow(post);
 *
 * DEPENDENCIES:
 * - TikTokClient (API communication)
 * - TikTokTemplate/TemplateManager (post templates)
 * - TikTokPost (post data model)
 * - Dog model (dog information)
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - Add new post generation strategies in generatePost()
 * - Update optimal posting logic in getOptimalPostTime()
 * - Add new analytics processing in updateAnalytics()
 *
 * @author Agent 11 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "TikTokPost.h"
#include "TikTokTemplate.h"
#include "TikTokClient.h"

// Forward declarations for Dog model
namespace wtl::core::models {
    struct Dog;
}

namespace wtl::content::tiktok {

// ============================================================================
// RESULT STRUCTURES
// ============================================================================

/**
 * @struct PostGenerationResult
 * @brief Result of post generation operation
 */
struct PostGenerationResult {
    bool success = false;
    std::optional<TikTokPost> post;
    std::string error_message;
    std::vector<std::string> warnings;

    Json::Value toJson() const;
};

/**
 * @struct PostingResult
 * @brief Result of posting operation
 */
struct PostingResult {
    bool success = false;
    std::optional<std::string> post_id;        ///< Database post ID
    std::optional<std::string> tiktok_post_id; ///< TikTok's post ID
    std::optional<std::string> tiktok_url;     ///< URL to the post
    std::string error_message;
    TikTokApiError api_error = TikTokApiError::NONE;

    Json::Value toJson() const;
};

/**
 * @struct SchedulingResult
 * @brief Result of scheduling operation
 */
struct SchedulingResult {
    bool success = false;
    std::optional<std::string> post_id;
    std::chrono::system_clock::time_point scheduled_at;
    std::string error_message;

    Json::Value toJson() const;
};

/**
 * @struct OptimalPostTime
 * @brief Recommended time for posting
 */
struct OptimalPostTime {
    std::chrono::system_clock::time_point recommended_time;
    int hour;                           ///< Hour of day (0-23)
    int day_of_week;                    ///< Day (0=Sunday, 6=Saturday)
    double confidence;                  ///< Confidence score (0-1)
    std::string reason;                 ///< Why this time is recommended

    Json::Value toJson() const;
};

/**
 * @struct EngineStatistics
 * @brief Statistics for the TikTok engine
 */
struct EngineStatistics {
    uint64_t posts_generated = 0;
    uint64_t posts_scheduled = 0;
    uint64_t posts_published = 0;
    uint64_t posts_failed = 0;
    uint64_t analytics_updated = 0;
    uint64_t auto_posts_generated = 0;

    // Performance
    double avg_generation_time_ms = 0.0;
    double avg_posting_time_ms = 0.0;

    // Engagement totals (from published posts)
    uint64_t total_views = 0;
    uint64_t total_likes = 0;
    uint64_t total_shares = 0;
    uint64_t total_comments = 0;

    Json::Value toJson() const;
};

// ============================================================================
// TIKTOK ENGINE CLASS
// ============================================================================

/**
 * @class TikTokEngine
 * @brief Singleton engine for TikTok automation
 *
 * Manages the full lifecycle of TikTok posts:
 * 1. Content generation from dog data and templates
 * 2. Scheduling for optimal posting times
 * 3. Actual posting via TikTok API
 * 4. Analytics tracking and updates
 */
class TikTokEngine {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the TikTokEngine singleton
     */
    static TikTokEngine& getInstance();

    // Delete copy/move constructors
    TikTokEngine(const TikTokEngine&) = delete;
    TikTokEngine& operator=(const TikTokEngine&) = delete;
    TikTokEngine(TikTokEngine&&) = delete;
    TikTokEngine& operator=(TikTokEngine&&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize the engine
     * @return true if initialized successfully
     */
    bool initialize();

    /**
     * @brief Initialize from Config singleton
     * @return true if initialized successfully
     */
    bool initializeFromConfig();

    /**
     * @brief Check if engine is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    /**
     * @brief Shutdown the engine
     */
    void shutdown();

    // ========================================================================
    // POST GENERATION
    // ========================================================================

    /**
     * @brief Generate a TikTok post for a dog
     *
     * @param dog The dog to create a post for
     * @param template_type Template type to use
     * @return PostGenerationResult Result containing the generated post
     *
     * Generates caption, selects hashtags, determines overlay settings
     * based on the template and dog's urgency level.
     *
     * @example
     * Dog dog = DogService::getInstance().findById(id);
     * auto result = TikTokEngine::getInstance().generatePost(
     *     dog, TikTokTemplateType::URGENT_COUNTDOWN);
     * if (result.success) {
     *     auto post = *result.post;
     * }
     */
    PostGenerationResult generatePost(
        const wtl::core::models::Dog& dog,
        TikTokTemplateType template_type);

    /**
     * @brief Generate a post using a specific template ID
     *
     * @param dog The dog to create a post for
     * @param template_id Specific template ID to use
     * @return PostGenerationResult Result containing the generated post
     */
    PostGenerationResult generatePostWithTemplate(
        const wtl::core::models::Dog& dog,
        const std::string& template_id);

    /**
     * @brief Generate a post with custom content
     *
     * @param dog The dog the post is about
     * @param caption Custom caption text
     * @param hashtags Custom hashtags
     * @return PostGenerationResult Result containing the generated post
     */
    PostGenerationResult generateCustomPost(
        const wtl::core::models::Dog& dog,
        const std::string& caption,
        const std::vector<std::string>& hashtags);

    /**
     * @brief Auto-select best template for a dog
     *
     * @param dog The dog to analyze
     * @return TikTokTemplateType Recommended template type
     */
    TikTokTemplateType selectBestTemplate(const wtl::core::models::Dog& dog);

    // ========================================================================
    // SCHEDULING
    // ========================================================================

    /**
     * @brief Schedule a post for future posting
     *
     * @param post The post to schedule
     * @return SchedulingResult Result with scheduled time
     *
     * Saves post to database and marks as scheduled.
     */
    SchedulingResult schedulePost(const TikTokPost& post);

    /**
     * @brief Schedule a post for a specific time
     *
     * @param post The post to schedule
     * @param scheduled_time When to post
     * @return SchedulingResult Result with scheduled time
     */
    SchedulingResult schedulePostAt(
        const TikTokPost& post,
        std::chrono::system_clock::time_point scheduled_time);

    /**
     * @brief Get optimal time for posting
     *
     * @param template_type Template type (affects optimal time)
     * @return OptimalPostTime Recommended posting time
     *
     * Analyzes engagement patterns to recommend best posting time.
     */
    OptimalPostTime getOptimalPostTime(TikTokTemplateType template_type =
                                        TikTokTemplateType::EDUCATIONAL);

    /**
     * @brief Get next N optimal posting slots
     *
     * @param count Number of slots to return
     * @param template_type Template type for recommendations
     * @return std::vector<OptimalPostTime> Recommended posting times
     */
    std::vector<OptimalPostTime> getOptimalPostSlots(
        int count,
        TikTokTemplateType template_type = TikTokTemplateType::EDUCATIONAL);

    // ========================================================================
    // POSTING
    // ========================================================================

    /**
     * @brief Post immediately to TikTok
     *
     * @param post The post to publish
     * @return PostingResult Result with TikTok post ID and URL
     *
     * Posts content to TikTok and updates database with result.
     */
    PostingResult postNow(TikTokPost& post);

    /**
     * @brief Post by database post ID
     *
     * @param post_id Database post ID
     * @return PostingResult Result of posting operation
     */
    PostingResult postById(const std::string& post_id);

    /**
     * @brief Process all due scheduled posts
     *
     * @return int Number of posts processed
     *
     * Called by TikTokWorker to process posts whose scheduled_at has passed.
     */
    int processScheduledPosts();

    /**
     * @brief Retry a failed post
     *
     * @param post_id Database post ID of failed post
     * @return PostingResult Result of retry attempt
     */
    PostingResult retryFailedPost(const std::string& post_id);

    // ========================================================================
    // ANALYTICS
    // ========================================================================

    /**
     * @brief Update analytics for a specific post
     *
     * @param post_id Database post ID
     * @return bool true if updated successfully
     *
     * Fetches latest analytics from TikTok and updates database.
     */
    bool updateAnalytics(const std::string& post_id);

    /**
     * @brief Update analytics for all published posts
     *
     * @param max_posts Maximum posts to update (0 = all)
     * @return int Number of posts updated
     */
    int updateAllAnalytics(int max_posts = 0);

    /**
     * @brief Get analytics summary across all posts
     *
     * @return Json::Value Aggregated analytics data
     */
    Json::Value getAnalyticsSummary();

    /**
     * @brief Get top performing posts
     *
     * @param count Number of posts to return
     * @param metric Metric to sort by (views, likes, shares, engagement)
     * @return std::vector<TikTokPost> Top performing posts
     */
    std::vector<TikTokPost> getTopPerformingPosts(
        int count, const std::string& metric = "views");

    // ========================================================================
    // POST MANAGEMENT
    // ========================================================================

    /**
     * @brief Save a post to database
     *
     * @param post The post to save
     * @return std::optional<std::string> Post ID if saved, nullopt on error
     */
    std::optional<std::string> savePost(const TikTokPost& post);

    /**
     * @brief Get a post by ID
     *
     * @param post_id Database post ID
     * @return std::optional<TikTokPost> The post if found
     */
    std::optional<TikTokPost> getPost(const std::string& post_id);

    /**
     * @brief Update a post in database
     *
     * @param post The post to update
     * @return bool true if updated
     */
    bool updatePost(const TikTokPost& post);

    /**
     * @brief Delete a post
     *
     * @param post_id Database post ID
     * @param delete_from_tiktok Also delete from TikTok if published
     * @return bool true if deleted
     */
    bool deletePost(const std::string& post_id, bool delete_from_tiktok = false);

    /**
     * @brief Get posts by status
     *
     * @param status Post status to filter by
     * @param limit Maximum posts to return
     * @return std::vector<TikTokPost> Matching posts
     */
    std::vector<TikTokPost> getPostsByStatus(
        TikTokPostStatus status, int limit = 100);

    /**
     * @brief Get posts for a specific dog
     *
     * @param dog_id Dog ID
     * @return std::vector<TikTokPost> Posts for the dog
     */
    std::vector<TikTokPost> getPostsByDog(const std::string& dog_id);

    /**
     * @brief Get all scheduled posts
     *
     * @return std::vector<TikTokPost> Scheduled posts ordered by time
     */
    std::vector<TikTokPost> getScheduledPosts();

    // ========================================================================
    // AUTOMATION
    // ========================================================================

    /**
     * @brief Generate posts for urgent dogs automatically
     *
     * @param max_posts Maximum posts to generate
     * @return int Number of posts generated
     *
     * Finds dogs with high urgency and generates posts for them.
     */
    int generateUrgentDogPosts(int max_posts = 5);

    /**
     * @brief Generate posts for longest waiting dogs
     *
     * @param max_posts Maximum posts to generate
     * @return int Number of posts generated
     */
    int generateLongestWaitingPosts(int max_posts = 3);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get engine statistics
     *
     * @return EngineStatistics Current statistics
     */
    EngineStatistics getStatistics() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

private:
    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================

    TikTokEngine() = default;
    ~TikTokEngine() = default;

    /**
     * @brief Build placeholder map from dog data
     *
     * @param dog The dog
     * @return std::unordered_map<std::string, std::string> Placeholder values
     */
    std::unordered_map<std::string, std::string> buildPlaceholders(
        const wtl::core::models::Dog& dog);

    /**
     * @brief Calculate wait time string for dog
     *
     * @param dog The dog
     * @return std::string Human-readable wait time
     */
    std::string calculateWaitTimeString(const wtl::core::models::Dog& dog);

    /**
     * @brief Calculate urgency string for dog
     *
     * @param dog The dog
     * @return std::string Human-readable urgency (e.g., "48 hours")
     */
    std::string calculateUrgencyString(const wtl::core::models::Dog& dog);

    /**
     * @brief Check if dog is "overlooked" type
     *
     * @param dog The dog
     * @return bool true if senior, black dog, pit bull, etc.
     */
    bool isDogOverlooked(const wtl::core::models::Dog& dog);

    /**
     * @brief Generate unique post ID
     * @return std::string UUID for post
     */
    std::string generatePostId();

    /**
     * @brief Update post status in database
     *
     * @param post_id Post ID
     * @param status New status
     * @param error_message Error message if failed (optional)
     */
    void updatePostStatus(const std::string& post_id,
                         TikTokPostStatus status,
                         const std::string& error_message = "");

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    EngineStatistics stats_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Configuration
    int max_daily_posts_ = 10;              ///< Maximum posts per day
    int max_posts_per_dog_ = 3;             ///< Max active posts per dog
    bool auto_schedule_ = true;             ///< Auto-schedule generated posts
    int urgent_threshold_hours_ = 72;       ///< Hours for "urgent" classification
    int long_wait_threshold_days_ = 90;     ///< Days for "long wait" classification
};

} // namespace wtl::content::tiktok
