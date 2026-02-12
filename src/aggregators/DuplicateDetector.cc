/**
 * @file DuplicateDetector.cc
 * @brief Implementation of duplicate dog detection
 * @see DuplicateDetector.h for documentation
 */

#include "aggregators/DuplicateDetector.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/models/Dog.h"

namespace wtl::aggregators {

// ============================================================================
// SINGLETON
// ============================================================================

DuplicateDetector& DuplicateDetector::getInstance() {
    static DuplicateDetector instance;
    return instance;
}

DuplicateDetector::DuplicateDetector() {
    // Enable all match types by default
    enabled_match_types_[MatchType::EXTERNAL_ID] = true;
    enabled_match_types_[MatchType::CROSS_SOURCE_ID] = true;
    enabled_match_types_[MatchType::NAME_SHELTER] = true;
    enabled_match_types_[MatchType::NAME_BREED_AGE] = true;
    enabled_match_types_[MatchType::PHOTO_HASH] = false;  // Disabled until implemented

    // Initialize stats
    matches_by_type_[MatchType::EXTERNAL_ID] = 0;
    matches_by_type_[MatchType::CROSS_SOURCE_ID] = 0;
    matches_by_type_[MatchType::NAME_SHELTER] = 0;
    matches_by_type_[MatchType::NAME_BREED_AGE] = 0;
    matches_by_type_[MatchType::PHOTO_HASH] = 0;

    // Load mapping cache
    loadMappingCache();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void DuplicateDetector::setMinConfidence(double confidence) {
    min_confidence_ = std::max(0.0, std::min(1.0, confidence));
}

void DuplicateDetector::setMatchTypeEnabled(MatchType type, bool enabled) {
    enabled_match_types_[type] = enabled;
}

void DuplicateDetector::setNameSimilarityThreshold(double threshold) {
    name_similarity_threshold_ = std::max(0.0, std::min(1.0, threshold));
}

// ============================================================================
// DUPLICATE DETECTION
// ============================================================================

DuplicateResult DuplicateDetector::findDuplicate(const wtl::core::models::Dog& dog) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_checks_++;

    DuplicateResult result;

    // Strategy 1: Check external ID (most reliable)
    if (enabled_match_types_[MatchType::EXTERNAL_ID] &&
        dog.external_id.has_value() && !dog.external_id->empty()) {
        result = findByExternalId(*dog.external_id, dog.source);
        if (result.is_duplicate && result.confidence >= min_confidence_) {
            duplicates_found_++;
            matches_by_type_[MatchType::EXTERNAL_ID]++;
            return result;
        }
    }

    // Strategy 2: Check name + shelter
    if (enabled_match_types_[MatchType::NAME_SHELTER] &&
        !dog.name.empty() && !dog.shelter_id.empty()) {
        result = findByNameAndShelter(dog.name, dog.shelter_id);
        if (result.is_duplicate && result.confidence >= min_confidence_) {
            duplicates_found_++;
            matches_by_type_[MatchType::NAME_SHELTER]++;
            return result;
        }
    }

    // Strategy 3: Check name + breed + age (fuzzy)
    if (enabled_match_types_[MatchType::NAME_BREED_AGE] &&
        !dog.name.empty() && !dog.breed_primary.empty()) {
        auto matches = findByNameBreedAge(dog.name, dog.breed_primary, dog.age_months);
        if (!matches.empty()) {
            result = matches[0];  // Best match
            if (result.is_duplicate && result.confidence >= min_confidence_) {
                duplicates_found_++;
                matches_by_type_[MatchType::NAME_BREED_AGE]++;
                return result;
            }
        }
    }

    // No duplicate found
    return DuplicateResult{};
}

DuplicateResult DuplicateDetector::findByExternalId(const std::string& external_id,
                                                     const std::string& source) {
    DuplicateResult result;

    if (external_id.empty()) {
        return result;
    }

    try {
        // Check cache first
        std::string cache_key = source + ":" + external_id;
        auto cache_it = mapping_cache_.external_to_internal.find(cache_key);
        if (cache_it != mapping_cache_.external_to_internal.end()) {
            result.is_duplicate = true;
            result.match_type = MatchType::EXTERNAL_ID;
            result.confidence = 1.0;
            result.matched_dog_id = cache_it->second;
            result.matched_external_id = external_id;
            result.matched_source = source;
            result.match_reasons.push_back("Exact external ID match");
            return result;
        }

        // Check database
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto db_result = txn.exec_params(
            "SELECT internal_id FROM external_mappings "
            "WHERE external_id = $1 AND source = $2",
            external_id, source
        );

        if (!db_result.empty()) {
            result.is_duplicate = true;
            result.match_type = MatchType::EXTERNAL_ID;
            result.confidence = 1.0;
            result.matched_dog_id = db_result[0]["internal_id"].as<std::string>();
            result.matched_external_id = external_id;
            result.matched_source = source;
            result.match_reasons.push_back("Exact external ID match from database");

            // Update cache
            mapping_cache_.external_to_internal[cache_key] = result.matched_dog_id;
        }

        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to check external ID: " + std::string(e.what()),
            {{"external_id", external_id}, {"source", source}}
        );
    }

    return result;
}

DuplicateResult DuplicateDetector::findByNameAndShelter(const std::string& name,
                                                         const std::string& shelter_id) {
    DuplicateResult result;

    if (name.empty() || shelter_id.empty()) {
        return result;
    }

    std::string normalized_name = normalizeName(name);

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Look for dogs with same name at same shelter
        auto db_result = txn.exec_params(
            "SELECT id, name, external_id, source FROM dogs "
            "WHERE shelter_id = $1 AND LOWER(TRIM(name)) = $2 "
            "AND is_available = true "
            "LIMIT 5",
            shelter_id, normalized_name
        );

        if (!db_result.empty()) {
            // Check similarity for each match
            double best_similarity = 0.0;
            std::string best_match_id;

            for (const auto& row : db_result) {
                std::string db_name = row["name"].as<std::string>();
                double similarity = calculateStringSimilarity(name, db_name);

                if (similarity > best_similarity) {
                    best_similarity = similarity;
                    best_match_id = row["id"].as<std::string>();
                    result.matched_external_id = row["external_id"].is_null()
                        ? "" : row["external_id"].as<std::string>();
                    result.matched_source = row["source"].as<std::string>();
                }
            }

            if (best_similarity >= name_similarity_threshold_) {
                result.is_duplicate = true;
                result.match_type = MatchType::NAME_SHELTER;
                result.confidence = best_similarity;
                result.matched_dog_id = best_match_id;
                result.match_reasons.push_back("Same name at same shelter");
                result.match_reasons.push_back("Name similarity: " +
                    std::to_string(static_cast<int>(best_similarity * 100)) + "%");
            }
        }

        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to check name/shelter: " + std::string(e.what()),
            {{"name", name}, {"shelter_id", shelter_id}}
        );
    }

    return result;
}

std::vector<DuplicateResult> DuplicateDetector::findByNameBreedAge(
    const std::string& name,
    const std::string& breed,
    int age_months) {

    std::vector<DuplicateResult> results;

    if (name.empty()) {
        return results;
    }

    std::string normalized_name = normalizeName(name);
    int age_tolerance_months = 6;  // Allow 6 month difference

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Look for dogs with similar name, same breed, and similar age
        auto db_result = txn.exec_params(
            "SELECT id, name, breed_primary, age_months, external_id, source "
            "FROM dogs "
            "WHERE breed_primary = $1 "
            "AND ABS(age_months - $2) <= $3 "
            "AND is_available = true "
            "LIMIT 20",
            breed, age_months, age_tolerance_months
        );

        for (const auto& row : db_result) {
            std::string db_name = row["name"].as<std::string>();
            double name_similarity = calculateStringSimilarity(name, db_name);

            if (name_similarity >= name_similarity_threshold_ * 0.9) {  // Slightly lower threshold
                DuplicateResult result;
                result.is_duplicate = true;
                result.match_type = MatchType::NAME_BREED_AGE;

                // Calculate overall confidence
                int db_age = row["age_months"].as<int>();
                double age_diff = std::abs(age_months - db_age);
                double age_similarity = 1.0 - (age_diff / static_cast<double>(age_tolerance_months));

                result.confidence = (name_similarity * 0.6) + (age_similarity * 0.2) + 0.2;  // Breed match is 0.2

                result.matched_dog_id = row["id"].as<std::string>();
                result.matched_external_id = row["external_id"].is_null()
                    ? "" : row["external_id"].as<std::string>();
                result.matched_source = row["source"].as<std::string>();

                result.match_reasons.push_back("Similar name: " + db_name);
                result.match_reasons.push_back("Same breed: " + breed);
                result.match_reasons.push_back("Similar age: " + std::to_string(db_age) + " months");

                results.push_back(result);
            }
        }

        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Sort by confidence
        std::sort(results.begin(), results.end(),
            [](const DuplicateResult& a, const DuplicateResult& b) {
                return a.confidence > b.confidence;
            });

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to check name/breed/age: " + std::string(e.what()),
            {{"name", name}, {"breed", breed}}
        );
    }

    return results;
}

// ============================================================================
// EXTERNAL ID MAPPING
// ============================================================================

void DuplicateDetector::registerMapping(const std::string& internal_id,
                                         const std::string& external_id,
                                         const std::string& source) {
    if (internal_id.empty() || external_id.empty() || source.empty()) {
        return;
    }

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Upsert mapping
        txn.exec_params(
            "INSERT INTO external_mappings (internal_id, external_id, source) "
            "VALUES ($1, $2, $3) "
            "ON CONFLICT (external_id, source) DO UPDATE "
            "SET internal_id = $1, updated_at = NOW()",
            internal_id, external_id, source
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Update cache
        std::string cache_key = source + ":" + external_id;
        mapping_cache_.external_to_internal[cache_key] = internal_id;
        mapping_cache_.internal_to_external[internal_id].push_back(cache_key);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to register mapping: " + std::string(e.what()),
            {{"internal_id", internal_id}, {"external_id", external_id}, {"source", source}}
        );
    }
}

std::string DuplicateDetector::getInternalId(const std::string& external_id,
                                              const std::string& source) {
    // Check cache
    std::string cache_key = source + ":" + external_id;
    auto it = mapping_cache_.external_to_internal.find(cache_key);
    if (it != mapping_cache_.external_to_internal.end()) {
        return it->second;
    }

    // Check database
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT internal_id FROM external_mappings "
            "WHERE external_id = $1 AND source = $2",
            external_id, source
        );

        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (!result.empty()) {
            std::string internal_id = result[0]["internal_id"].as<std::string>();
            mapping_cache_.external_to_internal[cache_key] = internal_id;
            return internal_id;
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get internal ID: " + std::string(e.what()),
            {{"external_id", external_id}, {"source", source}}
        );
    }

    return "";
}

std::vector<ExternalMapping> DuplicateDetector::getExternalMappings(
    const std::string& internal_id) {

    std::vector<ExternalMapping> mappings;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT id, internal_id, external_id, source, created_at, updated_at "
            "FROM external_mappings "
            "WHERE internal_id = $1",
            internal_id
        );

        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            ExternalMapping mapping;
            mapping.id = row["id"].as<std::string>();
            mapping.internal_id = row["internal_id"].as<std::string>();
            mapping.external_id = row["external_id"].as<std::string>();
            mapping.source = row["source"].as<std::string>();
            mappings.push_back(mapping);
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get external mappings: " + std::string(e.what()),
            {{"internal_id", internal_id}}
        );
    }

    return mappings;
}

void DuplicateDetector::removeMapping(const std::string& external_id,
                                       const std::string& source) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "DELETE FROM external_mappings WHERE external_id = $1 AND source = $2",
            external_id, source
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Remove from cache
        std::string cache_key = source + ":" + external_id;
        mapping_cache_.external_to_internal.erase(cache_key);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to remove mapping: " + std::string(e.what()),
            {{"external_id", external_id}, {"source", source}}
        );
    }
}

// ============================================================================
// RECORD MERGING
// ============================================================================

MergeResult DuplicateDetector::mergeRecords(const std::string& primary_id,
                                             const std::string& merge_id,
                                             bool prefer_newer) {
    MergeResult result;
    result.primary_dog_id = primary_id;
    result.merged_dog_id = merge_id;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get both records
        auto primary_result = txn.exec_params(
            "SELECT * FROM dogs WHERE id = $1", primary_id
        );
        auto merge_result = txn.exec_params(
            "SELECT * FROM dogs WHERE id = $1", merge_id
        );

        if (primary_result.empty()) {
            result.error = "Primary record not found";
            return result;
        }
        if (merge_result.empty()) {
            result.error = "Merge record not found";
            return result;
        }

        // Build update query for fields to merge
        std::vector<std::string> updates;
        std::vector<std::string> field_names;

        auto shouldUpdate = [&](const std::string& field) -> bool {
            bool primary_null = primary_result[0][field].is_null();
            bool merge_null = merge_result[0][field].is_null();

            if (primary_null && !merge_null) {
                return true;  // Primary missing, merge has value
            }
            if (!primary_null && !merge_null && prefer_newer) {
                // Check timestamps if preferring newer
                // For simplicity, check updated_at of merged record
                return false;  // Don't overwrite unless missing
            }
            return false;
        };

        // Check specific fields
        std::vector<std::string> mergeable_fields = {
            "description", "video_url", "weight_lbs"
        };

        for (const auto& field : mergeable_fields) {
            if (shouldUpdate(field)) {
                field_names.push_back(field);
            }
        }

        // Update external mapping to point to primary
        txn.exec_params(
            "UPDATE external_mappings SET internal_id = $1 WHERE internal_id = $2",
            primary_id, merge_id
        );

        // Mark merged record as duplicate (don't delete)
        txn.exec_params(
            "UPDATE dogs SET status = 'duplicate', "
            "merged_into_id = $1, updated_at = NOW() "
            "WHERE id = $2",
            primary_id, merge_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        result.success = true;
        result.fields_updated = field_names;

    } catch (const std::exception& e) {
        result.error = std::string("Merge failed: ") + e.what();
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to merge records: " + std::string(e.what()),
            {{"primary_id", primary_id}, {"merge_id", merge_id}}
        );
    }

    return result;
}

std::string DuplicateDetector::determinePrimaryRecord(
    const wtl::core::models::Dog& dog1,
    const wtl::core::models::Dog& dog2) {

    int score1 = scoreDataCompleteness(dog1) + getSourceReliability(dog1.source);
    int score2 = scoreDataCompleteness(dog2) + getSourceReliability(dog2.source);

    // Prefer newer if scores are equal
    if (score1 == score2) {
        return dog1.updated_at > dog2.updated_at ? dog1.id : dog2.id;
    }

    return score1 > score2 ? dog1.id : dog2.id;
}

// ============================================================================
// SOURCE OF TRUTH
// ============================================================================

void DuplicateDetector::setPreferredSource(const std::string& internal_id,
                                            const std::string& source) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "UPDATE dogs SET preferred_source = $1 WHERE id = $2",
            source, internal_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to set preferred source: " + std::string(e.what()),
            {{"internal_id", internal_id}, {"source", source}}
        );
    }
}

std::string DuplicateDetector::getPreferredSource(const std::string& internal_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT preferred_source FROM dogs WHERE id = $1",
            internal_id
        );

        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (!result.empty() && !result[0]["preferred_source"].is_null()) {
            return result[0]["preferred_source"].as<std::string>();
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get preferred source: " + std::string(e.what()),
            {{"internal_id", internal_id}}
        );
    }
    return "";
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value DuplicateDetector::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Json::Value stats;
    stats["total_checks"] = static_cast<Json::UInt64>(total_checks_);
    stats["duplicates_found"] = static_cast<Json::UInt64>(duplicates_found_);

    double rate = total_checks_ > 0
        ? (static_cast<double>(duplicates_found_) / static_cast<double>(total_checks_)) * 100.0
        : 0.0;
    stats["duplicate_rate_percent"] = rate;

    Json::Value by_type;
    for (const auto& [type, count] : matches_by_type_) {
        by_type[matchTypeToString(type)] = static_cast<Json::UInt64>(count);
    }
    stats["matches_by_type"] = by_type;

    stats["min_confidence"] = min_confidence_;
    stats["name_similarity_threshold"] = name_similarity_threshold_;

    return stats;
}

void DuplicateDetector::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_checks_ = 0;
    duplicates_found_ = 0;
    for (auto& [type, count] : matches_by_type_) {
        count = 0;
    }
}

// ============================================================================
// UTILITY
// ============================================================================

double DuplicateDetector::calculateStringSimilarity(const std::string& s1,
                                                     const std::string& s2) {
    if (s1.empty() && s2.empty()) return 1.0;
    if (s1.empty() || s2.empty()) return 0.0;

    std::string n1 = normalizeName(s1);
    std::string n2 = normalizeName(s2);

    if (n1 == n2) return 1.0;

    int distance = levenshteinDistance(n1, n2);
    int max_len = static_cast<int>(std::max(n1.length(), n2.length()));

    return 1.0 - (static_cast<double>(distance) / static_cast<double>(max_len));
}

std::string DuplicateDetector::normalizeName(const std::string& name) {
    std::string result;
    result.reserve(name.size());

    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result += std::tolower(static_cast<unsigned char>(c));
        }
    }

    return result;
}

void DuplicateDetector::loadMappingCache() {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Load recent mappings (last 10000)
        auto result = txn.exec(
            "SELECT internal_id, external_id, source FROM external_mappings "
            "ORDER BY updated_at DESC LIMIT 10000"
        );

        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            std::string internal = row["internal_id"].as<std::string>();
            std::string external = row["external_id"].as<std::string>();
            std::string source = row["source"].as<std::string>();

            std::string cache_key = source + ":" + external;
            mapping_cache_.external_to_internal[cache_key] = internal;
            mapping_cache_.internal_to_external[internal].push_back(cache_key);
        }

    } catch (const std::exception& e) {
        // Cache load failure is not critical
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to load mapping cache: " + std::string(e.what()),
            {}
        );
    }
}

int DuplicateDetector::levenshteinDistance(const std::string& s1, const std::string& s2) {
    size_t m = s1.length();
    size_t n = s2.length();

    if (m == 0) return static_cast<int>(n);
    if (n == 0) return static_cast<int>(m);

    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    for (size_t i = 0; i <= m; i++) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= n; j++) dp[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= m; i++) {
        for (size_t j = 1; j <= n; j++) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i-1][j] + 1,
                dp[i][j-1] + 1,
                dp[i-1][j-1] + cost
            });
        }
    }

    return dp[m][n];
}

int DuplicateDetector::scoreDataCompleteness(const wtl::core::models::Dog& dog) const {
    int score = 0;

    if (!dog.name.empty()) score += 10;
    if (!dog.breed_primary.empty()) score += 5;
    if (dog.breed_secondary.has_value()) score += 3;
    if (!dog.description.empty()) score += 10;
    if (!dog.photo_urls.empty()) score += 15;
    if (dog.photo_urls.size() > 1) score += 5;
    if (dog.video_url.has_value()) score += 5;
    if (dog.weight_lbs.has_value()) score += 3;
    if (!dog.tags.empty()) score += 5;

    return score;
}

int DuplicateDetector::getSourceReliability(const std::string& source) const {
    // Assign reliability scores to sources
    if (source == "shelter_direct") return 100;  // Direct from shelter is most reliable
    if (source == "rescuegroups") return 80;
    // DEPRECATED: No Petfinder API exists
    // if (source == "petfinder") return 70;
    return 50;  // Unknown source
}

} // namespace wtl::aggregators
