/**
 * @file TemplateRepository.h
 * @brief Repository for loading, caching, and managing content templates
 *
 * PURPOSE:
 * Provides a unified interface for accessing content templates from both
 * file system and database storage. Handles caching, validation, and
 * versioning of templates for the WaitingTheLongest platform.
 *
 * USAGE:
 * auto& repo = TemplateRepository::getInstance();
 * repo.initialize();
 *
 * // Load template by category and name
 * auto tpl = repo.getTemplate("tiktok", "urgent_countdown");
 *
 * // Reload all templates
 * repo.reloadAll();
 *
 * // Validate template content
 * auto [valid, error] = repo.validateTemplate(content);
 *
 * DEPENDENCIES:
 * - TemplateEngine (for template validation)
 * - ConnectionPool (for database storage)
 * - ErrorCapture (error logging)
 *
 * MODIFICATION GUIDE:
 * - Add new template categories in TemplateCategory enum
 * - Extend getTemplate to support new storage backends
 * - Update validateTemplate for new validation rules
 *
 * @author Agent 16 - WaitingTheLongest Build System
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
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::content::templates {

// ============================================================================
// TEMPLATE CATEGORY ENUM
// ============================================================================

/**
 * @enum TemplateCategory
 * @brief Categories of content templates
 */
enum class TemplateCategory {
    TIKTOK,         ///< TikTok video templates
    INSTAGRAM,      ///< Instagram post/story templates
    FACEBOOK,       ///< Facebook post templates
    TWITTER,        ///< Twitter/X thread templates
    EMAIL,          ///< Email templates (HTML)
    SMS,            ///< SMS text templates
    PUSH,           ///< Push notification templates
    SOCIAL,         ///< Generic social media templates
    CUSTOM          ///< Custom/user-defined templates
};

// ============================================================================
// TEMPLATE METADATA STRUCT
// ============================================================================

/**
 * @struct TemplateMetadata
 * @brief Metadata about a content template
 */
struct TemplateMetadata {
    std::string id;                          ///< UUID or unique identifier
    std::string name;                        ///< Template name (e.g., "urgent_countdown")
    std::string category;                    ///< Category name (e.g., "tiktok")
    std::string type;                        ///< Content type (e.g., "URGENT_COUNTDOWN")
    std::string description;                 ///< Human-readable description
    int version{1};                          ///< Template version number
    bool is_active{true};                    ///< Whether template is active
    bool is_default{false};                  ///< Whether this is the default for its type

    std::vector<std::string> use_for_urgency;     ///< Urgency levels this applies to
    std::vector<std::string> use_for_platforms;   ///< Platforms this applies to
    std::vector<std::string> hashtags;            ///< Default hashtags

    std::string overlay_style;               ///< Video overlay style (for TikTok)
    int character_limit{0};                  ///< Max characters (0 = no limit)

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::string created_by;                  ///< User who created the template
    std::string updated_by;                  ///< User who last updated

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return TemplateMetadata instance
     */
    static TemplateMetadata fromJson(const Json::Value& json);
};

// ============================================================================
// TEMPLATE STRUCT
// ============================================================================

/**
 * @struct Template
 * @brief Complete template with content and metadata
 */
struct Template {
    TemplateMetadata metadata;
    std::string content;                     ///< The actual template content
    Json::Value variables;                   ///< Required variables with defaults
    Json::Value config;                      ///< Additional configuration

    /**
     * @brief Check if template is valid
     * @return true if valid
     */
    bool isValid() const;

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return Template instance
     */
    static Template fromJson(const Json::Value& json);
};

// ============================================================================
// TEMPLATE VALIDATION RESULT
// ============================================================================

/**
 * @struct TemplateValidationResult
 * @brief Result of template validation
 */
struct TemplateValidationResult {
    bool is_valid{false};
    std::string error_message;
    std::vector<std::string> warnings;
    std::vector<std::string> required_variables;
    int estimated_render_length{0};
};

// ============================================================================
// TEMPLATE REPOSITORY CLASS
// ============================================================================

/**
 * @class TemplateRepository
 * @brief Singleton repository for content templates
 *
 * Manages loading, caching, and retrieval of content templates
 * from both file system and database sources.
 */
class TemplateRepository {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the TemplateRepository singleton
     */
    static TemplateRepository& getInstance();

    // Delete copy/move constructors
    TemplateRepository(const TemplateRepository&) = delete;
    TemplateRepository& operator=(const TemplateRepository&) = delete;
    TemplateRepository(TemplateRepository&&) = delete;
    TemplateRepository& operator=(TemplateRepository&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the repository
     * @param templates_dir Base directory for template files
     * @param use_database Whether to also load from database
     */
    void initialize(const std::string& templates_dir = "content_templates",
                   bool use_database = true);

    /**
     * @brief Initialize from configuration
     */
    void initializeFromConfig();

    // =========================================================================
    // TEMPLATE RETRIEVAL
    // =========================================================================

    /**
     * @brief Get a template by category and name
     *
     * @param category Template category (e.g., "tiktok", "email")
     * @param name Template name (e.g., "urgent_countdown")
     * @return Template if found
     *
     * @example
     * auto tpl = repo.getTemplate("tiktok", "urgent_countdown");
     */
    std::optional<Template> getTemplate(const std::string& category, const std::string& name);

    /**
     * @brief Get template by ID
     *
     * @param id Template UUID
     * @return Template if found
     */
    std::optional<Template> getTemplateById(const std::string& id);

    /**
     * @brief Get all templates in a category
     *
     * @param category Template category
     * @return Vector of templates
     */
    std::vector<Template> getTemplatesByCategory(const std::string& category);

    /**
     * @brief Get all templates for a specific urgency level
     *
     * @param urgency_level Urgency level (critical, high, medium, normal)
     * @return Vector of matching templates
     */
    std::vector<Template> getTemplatesForUrgency(const std::string& urgency_level);

    /**
     * @brief Get default template for a type
     *
     * @param type Template type (e.g., "URGENT_COUNTDOWN")
     * @return Default template for that type
     */
    std::optional<Template> getDefaultTemplate(const std::string& type);

    /**
     * @brief Get all available categories
     *
     * @return Vector of category names
     */
    std::vector<std::string> getCategories();

    /**
     * @brief Get all template names in a category
     *
     * @param category Template category
     * @return Vector of template names
     */
    std::vector<std::string> getTemplateNames(const std::string& category);

    // =========================================================================
    // TEMPLATE MANAGEMENT
    // =========================================================================

    /**
     * @brief Save a template to storage
     *
     * @param tpl Template to save
     * @param to_database Whether to save to database (default: true)
     * @return Saved template with updated ID/timestamps
     */
    Template saveTemplate(const Template& tpl, bool to_database = true);

    /**
     * @brief Update an existing template
     *
     * @param id Template ID
     * @param tpl Updated template data
     * @return Updated template
     */
    std::optional<Template> updateTemplate(const std::string& id, const Template& tpl);

    /**
     * @brief Delete a template
     *
     * @param id Template ID
     * @return true if deleted
     */
    bool deleteTemplate(const std::string& id);

    /**
     * @brief Create a new version of a template
     *
     * @param id Original template ID
     * @param new_content New content
     * @return New version of the template
     */
    std::optional<Template> createVersion(const std::string& id, const std::string& new_content);

    /**
     * @brief Get version history for a template
     *
     * @param name Template name
     * @param category Template category
     * @return Vector of template versions
     */
    std::vector<Template> getVersionHistory(const std::string& category, const std::string& name);

    // =========================================================================
    // CACHE MANAGEMENT
    // =========================================================================

    /**
     * @brief Reload all templates from storage
     *
     * @return Number of templates loaded
     */
    int reloadAll();

    /**
     * @brief Reload templates from a specific category
     *
     * @param category Template category
     * @return Number of templates loaded
     */
    int reloadCategory(const std::string& category);

    /**
     * @brief Clear the template cache
     */
    void clearCache();

    /**
     * @brief Preload templates into cache
     *
     * @return Number of templates preloaded
     */
    int preloadAll();

    // =========================================================================
    // VALIDATION
    // =========================================================================

    /**
     * @brief Validate template content
     *
     * @param content Template content to validate
     * @return Validation result
     */
    TemplateValidationResult validateTemplate(const std::string& content);

    /**
     * @brief Validate a complete template
     *
     * @param tpl Template to validate
     * @return Validation result
     */
    TemplateValidationResult validateTemplate(const Template& tpl);

    /**
     * @brief Check if a template name is available
     *
     * @param category Category name
     * @param name Template name
     * @return true if name is available
     */
    bool isNameAvailable(const std::string& category, const std::string& name);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get repository statistics
     *
     * @return JSON with stats
     */
    Json::Value getStats() const;

    /**
     * @brief Get count of templates
     *
     * @return Total template count
     */
    size_t getCount() const;

    /**
     * @brief Get count by category
     *
     * @param category Category name
     * @return Template count for category
     */
    size_t getCountByCategory(const std::string& category) const;

    // =========================================================================
    // UTILITY
    // =========================================================================

    /**
     * @brief Convert category enum to string
     * @param category TemplateCategory value
     * @return Category name
     */
    static std::string categoryToString(TemplateCategory category);

    /**
     * @brief Convert string to category enum
     * @param str Category name
     * @return TemplateCategory value
     */
    static TemplateCategory stringToCategory(const std::string& str);

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    TemplateRepository() = default;
    ~TemplateRepository() = default;

    /**
     * @brief Load templates from file system
     * @return Number loaded
     */
    int loadFromFiles();

    /**
     * @brief Load templates from database
     * @return Number loaded
     */
    int loadFromDatabase();

    /**
     * @brief Load a single template file
     * @param file_path Path to template file
     * @return Template if loaded successfully
     */
    std::optional<Template> loadTemplateFile(const std::string& file_path);

    /**
     * @brief Save template to file
     * @param tpl Template to save
     * @return true if saved
     */
    bool saveTemplateToFile(const Template& tpl);

    /**
     * @brief Save template to database
     * @param tpl Template to save
     * @return Saved template with ID
     */
    Template saveTemplateToDatabase(const Template& tpl);

    /**
     * @brief Generate cache key
     * @param category Category name
     * @param name Template name
     * @return Cache key
     */
    std::string getCacheKey(const std::string& category, const std::string& name) const;

    // Configuration
    std::string templates_dir_{"content_templates"};
    bool use_database_{true};

    // Template cache: key = "category/name", value = Template
    std::unordered_map<std::string, Template> cache_;

    // Index by ID
    std::unordered_map<std::string, std::string> id_to_key_;

    // Category index: category -> list of template keys
    std::unordered_map<std::string, std::vector<std::string>> category_index_;

    // Statistics
    mutable std::atomic<uint64_t> load_count_{0};
    mutable std::atomic<uint64_t> cache_hits_{0};
    mutable std::atomic<uint64_t> cache_misses_{0};

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Initialized flag
    bool initialized_{false};
};

} // namespace wtl::content::templates
