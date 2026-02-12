/**
 * @file DuplicateDetector.h
 * @brief Detects and manages duplicate dogs across data sources
 *
 * PURPOSE:
 * Identifies duplicate dog records from different aggregators to prevent
 * showing the same dog multiple times. Uses multiple matching strategies
 * including external ID matching, name + shelter matching, and photo
 * similarity detection.
 *
 * USAGE:
 * DuplicateDetector detector;
 * auto result = detector.findDuplicate(dog);
 * if (result.is_duplicate) {
 *     // Handle duplicate
 * }
 *
 * DEPENDENCIES:
 * - Dog model
 * - ConnectionPool for database access
 *
 * MODIFICATION GUIDE:
 * - Add new matching strategies as needed
 * - Tune matching thresholds based on data quality
 * - Consider image hashing for photo matching
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

// Forward declarations
namespace wtl::core::models {
    struct Dog;
}

namespace wtl::aggregators {

/**
 * @enum MatchType
 * @brief How a duplicate was identified
 */
enum class MatchType {
    NONE = 0,              ///< No match found
    EXTERNAL_ID,           ///< Same external ID from same source
    CROSS_SOURCE_ID,       ///< Matched across sources via mapping
    NAME_SHELTER,          ///< Same name at same shelter
    NAME_BREED_AGE,        ///< Same name, breed, and similar age
    PHOTO_HASH             ///< Similar photos detected
};

/**
 * @brief Convert MatchType to string
 */
inline std::string matchTypeToString(MatchType type) {
    switch (type) {
        case MatchType::EXTERNAL_ID:    return "external_id";
        case MatchType::CROSS_SOURCE_ID: return "cross_source_id";
        case MatchType::NAME_SHELTER:   return "name_shelter";
        case MatchType::NAME_BREED_AGE: return "name_breed_age";
        case MatchType::PHOTO_HASH:     return "photo_hash";
        default:                        return "none";
    }
}

/**
 * @struct DuplicateResult
 * @brief Result of duplicate detection
 */
struct DuplicateResult {
    bool is_duplicate{false};          ///< Whether a duplicate was found
    MatchType match_type{MatchType::NONE};  ///< How the match was found
    double confidence{0.0};            ///< Match confidence (0.0-1.0)
    std::string matched_dog_id;        ///< ID of the existing dog
    std::string matched_external_id;   ///< External ID of match
    std::string matched_source;        ///< Source of the matched dog
    std::vector<std::string> match_reasons;  ///< Why it was considered a match

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const {
        Json::Value json;
        json["is_duplicate"] = is_duplicate;
        json["match_type"] = matchTypeToString(match_type);
        json["confidence"] = confidence;
        json["matched_dog_id"] = matched_dog_id;
        json["matched_external_id"] = matched_external_id;
        json["matched_source"] = matched_source;

        Json::Value reasons(Json::arrayValue);
        for (const auto& reason : match_reasons) {
            reasons.append(reason);
        }
        json["match_reasons"] = reasons;

        return json;
    }
};

/**
 * @struct MergeResult
 * @brief Result of merging duplicate records
 */
struct MergeResult {
    bool success{false};               ///< Whether merge succeeded
    std::string primary_dog_id;        ///< ID of the primary (kept) record
    std::string merged_dog_id;         ///< ID of the merged (potentially removed) record
    std::vector<std::string> fields_updated;  ///< Fields updated from merged record
    std::string error;                 ///< Error message if failed

    Json::Value toJson() const {
        Json::Value json;
        json["success"] = success;
        json["primary_dog_id"] = primary_dog_id;
        json["merged_dog_id"] = merged_dog_id;
        json["error"] = error;

        Json::Value fields(Json::arrayValue);
        for (const auto& field : fields_updated) {
            fields.append(field);
        }
        json["fields_updated"] = fields;

        return json;
    }
};

/**
 * @struct ExternalMapping
 * @brief Maps external IDs to internal IDs
 */
struct ExternalMapping {
    std::string id;                    ///< Mapping record ID
    std::string internal_id;           ///< Our internal dog ID
    std::string external_id;           ///< External source ID
    std::string source;                ///< Source name (rescuegroups, shelter_direct) - NO Petfinder API exists
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

/**
 * @class DuplicateDetector
 * @brief Detects duplicate dogs across sources
 *
 * Uses multiple strategies to identify duplicates:
 * 1. External ID match (same source)
 * 2. Cross-source ID mapping
 * 3. Name + shelter combination
 * 4. Name + breed + age (with fuzzy matching)
 * 5. Photo hash similarity (future)
 */
class DuplicateDetector {
public:
    // =========================================================================
    // SINGLETON
    // =========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to detector
     */
    static DuplicateDetector& getInstance();

    // Delete copy/move
    DuplicateDetector(const DuplicateDetector&) = delete;
    DuplicateDetector& operator=(const DuplicateDetector&) = delete;

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Set minimum confidence for duplicate detection
     * @param confidence Minimum confidence (0.0-1.0)
     */
    void setMinConfidence(double confidence);

    /**
     * @brief Enable/disable specific match types
     * @param type Match type
     * @param enabled Whether to enable
     */
    void setMatchTypeEnabled(MatchType type, bool enabled);

    /**
     * @brief Set name similarity threshold
     * @param threshold Threshold (0.0-1.0) for name matching
     */
    void setNameSimilarityThreshold(double threshold);

    // =========================================================================
    // DUPLICATE DETECTION
    // =========================================================================

    /**
     * @brief Find potential duplicate for a dog
     *
     * @param dog Dog to check
     * @return DuplicateResult with match information
     *
     * Checks all enabled match strategies in order of reliability.
     */
    DuplicateResult findDuplicate(const wtl::core::models::Dog& dog);

    /**
     * @brief Find duplicates by external ID
     *
     * @param external_id External ID to check
     * @param source Source name
     * @return DuplicateResult
     */
    DuplicateResult findByExternalId(const std::string& external_id,
                                      const std::string& source);

    /**
     * @brief Find duplicates by name and shelter
     *
     * @param name Dog name
     * @param shelter_id Shelter ID
     * @return DuplicateResult
     */
    DuplicateResult findByNameAndShelter(const std::string& name,
                                          const std::string& shelter_id);

    /**
     * @brief Find potential matches by name, breed, and age
     *
     * @param name Dog name
     * @param breed Primary breed
     * @param age_months Age in months
     * @return Vector of potential matches with confidence
     */
    std::vector<DuplicateResult> findByNameBreedAge(const std::string& name,
                                                     const std::string& breed,
                                                     int age_months);

    // =========================================================================
    // EXTERNAL ID MAPPING
    // =========================================================================

    /**
     * @brief Register an external ID mapping
     *
     * @param internal_id Our internal dog ID
     * @param external_id External source ID
     * @param source Source name
     */
    void registerMapping(const std::string& internal_id,
                         const std::string& external_id,
                         const std::string& source);

    /**
     * @brief Get internal ID from external ID
     *
     * @param external_id External ID
     * @param source Source name
     * @return Internal ID or empty if not found
     */
    std::string getInternalId(const std::string& external_id,
                               const std::string& source);

    /**
     * @brief Get all external IDs for an internal ID
     *
     * @param internal_id Our internal dog ID
     * @return Vector of ExternalMapping
     */
    std::vector<ExternalMapping> getExternalMappings(const std::string& internal_id);

    /**
     * @brief Remove a mapping
     *
     * @param external_id External ID
     * @param source Source name
     */
    void removeMapping(const std::string& external_id, const std::string& source);

    // =========================================================================
    // RECORD MERGING
    // =========================================================================

    /**
     * @brief Merge two dog records
     *
     * @param primary_id ID of the primary record to keep
     * @param merge_id ID of the record to merge from
     * @param prefer_newer Use newer data when conflicting
     * @return MergeResult
     *
     * Merges data from merge_id into primary_id, optionally preferring
     * newer data. Does not delete the merged record.
     */
    MergeResult mergeRecords(const std::string& primary_id,
                              const std::string& merge_id,
                              bool prefer_newer = true);

    /**
     * @brief Determine which record should be primary
     *
     * @param dog1 First dog
     * @param dog2 Second dog
     * @return ID of the record that should be primary
     *
     * Uses heuristics like data completeness, source reliability,
     * and recency to determine the best primary record.
     */
    std::string determinePrimaryRecord(const wtl::core::models::Dog& dog1,
                                        const wtl::core::models::Dog& dog2);

    // =========================================================================
    // SOURCE OF TRUTH
    // =========================================================================

    /**
     * @brief Set preferred source for a dog
     *
     * @param internal_id Internal dog ID
     * @param source Preferred source name
     *
     * When data conflicts, prefer data from this source.
     */
    void setPreferredSource(const std::string& internal_id,
                            const std::string& source);

    /**
     * @brief Get preferred source for a dog
     *
     * @param internal_id Internal dog ID
     * @return Preferred source or empty
     */
    std::string getPreferredSource(const std::string& internal_id);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get duplicate detection statistics
     * @return JSON with statistics
     */
    Json::Value getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    // =========================================================================
    // UTILITY
    // =========================================================================

    /**
     * @brief Calculate string similarity (Levenshtein-based)
     *
     * @param s1 First string
     * @param s2 Second string
     * @return Similarity (0.0-1.0)
     */
    static double calculateStringSimilarity(const std::string& s1,
                                            const std::string& s2);

    /**
     * @brief Normalize name for comparison
     *
     * @param name Name to normalize
     * @return Normalized name
     */
    static std::string normalizeName(const std::string& name);

private:
    DuplicateDetector();

    // Configuration
    double min_confidence_{0.8};
    double name_similarity_threshold_{0.85};
    std::unordered_map<MatchType, bool> enabled_match_types_;

    // In-memory cache of recent mappings
    struct MappingCache {
        std::unordered_map<std::string, std::string> external_to_internal;
        std::unordered_map<std::string, std::vector<std::string>> internal_to_external;
    };
    MappingCache mapping_cache_;

    // Statistics
    mutable std::mutex stats_mutex_;
    size_t total_checks_{0};
    size_t duplicates_found_{0};
    std::unordered_map<MatchType, size_t> matches_by_type_;

    /**
     * @brief Load mapping cache from database
     */
    void loadMappingCache();

    /**
     * @brief Calculate Levenshtein distance
     */
    static int levenshteinDistance(const std::string& s1, const std::string& s2);

    /**
     * @brief Score data completeness of a dog record
     * @param dog Dog to score
     * @return Completeness score
     */
    int scoreDataCompleteness(const wtl::core::models::Dog& dog) const;

    /**
     * @brief Get source reliability score
     * @param source Source name
     * @return Reliability score (higher is better)
     */
    int getSourceReliability(const std::string& source) const;
};

} // namespace wtl::aggregators
