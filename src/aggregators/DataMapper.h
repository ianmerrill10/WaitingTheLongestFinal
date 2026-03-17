/**
 * @file DataMapper.h
 * @brief Maps external API data to internal models
 *
 * PURPOSE:
 * Transforms data from external sources (RescueGroups, direct feeds, etc.)
 * to the internal Dog and Shelter models. Handles data normalization,
 * validation, and missing/null values.
 *
 * USAGE:
 * DataMapper mapper;
 * Dog dog = mapper.mapToDog(external_json, "rescuegroups");
 * Shelter shelter = mapper.mapToShelter(external_json, "rescuegroups");
 *
 * DEPENDENCIES:
 * - Dog model
 * - Shelter model
 * - jsoncpp
 *
 * MODIFICATION GUIDE:
 * - Add mappings for new external sources
 * - Extend normalization tables as needed
 * - Add new field mappings as APIs change
 *
 * @author Agent 19 - WaitingTheLongest Build System
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

// Forward declarations
namespace wtl::core::models {
    struct Dog;
    struct Shelter;
}

namespace wtl::aggregators {

/**
 * @struct MappingResult
 * @brief Result of a mapping operation
 */
template<typename T>
struct MappingResult {
    bool success{false};               ///< Whether mapping succeeded
    T data;                            ///< The mapped data
    std::vector<std::string> warnings; ///< Warnings during mapping
    std::string error;                 ///< Error if mapping failed

    /**
     * @brief Check if mapping was successful
     */
    explicit operator bool() const { return success; }
};

/**
 * @struct BreedMapping
 * @brief Mapping entry for breed normalization
 */
struct BreedMapping {
    std::string source_name;           ///< Name from source API
    std::string normalized_name;       ///< Our standardized name
    std::string category;              ///< Breed category (sporting, herding, etc.)
    bool is_common{false};             ///< Whether commonly seen
};

/**
 * @struct SizeCategory
 * @brief Size category definition
 */
struct SizeCategory {
    std::string name;                  ///< Category name (small, medium, large, xlarge)
    double min_weight_lbs;             ///< Minimum weight
    double max_weight_lbs;             ///< Maximum weight
};

/**
 * @struct AgeCategory
 * @brief Age category definition
 */
struct AgeCategory {
    std::string name;                  ///< Category name (puppy, young, adult, senior)
    int min_months;                    ///< Minimum age in months
    int max_months;                    ///< Maximum age in months
};

/**
 * @class DataMapper
 * @brief Maps external data to internal models
 *
 * Singleton that handles data transformation from various external
 * sources to our internal Dog and Shelter models.
 */
class DataMapper {
public:
    // =========================================================================
    // SINGLETON
    // =========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to DataMapper
     */
    static DataMapper& getInstance();

    // Delete copy/move
    DataMapper(const DataMapper&) = delete;
    DataMapper& operator=(const DataMapper&) = delete;

    // =========================================================================
    // DOG MAPPING
    // =========================================================================

    /**
     * @brief Map external data to Dog model
     *
     * @param data External JSON data
     * @param source Source identifier (rescuegroups, shelter_direct, etc.)
     * @return MappingResult with Dog or error
     */
    MappingResult<wtl::core::models::Dog> mapToDog(
        const Json::Value& data,
        const std::string& source);

    /**
     * @brief Map RescueGroups animal to Dog
     * @param data RescueGroups API response for an animal
     * @return MappingResult with Dog
     */
    MappingResult<wtl::core::models::Dog> mapRescueGroupsDog(const Json::Value& data);

    /**
     * @brief Map generic JSON to Dog (direct feed)
     * @param data JSON data in our expected format
     * @return MappingResult with Dog
     */
    MappingResult<wtl::core::models::Dog> mapDirectFeedDog(const Json::Value& data);

    // =========================================================================
    // SHELTER MAPPING
    // =========================================================================

    /**
     * @brief Map external data to Shelter model
     *
     * @param data External JSON data
     * @param source Source identifier
     * @return MappingResult with Shelter or error
     */
    MappingResult<wtl::core::models::Shelter> mapToShelter(
        const Json::Value& data,
        const std::string& source);

    /**
     * @brief Map RescueGroups organization to Shelter
     * @param data RescueGroups API response for an organization
     * @return MappingResult with Shelter
     */
    MappingResult<wtl::core::models::Shelter> mapRescueGroupsShelter(const Json::Value& data);

    // =========================================================================
    // BREED NORMALIZATION
    // =========================================================================

    /**
     * @brief Normalize a breed name
     *
     * @param breed_name Raw breed name from external source
     * @return Normalized breed name
     *
     * Handles variations like "Lab", "Labrador", "Labrador Retriever"
     * and maps them to a consistent name.
     */
    std::string normalizeBreed(const std::string& breed_name);

    /**
     * @brief Check if breed name is recognized
     * @param breed_name Breed name to check
     * @return true if recognized
     */
    bool isKnownBreed(const std::string& breed_name) const;

    /**
     * @brief Get breed category
     * @param breed_name Normalized breed name
     * @return Category (sporting, herding, toy, etc.) or "unknown"
     */
    std::string getBreedCategory(const std::string& breed_name) const;

    /**
     * @brief Add a breed mapping
     * @param source_name Name as it appears in source
     * @param normalized_name Our standardized name
     * @param category Breed category
     */
    void addBreedMapping(const std::string& source_name,
                         const std::string& normalized_name,
                         const std::string& category = "");

    // =========================================================================
    // SIZE NORMALIZATION
    // =========================================================================

    /**
     * @brief Normalize size category
     *
     * @param size Raw size from external source
     * @return Normalized size (small, medium, large, xlarge)
     */
    std::string normalizeSize(const std::string& size);

    /**
     * @brief Get size category from weight
     * @param weight_lbs Weight in pounds
     * @return Size category
     */
    std::string getSizeFromWeight(double weight_lbs) const;

    // =========================================================================
    // AGE NORMALIZATION
    // =========================================================================

    /**
     * @brief Normalize age category
     *
     * @param age Raw age from external source
     * @return Normalized age (puppy, young, adult, senior)
     */
    std::string normalizeAge(const std::string& age);

    /**
     * @brief Get age category from months
     * @param months Age in months
     * @return Age category
     */
    std::string getAgeFromMonths(int months) const;

    /**
     * @brief Parse age string to months
     * @param age_str Age string (e.g., "2 years", "6 months")
     * @return Age in months, or -1 if cannot parse
     */
    int parseAgeToMonths(const std::string& age_str) const;

    // =========================================================================
    // STATUS NORMALIZATION
    // =========================================================================

    /**
     * @brief Normalize dog status
     *
     * @param status Raw status from external source
     * @return Normalized status (adoptable, pending, adopted, etc.)
     */
    std::string normalizeStatus(const std::string& status);

    /**
     * @brief Check if status indicates available
     * @param status Status to check
     * @return true if dog is available for adoption
     */
    bool isAvailableStatus(const std::string& status) const;

    // =========================================================================
    // GENDER NORMALIZATION
    // =========================================================================

    /**
     * @brief Normalize gender
     *
     * @param gender Raw gender from external source
     * @return Normalized gender (male, female, unknown)
     */
    std::string normalizeGender(const std::string& gender);

    // =========================================================================
    // COLOR NORMALIZATION
    // =========================================================================

    /**
     * @brief Normalize coat color
     *
     * @param color Raw color from external source
     * @return Normalized color
     */
    std::string normalizeColor(const std::string& color);

    // =========================================================================
    // UTILITY METHODS
    // =========================================================================

    /**
     * @brief Get string value with default
     * @param json JSON object
     * @param key Key to retrieve
     * @param default_val Default if missing
     * @return String value
     */
    static std::string getString(const Json::Value& json,
                                 const std::string& key,
                                 const std::string& default_val = "");

    /**
     * @brief Get nested string value
     * @param json JSON object
     * @param path Dot-separated path (e.g., "contact.email")
     * @param default_val Default if missing
     * @return String value
     */
    static std::string getNestedString(const Json::Value& json,
                                       const std::string& path,
                                       const std::string& default_val = "");

    /**
     * @brief Get int value with default
     * @param json JSON object
     * @param key Key to retrieve
     * @param default_val Default if missing
     * @return Int value
     */
    static int getInt(const Json::Value& json,
                      const std::string& key,
                      int default_val = 0);

    /**
     * @brief Get bool value with default
     * @param json JSON object
     * @param key Key to retrieve
     * @param default_val Default if missing
     * @return Bool value
     */
    static bool getBool(const Json::Value& json,
                        const std::string& key,
                        bool default_val = false);

    /**
     * @brief Get array as vector of strings
     * @param json JSON object
     * @param key Key to retrieve
     * @return Vector of strings
     */
    static std::vector<std::string> getStringArray(const Json::Value& json,
                                                    const std::string& key);

    /**
     * @brief Parse ISO 8601 timestamp
     * @param timestamp Timestamp string
     * @return time_point or epoch if parse fails
     */
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);

    /**
     * @brief Load mappings from JSON configuration
     * @param config Configuration JSON
     */
    void loadMappings(const Json::Value& config);

private:
    DataMapper();

    // Breed mappings (source name -> normalized name)
    std::unordered_map<std::string, BreedMapping> breed_mappings_;

    // Size mappings (source name -> normalized name)
    std::unordered_map<std::string, std::string> size_mappings_;

    // Age mappings (source name -> normalized name)
    std::unordered_map<std::string, std::string> age_mappings_;

    // Status mappings (source name -> normalized name)
    std::unordered_map<std::string, std::string> status_mappings_;

    // Gender mappings (source name -> normalized name)
    std::unordered_map<std::string, std::string> gender_mappings_;

    // Color mappings (source name -> normalized name)
    std::unordered_map<std::string, std::string> color_mappings_;

    // Size categories
    std::vector<SizeCategory> size_categories_;

    // Age categories
    std::vector<AgeCategory> age_categories_;

    /**
     * @brief Initialize default mappings
     */
    void initializeDefaultMappings();

    /**
     * @brief Normalize string for comparison
     * @param str String to normalize
     * @return Lowercase, trimmed string
     */
    static std::string normalizeString(const std::string& str);
};

} // namespace wtl::aggregators
