/**
 * @file ContentGenerator.h
 * @brief Markdown content generator for blog posts
 *
 * PURPOSE:
 * Provides the MarkdownGenerator class for generating blog content from
 * templates. Supports dog feature articles, urgent roundups, success stories,
 * and educational content with SEO optimization.
 *
 * USAGE:
 * auto& generator = MarkdownGenerator::getInstance();
 * std::string content = generator.generateDogFeature(dog);
 * std::string roundup = generator.generateUrgentRoundup(dogs);
 *
 * DEPENDENCIES:
 * - Dog model (for dog data)
 * - FosterPlacement model (for adoption stories)
 * - Shelter model (for shelter info)
 * - BlogPost model (for output)
 *
 * MODIFICATION GUIDE:
 * - Add new template types by adding methods and template files
 * - Placeholders use {{PLACEHOLDER_NAME}} format
 * - SEO optimization methods can be extended for new content types
 *
 * @author Agent 12 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/blog/BlogPost.h"
#include "content/blog/BlogCategory.h"
#include "core/models/Dog.h"
#include "core/models/FosterPlacement.h"
#include "core/models/Shelter.h"

namespace wtl::content::blog {

/**
 * @struct GeneratorConfig
 * @brief Configuration for content generation
 */
struct GeneratorConfig {
    std::string template_directory = "content_templates/blog";  ///< Template location
    std::string site_url = "https://waitingthelongest.com";     ///< Base site URL
    std::string site_name = "WaitingTheLongest";                ///< Site name for SEO
    int max_excerpt_length = 200;                               ///< Max excerpt characters
    int max_meta_description_length = 155;                      ///< SEO meta description limit
    bool include_share_buttons = true;                          ///< Include social sharing
    bool include_related_dogs = true;                           ///< Include related dogs section
    std::string default_author = "WTL Team";                    ///< Default author name
};

/**
 * @struct SeoMetadata
 * @brief SEO metadata for generated content
 */
struct SeoMetadata {
    std::string title;           ///< SEO title (max 60 chars)
    std::string description;     ///< Meta description (max 155 chars)
    std::string og_title;        ///< Open Graph title
    std::string og_description;  ///< Open Graph description
    std::string og_image;        ///< Open Graph image URL
    std::string canonical_url;   ///< Canonical URL
    std::vector<std::string> keywords;  ///< SEO keywords
};

/**
 * @class MarkdownGenerator
 * @brief Singleton class for generating blog content from templates
 *
 * Generates markdown content for various blog post types using templates
 * with placeholder substitution. Includes SEO optimization.
 */
class MarkdownGenerator {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the MarkdownGenerator instance
     */
    static MarkdownGenerator& getInstance();

    // Prevent copying
    MarkdownGenerator(const MarkdownGenerator&) = delete;
    MarkdownGenerator& operator=(const MarkdownGenerator&) = delete;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Set generator configuration
     * @param config Configuration settings
     */
    void setConfig(const GeneratorConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    GeneratorConfig getConfig() const;

    // ========================================================================
    // DOG FEATURE GENERATION
    // ========================================================================

    /**
     * @brief Generate a feature article for a single dog
     *
     * Creates a compelling dog feature article with:
     * - Introduction and personality description
     * - Wait time display (how long waiting)
     * - Photo gallery
     * - Adoption call-to-action
     *
     * @param dog The dog to feature
     * @param shelter Optional shelter info for location details
     * @return BlogPost Complete blog post ready for publishing
     *
     * @example
     * auto post = generator.generateDogFeature(max, shelter);
     * blogService.save(post);
     */
    BlogPost generateDogFeature(
        const wtl::core::models::Dog& dog,
        const std::optional<wtl::core::models::Shelter>& shelter = std::nullopt
    );

    /**
     * @brief Generate content markdown for a dog feature
     *
     * @param dog The dog data
     * @param shelter Optional shelter data
     * @return std::string Markdown content
     */
    std::string generateDogFeatureContent(
        const wtl::core::models::Dog& dog,
        const std::optional<wtl::core::models::Shelter>& shelter = std::nullopt
    );

    // ========================================================================
    // URGENT ROUNDUP GENERATION
    // ========================================================================

    /**
     * @brief Generate daily urgent dog roundup
     *
     * Creates a roundup article featuring multiple urgent dogs:
     * - List of dogs with immediate needs
     * - Countdown timers for euthanasia dates
     * - Contact information for each shelter
     * - Urgent call-to-action
     *
     * @param dogs Vector of urgent dogs to feature
     * @param date Date for the roundup (defaults to today)
     * @return BlogPost Complete blog post
     */
    BlogPost generateUrgentRoundup(
        const std::vector<wtl::core::models::Dog>& dogs,
        const std::chrono::system_clock::time_point& date = std::chrono::system_clock::now()
    );

    /**
     * @brief Generate content markdown for urgent roundup
     *
     * @param dogs Vector of urgent dogs
     * @param date Roundup date
     * @return std::string Markdown content
     */
    std::string generateUrgentRoundupContent(
        const std::vector<wtl::core::models::Dog>& dogs,
        const std::chrono::system_clock::time_point& date
    );

    // ========================================================================
    // SUCCESS STORY GENERATION
    // ========================================================================

    /**
     * @brief Generate an adoption success story
     *
     * Creates a heartwarming success story featuring:
     * - Before and after narrative
     * - Foster/adoption journey
     * - Photos from the new family
     * - Inspiring message
     *
     * @param dog The adopted dog
     * @param adoption The adoption/foster placement record
     * @param adopter_story Optional story from the adopter
     * @param after_photos URLs of photos in new home
     * @return BlogPost Complete blog post
     */
    BlogPost generateSuccessStory(
        const wtl::core::models::Dog& dog,
        const wtl::core::models::FosterPlacement& adoption,
        const std::string& adopter_story = "",
        const std::vector<std::string>& after_photos = {}
    );

    /**
     * @brief Generate content markdown for success story
     *
     * @param dog The dog data
     * @param adoption The adoption data
     * @param adopter_story Story from adopter
     * @param after_photos Photo URLs
     * @return std::string Markdown content
     */
    std::string generateSuccessStoryContent(
        const wtl::core::models::Dog& dog,
        const wtl::core::models::FosterPlacement& adoption,
        const std::string& adopter_story,
        const std::vector<std::string>& after_photos
    );

    // ========================================================================
    // EDUCATIONAL CONTENT GENERATION
    // ========================================================================

    /**
     * @brief Generate educational content from topic
     *
     * Creates educational article based on topic templates:
     * - Structured sections with headers
     * - Key points and tips
     * - Related resources
     *
     * @param topic Topic identifier (e.g., "puppy-care", "senior-health")
     * @param custom_data Optional custom data to inject
     * @return BlogPost Complete blog post
     */
    BlogPost generateEducationalPost(
        const std::string& topic,
        const Json::Value& custom_data = Json::Value()
    );

    /**
     * @brief Generate content markdown for educational topic
     *
     * @param topic Topic identifier
     * @param custom_data Custom data
     * @return std::string Markdown content
     */
    std::string generateEducationalContent(
        const std::string& topic,
        const Json::Value& custom_data
    );

    // ========================================================================
    // SEO OPTIMIZATION
    // ========================================================================

    /**
     * @brief Generate SEO metadata for a dog feature
     *
     * @param dog The dog data
     * @param shelter Optional shelter data
     * @return SeoMetadata SEO metadata
     */
    SeoMetadata generateDogFeatureSeo(
        const wtl::core::models::Dog& dog,
        const std::optional<wtl::core::models::Shelter>& shelter = std::nullopt
    );

    /**
     * @brief Generate SEO metadata for urgent roundup
     *
     * @param dog_count Number of dogs in roundup
     * @param date Roundup date
     * @return SeoMetadata SEO metadata
     */
    SeoMetadata generateUrgentRoundupSeo(
        int dog_count,
        const std::chrono::system_clock::time_point& date
    );

    /**
     * @brief Generate SEO metadata for success story
     *
     * @param dog The dog data
     * @return SeoMetadata SEO metadata
     */
    SeoMetadata generateSuccessStorySeo(const wtl::core::models::Dog& dog);

    /**
     * @brief Generate SEO keywords for a dog
     *
     * @param dog The dog data
     * @return std::vector<std::string> Keywords
     */
    std::vector<std::string> generateDogKeywords(const wtl::core::models::Dog& dog);

    // ========================================================================
    // TEMPLATE UTILITIES
    // ========================================================================

    /**
     * @brief Load a template file
     *
     * @param template_name Template filename (without path)
     * @return std::string Template content
     */
    std::string loadTemplate(const std::string& template_name);

    /**
     * @brief Apply placeholders to template
     *
     * @param template_content Template with {{PLACEHOLDERS}}
     * @param placeholders Map of placeholder name to value
     * @return std::string Processed content
     */
    std::string applyPlaceholders(
        const std::string& template_content,
        const std::map<std::string, std::string>& placeholders
    );

    /**
     * @brief Generate excerpt from content
     *
     * @param content Full content
     * @param max_length Maximum length
     * @return std::string Truncated excerpt
     */
    std::string generateExcerpt(const std::string& content, size_t max_length = 200);

    /**
     * @brief Format date for display
     *
     * @param tp Time point
     * @param format strftime format string
     * @return std::string Formatted date
     */
    std::string formatDate(
        const std::chrono::system_clock::time_point& tp,
        const std::string& format = "%B %d, %Y"
    );

    /**
     * @brief Format wait time for display
     *
     * @param intake_date Dog's intake date
     * @return std::string Human-readable wait time
     */
    std::string formatWaitTime(const std::chrono::system_clock::time_point& intake_date);

    /**
     * @brief Format countdown for display
     *
     * @param euthanasia_date Scheduled euthanasia date
     * @return std::string Human-readable countdown
     */
    std::string formatCountdown(const std::chrono::system_clock::time_point& euthanasia_date);

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    MarkdownGenerator();
    ~MarkdownGenerator();

    // ========================================================================
    // PRIVATE HELPERS
    // ========================================================================

    /**
     * @brief Generate title for dog feature
     */
    std::string generateDogFeatureTitle(const wtl::core::models::Dog& dog);

    /**
     * @brief Generate photo gallery markdown
     */
    std::string generatePhotoGallery(const std::vector<std::string>& photos);

    /**
     * @brief Generate dog attributes section
     */
    std::string generateDogAttributes(const wtl::core::models::Dog& dog);

    /**
     * @brief Generate personality traits section
     */
    std::string generatePersonalitySection(const wtl::core::models::Dog& dog);

    /**
     * @brief Generate contact/CTA section
     */
    std::string generateCtaSection(
        const wtl::core::models::Dog& dog,
        const std::optional<wtl::core::models::Shelter>& shelter
    );

    /**
     * @brief Generate single urgent dog entry
     */
    std::string generateUrgentDogEntry(
        const wtl::core::models::Dog& dog,
        int index
    );

    /**
     * @brief Truncate string at word boundary
     */
    std::string truncateAtWord(const std::string& text, size_t max_length);

    /**
     * @brief Strip markdown formatting from text
     */
    std::string stripMarkdown(const std::string& markdown);

    /**
     * @brief Escape special markdown characters
     */
    std::string escapeMarkdown(const std::string& text);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    GeneratorConfig config_;           ///< Current configuration
    std::map<std::string, std::string> template_cache_;  ///< Cached templates
    mutable std::mutex mutex_;         ///< Thread safety mutex
};

} // namespace wtl::content::blog
