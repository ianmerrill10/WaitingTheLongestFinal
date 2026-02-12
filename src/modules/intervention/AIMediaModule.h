/**
 * @file AIMediaModule.h
 * @brief Module for AI-powered media generation to increase dog visibility
 *
 * PURPOSE:
 * Uses AI to generate compelling content for shelter dogs, including
 * personality descriptions, adoption appeal text, and social media
 * captions. Helps dogs that might be overlooked get more attention.
 *
 * USAGE:
 * Enable in config.json:
 * {
 *     "modules": {
 *         "ai_media": {
 *             "enabled": true,
 *             "auto_generate_descriptions": true,
 *             "social_caption_styles": ["emotional", "funny", "informative"]
 *         }
 *     }
 * }
 *
 * DEPENDENCIES:
 * - IModule.h (module interface)
 * - EventBus.h (event subscription)
 *
 * @author WaitingTheLongest Integration Agent
 * @date 2024-01-28
 */

#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <functional>

#include <json/json.h>

#include "modules/IModule.h"
#include "core/EventBus.h"

namespace wtl::modules::intervention {

/**
 * @enum ContentType
 * @brief Types of AI-generated content
 */
enum class ContentType {
    DESCRIPTION,        // Full personality description
    SHORT_BIO,          // Brief bio for listings
    SOCIAL_CAPTION,     // Social media caption
    ADOPTION_APPEAL,    // Adoption plea text
    VIDEO_SCRIPT,       // Script for video content
    HASHTAGS,           // Relevant hashtags
    ALT_TEXT            // Image accessibility text
};

/**
 * @enum CaptionStyle
 * @brief Styles for social media captions
 */
enum class CaptionStyle {
    EMOTIONAL,      // Heartfelt, touching
    FUNNY,          // Humorous, lighthearted
    INFORMATIVE,    // Facts about the dog
    URGENT,         // Time-sensitive appeals
    STORYTELLING    // Narrative style
};

/**
 * @struct DogProfile
 * @brief Information about a dog for content generation
 */
struct DogProfile {
    std::string id;
    std::string name;
    std::string breed;
    std::string size;
    int age_years;
    int age_months;
    std::string gender;
    std::string color;
    int wait_time_days;
    bool is_urgent;
    int days_until_euthanasia;
    std::string shelter_name;
    std::string shelter_city;
    std::string shelter_state;
    std::vector<std::string> traits; // friendly, playful, calm, etc.
    std::vector<std::string> special_needs;
    bool good_with_dogs;
    bool good_with_cats;
    bool good_with_kids;
    std::string existing_description;
    std::vector<std::string> photo_urls;
};

/**
 * @struct GeneratedContent
 * @brief AI-generated content for a dog
 */
struct GeneratedContent {
    std::string id;
    std::string dog_id;
    ContentType type;
    CaptionStyle style; // Only for SOCIAL_CAPTION
    std::string content;
    std::vector<std::string> hashtags;
    std::string generated_at;
    bool approved;
    int engagement_score;
    Json::Value metadata;
};

/**
 * @struct ContentRequest
 * @brief Request for AI content generation
 */
struct ContentRequest {
    std::string id;
    std::string dog_id;
    ContentType type;
    CaptionStyle style;
    std::string platform; // tiktok, instagram, facebook, twitter
    int max_length;
    std::string tone;
    std::string status; // pending, processing, completed, failed
    std::string created_at;
};

/**
 * @class AIMediaModule
 * @brief Module for AI-powered content generation
 *
 * Features:
 * - Automatic description generation for new dogs
 * - Social media caption generation in multiple styles
 * - Adoption appeal text for urgent dogs
 * - Video script generation
 * - Hashtag suggestions
 * - Content A/B testing
 * - Engagement tracking
 */
class AIMediaModule : public IModule {
public:
    AIMediaModule() = default;
    ~AIMediaModule() override = default;

    // ========================================================================
    // IModule INTERFACE - LIFECYCLE
    // ========================================================================

    bool initialize() override;
    void shutdown() override;
    void onConfigUpdate(const Json::Value& config) override;

    // ========================================================================
    // IModule INTERFACE - IDENTITY
    // ========================================================================

    std::string getName() const override { return "ai_media"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override {
        return "AI-powered content generation for dog profiles and social media";
    }
    std::vector<std::string> getDependencies() const override {
        return {}; // No dependencies
    }

    // ========================================================================
    // IModule INTERFACE - STATUS
    // ========================================================================

    bool isEnabled() const override { return enabled_; }
    bool isHealthy() const override;
    Json::Value getStatus() const override;
    Json::Value getMetrics() const override;

    // ========================================================================
    // IModule INTERFACE - CONFIGURATION
    // ========================================================================

    void configure(const Json::Value& config) override;
    Json::Value getDefaultConfig() const override;

    // ========================================================================
    // CONTENT GENERATION API
    // ========================================================================

    /**
     * @brief Generate a description for a dog
     * @param profile The dog profile
     * @return Generated description content
     */
    GeneratedContent generateDescription(const DogProfile& profile);

    /**
     * @brief Generate a short bio for a dog
     * @param profile The dog profile
     * @param max_length Maximum character length
     * @return Generated short bio
     */
    GeneratedContent generateShortBio(const DogProfile& profile, int max_length = 150);

    /**
     * @brief Generate a social media caption
     * @param profile The dog profile
     * @param style Caption style
     * @param platform Target platform
     * @return Generated caption
     */
    GeneratedContent generateSocialCaption(const DogProfile& profile,
                                            CaptionStyle style,
                                            const std::string& platform = "instagram");

    /**
     * @brief Generate an adoption appeal for urgent dogs
     * @param profile The dog profile
     * @return Generated appeal text
     */
    GeneratedContent generateAdoptionAppeal(const DogProfile& profile);

    /**
     * @brief Generate a video script
     * @param profile The dog profile
     * @param duration_seconds Target video duration
     * @return Generated video script
     */
    GeneratedContent generateVideoScript(const DogProfile& profile, int duration_seconds = 30);

    /**
     * @brief Generate hashtags for a dog
     * @param profile The dog profile
     * @param count Number of hashtags to generate
     * @return Generated hashtags
     */
    std::vector<std::string> generateHashtags(const DogProfile& profile, int count = 10);

    /**
     * @brief Generate alt text for a dog photo
     * @param profile The dog profile
     * @param photo_context Additional context about the photo
     * @return Generated alt text
     */
    std::string generateAltText(const DogProfile& profile, const std::string& photo_context = "");

    /**
     * @brief Queue a content generation request
     * @param request The content request
     * @return The request ID
     */
    std::string queueContentRequest(const ContentRequest& request);

    /**
     * @brief Get generated content by ID
     * @param content_id The content ID
     * @return The generated content if found
     */
    std::optional<GeneratedContent> getGeneratedContent(const std::string& content_id);

    /**
     * @brief Get all generated content for a dog
     * @param dog_id The dog ID
     * @return List of generated content
     */
    std::vector<GeneratedContent> getContentForDog(const std::string& dog_id);

    /**
     * @brief Approve generated content
     * @param content_id The content ID
     * @return true if approved
     */
    bool approveContent(const std::string& content_id);

    /**
     * @brief Record content engagement
     * @param content_id The content ID
     * @param engagement_type Type of engagement (view, like, share, click)
     */
    void recordEngagement(const std::string& content_id, const std::string& engagement_type);

    /**
     * @brief Get content generation statistics
     * @return Statistics JSON
     */
    Json::Value getContentStats();

private:
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    void handleDogCreated(const core::Event& event);
    void handleDogBecameUrgent(const core::Event& event);
    void handleContentRequest(const core::Event& event);

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    void processContentQueue();
    std::string generateContentId();
    std::string buildPrompt(const DogProfile& profile, ContentType type, CaptionStyle style);
    std::string callAIService(const std::string& prompt);
    std::string formatForPlatform(const std::string& content, const std::string& platform);
    void loadTemplates();

    // Template-based generation (fallback when AI unavailable)
    std::string generateFromTemplate(const DogProfile& profile, ContentType type);
    std::string fillTemplate(const std::string& template_str, const DogProfile& profile);

    // ========================================================================
    // STATE
    // ========================================================================

    bool enabled_ = false;
    bool auto_generate_descriptions_ = true;
    bool auto_generate_urgent_appeals_ = true;
    std::vector<CaptionStyle> enabled_caption_styles_;
    std::string ai_service_url_;
    std::string ai_api_key_;

    // Subscription IDs for cleanup
    std::vector<std::string> subscription_ids_;

    // Content queue and storage
    std::queue<ContentRequest> content_queue_;
    std::map<std::string, GeneratedContent> generated_content_;
    std::map<std::string, std::vector<std::string>> dog_content_map_; // dog_id -> content_ids
    mutable std::mutex data_mutex_;

    // Templates (fallback)
    std::map<ContentType, std::vector<std::string>> content_templates_;

    // Metrics
    std::atomic<int64_t> descriptions_generated_{0};
    std::atomic<int64_t> captions_generated_{0};
    std::atomic<int64_t> appeals_generated_{0};
    std::atomic<int64_t> scripts_generated_{0};
    std::atomic<int64_t> content_approved_{0};
    std::atomic<int64_t> total_views_{0};
    std::atomic<int64_t> total_engagements_{0};
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string contentTypeToString(ContentType type);
ContentType stringToContentType(const std::string& str);
std::string captionStyleToString(CaptionStyle style);
CaptionStyle stringToCaptionStyle(const std::string& str);

} // namespace wtl::modules::intervention
