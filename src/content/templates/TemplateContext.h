/**
 * @file TemplateContext.h
 * @brief Template context builder for WaitingTheLongest content templates
 *
 * PURPOSE:
 * Provides a fluent API for building render contexts for templates.
 * Supports setting individual values, dogs, shelters, wait times,
 * countdowns, and other domain-specific data.
 *
 * USAGE:
 * TemplateContext ctx;
 * ctx.set("title", "Urgent Dog Alert")
 *    .setDog(dog)
 *    .setShelter(shelter)
 *    .setWaitTime({2, 3, 15, 8, 45, 32})
 *    .setCountdown({2, 14, 30, 0})
 *    .setUrgency("critical");
 *
 * std::string result = TemplateEngine::getInstance().render("tiktok/urgent", ctx);
 *
 * DEPENDENCIES:
 * - jsoncpp (JSON handling)
 * - Dog, Shelter models (for domain-specific setters)
 *
 * MODIFICATION GUIDE:
 * - Add new domain-specific setters as needed
 * - Ensure all setters return *this for fluent chaining
 * - Keep toJson() up to date with all stored data
 *
 * @author Agent 16 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::content::templates {

// ============================================================================
// WAIT TIME COMPONENTS
// ============================================================================

/**
 * @struct WaitTimeComponents
 * @brief Components of a wait time duration
 *
 * Represents the time a dog has been waiting in shelter
 * in years, months, days, hours, minutes, seconds.
 */
struct WaitTimeComponents {
    int years{0};
    int months{0};
    int days{0};
    int hours{0};
    int minutes{0};
    int seconds{0};

    /**
     * @brief Get formatted string "YY:MM:DD:HH:MM:SS"
     * @return Formatted wait time string
     */
    std::string formatted() const;

    /**
     * @brief Get human-readable format "2 years, 3 months, 15 days"
     * @return Human-readable string
     */
    std::string humanReadable() const;

    /**
     * @brief Convert to total seconds
     * @return Total seconds
     */
    int64_t totalSeconds() const;

    /**
     * @brief Convert to JSON
     * @return JSON object with all components
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return WaitTimeComponents instance
     */
    static WaitTimeComponents fromJson(const Json::Value& json);

    /**
     * @brief Create from total seconds
     * @param total_seconds Total seconds
     * @return WaitTimeComponents instance
     */
    static WaitTimeComponents fromSeconds(int64_t total_seconds);

    /**
     * @brief Create from intake date
     * @param intake_date Date dog entered shelter
     * @return WaitTimeComponents instance
     */
    static WaitTimeComponents fromIntakeDate(std::chrono::system_clock::time_point intake_date);
};

// ============================================================================
// COUNTDOWN COMPONENTS
// ============================================================================

/**
 * @struct CountdownComponents
 * @brief Components of a countdown timer
 *
 * Represents remaining time until euthanasia date
 * in days, hours, minutes, seconds.
 */
struct CountdownComponents {
    int days{0};
    int hours{0};
    int minutes{0};
    int seconds{0};
    bool is_expired{false};

    /**
     * @brief Get formatted string "DD:HH:MM:SS"
     * @return Formatted countdown string
     */
    std::string formatted() const;

    /**
     * @brief Get human-readable format "2 days, 14 hours remaining"
     * @return Human-readable string
     */
    std::string humanReadable() const;

    /**
     * @brief Convert to total seconds
     * @return Total seconds
     */
    int64_t totalSeconds() const;

    /**
     * @brief Check if critical (<24 hours)
     * @return true if critical
     */
    bool isCritical() const;

    /**
     * @brief Check if high urgency (<72 hours)
     * @return true if high urgency
     */
    bool isHighUrgency() const;

    /**
     * @brief Get urgency level string
     * @return "critical", "high", "medium", or "normal"
     */
    std::string urgencyLevel() const;

    /**
     * @brief Convert to JSON
     * @return JSON object with all components
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json JSON object
     * @return CountdownComponents instance
     */
    static CountdownComponents fromJson(const Json::Value& json);

    /**
     * @brief Create from total seconds
     * @param total_seconds Total seconds
     * @return CountdownComponents instance
     */
    static CountdownComponents fromSeconds(int64_t total_seconds);

    /**
     * @brief Create from euthanasia date
     * @param euthanasia_date Scheduled euthanasia date
     * @return CountdownComponents instance
     */
    static CountdownComponents fromEuthanasiaDate(std::chrono::system_clock::time_point euthanasia_date);
};

// ============================================================================
// TEMPLATE CONTEXT CLASS
// ============================================================================

/**
 * @class TemplateContext
 * @brief Fluent builder for template render contexts
 *
 * Provides a convenient API for building JSON contexts to pass
 * to the template engine. Supports both generic and domain-specific setters.
 */
class TemplateContext {
public:
    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Default constructor
     */
    TemplateContext() = default;

    /**
     * @brief Construct from JSON object
     * @param json Initial context values
     */
    explicit TemplateContext(const Json::Value& json);

    /**
     * @brief Copy constructor
     */
    TemplateContext(const TemplateContext& other);

    /**
     * @brief Move constructor
     */
    TemplateContext(TemplateContext&& other) noexcept;

    /**
     * @brief Copy assignment
     */
    TemplateContext& operator=(const TemplateContext& other);

    /**
     * @brief Move assignment
     */
    TemplateContext& operator=(TemplateContext&& other) noexcept;

    // =========================================================================
    // GENERIC SETTERS
    // =========================================================================

    /**
     * @brief Set a string value
     * @param key Variable name
     * @param value String value
     * @return Reference to this for chaining
     */
    TemplateContext& set(const std::string& key, const std::string& value);

    /**
     * @brief Set an integer value
     * @param key Variable name
     * @param value Integer value
     * @return Reference to this for chaining
     */
    TemplateContext& set(const std::string& key, int value);

    /**
     * @brief Set an integer value (int64)
     * @param key Variable name
     * @param value Integer value
     * @return Reference to this for chaining
     */
    TemplateContext& set(const std::string& key, int64_t value);

    /**
     * @brief Set a double value
     * @param key Variable name
     * @param value Double value
     * @return Reference to this for chaining
     */
    TemplateContext& set(const std::string& key, double value);

    /**
     * @brief Set a boolean value
     * @param key Variable name
     * @param value Boolean value
     * @return Reference to this for chaining
     */
    TemplateContext& set(const std::string& key, bool value);

    /**
     * @brief Set a JSON value
     * @param key Variable name
     * @param value JSON value
     * @return Reference to this for chaining
     */
    TemplateContext& set(const std::string& key, const Json::Value& value);

    /**
     * @brief Set a string array
     * @param key Variable name
     * @param values Vector of strings
     * @return Reference to this for chaining
     */
    TemplateContext& setArray(const std::string& key, const std::vector<std::string>& values);

    /**
     * @brief Set a JSON array
     * @param key Variable name
     * @param values JSON array value
     * @return Reference to this for chaining
     */
    TemplateContext& setArray(const std::string& key, const Json::Value& values);

    /**
     * @brief Set a nested object
     * @param key Variable name
     * @param values Map of key-value pairs
     * @return Reference to this for chaining
     */
    TemplateContext& setObject(const std::string& key,
                               const std::unordered_map<std::string, std::string>& values);

    /**
     * @brief Remove a value
     * @param key Variable name to remove
     * @return Reference to this for chaining
     */
    TemplateContext& remove(const std::string& key);

    /**
     * @brief Clear all values
     * @return Reference to this for chaining
     */
    TemplateContext& clear();

    // =========================================================================
    // DOMAIN-SPECIFIC SETTERS
    // =========================================================================

    /**
     * @brief Set dog data from a Dog model
     * @param dog_json JSON representation of a dog
     * @return Reference to this for chaining
     *
     * Sets dog_id, dog_name, dog_breed, dog_age, dog_size, dog_gender,
     * dog_photo_url, dog_description, dog_status, and dog_* for all fields.
     */
    TemplateContext& setDog(const Json::Value& dog_json);

    /**
     * @brief Set individual dog fields
     * @param id Dog UUID
     * @param name Dog name
     * @param breed Primary breed
     * @param age_months Age in months
     * @return Reference to this for chaining
     */
    TemplateContext& setDogBasic(const std::string& id, const std::string& name,
                                  const std::string& breed, int age_months);

    /**
     * @brief Set shelter data from a Shelter model
     * @param shelter_json JSON representation of a shelter
     * @return Reference to this for chaining
     *
     * Sets shelter_id, shelter_name, shelter_city, shelter_state,
     * shelter_phone, shelter_email, shelter_is_kill_shelter, etc.
     */
    TemplateContext& setShelter(const Json::Value& shelter_json);

    /**
     * @brief Set individual shelter fields
     * @param id Shelter UUID
     * @param name Shelter name
     * @param city City
     * @param state_code State code
     * @return Reference to this for chaining
     */
    TemplateContext& setShelterBasic(const std::string& id, const std::string& name,
                                      const std::string& city, const std::string& state_code);

    /**
     * @brief Set wait time components
     * @param wait_time Wait time components
     * @return Reference to this for chaining
     *
     * Sets wait_time_years, wait_time_months, wait_time_days, etc.
     * Also sets wait_time_formatted and wait_time_human.
     */
    TemplateContext& setWaitTime(const WaitTimeComponents& wait_time);

    /**
     * @brief Set countdown components
     * @param countdown Countdown components
     * @return Reference to this for chaining
     *
     * Sets countdown_days, countdown_hours, countdown_minutes, etc.
     * Also sets countdown_formatted, countdown_human, countdown_is_critical.
     */
    TemplateContext& setCountdown(const CountdownComponents& countdown);

    /**
     * @brief Set urgency level
     * @param level Urgency level (critical, high, medium, normal)
     * @return Reference to this for chaining
     *
     * Sets urgency_level, urgency_is_critical, urgency_is_high, etc.
     */
    TemplateContext& setUrgency(const std::string& level);

    /**
     * @brief Set user data
     * @param user_json JSON representation of a user
     * @return Reference to this for chaining
     *
     * Sets user_id, user_name, user_email (sanitized), etc.
     */
    TemplateContext& setUser(const Json::Value& user_json);

    /**
     * @brief Set foster home data
     * @param foster_json JSON representation of a foster home
     * @return Reference to this for chaining
     */
    TemplateContext& setFosterHome(const Json::Value& foster_json);

    /**
     * @brief Set hashtags for social media
     * @param hashtags Vector of hashtag strings (without #)
     * @return Reference to this for chaining
     *
     * Sets hashtags array and hashtags_formatted string.
     */
    TemplateContext& setHashtags(const std::vector<std::string>& hashtags);

    /**
     * @brief Set platform-specific data
     * @param platform Platform name (tiktok, instagram, facebook, twitter)
     * @return Reference to this for chaining
     *
     * Sets platform_name and adjusts character limits.
     */
    TemplateContext& setPlatform(const std::string& platform);

    /**
     * @brief Set email-specific data
     * @param subject Email subject
     * @param preheader Email preheader text
     * @return Reference to this for chaining
     */
    TemplateContext& setEmailMeta(const std::string& subject, const std::string& preheader);

    /**
     * @brief Set branding/site data
     * @param site_name Site name
     * @param site_url Site URL
     * @return Reference to this for chaining
     */
    TemplateContext& setSiteBranding(const std::string& site_name, const std::string& site_url);

    /**
     * @brief Set current date/time values
     * @return Reference to this for chaining
     *
     * Sets current_date, current_time, current_year, etc.
     */
    TemplateContext& setCurrentDateTime();

    /**
     * @brief Set a list of dogs (for digest templates)
     * @param dogs JSON array of dog objects
     * @return Reference to this for chaining
     */
    TemplateContext& setDogList(const Json::Value& dogs);

    // =========================================================================
    // ACCESSORS
    // =========================================================================

    /**
     * @brief Get a value by key
     * @param key Variable name
     * @return JSON value or null if not found
     */
    Json::Value get(const std::string& key) const;

    /**
     * @brief Check if key exists
     * @param key Variable name
     * @return true if key exists
     */
    bool has(const std::string& key) const;

    /**
     * @brief Get string value
     * @param key Variable name
     * @param default_value Default if not found
     * @return String value
     */
    std::string getString(const std::string& key, const std::string& default_value = "") const;

    /**
     * @brief Get integer value
     * @param key Variable name
     * @param default_value Default if not found
     * @return Integer value
     */
    int getInt(const std::string& key, int default_value = 0) const;

    /**
     * @brief Get boolean value
     * @param key Variable name
     * @param default_value Default if not found
     * @return Boolean value
     */
    bool getBool(const std::string& key, bool default_value = false) const;

    /**
     * @brief Convert to JSON object
     * @return JSON object with all context values
     */
    Json::Value toJson() const;

    /**
     * @brief Get all keys
     * @return Vector of variable names
     */
    std::vector<std::string> getKeys() const;

    // =========================================================================
    // MERGING
    // =========================================================================

    /**
     * @brief Merge another context into this one
     * @param other Context to merge
     * @param overwrite Whether to overwrite existing keys
     * @return Reference to this for chaining
     */
    TemplateContext& merge(const TemplateContext& other, bool overwrite = true);

    /**
     * @brief Merge JSON object into this context
     * @param json JSON to merge
     * @param overwrite Whether to overwrite existing keys
     * @return Reference to this for chaining
     */
    TemplateContext& merge(const Json::Value& json, bool overwrite = true);

    // =========================================================================
    // STATIC FACTORY METHODS
    // =========================================================================

    /**
     * @brief Create context for urgent dog alert
     * @param dog_json Dog JSON
     * @param shelter_json Shelter JSON
     * @param countdown Countdown to euthanasia
     * @return Configured context
     */
    static TemplateContext forUrgentAlert(const Json::Value& dog_json,
                                          const Json::Value& shelter_json,
                                          const CountdownComponents& countdown);

    /**
     * @brief Create context for longest waiting dog
     * @param dog_json Dog JSON
     * @param shelter_json Shelter JSON
     * @param wait_time Wait time components
     * @return Configured context
     */
    static TemplateContext forLongestWaiting(const Json::Value& dog_json,
                                              const Json::Value& shelter_json,
                                              const WaitTimeComponents& wait_time);

    /**
     * @brief Create context for success story
     * @param dog_json Dog JSON
     * @param adopter_name Adopter's name
     * @param days_in_shelter Days dog spent in shelter
     * @return Configured context
     */
    static TemplateContext forSuccessStory(const Json::Value& dog_json,
                                           const std::string& adopter_name,
                                           int days_in_shelter);

private:
    Json::Value context_;
};

} // namespace wtl::content::templates
