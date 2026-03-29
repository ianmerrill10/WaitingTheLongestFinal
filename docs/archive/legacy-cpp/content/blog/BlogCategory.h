/**
 * @file BlogCategory.h
 * @brief Blog category enumeration for content classification
 *
 * PURPOSE:
 * Defines the BlogCategory enum used to classify blog posts on the
 * WaitingTheLongest platform. Categories help organize content by type
 * and purpose, enabling filtered views and targeted content delivery.
 *
 * USAGE:
 * BlogCategory category = BlogCategory::URGENT;
 * std::string name = blogCategoryToString(category);
 * BlogCategory parsed = blogCategoryFromString("urgent");
 *
 * DEPENDENCIES:
 * - None (standalone enum header)
 *
 * MODIFICATION GUIDE:
 * - Add new categories to the enum
 * - Update toString/fromString functions
 * - Update getDescription and getSlug functions
 * - Update database enum if applicable
 *
 * @author Agent 12 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>
#include <vector>
#include <stdexcept>

namespace wtl::content::blog {

/**
 * @enum BlogCategory
 * @brief Categories for blog posts
 *
 * Classifies blog content to enable filtering, organization, and
 * targeted content delivery to users.
 */
enum class BlogCategory {
    URGENT,           ///< Urgent dogs needing immediate help (euthanasia lists, critical cases)
    LONGEST_WAITING,  ///< Dogs waiting the longest for adoption
    OVERLOOKED,       ///< Dogs that get passed over (seniors, black dogs, etc.)
    SUCCESS_STORY,    ///< Adoption success stories with happy endings
    EDUCATIONAL,      ///< Educational content about dog care, training, health
    NEW_ARRIVALS      ///< Newly arrived dogs at shelters
};

// ============================================================================
// STRING CONVERSION FUNCTIONS
// ============================================================================

/**
 * @brief Convert BlogCategory enum to string representation
 *
 * @param category The category to convert
 * @return std::string Lowercase string representation
 *
 * @example
 * std::string name = blogCategoryToString(BlogCategory::URGENT);
 * // Returns "urgent"
 */
inline std::string blogCategoryToString(BlogCategory category) {
    switch (category) {
        case BlogCategory::URGENT:
            return "urgent";
        case BlogCategory::LONGEST_WAITING:
            return "longest_waiting";
        case BlogCategory::OVERLOOKED:
            return "overlooked";
        case BlogCategory::SUCCESS_STORY:
            return "success_story";
        case BlogCategory::EDUCATIONAL:
            return "educational";
        case BlogCategory::NEW_ARRIVALS:
            return "new_arrivals";
        default:
            return "unknown";
    }
}

/**
 * @brief Parse string to BlogCategory enum
 *
 * @param str String to parse (case-insensitive)
 * @return BlogCategory The parsed category
 * @throws std::invalid_argument if string is not a valid category
 *
 * @example
 * BlogCategory category = blogCategoryFromString("urgent");
 * // Returns BlogCategory::URGENT
 */
inline BlogCategory blogCategoryFromString(const std::string& str) {
    // Convert to lowercase for comparison
    std::string lower = str;
    for (auto& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower == "urgent") return BlogCategory::URGENT;
    if (lower == "longest_waiting") return BlogCategory::LONGEST_WAITING;
    if (lower == "overlooked") return BlogCategory::OVERLOOKED;
    if (lower == "success_story") return BlogCategory::SUCCESS_STORY;
    if (lower == "educational") return BlogCategory::EDUCATIONAL;
    if (lower == "new_arrivals") return BlogCategory::NEW_ARRIVALS;

    throw std::invalid_argument("Invalid blog category: " + str);
}

/**
 * @brief Check if string is a valid BlogCategory
 *
 * @param str String to check
 * @return bool True if valid category
 */
inline bool isValidBlogCategory(const std::string& str) {
    try {
        blogCategoryFromString(str);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

/**
 * @brief Get human-readable display name for category
 *
 * @param category The category
 * @return std::string Display name suitable for UI
 */
inline std::string blogCategoryDisplayName(BlogCategory category) {
    switch (category) {
        case BlogCategory::URGENT:
            return "Urgent Dogs";
        case BlogCategory::LONGEST_WAITING:
            return "Longest Waiting";
        case BlogCategory::OVERLOOKED:
            return "Overlooked Dogs";
        case BlogCategory::SUCCESS_STORY:
            return "Success Stories";
        case BlogCategory::EDUCATIONAL:
            return "Educational";
        case BlogCategory::NEW_ARRIVALS:
            return "New Arrivals";
        default:
            return "Unknown";
    }
}

/**
 * @brief Get URL-friendly slug for category
 *
 * @param category The category
 * @return std::string URL slug
 */
inline std::string blogCategorySlug(BlogCategory category) {
    switch (category) {
        case BlogCategory::URGENT:
            return "urgent";
        case BlogCategory::LONGEST_WAITING:
            return "longest-waiting";
        case BlogCategory::OVERLOOKED:
            return "overlooked";
        case BlogCategory::SUCCESS_STORY:
            return "success-stories";
        case BlogCategory::EDUCATIONAL:
            return "educational";
        case BlogCategory::NEW_ARRIVALS:
            return "new-arrivals";
        default:
            return "unknown";
    }
}

/**
 * @brief Get category from URL slug
 *
 * @param slug URL slug to parse
 * @return BlogCategory The parsed category
 * @throws std::invalid_argument if slug is not valid
 */
inline BlogCategory blogCategoryFromSlug(const std::string& slug) {
    if (slug == "urgent") return BlogCategory::URGENT;
    if (slug == "longest-waiting") return BlogCategory::LONGEST_WAITING;
    if (slug == "overlooked") return BlogCategory::OVERLOOKED;
    if (slug == "success-stories") return BlogCategory::SUCCESS_STORY;
    if (slug == "educational") return BlogCategory::EDUCATIONAL;
    if (slug == "new-arrivals") return BlogCategory::NEW_ARRIVALS;

    throw std::invalid_argument("Invalid blog category slug: " + slug);
}

/**
 * @brief Get description for category
 *
 * @param category The category
 * @return std::string Description text
 */
inline std::string blogCategoryDescription(BlogCategory category) {
    switch (category) {
        case BlogCategory::URGENT:
            return "Dogs in critical situations who need immediate help. "
                   "These dogs may be on euthanasia lists or in overcrowded shelters.";
        case BlogCategory::LONGEST_WAITING:
            return "Dogs who have been waiting the longest for their forever homes. "
                   "These patient pups deserve extra attention and love.";
        case BlogCategory::OVERLOOKED:
            return "Dogs that often get passed over - seniors, black dogs, large breeds, "
                   "and those with special needs. Every dog deserves a chance.";
        case BlogCategory::SUCCESS_STORY:
            return "Heartwarming stories of dogs who found their forever homes. "
                   "These happy endings inspire hope and show the power of adoption.";
        case BlogCategory::EDUCATIONAL:
            return "Helpful articles about dog care, training, health, nutrition, "
                   "and preparing for pet parenthood.";
        case BlogCategory::NEW_ARRIVALS:
            return "Meet the newest dogs available for adoption. "
                   "Fresh faces looking for loving families.";
        default:
            return "";
    }
}

/**
 * @brief Get icon class for category (for UI rendering)
 *
 * @param category The category
 * @return std::string CSS icon class name
 */
inline std::string blogCategoryIcon(BlogCategory category) {
    switch (category) {
        case BlogCategory::URGENT:
            return "icon-alert-triangle";
        case BlogCategory::LONGEST_WAITING:
            return "icon-clock";
        case BlogCategory::OVERLOOKED:
            return "icon-eye-off";
        case BlogCategory::SUCCESS_STORY:
            return "icon-heart";
        case BlogCategory::EDUCATIONAL:
            return "icon-book-open";
        case BlogCategory::NEW_ARRIVALS:
            return "icon-star";
        default:
            return "icon-file";
    }
}

/**
 * @brief Get color for category (for UI rendering)
 *
 * @param category The category
 * @return std::string Hex color code
 */
inline std::string blogCategoryColor(BlogCategory category) {
    switch (category) {
        case BlogCategory::URGENT:
            return "#dc2626";  // Red - urgency
        case BlogCategory::LONGEST_WAITING:
            return "#f59e0b";  // Amber - attention needed
        case BlogCategory::OVERLOOKED:
            return "#8b5cf6";  // Purple - special attention
        case BlogCategory::SUCCESS_STORY:
            return "#10b981";  // Green - positive
        case BlogCategory::EDUCATIONAL:
            return "#3b82f6";  // Blue - informational
        case BlogCategory::NEW_ARRIVALS:
            return "#06b6d4";  // Cyan - fresh/new
        default:
            return "#6b7280";  // Gray
    }
}

/**
 * @brief Get all blog categories as a vector
 *
 * @return std::vector<BlogCategory> All categories
 */
inline std::vector<BlogCategory> getAllBlogCategories() {
    return {
        BlogCategory::URGENT,
        BlogCategory::LONGEST_WAITING,
        BlogCategory::OVERLOOKED,
        BlogCategory::SUCCESS_STORY,
        BlogCategory::EDUCATIONAL,
        BlogCategory::NEW_ARRIVALS
    };
}

/**
 * @brief Get priority order for category (lower = higher priority)
 *
 * @param category The category
 * @return int Priority (1 = highest)
 */
inline int blogCategoryPriority(BlogCategory category) {
    switch (category) {
        case BlogCategory::URGENT:
            return 1;  // Highest priority - life-saving content
        case BlogCategory::LONGEST_WAITING:
            return 2;
        case BlogCategory::OVERLOOKED:
            return 3;
        case BlogCategory::NEW_ARRIVALS:
            return 4;
        case BlogCategory::SUCCESS_STORY:
            return 5;
        case BlogCategory::EDUCATIONAL:
            return 6;
        default:
            return 99;
    }
}

} // namespace wtl::content::blog
