/**
 * @file BlogPost.h
 * @brief Blog post model for the DOGBLOG Content Engine
 *
 * PURPOSE:
 * Defines the BlogPost struct for storing and managing blog content
 * on the WaitingTheLongest platform. Blog posts include dog features,
 * urgent roundups, success stories, and educational content.
 *
 * USAGE:
 * BlogPost post;
 * post.title = "Meet Max: A Senior Sweetheart";
 * post.content = "# Introduction\n\nMax is...";
 * Json::Value json = post.toJson();
 *
 * DEPENDENCIES:
 * - BlogCategory (for post categorization)
 * - jsoncpp (Json::Value)
 * - pqxx (database row conversion)
 * - chrono (timestamps)
 *
 * MODIFICATION GUIDE:
 * - Add new fields to struct and update all conversion methods
 * - Update database schema if adding persistent fields
 * - Maintain backward compatibility with JSON format
 *
 * @author Agent 12 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

// Project includes
#include "content/blog/BlogCategory.h"

namespace wtl::content::blog {

/**
 * @enum PostStatus
 * @brief Publication status of a blog post
 */
enum class PostStatus {
    DRAFT,      ///< Post is being written, not visible
    SCHEDULED,  ///< Post is scheduled for future publication
    PUBLISHED,  ///< Post is live and visible
    ARCHIVED    ///< Post is hidden but preserved
};

/**
 * @brief Convert PostStatus to string
 */
inline std::string postStatusToString(PostStatus status) {
    switch (status) {
        case PostStatus::DRAFT:
            return "draft";
        case PostStatus::SCHEDULED:
            return "scheduled";
        case PostStatus::PUBLISHED:
            return "published";
        case PostStatus::ARCHIVED:
            return "archived";
        default:
            return "unknown";
    }
}

/**
 * @brief Parse string to PostStatus
 */
inline PostStatus postStatusFromString(const std::string& str) {
    std::string lower = str;
    for (auto& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower == "draft") return PostStatus::DRAFT;
    if (lower == "scheduled") return PostStatus::SCHEDULED;
    if (lower == "published") return PostStatus::PUBLISHED;
    if (lower == "archived") return PostStatus::ARCHIVED;

    return PostStatus::DRAFT;  // Default to draft
}

/**
 * @struct BlogPost
 * @brief Represents a blog post in the content system
 *
 * Contains all information about a blog post including content,
 * metadata, SEO fields, and engagement metrics.
 */
struct BlogPost {
    // ========================================================================
    // IDENTITY
    // ========================================================================

    std::string id;                              ///< UUID primary key
    std::string title;                           ///< Post title
    std::string slug;                            ///< URL-friendly slug (e.g., "meet-max-senior-sweetheart")

    // ========================================================================
    // CONTENT
    // ========================================================================

    std::string content;                         ///< Full post content in Markdown
    std::string excerpt;                         ///< Short excerpt/summary (150-200 chars)

    // ========================================================================
    // CLASSIFICATION
    // ========================================================================

    BlogCategory category;                       ///< Primary category
    std::vector<std::string> tags;               ///< Tags for search and filtering

    // ========================================================================
    // MEDIA
    // ========================================================================

    std::optional<std::string> featured_image;   ///< URL of featured/hero image
    std::vector<std::string> gallery_images;     ///< Additional images for the post

    // ========================================================================
    // RELATIONSHIPS
    // ========================================================================

    std::optional<std::string> dog_id;           ///< FK to dogs table (for dog features)
    std::optional<std::string> adoption_id;      ///< FK to foster_placements (for success stories)
    std::optional<std::string> shelter_id;       ///< FK to shelters table

    // ========================================================================
    // PUBLICATION STATUS
    // ========================================================================

    PostStatus status;                           ///< Current publication status
    std::optional<std::chrono::system_clock::time_point> scheduled_at;  ///< When to publish (if scheduled)
    std::optional<std::chrono::system_clock::time_point> published_at;  ///< When actually published

    // ========================================================================
    // SEO METADATA
    // ========================================================================

    std::string meta_description;                ///< SEO meta description (155 chars max)
    std::optional<std::string> og_title;         ///< Open Graph title (for social sharing)
    std::optional<std::string> og_description;   ///< Open Graph description
    std::optional<std::string> og_image;         ///< Open Graph image URL

    // ========================================================================
    // ENGAGEMENT METRICS
    // ========================================================================

    int view_count = 0;                          ///< Number of views
    int share_count = 0;                         ///< Number of social shares
    int like_count = 0;                          ///< Number of likes/reactions

    // ========================================================================
    // AUTHOR INFO
    // ========================================================================

    std::optional<std::string> author_id;        ///< FK to users table (optional)
    std::string author_name;                     ///< Display name (can be "WTL Team")
    bool is_auto_generated = false;              ///< Whether generated by BlogEngine

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;   ///< Record creation time
    std::chrono::system_clock::time_point updated_at;   ///< Last update time

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create a BlogPost from JSON data
     *
     * @param json The JSON object containing post data
     * @return BlogPost The constructed BlogPost object
     *
     * @example
     * Json::Value json;
     * json["title"] = "Meet Max";
     * json["content"] = "# Max\n\nMax is a great dog...";
     * BlogPost post = BlogPost::fromJson(json);
     */
    static BlogPost fromJson(const Json::Value& json);

    /**
     * @brief Create a BlogPost from a database row
     *
     * @param row The pqxx database row
     * @return BlogPost The constructed BlogPost object
     */
    static BlogPost fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert BlogPost to JSON for API responses
     *
     * @return Json::Value The JSON representation
     */
    Json::Value toJson() const;

    /**
     * @brief Convert BlogPost to JSON for list views (without full content)
     *
     * @return Json::Value Minimal JSON for listing
     */
    Json::Value toListJson() const;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Generate a URL-friendly slug from the title
     *
     * @param title The title to convert
     * @return std::string URL-friendly slug
     */
    static std::string generateSlug(const std::string& title);

    /**
     * @brief Generate an excerpt from the content
     *
     * @param content The markdown content
     * @param max_length Maximum length of excerpt (default 200)
     * @return std::string Truncated excerpt
     */
    static std::string generateExcerpt(const std::string& content, size_t max_length = 200);

    /**
     * @brief Check if the post is ready to be published
     *
     * @return bool True if has required fields
     */
    bool isPublishable() const;

    /**
     * @brief Check if the post is currently visible to public
     *
     * @return bool True if published and not scheduled for future
     */
    bool isVisible() const;

    /**
     * @brief Get the effective title for display
     *
     * @return std::string Title or "Untitled"
     */
    std::string getDisplayTitle() const;

    /**
     * @brief Get reading time estimate in minutes
     *
     * @return int Estimated reading time
     */
    int getReadingTimeMinutes() const;

    /**
     * @brief Get word count of content
     *
     * @return int Number of words
     */
    int getWordCount() const;

    /**
     * @brief Parse a PostgreSQL array string to vector
     * @param array_str The PostgreSQL array string format
     * @return std::vector<std::string> The parsed vector
     */
    static std::vector<std::string> parsePostgresArray(const std::string& array_str);

    /**
     * @brief Format a vector to PostgreSQL array string
     * @param vec The vector to format
     * @return std::string PostgreSQL array format
     */
    static std::string formatPostgresArray(const std::vector<std::string>& vec);

private:
    // ========================================================================
    // PRIVATE HELPERS
    // ========================================================================

    /**
     * @brief Parse a timestamp string to time_point
     * @param timestamp ISO 8601 formatted timestamp string
     * @return time_point The parsed time
     */
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);

    /**
     * @brief Format a time_point to ISO 8601 string
     * @param tp The time point to format
     * @return std::string ISO 8601 formatted string
     */
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
};

} // namespace wtl::content::blog
