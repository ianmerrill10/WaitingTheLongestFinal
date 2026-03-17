/**
 * @file TikTokTemplate.h
 * @brief TikTok post templates for dog awareness campaigns
 *
 * PURPOSE:
 * Defines templates for generating TikTok posts with consistent styling
 * and messaging. Templates include caption templates with placeholders,
 * hashtags, overlay styles, and music suggestions for different urgency
 * levels and campaign types.
 *
 * TEMPLATE TYPES:
 * - URGENT_COUNTDOWN: Dogs with imminent euthanasia dates
 * - LONGEST_WAITING: Dogs waiting longest in shelter
 * - OVERLOOKED_ANGEL: Dogs often overlooked (seniors, black dogs, pit bulls)
 * - FOSTER_PLEA: Foster requests for specific dogs
 * - HAPPY_UPDATE: Success stories and adoption updates
 * - EDUCATIONAL: Pet adoption tips and shelter info
 *
 * USAGE:
 * auto& manager = TemplateManager::getInstance();
 * auto template = manager.getTemplate(TikTokTemplateType::URGENT_COUNTDOWN);
 * std::string caption = template->formatCaption(placeholders);
 *
 * DEPENDENCIES:
 * - jsoncpp (JSON handling)
 * - Config (configuration loading)
 *
 * MODIFICATION GUIDE:
 * - Add new template types to TikTokTemplateType enum
 * - Register new templates in TemplateManager::loadDefaults()
 * - Update tiktok_templates.json for customization
 *
 * @author Agent 11 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <shared_mutex>

// Third-party includes
#include <json/json.h>

namespace wtl::content::tiktok {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum TikTokTemplateType
 * @brief Types of TikTok post templates
 */
enum class TikTokTemplateType {
    URGENT_COUNTDOWN = 0,   ///< Dogs with imminent euthanasia dates
    LONGEST_WAITING,        ///< Dogs waiting longest in shelter
    OVERLOOKED_ANGEL,       ///< Dogs often overlooked (seniors, black dogs, etc.)
    FOSTER_PLEA,            ///< Foster requests
    HAPPY_UPDATE,           ///< Success stories and updates
    EDUCATIONAL             ///< Educational content about adoption
};

/**
 * @enum OverlayStyle
 * @brief Predefined overlay styles for videos
 */
enum class OverlayStyle {
    NONE = 0,               ///< No overlay
    URGENT_RED,             ///< Red urgent styling with countdown
    WAIT_TIME_GREEN,        ///< Green wait time display
    HEARTWARMING,           ///< Soft warm colors for success stories
    INFORMATIVE,            ///< Clean educational style
    FOSTER_APPEAL           ///< Foster-specific styling
};

// ============================================================================
// TEMPLATE STRUCTURE
// ============================================================================

/**
 * @struct TikTokTemplate
 * @brief Represents a template for generating TikTok posts
 */
struct TikTokTemplate {
    // ========================================================================
    // IDENTITY
    // ========================================================================

    std::string id;                             ///< Unique template identifier
    std::string name;                           ///< Display name
    TikTokTemplateType type;                    ///< Template type category
    std::string description;                    ///< Template description

    // ========================================================================
    // CAPTION TEMPLATE
    // ========================================================================

    std::string caption_template;               ///< Caption with placeholders
    std::vector<std::string> required_placeholders;  ///< Placeholders that must be filled
    std::vector<std::string> optional_placeholders;  ///< Placeholders that are optional

    // ========================================================================
    // HASHTAGS
    // ========================================================================

    std::vector<std::string> default_hashtags;  ///< Hashtags always included
    std::vector<std::string> optional_hashtags; ///< Hashtags that may be added
    int max_hashtags = 10;                      ///< Maximum hashtags to use

    // ========================================================================
    // STYLING
    // ========================================================================

    OverlayStyle overlay_style = OverlayStyle::NONE;  ///< Video overlay style
    std::optional<std::string> suggested_music_id;    ///< Suggested TikTok sound
    std::optional<std::string> suggested_music_name;  ///< Sound display name
    bool include_countdown = false;             ///< Show urgency countdown
    bool include_wait_time = false;             ///< Show wait time
    std::optional<std::string> text_overlay_template; ///< Text overlay template

    // ========================================================================
    // TIMING
    // ========================================================================

    std::vector<int> optimal_post_hours;        ///< Best hours to post (0-23)
    bool auto_generate_enabled = false;         ///< Enable auto-generation
    int priority = 0;                           ///< Priority for auto-generation

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Convert template to JSON
     * @return Json::Value The JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create template from JSON
     * @param json The JSON data
     * @return TikTokTemplate The parsed template
     */
    static TikTokTemplate fromJson(const Json::Value& json);

    // ========================================================================
    // FORMATTING METHODS
    // ========================================================================

    /**
     * @brief Format the caption template with placeholders
     *
     * @param placeholders Map of placeholder names to values
     * @return std::string Formatted caption
     *
     * Placeholders in template are formatted as {placeholder_name}.
     * Example: "Meet {dog_name}! Waiting {wait_time}"
     *
     * @example
     * std::unordered_map<std::string, std::string> values = {
     *     {"dog_name", "Buddy"},
     *     {"wait_time", "6 months"}
     * };
     * std::string caption = template.formatCaption(values);
     */
    std::string formatCaption(const std::unordered_map<std::string, std::string>& placeholders) const;

    /**
     * @brief Format the text overlay template
     *
     * @param placeholders Map of placeholder names to values
     * @return std::optional<std::string> Formatted overlay text, nullopt if no overlay
     */
    std::optional<std::string> formatOverlayText(
        const std::unordered_map<std::string, std::string>& placeholders) const;

    /**
     * @brief Get hashtags for this template
     *
     * @param additional Additional hashtags to include
     * @return std::vector<std::string> Combined hashtag list (respects max)
     */
    std::vector<std::string> getHashtags(
        const std::vector<std::string>& additional = {}) const;

    /**
     * @brief Validate that all required placeholders are provided
     *
     * @param placeholders The provided placeholder values
     * @return std::vector<std::string> List of missing required placeholders
     */
    std::vector<std::string> validatePlaceholders(
        const std::unordered_map<std::string, std::string>& placeholders) const;

    // ========================================================================
    // STATIC UTILITY METHODS
    // ========================================================================

    /**
     * @brief Convert TikTokTemplateType to string
     * @param type The template type
     * @return std::string String representation
     */
    static std::string typeToString(TikTokTemplateType type);

    /**
     * @brief Convert string to TikTokTemplateType
     * @param str The string to convert
     * @return TikTokTemplateType The enum value
     */
    static TikTokTemplateType stringToType(const std::string& str);

    /**
     * @brief Convert OverlayStyle to string
     * @param style The overlay style
     * @return std::string String representation
     */
    static std::string overlayStyleToString(OverlayStyle style);

    /**
     * @brief Convert string to OverlayStyle
     * @param str The string to convert
     * @return OverlayStyle The enum value
     */
    static OverlayStyle stringToOverlayStyle(const std::string& str);
};

// ============================================================================
// TEMPLATE MANAGER CLASS
// ============================================================================

/**
 * @class TemplateManager
 * @brief Singleton class for managing TikTok post templates
 *
 * Loads templates from configuration and provides access by type.
 * Supports custom templates and template overrides.
 */
class TemplateManager {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the TemplateManager singleton
     */
    static TemplateManager& getInstance();

    // Delete copy/move constructors
    TemplateManager(const TemplateManager&) = delete;
    TemplateManager& operator=(const TemplateManager&) = delete;
    TemplateManager(TemplateManager&&) = delete;
    TemplateManager& operator=(TemplateManager&&) = delete;

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize from configuration file
     *
     * @param config_path Path to tiktok_templates.json
     * @return true if loaded successfully
     */
    bool initialize(const std::string& config_path);

    /**
     * @brief Initialize from Config singleton
     * @return true if loaded successfully
     */
    bool initializeFromConfig();

    /**
     * @brief Load default templates
     *
     * Creates built-in default templates that can be overridden
     * by configuration file.
     */
    void loadDefaults();

    /**
     * @brief Check if manager is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    // ========================================================================
    // TEMPLATE ACCESS
    // ========================================================================

    /**
     * @brief Get template by type
     *
     * @param type The template type
     * @return std::optional<TikTokTemplate> The template if found
     */
    std::optional<TikTokTemplate> getTemplate(TikTokTemplateType type) const;

    /**
     * @brief Get template by ID
     *
     * @param id The template ID
     * @return std::optional<TikTokTemplate> The template if found
     */
    std::optional<TikTokTemplate> getTemplateById(const std::string& id) const;

    /**
     * @brief Get all templates of a type
     *
     * @param type The template type
     * @return std::vector<TikTokTemplate> All templates of that type
     */
    std::vector<TikTokTemplate> getTemplatesByType(TikTokTemplateType type) const;

    /**
     * @brief Get all templates
     *
     * @return std::vector<TikTokTemplate> All registered templates
     */
    std::vector<TikTokTemplate> getAllTemplates() const;

    /**
     * @brief Get templates enabled for auto-generation
     *
     * @return std::vector<TikTokTemplate> Templates with auto_generate_enabled
     */
    std::vector<TikTokTemplate> getAutoGenerateTemplates() const;

    // ========================================================================
    // TEMPLATE MANAGEMENT
    // ========================================================================

    /**
     * @brief Register a new template
     *
     * @param tmpl The template to register
     * @return true if registered (false if ID exists)
     */
    bool registerTemplate(const TikTokTemplate& tmpl);

    /**
     * @brief Update an existing template
     *
     * @param tmpl The updated template (matched by ID)
     * @return true if updated (false if not found)
     */
    bool updateTemplate(const TikTokTemplate& tmpl);

    /**
     * @brief Remove a template
     *
     * @param id The template ID to remove
     * @return true if removed
     */
    bool removeTemplate(const std::string& id);

    /**
     * @brief Reload templates from configuration
     * @return true if reloaded successfully
     */
    bool reload();

    // ========================================================================
    // UTILITY METHODS
    // ========================================================================

    /**
     * @brief Get optimal post times for a template type
     *
     * @param type The template type
     * @return std::vector<int> Hours of day (0-23) optimal for posting
     */
    std::vector<int> getOptimalPostHours(TikTokTemplateType type) const;

    /**
     * @brief Get the best template for a dog based on urgency
     *
     * @param days_until_euthanasia Days until euthanasia (-1 if no date)
     * @param wait_days Days dog has been waiting
     * @param is_overlooked Whether dog is often overlooked type
     * @return TikTokTemplateType Recommended template type
     */
    TikTokTemplateType recommendTemplateType(
        int days_until_euthanasia,
        int wait_days,
        bool is_overlooked) const;

    /**
     * @brief Export all templates to JSON
     * @return Json::Value All templates as JSON array
     */
    Json::Value exportToJson() const;

private:
    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================

    TemplateManager() = default;
    ~TemplateManager() = default;

    /**
     * @brief Load templates from JSON configuration
     * @param json The JSON configuration
     * @return Number of templates loaded
     */
    int loadFromJson(const Json::Value& json);

    // Template storage by ID
    std::unordered_map<std::string, TikTokTemplate> templates_;

    // Index by type for fast lookup
    std::unordered_map<TikTokTemplateType, std::vector<std::string>> type_index_;

    // Configuration path for reloading
    std::string config_path_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Initialization flag
    bool initialized_ = false;
};

} // namespace wtl::content::tiktok
