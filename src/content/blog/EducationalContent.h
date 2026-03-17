/**
 * @file EducationalContent.h
 * @brief Educational content model for the dog care library
 *
 * PURPOSE:
 * Defines the EducationalContent struct for storing and managing
 * educational articles about dog care, training, health, nutrition,
 * and preparing for pet parenthood on the WaitingTheLongest platform.
 *
 * USAGE:
 * EducationalContent article;
 * article.title = "How to Prepare Your Home for a New Dog";
 * article.life_stage = LifeStage::ALL;
 * auto articles = EducationalContentRepository::getByLifeStage(LifeStage::PUPPY);
 *
 * DEPENDENCIES:
 * - jsoncpp (Json::Value)
 * - pqxx (database row conversion)
 * - chrono (timestamps)
 *
 * MODIFICATION GUIDE:
 * - Add new topics or life stages as needed
 * - Update database schema for new fields
 * - Ensure ContentGenerator handles new topics
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

namespace wtl::content::blog {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum LifeStage
 * @brief Dog life stages for educational content targeting
 */
enum class LifeStage {
    PUPPY,    ///< 0-12 months
    YOUNG,    ///< 1-3 years
    ADULT,    ///< 3-7 years
    SENIOR,   ///< 7+ years
    ALL       ///< Applicable to all life stages
};

/**
 * @brief Convert LifeStage to string
 */
inline std::string lifeStageToString(LifeStage stage) {
    switch (stage) {
        case LifeStage::PUPPY:
            return "puppy";
        case LifeStage::YOUNG:
            return "young";
        case LifeStage::ADULT:
            return "adult";
        case LifeStage::SENIOR:
            return "senior";
        case LifeStage::ALL:
            return "all";
        default:
            return "unknown";
    }
}

/**
 * @brief Parse string to LifeStage
 */
inline LifeStage lifeStageFromString(const std::string& str) {
    std::string lower = str;
    for (auto& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower == "puppy") return LifeStage::PUPPY;
    if (lower == "young") return LifeStage::YOUNG;
    if (lower == "adult") return LifeStage::ADULT;
    if (lower == "senior") return LifeStage::SENIOR;
    if (lower == "all") return LifeStage::ALL;

    return LifeStage::ALL;
}

/**
 * @brief Get display name for LifeStage
 */
inline std::string lifeStageDisplayName(LifeStage stage) {
    switch (stage) {
        case LifeStage::PUPPY:
            return "Puppy (0-12 months)";
        case LifeStage::YOUNG:
            return "Young (1-3 years)";
        case LifeStage::ADULT:
            return "Adult (3-7 years)";
        case LifeStage::SENIOR:
            return "Senior (7+ years)";
        case LifeStage::ALL:
            return "All Ages";
        default:
            return "Unknown";
    }
}

/**
 * @enum EducationalTopic
 * @brief Topics for educational content classification
 */
enum class EducationalTopic {
    HEALTH,           ///< Health conditions, veterinary care
    TRAINING,         ///< Training methods, behavior modification
    NUTRITION,        ///< Feeding, diet, treats
    GROOMING,         ///< Grooming, hygiene, coat care
    BEHAVIOR,         ///< Understanding dog behavior
    ADOPTION_PREP,    ///< Preparing for adoption
    HOME_SAFETY,      ///< Dog-proofing, safety tips
    EXERCISE,         ///< Exercise needs, activity ideas
    SOCIALIZATION,    ///< Socializing with people and pets
    TRAVEL,           ///< Traveling with dogs
    SEASONAL_CARE,    ///< Season-specific care tips
    FIRST_TIME_OWNER, ///< Guides for first-time dog owners
    SPECIAL_NEEDS,    ///< Caring for special needs dogs
    END_OF_LIFE       ///< Senior care, end-of-life decisions
};

/**
 * @brief Convert EducationalTopic to string
 */
inline std::string educationalTopicToString(EducationalTopic topic) {
    switch (topic) {
        case EducationalTopic::HEALTH:
            return "health";
        case EducationalTopic::TRAINING:
            return "training";
        case EducationalTopic::NUTRITION:
            return "nutrition";
        case EducationalTopic::GROOMING:
            return "grooming";
        case EducationalTopic::BEHAVIOR:
            return "behavior";
        case EducationalTopic::ADOPTION_PREP:
            return "adoption_prep";
        case EducationalTopic::HOME_SAFETY:
            return "home_safety";
        case EducationalTopic::EXERCISE:
            return "exercise";
        case EducationalTopic::SOCIALIZATION:
            return "socialization";
        case EducationalTopic::TRAVEL:
            return "travel";
        case EducationalTopic::SEASONAL_CARE:
            return "seasonal_care";
        case EducationalTopic::FIRST_TIME_OWNER:
            return "first_time_owner";
        case EducationalTopic::SPECIAL_NEEDS:
            return "special_needs";
        case EducationalTopic::END_OF_LIFE:
            return "end_of_life";
        default:
            return "unknown";
    }
}

/**
 * @brief Parse string to EducationalTopic
 */
inline EducationalTopic educationalTopicFromString(const std::string& str) {
    std::string lower = str;
    for (auto& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower == "health") return EducationalTopic::HEALTH;
    if (lower == "training") return EducationalTopic::TRAINING;
    if (lower == "nutrition") return EducationalTopic::NUTRITION;
    if (lower == "grooming") return EducationalTopic::GROOMING;
    if (lower == "behavior") return EducationalTopic::BEHAVIOR;
    if (lower == "adoption_prep") return EducationalTopic::ADOPTION_PREP;
    if (lower == "home_safety") return EducationalTopic::HOME_SAFETY;
    if (lower == "exercise") return EducationalTopic::EXERCISE;
    if (lower == "socialization") return EducationalTopic::SOCIALIZATION;
    if (lower == "travel") return EducationalTopic::TRAVEL;
    if (lower == "seasonal_care") return EducationalTopic::SEASONAL_CARE;
    if (lower == "first_time_owner") return EducationalTopic::FIRST_TIME_OWNER;
    if (lower == "special_needs") return EducationalTopic::SPECIAL_NEEDS;
    if (lower == "end_of_life") return EducationalTopic::END_OF_LIFE;

    return EducationalTopic::HEALTH;
}

/**
 * @brief Get display name for EducationalTopic
 */
inline std::string educationalTopicDisplayName(EducationalTopic topic) {
    switch (topic) {
        case EducationalTopic::HEALTH:
            return "Health & Veterinary Care";
        case EducationalTopic::TRAINING:
            return "Training & Commands";
        case EducationalTopic::NUTRITION:
            return "Nutrition & Diet";
        case EducationalTopic::GROOMING:
            return "Grooming & Hygiene";
        case EducationalTopic::BEHAVIOR:
            return "Understanding Behavior";
        case EducationalTopic::ADOPTION_PREP:
            return "Adoption Preparation";
        case EducationalTopic::HOME_SAFETY:
            return "Home Safety";
        case EducationalTopic::EXERCISE:
            return "Exercise & Activity";
        case EducationalTopic::SOCIALIZATION:
            return "Socialization";
        case EducationalTopic::TRAVEL:
            return "Traveling with Dogs";
        case EducationalTopic::SEASONAL_CARE:
            return "Seasonal Care";
        case EducationalTopic::FIRST_TIME_OWNER:
            return "First-Time Owner Guide";
        case EducationalTopic::SPECIAL_NEEDS:
            return "Special Needs Care";
        case EducationalTopic::END_OF_LIFE:
            return "Senior & End-of-Life Care";
        default:
            return "Unknown";
    }
}

/**
 * @brief Get icon class for topic
 */
inline std::string educationalTopicIcon(EducationalTopic topic) {
    switch (topic) {
        case EducationalTopic::HEALTH:
            return "icon-heart-pulse";
        case EducationalTopic::TRAINING:
            return "icon-award";
        case EducationalTopic::NUTRITION:
            return "icon-utensils";
        case EducationalTopic::GROOMING:
            return "icon-scissors";
        case EducationalTopic::BEHAVIOR:
            return "icon-brain";
        case EducationalTopic::ADOPTION_PREP:
            return "icon-home";
        case EducationalTopic::HOME_SAFETY:
            return "icon-shield";
        case EducationalTopic::EXERCISE:
            return "icon-activity";
        case EducationalTopic::SOCIALIZATION:
            return "icon-users";
        case EducationalTopic::TRAVEL:
            return "icon-car";
        case EducationalTopic::SEASONAL_CARE:
            return "icon-calendar";
        case EducationalTopic::FIRST_TIME_OWNER:
            return "icon-star";
        case EducationalTopic::SPECIAL_NEEDS:
            return "icon-accessibility";
        case EducationalTopic::END_OF_LIFE:
            return "icon-sunset";
        default:
            return "icon-file";
    }
}

/**
 * @brief Get all educational topics
 */
inline std::vector<EducationalTopic> getAllEducationalTopics() {
    return {
        EducationalTopic::HEALTH,
        EducationalTopic::TRAINING,
        EducationalTopic::NUTRITION,
        EducationalTopic::GROOMING,
        EducationalTopic::BEHAVIOR,
        EducationalTopic::ADOPTION_PREP,
        EducationalTopic::HOME_SAFETY,
        EducationalTopic::EXERCISE,
        EducationalTopic::SOCIALIZATION,
        EducationalTopic::TRAVEL,
        EducationalTopic::SEASONAL_CARE,
        EducationalTopic::FIRST_TIME_OWNER,
        EducationalTopic::SPECIAL_NEEDS,
        EducationalTopic::END_OF_LIFE
    };
}

// ============================================================================
// EDUCATIONAL CONTENT STRUCT
// ============================================================================

/**
 * @struct EducationalContent
 * @brief Represents an educational article in the content library
 */
struct EducationalContent {
    // ========================================================================
    // IDENTITY
    // ========================================================================

    std::string id;                              ///< UUID primary key
    std::string title;                           ///< Article title
    std::string slug;                            ///< URL-friendly slug

    // ========================================================================
    // CONTENT
    // ========================================================================

    std::string content;                         ///< Full article content in Markdown
    std::string summary;                         ///< Short summary (200-300 chars)
    std::vector<std::string> key_points;         ///< Bulleted key takeaways

    // ========================================================================
    // CLASSIFICATION
    // ========================================================================

    EducationalTopic topic;                      ///< Primary topic category
    std::vector<EducationalTopic> secondary_topics; ///< Additional related topics
    LifeStage life_stage;                        ///< Target life stage
    std::vector<std::string> tags;               ///< Search tags/keywords

    // ========================================================================
    // MEDIA
    // ========================================================================

    std::optional<std::string> featured_image;   ///< Hero image URL
    std::vector<std::string> images;             ///< Additional images
    std::optional<std::string> video_url;        ///< Optional video link

    // ========================================================================
    // SEO
    // ========================================================================

    std::string meta_description;                ///< SEO meta description
    std::optional<std::string> meta_keywords;    ///< SEO keywords

    // ========================================================================
    // ENGAGEMENT
    // ========================================================================

    int view_count = 0;                          ///< Number of views
    int helpful_count = 0;                       ///< "Was this helpful?" yes count
    int not_helpful_count = 0;                   ///< "Was this helpful?" no count
    double helpfulness_score = 0.0;              ///< Computed helpfulness (0-1)

    // ========================================================================
    // STATUS
    // ========================================================================

    bool is_published = false;                   ///< Whether article is live
    bool is_featured = false;                    ///< Whether to feature prominently
    int display_order = 0;                       ///< Order within topic

    // ========================================================================
    // AUTHOR
    // ========================================================================

    std::optional<std::string> author_id;        ///< FK to users table
    std::string author_name;                     ///< Display name
    std::optional<std::string> reviewer_id;      ///< Veterinary/expert reviewer
    std::optional<std::string> reviewer_name;    ///< Reviewer display name

    // ========================================================================
    // TIMESTAMPS
    // ========================================================================

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::optional<std::chrono::system_clock::time_point> published_at;
    std::optional<std::chrono::system_clock::time_point> last_reviewed_at;

    // ========================================================================
    // CONVERSION METHODS
    // ========================================================================

    /**
     * @brief Create from JSON data
     */
    static EducationalContent fromJson(const Json::Value& json);

    /**
     * @brief Create from database row
     */
    static EducationalContent fromDbRow(const pqxx::row& row);

    /**
     * @brief Convert to JSON for API responses
     */
    Json::Value toJson() const;

    /**
     * @brief Convert to minimal JSON for listings
     */
    Json::Value toListJson() const;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    /**
     * @brief Generate slug from title
     */
    static std::string generateSlug(const std::string& title);

    /**
     * @brief Get reading time in minutes
     */
    int getReadingTimeMinutes() const;

    /**
     * @brief Get word count
     */
    int getWordCount() const;

    /**
     * @brief Calculate helpfulness score from counts
     */
    void updateHelpfulnessScore();

    /**
     * @brief Check if content is ready for publication
     */
    bool isPublishable() const;

private:
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
    static std::vector<std::string> parsePostgresArray(const std::string& array_str);
};

// ============================================================================
// REPOSITORY CLASS
// ============================================================================

/**
 * @class EducationalContentRepository
 * @brief Static methods for querying educational content
 *
 * Provides convenient static methods for common query patterns.
 * Actual implementation uses database via services.
 */
class EducationalContentRepository {
public:
    /**
     * @brief Get content by life stage
     * @param stage Target life stage
     * @param limit Maximum results
     * @return Vector of matching content
     */
    static std::vector<EducationalContent> getByLifeStage(LifeStage stage, int limit = 50);

    /**
     * @brief Get content by topic category
     * @param topic Topic category
     * @param limit Maximum results
     * @return Vector of matching content
     */
    static std::vector<EducationalContent> getByTopic(EducationalTopic topic, int limit = 50);

    /**
     * @brief Search content by keyword
     * @param keyword Search term
     * @param limit Maximum results
     * @return Vector of matching content
     */
    static std::vector<EducationalContent> searchByKeyword(const std::string& keyword, int limit = 50);

    /**
     * @brief Get featured content
     * @param limit Maximum results
     * @return Vector of featured content
     */
    static std::vector<EducationalContent> getFeatured(int limit = 10);

    /**
     * @brief Get most helpful content
     * @param limit Maximum results
     * @return Vector sorted by helpfulness
     */
    static std::vector<EducationalContent> getMostHelpful(int limit = 10);

    /**
     * @brief Get content for first-time dog owners
     * @return Vector of essential content
     */
    static std::vector<EducationalContent> getFirstTimeOwnerGuides();

    /**
     * @brief Get topic summary with article counts
     * @return Map of topic to article count
     */
    static std::vector<std::pair<EducationalTopic, int>> getTopicSummary();
};

} // namespace wtl::content::blog
