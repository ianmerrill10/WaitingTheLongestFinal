/**
 * @file TemplateHelpers.h
 * @brief Built-in template helpers for WaitingTheLongest content templates
 *
 * PURPOSE:
 * Provides a collection of helper functions for formatting data in templates.
 * These helpers handle wait times, countdowns, dates, text formatting, and
 * domain-specific transformations used across social media, email, and SMS content.
 *
 * USAGE:
 * // Register all helpers with the template engine
 * TemplateHelpers::registerAll(TemplateEngine::getInstance());
 *
 * // Use helpers in templates:
 * {{formatWaitTime wait_time}}      -> "2 years, 3 months"
 * {{formatCountdown countdown}}     -> "2 days remaining"
 * {{formatUrgency urgency_level}}   -> "CRITICAL"
 * {{pluralize count "dog" "dogs"}}  -> "5 dogs"
 * {{truncate description 100}}      -> "This is a long..."
 * {{uppercase name}}                -> "MAX"
 *
 * DEPENDENCIES:
 * - TemplateEngine (for registration)
 * - jsoncpp (JSON handling)
 *
 * MODIFICATION GUIDE:
 * - Add new helpers by creating a static function and registering it
 * - All helpers take (Json::Value& args, const Json::Value& context)
 * - Return std::string from helper functions
 *
 * @author Agent 16 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Forward declarations
namespace wtl::content::templates {
class TemplateEngine;
struct WaitTimeComponents;
struct CountdownComponents;
}

namespace wtl::content::templates {

/**
 * @class TemplateHelpers
 * @brief Collection of built-in template helper functions
 *
 * Static class providing helper functions for template rendering.
 * All helpers are designed to be used with the TemplateEngine.
 */
class TemplateHelpers {
public:
    // =========================================================================
    // REGISTRATION
    // =========================================================================

    /**
     * @brief Register all built-in helpers with a template engine
     * @param engine The template engine to register helpers with
     */
    static void registerAll(TemplateEngine& engine);

    // =========================================================================
    // WAIT TIME HELPERS
    // =========================================================================

    /**
     * @brief Format wait time components into human-readable string
     *
     * @param components WaitTimeComponents or JSON with years, months, days, etc.
     * @return Formatted string like "2 years, 3 months" or "45 days"
     *
     * @example In template: {{formatWaitTime wait_time}}
     */
    static std::string formatWaitTime(const Json::Value& components);

    /**
     * @brief Format wait time as LED-style counter "YY:MM:DD:HH:MM:SS"
     *
     * @param components Wait time components
     * @return Formatted LED string
     *
     * @example In template: {{formatWaitTimeLED wait_time}}
     */
    static std::string formatWaitTimeLED(const Json::Value& components);

    /**
     * @brief Get the most significant unit of wait time
     *
     * @param components Wait time components
     * @return String like "2 years" or "45 days" - just the largest unit
     *
     * @example In template: {{waitTimePrimary wait_time}}
     */
    static std::string waitTimePrimary(const Json::Value& components);

    // =========================================================================
    // COUNTDOWN HELPERS
    // =========================================================================

    /**
     * @brief Format countdown into human-readable remaining time
     *
     * @param components CountdownComponents or JSON with days, hours, minutes, seconds
     * @return Formatted string like "2 days remaining" or "4 hours left"
     *
     * @example In template: {{formatCountdown countdown}}
     */
    static std::string formatCountdown(const Json::Value& components);

    /**
     * @brief Format countdown as LED-style counter "DD:HH:MM:SS"
     *
     * @param components Countdown components
     * @return Formatted LED string
     *
     * @example In template: {{formatCountdownLED countdown}}
     */
    static std::string formatCountdownLED(const Json::Value& components);

    /**
     * @brief Get urgent countdown text for social media
     *
     * @param components Countdown components
     * @return Urgent text like "ONLY 2 DAYS LEFT!" or "LESS THAN 24 HOURS!"
     *
     * @example In template: {{countdownUrgent countdown}}
     */
    static std::string countdownUrgent(const Json::Value& components);

    // =========================================================================
    // URGENCY HELPERS
    // =========================================================================

    /**
     * @brief Format urgency level for display
     *
     * @param level Urgency level (critical, high, medium, normal)
     * @return Formatted label like "CRITICAL" or "HIGH URGENCY"
     *
     * @example In template: {{formatUrgency urgency_level}}
     */
    static std::string formatUrgency(const std::string& level);

    /**
     * @brief Get urgency emoji prefix
     *
     * @param level Urgency level
     * @return Emoji string like "!!!!!!" for critical
     *
     * @example In template: {{urgencyEmoji urgency_level}}
     */
    static std::string urgencyEmoji(const std::string& level);

    /**
     * @brief Get urgency color code
     *
     * @param level Urgency level
     * @return Hex color code like "#FF0000" for critical
     *
     * @example In template: {{urgencyColor urgency_level}}
     */
    static std::string urgencyColor(const std::string& level);

    /**
     * @brief Check if urgency is critical or high
     *
     * @param level Urgency level
     * @return "true" or "false" string for template conditionals
     */
    static std::string isUrgent(const std::string& level);

    // =========================================================================
    // TEXT FORMATTING HELPERS
    // =========================================================================

    /**
     * @brief Pluralize a word based on count
     *
     * @param count The count
     * @param singular Singular form
     * @param plural Plural form
     * @return Formatted string like "1 dog" or "5 dogs"
     *
     * @example In template: {{pluralize dog_count "dog" "dogs"}}
     */
    static std::string pluralize(int count, const std::string& singular,
                                 const std::string& plural);

    /**
     * @brief Truncate text to maximum length with ellipsis
     *
     * @param text Text to truncate
     * @param max_length Maximum length
     * @param suffix Suffix to add (default "...")
     * @return Truncated text
     *
     * @example In template: {{truncate description 100}}
     */
    static std::string truncate(const std::string& text, int max_length,
                               const std::string& suffix = "...");

    /**
     * @brief Convert text to uppercase
     *
     * @param text Input text
     * @return Uppercase text
     *
     * @example In template: {{uppercase name}}
     */
    static std::string uppercase(const std::string& text);

    /**
     * @brief Convert text to lowercase
     *
     * @param text Input text
     * @return Lowercase text
     *
     * @example In template: {{lowercase name}}
     */
    static std::string lowercase(const std::string& text);

    /**
     * @brief Capitalize first letter of text
     *
     * @param text Input text
     * @return Text with first letter capitalized
     *
     * @example In template: {{capitalize name}}
     */
    static std::string capitalize(const std::string& text);

    /**
     * @brief Capitalize first letter of each word (title case)
     *
     * @param text Input text
     * @return Title case text
     *
     * @example In template: {{titleCase name}}
     */
    static std::string titleCase(const std::string& text);

    /**
     * @brief Strip HTML tags from text
     *
     * @param text Input text with potential HTML
     * @return Plain text without tags
     *
     * @example In template: {{stripHtml description}}
     */
    static std::string stripHtml(const std::string& text);

    /**
     * @brief Escape special characters for HTML
     *
     * @param text Input text
     * @return HTML-escaped text
     */
    static std::string escapeHtml(const std::string& text);

    /**
     * @brief Convert newlines to <br> tags
     *
     * @param text Input text
     * @return Text with newlines converted
     */
    static std::string nl2br(const std::string& text);

    // =========================================================================
    // DATE/TIME HELPERS
    // =========================================================================

    /**
     * @brief Format timestamp to readable date
     *
     * @param timestamp Unix timestamp or ISO date string
     * @param format Format string (strftime format, default "%B %d, %Y")
     * @return Formatted date like "January 28, 2024"
     *
     * @example In template: {{date intake_date "%B %d, %Y"}}
     */
    static std::string formatDate(const std::string& timestamp,
                                  const std::string& format = "%B %d, %Y");

    /**
     * @brief Format timestamp as relative time
     *
     * @param timestamp Unix timestamp or ISO date string
     * @return Relative time like "2 hours ago" or "3 days ago"
     *
     * @example In template: {{timeAgo created_at}}
     */
    static std::string timeAgo(const std::string& timestamp);

    /**
     * @brief Get current date in specified format
     *
     * @param format Format string
     * @return Current date formatted
     *
     * @example In template: {{now "%Y-%m-%d"}}
     */
    static std::string now(const std::string& format = "%Y-%m-%d %H:%M:%S");

    // =========================================================================
    // NUMBER FORMATTING HELPERS
    // =========================================================================

    /**
     * @brief Format number with comma separators
     *
     * @param number The number
     * @return Formatted string like "1,234,567"
     *
     * @example In template: {{formatNumber total_count}}
     */
    static std::string formatNumber(int64_t number);

    /**
     * @brief Format number as ordinal
     *
     * @param number The number
     * @return Ordinal string like "1st", "2nd", "3rd", "4th"
     *
     * @example In template: {{ordinal rank}}
     */
    static std::string ordinal(int number);

    // =========================================================================
    // DOG-SPECIFIC HELPERS
    // =========================================================================

    /**
     * @brief Get age description for a dog
     *
     * @param age_months Age in months
     * @return Friendly description like "puppy", "young adult", "senior"
     *
     * @example In template: {{ageCategory dog_age_months}}
     */
    static std::string ageCategory(int age_months);

    /**
     * @brief Format dog age in human-readable form
     *
     * @param age_months Age in months
     * @return String like "2 years, 3 months" or "8 months"
     *
     * @example In template: {{formatAge dog_age_months}}
     */
    static std::string formatAge(int age_months);

    /**
     * @brief Get pronoun for dog based on gender
     *
     * @param gender Gender (male, female, unknown)
     * @param type Pronoun type (subject, object, possessive)
     * @return Appropriate pronoun
     *
     * @example In template: {{pronoun dog_gender "subject"}} -> "he" or "she"
     */
    static std::string pronoun(const std::string& gender, const std::string& type = "subject");

    /**
     * @brief Format size for display
     *
     * @param size Size (small, medium, large, xlarge)
     * @return Formatted size like "Small", "Extra Large"
     *
     * @example In template: {{formatSize dog_size}}
     */
    static std::string formatSize(const std::string& size);

    // =========================================================================
    // HASHTAG HELPERS
    // =========================================================================

    /**
     * @brief Format array of hashtags with # prefix
     *
     * @param tags Vector of tag strings
     * @return Formatted hashtag string like "#dog #adopt #rescue"
     *
     * @example In template: {{formatHashtags hashtags}}
     */
    static std::string formatHashtags(const std::vector<std::string>& tags);

    /**
     * @brief Generate hashtags from dog data
     *
     * @param breed Primary breed
     * @param state State code
     * @param urgency Urgency level
     * @return Recommended hashtags for this dog
     */
    static std::vector<std::string> generateHashtags(const std::string& breed,
                                                      const std::string& state,
                                                      const std::string& urgency);

    // =========================================================================
    // URL HELPERS
    // =========================================================================

    /**
     * @brief Encode string for URL
     *
     * @param text Text to encode
     * @return URL-encoded string
     */
    static std::string urlEncode(const std::string& text);

    /**
     * @brief Generate dog profile URL
     *
     * @param dog_id Dog UUID
     * @return Full URL to dog profile
     */
    static std::string dogUrl(const std::string& dog_id);

    /**
     * @brief Generate shelter profile URL
     *
     * @param shelter_id Shelter UUID
     * @return Full URL to shelter profile
     */
    static std::string shelterUrl(const std::string& shelter_id);

private:
    // Helper to parse timestamp
    static int64_t parseTimestamp(const std::string& timestamp);
};

} // namespace wtl::content::templates
