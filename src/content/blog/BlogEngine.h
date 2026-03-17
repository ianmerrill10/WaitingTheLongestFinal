/**
 * @file BlogEngine.h
 * @brief Main engine for the DOGBLOG Content System
 *
 * PURPOSE:
 * Central orchestrator for all blog content generation, scheduling, and
 * publishing. Coordinates between ContentGenerator, BlogService, and
 * other platform services to automate content creation.
 *
 * USAGE:
 * auto& engine = BlogEngine::getInstance();
 * engine.generateUrgentRoundup();
 * engine.generateDogFeature(dog);
 * engine.schedulePosts();
 * engine.publishPost(post_id);
 *
 * DEPENDENCIES:
 * - ContentGenerator (markdown generation)
 * - BlogService (CRUD operations)
 * - DogService (dog data)
 * - ShelterService (shelter data)
 * - FosterService (adoption data)
 * - UrgencyCalculator (urgency data)
 * - EventBus (event publishing)
 *
 * MODIFICATION GUIDE:
 * - Add new generation methods for new content types
 * - Extend scheduling logic for different frequencies
 * - Add event triggers for real-time content updates
 *
 * @author Agent 12 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/blog/BlogPost.h"
#include "content/blog/BlogCategory.h"
#include "content/blog/ContentGenerator.h"
#include "core/models/Dog.h"
#include "core/models/FosterPlacement.h"

namespace wtl::content::blog {

/**
 * @struct ScheduleConfig
 * @brief Configuration for automatic content scheduling
 */
struct ScheduleConfig {
    bool auto_generate_urgent_roundup = true;     ///< Generate daily urgent roundup
    int urgent_roundup_hour = 6;                  ///< Hour to publish (0-23, default 6am)

    bool auto_feature_longest_waiting = true;    ///< Auto-feature longest waiting dogs
    int feature_frequency_hours = 24;             ///< Hours between features

    bool auto_publish_success_stories = true;    ///< Auto-publish success stories

    int max_scheduled_posts = 10;                 ///< Max posts to keep scheduled
    int min_hours_between_posts = 4;              ///< Minimum spacing between posts
};

/**
 * @struct GenerationResult
 * @brief Result of a content generation operation
 */
struct GenerationResult {
    bool success;                   ///< Whether generation succeeded
    std::string post_id;           ///< ID of generated post (if success)
    std::string error;             ///< Error message (if failed)
    BlogPost post;                 ///< The generated post
};

/**
 * @struct ScheduleInfo
 * @brief Information about a scheduled post
 */
struct ScheduleInfo {
    std::string post_id;
    std::string title;
    BlogCategory category;
    std::chrono::system_clock::time_point scheduled_at;
    bool is_auto_generated;
};

/**
 * @class BlogEngine
 * @brief Singleton engine for blog content generation and management
 *
 * Orchestrates all blog content creation, from generating individual
 * dog features to daily urgent roundups and success stories. Handles
 * automatic scheduling and publishing.
 */
class BlogEngine {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the BlogEngine instance
     */
    static BlogEngine& getInstance();

    // Prevent copying
    BlogEngine(const BlogEngine&) = delete;
    BlogEngine& operator=(const BlogEngine&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize the blog engine
     * @param schedule_config Scheduling configuration
     */
    void initialize(const ScheduleConfig& schedule_config = ScheduleConfig());

    /**
     * @brief Shutdown the blog engine
     */
    void shutdown();

    /**
     * @brief Check if engine is running
     */
    bool isRunning() const;

    // ========================================================================
    // URGENT ROUNDUP
    // ========================================================================

    /**
     * @brief Generate a daily urgent dog roundup post
     *
     * Creates a roundup of all dogs currently in urgent/critical situations.
     * Can be called manually or automatically via scheduler.
     *
     * @param auto_publish Whether to publish immediately
     * @return GenerationResult Result of generation
     *
     * @example
     * auto result = engine.generateUrgentRoundup();
     * if (result.success) {
     *     // Post created with ID result.post_id
     * }
     */
    GenerationResult generateUrgentRoundup(bool auto_publish = false);

    /**
     * @brief Generate urgent roundup for specific date
     * @param date Date for the roundup
     * @param auto_publish Whether to publish immediately
     * @return GenerationResult Result
     */
    GenerationResult generateUrgentRoundupForDate(
        const std::chrono::system_clock::time_point& date,
        bool auto_publish = false
    );

    // ========================================================================
    // DOG FEATURES
    // ========================================================================

    /**
     * @brief Generate a feature post for a specific dog
     *
     * Creates an engaging feature article highlighting a single dog,
     * including their story, attributes, photos, and adoption info.
     *
     * @param dog The dog to feature
     * @param auto_publish Whether to publish immediately
     * @return GenerationResult Result of generation
     */
    GenerationResult generateDogFeature(
        const wtl::core::models::Dog& dog,
        bool auto_publish = false
    );

    /**
     * @brief Generate feature for dog by ID
     * @param dog_id Dog UUID
     * @param auto_publish Whether to publish immediately
     * @return GenerationResult Result
     */
    GenerationResult generateDogFeatureById(
        const std::string& dog_id,
        bool auto_publish = false
    );

    /**
     * @brief Generate features for longest waiting dogs
     *
     * Creates feature posts for dogs waiting the longest who don't
     * already have recent feature posts.
     *
     * @param count Number of dogs to feature
     * @return std::vector<GenerationResult> Results for each dog
     */
    std::vector<GenerationResult> generateLongestWaitingFeatures(int count = 5);

    /**
     * @brief Generate features for overlooked dogs
     *
     * Creates feature posts for dogs that tend to get overlooked:
     * seniors, large breeds, black dogs, special needs.
     *
     * @param count Number of dogs to feature
     * @return std::vector<GenerationResult> Results
     */
    std::vector<GenerationResult> generateOverlookedDogFeatures(int count = 5);

    // ========================================================================
    // SUCCESS STORIES
    // ========================================================================

    /**
     * @brief Generate a success story from an adoption
     *
     * Creates a heartwarming success story post from adoption data.
     *
     * @param adoption The adoption/placement record
     * @return GenerationResult Result
     */
    GenerationResult generateSuccessStory(
        const wtl::core::models::FosterPlacement& adoption
    );

    /**
     * @brief Generate success story by ID
     * @param adoption_id Adoption/placement UUID
     * @param adopter_story Optional story from adopter
     * @param after_photos Optional photos from new home
     * @return GenerationResult Result
     */
    GenerationResult generateSuccessStoryById(
        const std::string& adoption_id,
        const std::string& adopter_story = "",
        const std::vector<std::string>& after_photos = {}
    );

    /**
     * @brief Generate success stories for recent adoptions
     *
     * Creates success story posts for adoptions that completed
     * recently and don't have stories yet.
     *
     * @param days_back How many days back to look
     * @param max_count Maximum stories to generate
     * @return std::vector<GenerationResult> Results
     */
    std::vector<GenerationResult> generateRecentSuccessStories(
        int days_back = 30,
        int max_count = 10
    );

    // ========================================================================
    // EDUCATIONAL CONTENT
    // ========================================================================

    /**
     * @brief Generate educational post for a topic
     *
     * @param topic Topic identifier (e.g., "puppy-care")
     * @param custom_data Optional custom data to inject
     * @return GenerationResult Result
     */
    GenerationResult generateEducationalPost(
        const std::string& topic,
        const Json::Value& custom_data = Json::Value()
    );

    // ========================================================================
    // SCHEDULING
    // ========================================================================

    /**
     * @brief Schedule posts for publishing
     *
     * Reviews draft posts and schedules them for publication
     * based on priority and timing rules.
     *
     * @return int Number of posts scheduled
     */
    int schedulePosts();

    /**
     * @brief Get scheduled posts
     * @return std::vector<ScheduleInfo> Scheduled post information
     */
    std::vector<ScheduleInfo> getScheduledPosts();

    /**
     * @brief Reschedule a post
     * @param post_id Post UUID
     * @param new_time New scheduled time
     * @return bool Success
     */
    bool reschedulePost(
        const std::string& post_id,
        const std::chrono::system_clock::time_point& new_time
    );

    /**
     * @brief Cancel a scheduled post (return to draft)
     * @param post_id Post UUID
     * @return bool Success
     */
    bool cancelScheduledPost(const std::string& post_id);

    // ========================================================================
    // PUBLISHING
    // ========================================================================

    /**
     * @brief Publish a post immediately
     *
     * @param post_id Post UUID
     * @return bool Success
     */
    bool publishPost(const std::string& post_id);

    /**
     * @brief Unpublish a post (return to draft)
     *
     * @param post_id Post UUID
     * @return bool Success
     */
    bool unpublishPost(const std::string& post_id);

    /**
     * @brief Process scheduled posts that are due
     *
     * Called by scheduler to publish posts whose scheduled
     * time has arrived.
     *
     * @return int Number of posts published
     */
    int processScheduledPosts();

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @struct EngineStats
     * @brief Statistics about blog engine activity
     */
    struct EngineStats {
        int total_posts_generated = 0;
        int posts_published_today = 0;
        int posts_scheduled = 0;
        int urgent_roundups_this_week = 0;
        int dog_features_this_week = 0;
        int success_stories_this_week = 0;
        std::chrono::system_clock::time_point last_urgent_roundup;
        std::chrono::system_clock::time_point last_feature;
        std::chrono::system_clock::time_point last_success_story;
    };

    /**
     * @brief Get engine statistics
     */
    EngineStats getStats() const;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Set schedule configuration
     */
    void setScheduleConfig(const ScheduleConfig& config);

    /**
     * @brief Get current schedule configuration
     */
    ScheduleConfig getScheduleConfig() const;

    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    /**
     * @brief Handle dog urgency change event
     *
     * Called when a dog's urgency level changes, potentially
     * triggering immediate content generation.
     *
     * @param dog_id Dog UUID
     * @param new_urgency New urgency level
     */
    void onDogUrgencyChanged(const std::string& dog_id, const std::string& new_urgency);

    /**
     * @brief Handle adoption completed event
     *
     * Called when an adoption is finalized, potentially
     * queuing a success story.
     *
     * @param adoption_id Adoption UUID
     */
    void onAdoptionCompleted(const std::string& adoption_id);

    /**
     * @brief Handle new dog added event
     *
     * Called when a new dog is added to the system.
     *
     * @param dog_id Dog UUID
     */
    void onNewDogAdded(const std::string& dog_id);

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    BlogEngine();
    ~BlogEngine();

    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Background scheduler thread function
     */
    void schedulerLoop();

    /**
     * @brief Get urgent dogs for roundup
     */
    std::vector<wtl::core::models::Dog> getUrgentDogs();

    /**
     * @brief Get dogs waiting longest without recent features
     */
    std::vector<wtl::core::models::Dog> getLongestWaitingWithoutFeatures(int count);

    /**
     * @brief Get overlooked dogs without recent features
     */
    std::vector<wtl::core::models::Dog> getOverlookedDogsWithoutFeatures(int count);

    /**
     * @brief Get recent adoptions without stories
     */
    std::vector<wtl::core::models::FosterPlacement> getRecentAdoptionsWithoutStories(
        int days_back, int max_count
    );

    /**
     * @brief Check if a dog already has a recent feature
     */
    bool dogHasRecentFeature(const std::string& dog_id, int days = 30);

    /**
     * @brief Check if adoption has a story
     */
    bool adoptionHasStory(const std::string& adoption_id);

    /**
     * @brief Calculate next available publish slot
     */
    std::chrono::system_clock::time_point getNextPublishSlot();

    /**
     * @brief Save generated post
     */
    std::string savePost(const BlogPost& post);

    /**
     * @brief Update engine statistics
     */
    void updateStats(BlogCategory category);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    ScheduleConfig schedule_config_;             ///< Schedule configuration
    EngineStats stats_;                          ///< Engine statistics
    bool running_;                               ///< Whether engine is running
    std::thread scheduler_thread_;               ///< Background scheduler thread
    mutable std::mutex mutex_;                   ///< Thread safety mutex
    std::condition_variable cv_;                 ///< Condition variable for shutdown
};

} // namespace wtl::content::blog
