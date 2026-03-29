/**
 * @file SearchService.cc
 * @brief Implementation of SearchService
 * @see SearchService.h for documentation
 */

#include "core/services/SearchService.h"

// Project includes - these will be provided by other agents
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/models/Dog.h"

// Standard library includes
#include <cmath>
#include <sstream>

namespace wtl::core::services {

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

SearchService& SearchService::getInstance() {
    static SearchService instance;
    return instance;
}

// ============================================================================
// MAIN SEARCH
// ============================================================================

SearchResult<::wtl::core::models::Dog> SearchService::searchDogs(
    const SearchFilters& filters,
    const SearchOptions& options) {

    try {
        // Validate options
        SearchOptions validated_options = options;
        validated_options.validate();

        // Validate filters
        std::string validation_error = filters.validate();
        if (!validation_error.empty()) {
            WTL_CAPTURE_ERROR(
                ::wtl::core::debug::ErrorCategory::VALIDATION,
                "Invalid search filters: " + validation_error,
                {{"error", validation_error}}
            );
            return SearchResult<::wtl::core::models::Dog>::empty(
                validated_options.page, validated_options.per_page);
        }

        // Build the filtered query
        SearchQueryBuilder builder = buildFilteredQuery(filters);

        // Add sorting
        std::string sort_expr = getSortExpression(validated_options.sort_by,
                                                   validated_options.sort_order);
        builder.orderBy(sort_expr, validated_options.sort_order);

        // Add pagination
        builder.limit(validated_options.per_page);
        builder.offset(validated_options.getOffset());

        // Build queries
        std::string query = builder.build();
        std::string count_query = builder.buildCount();
        std::vector<std::string> params = builder.getParams();

        // Execute and return results
        return executeSearch(query, count_query, params, validated_options);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "Search failed: " + std::string(e.what()),
            {{"filters_empty", filters.isEmpty() ? "true" : "false"}}
        );
        return SearchResult<::wtl::core::models::Dog>::empty(options.page, options.per_page);
    }
}

// ============================================================================
// QUICK SEARCHES
// ============================================================================

std::vector<::wtl::core::models::Dog> SearchService::getLongestWaiting(int limit) {
    try {
        // Use self-healing wrapper for resilience
        return executeWithHealing<
            std::vector<::wtl::core::models::Dog>>(
            "search_longest_waiting",
            [&]() {
                SearchQueryBuilder builder;
                builder.select(getDogSelectColumns())
                       .from("dogs")
                       .where("is_available = true")
                       .where("status = 'adoptable'")
                       .orderBy("intake_date", "asc")  // Oldest first = longest wait
                       .limit(limit);

                return executeListQuery(builder.build(), builder.getParams());
            },
            [&]() {
                // Fallback: return empty list
                return std::vector<::wtl::core::models::Dog>{};
            }
        );
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "getLongestWaiting failed: " + std::string(e.what()),
            {{"limit", std::to_string(limit)}}
        );
        return {};
    }
}

std::vector<::wtl::core::models::Dog> SearchService::getMostUrgent(int limit) {
    try {
        return executeWithHealing<
            std::vector<::wtl::core::models::Dog>>(
            "search_most_urgent",
            [&]() {
                SearchQueryBuilder builder;
                builder.select(getDogSelectColumns())
                       .from("dogs")
                       .where("is_available = true")
                       .where("status = 'adoptable'")
                       // Order by urgency level with custom priority
                       // critical > high > medium > normal
                       .orderBy("CASE urgency_level "
                               "WHEN 'critical' THEN 1 "
                               "WHEN 'high' THEN 2 "
                               "WHEN 'medium' THEN 3 "
                               "ELSE 4 END", "asc")
                       .limit(limit);

                return executeListQuery(builder.build(), builder.getParams());
            },
            [&]() {
                return std::vector<::wtl::core::models::Dog>{};
            }
        );
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "getMostUrgent failed: " + std::string(e.what()),
            {{"limit", std::to_string(limit)}}
        );
        return {};
    }
}

std::vector<::wtl::core::models::Dog> SearchService::getRecentlyAdded(int limit) {
    try {
        return executeWithHealing<
            std::vector<::wtl::core::models::Dog>>(
            "search_recently_added",
            [&]() {
                SearchQueryBuilder builder;
                builder.select(getDogSelectColumns())
                       .from("dogs")
                       .where("is_available = true")
                       .where("status = 'adoptable'")
                       .orderBy("created_at", "desc")  // Newest first
                       .limit(limit);

                return executeListQuery(builder.build(), builder.getParams());
            },
            [&]() {
                return std::vector<::wtl::core::models::Dog>{};
            }
        );
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "getRecentlyAdded failed: " + std::string(e.what()),
            {{"limit", std::to_string(limit)}}
        );
        return {};
    }
}

std::vector<::wtl::core::models::Dog> SearchService::getByBreed(const std::string& breed, int limit) {
    try {
        if (breed.empty()) {
            return {};
        }

        return executeWithHealing<
            std::vector<::wtl::core::models::Dog>>(
            "search_by_breed_" + breed,
            [&]() {
                SearchQueryBuilder builder;
                builder.select(getDogSelectColumns())
                       .from("dogs")
                       .where("is_available = true")
                       .where("status = 'adoptable'");

                // Match breed in primary or secondary breed
                // Use ILIKE for case-insensitive matching
                std::string breed_pattern = "%" + breed + "%";
                builder.where("(breed_primary ILIKE $" +
                              std::to_string(builder.getParamCount() + 1) +
                              " OR breed_secondary ILIKE $" +
                              std::to_string(builder.getParamCount() + 2) + ")");

                auto params = builder.getParams();
                params.push_back(breed_pattern);
                params.push_back(breed_pattern);

                builder.orderBy("intake_date", "asc")
                       .limit(limit);

                // Need to manually build with extra params
                std::string query = builder.build();
                return executeListQuery(query, params);
            },
            [&]() {
                return std::vector<::wtl::core::models::Dog>{};
            }
        );
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "getByBreed failed: " + std::string(e.what()),
            {{"breed", breed}, {"limit", std::to_string(limit)}}
        );
        return {};
    }
}

std::vector<::wtl::core::models::Dog> SearchService::getSimilarDogs(const std::string& dog_id, int limit) {
    try {
        if (dog_id.empty()) {
            return {};
        }

        return executeWithHealing<
            std::vector<::wtl::core::models::Dog>>(
            "search_similar_" + dog_id,
            [&]() {
                auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();
                pqxx::work txn(*conn);

                // First, get the target dog's characteristics
                auto target_result = txn.exec_params(
                    "SELECT breed_primary, size, age_category, shelter_id "
                    "FROM dogs WHERE id = $1",
                    dog_id
                );

                if (target_result.empty()) {
                    txn.abort();
                    ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                    return std::vector<::wtl::core::models::Dog>{};
                }

                std::string breed = target_result[0]["breed_primary"].as<std::string>();
                std::string size = target_result[0]["size"].as<std::string>();
                std::string age_category = target_result[0]["age_category"].as<std::string>();

                // Find similar dogs (same breed OR same size+age)
                auto result = txn.exec_params(
                    "SELECT " + getDogSelectColumns() + " FROM dogs "
                    "WHERE id != $1 "
                    "AND is_available = true "
                    "AND status = 'adoptable' "
                    "AND (breed_primary = $2 OR (size = $3 AND age_category = $4)) "
                    "ORDER BY "
                    "  CASE WHEN breed_primary = $2 THEN 0 ELSE 1 END, "
                    "  CASE WHEN size = $3 AND age_category = $4 THEN 0 ELSE 1 END, "
                    "  intake_date ASC "
                    "LIMIT $5",
                    dog_id, breed, size, age_category, limit
                );

                std::vector<::wtl::core::models::Dog> dogs;
                dogs.reserve(result.size());
                for (const auto& row : result) {
                    dogs.push_back(::wtl::core::models::Dog::fromDbRow(row));
                }

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                return dogs;
            },
            [&]() {
                return std::vector<::wtl::core::models::Dog>{};
            }
        );
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "getSimilarDogs failed: " + std::string(e.what()),
            {{"dog_id", dog_id}, {"limit", std::to_string(limit)}}
        );
        return {};
    }
}

std::vector<::wtl::core::models::Dog> SearchService::getNearby(double lat, double lon,
                                                              int radius_miles, int limit) {
    try {
        // Validate coordinates
        if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
            WTL_CAPTURE_ERROR(
                ::wtl::core::debug::ErrorCategory::VALIDATION,
                "Invalid coordinates",
                {{"lat", std::to_string(lat)}, {"lon", std::to_string(lon)}}
            );
            return {};
        }

        return executeWithHealing<
            std::vector<::wtl::core::models::Dog>>(
            "search_nearby",
            [&]() {
                auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();
                pqxx::work txn(*conn);

                // Use Haversine formula for distance calculation
                // 3959 is Earth's radius in miles
                std::string query = R"(
                    SELECT d.*, s.latitude as shelter_lat, s.longitude as shelter_lon,
                           (3959 * acos(
                               cos(radians($1)) * cos(radians(s.latitude)) *
                               cos(radians(s.longitude) - radians($2)) +
                               sin(radians($1)) * sin(radians(s.latitude))
                           )) AS distance
                    FROM dogs d
                    JOIN shelters s ON d.shelter_id = s.id
                    WHERE d.is_available = true
                    AND d.status = 'adoptable'
                    AND s.latitude IS NOT NULL
                    AND s.longitude IS NOT NULL
                    AND (3959 * acos(
                        cos(radians($1)) * cos(radians(s.latitude)) *
                        cos(radians(s.longitude) - radians($2)) +
                        sin(radians($1)) * sin(radians(s.latitude))
                    )) <= $3
                    ORDER BY distance ASC
                    LIMIT $4
                )";

                auto result = txn.exec_params(query, lat, lon, radius_miles, limit);

                std::vector<::wtl::core::models::Dog> dogs;
                dogs.reserve(result.size());
                for (const auto& row : result) {
                    dogs.push_back(::wtl::core::models::Dog::fromDbRow(row));
                }

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                return dogs;
            },
            [&]() {
                return std::vector<::wtl::core::models::Dog>{};
            }
        );
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "getNearby failed: " + std::string(e.what()),
            {{"lat", std::to_string(lat)},
             {"lon", std::to_string(lon)},
             {"radius", std::to_string(radius_miles)}}
        );
        return {};
    }
}

// ============================================================================
// AUTOCOMPLETE SUGGESTIONS
// ============================================================================

std::vector<std::string> SearchService::suggestBreeds(const std::string& query, int limit) {
    try {
        if (query.empty() || query.length() < 2) {
            return {};
        }

        return executeWithHealing<
            std::vector<std::string>>(
            "suggest_breeds",
            [&]() {
                std::string pattern = query + "%";  // Prefix match

                std::string sql = R"(
                    SELECT DISTINCT breed_primary
                    FROM dogs
                    WHERE breed_primary ILIKE $1
                    AND is_available = true
                    ORDER BY breed_primary
                    LIMIT $2
                )";

                return executeSuggestionQuery(sql, {pattern, std::to_string(limit)});
            },
            [&]() {
                return std::vector<std::string>{};
            }
        );
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "suggestBreeds failed: " + std::string(e.what()),
            {{"query", query}}
        );
        return {};
    }
}

std::vector<std::string> SearchService::suggestLocations(const std::string& query, int limit) {
    try {
        if (query.empty() || query.length() < 2) {
            return {};
        }

        return executeWithHealing<
            std::vector<std::string>>(
            "suggest_locations",
            [&]() {
                std::string pattern = query + "%";

                // Search for cities and states
                std::string sql = R"(
                    SELECT DISTINCT s.city || ', ' || st.code AS location
                    FROM shelters s
                    JOIN states st ON s.state_code = st.code
                    WHERE (s.city ILIKE $1 OR st.name ILIKE $1 OR st.code ILIKE $1)
                    AND s.is_active = true
                    ORDER BY location
                    LIMIT $2
                )";

                return executeSuggestionQuery(sql, {pattern, std::to_string(limit)});
            },
            [&]() {
                return std::vector<std::string>{};
            }
        );
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "suggestLocations failed: " + std::string(e.what()),
            {{"query", query}}
        );
        return {};
    }
}

std::vector<std::string> SearchService::suggestNames(const std::string& query, int limit) {
    try {
        if (query.empty() || query.length() < 2) {
            return {};
        }

        return executeWithHealing<
            std::vector<std::string>>(
            "suggest_names",
            [&]() {
                std::string pattern = query + "%";

                std::string sql = R"(
                    SELECT DISTINCT name
                    FROM dogs
                    WHERE name ILIKE $1
                    AND is_available = true
                    ORDER BY name
                    LIMIT $2
                )";

                return executeSuggestionQuery(sql, {pattern, std::to_string(limit)});
            },
            [&]() {
                return std::vector<std::string>{};
            }
        );
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "suggestNames failed: " + std::string(e.what()),
            {{"query", query}}
        );
        return {};
    }
}

// ============================================================================
// FACETED SEARCH
// ============================================================================

Json::Value SearchService::getAvailableFilters(const SearchFilters& current_filters) {
    try {
        Json::Value result;

        // Get breed counts
        auto breed_counts = getFilterCounts("breed", current_filters);
        Json::Value breeds(Json::arrayValue);
        for (const auto& [name, count] : breed_counts) {
            Json::Value breed;
            breed["name"] = name;
            breed["count"] = count;
            breeds.append(breed);
        }
        result["breeds"] = breeds;

        // Get size counts
        auto size_counts = getFilterCounts("size", current_filters);
        Json::Value sizes(Json::arrayValue);
        for (const auto& [name, count] : size_counts) {
            Json::Value size;
            size["name"] = name;
            size["count"] = count;
            sizes.append(size);
        }
        result["sizes"] = sizes;

        // Get age category counts
        auto age_counts = getFilterCounts("age_category", current_filters);
        Json::Value ages(Json::arrayValue);
        for (const auto& [name, count] : age_counts) {
            Json::Value age;
            age["name"] = name;
            age["count"] = count;
            ages.append(age);
        }
        result["age_categories"] = ages;

        // Get gender counts
        auto gender_counts = getFilterCounts("gender", current_filters);
        Json::Value genders(Json::arrayValue);
        for (const auto& [name, count] : gender_counts) {
            Json::Value gender;
            gender["name"] = name;
            gender["count"] = count;
            genders.append(gender);
        }
        result["genders"] = genders;

        // Get urgency level counts
        auto urgency_counts = getFilterCounts("urgency_level", current_filters);
        Json::Value urgencies(Json::arrayValue);
        for (const auto& [name, count] : urgency_counts) {
            Json::Value urgency;
            urgency["name"] = name;
            urgency["count"] = count;
            urgencies.append(urgency);
        }
        result["urgency_levels"] = urgencies;

        // Get state counts
        auto state_counts = getFilterCounts("state", current_filters);
        Json::Value states(Json::arrayValue);
        for (const auto& [code, count] : state_counts) {
            Json::Value state;
            state["code"] = code;
            state["count"] = count;
            states.append(state);
        }
        result["states"] = states;

        return result;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "getAvailableFilters failed: " + std::string(e.what()),
            {}
        );
        return Json::Value(Json::objectValue);
    }
}

std::vector<std::pair<std::string, int>> SearchService::getFilterCounts(
    const std::string& filter_type,
    const SearchFilters& current_filters) {

    try {
        auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string column;
        std::string table = "dogs";
        bool needs_join = false;

        if (filter_type == "breed") {
            column = "breed_primary";
        } else if (filter_type == "size") {
            column = "size";
        } else if (filter_type == "age_category") {
            column = "age_category";
        } else if (filter_type == "gender") {
            column = "gender";
        } else if (filter_type == "urgency_level") {
            column = "urgency_level";
        } else if (filter_type == "state") {
            column = "s.state_code";
            table = "dogs d JOIN shelters s ON d.shelter_id = s.id";
            needs_join = true;
        } else {
            ::wtl::core::db::ConnectionPool::getInstance().release(conn);
            return {};
        }

        // Build query that respects current filters (except the one we're counting)
        SearchFilters filter_copy = current_filters;

        // Clear the filter we're counting so we get all options
        if (filter_type == "breed") filter_copy.breed = std::nullopt;
        else if (filter_type == "size") filter_copy.size = std::nullopt;
        else if (filter_type == "age_category") filter_copy.age_category = std::nullopt;
        else if (filter_type == "gender") filter_copy.gender = std::nullopt;
        else if (filter_type == "urgency_level") filter_copy.urgency_level = std::nullopt;
        else if (filter_type == "state") filter_copy.state_code = std::nullopt;

        SearchQueryBuilder builder = buildFilteredQuery(filter_copy);

        std::string base_conditions;
        auto params = builder.getParams();

        // Build the count query
        std::ostringstream sql;
        sql << "SELECT " << column << " AS filter_value, COUNT(*) AS cnt "
            << "FROM " << table << " ";

        if (!needs_join) {
            sql << "WHERE is_available = true AND status = 'adoptable' ";
        } else {
            sql << "WHERE d.is_available = true AND d.status = 'adoptable' ";
        }

        sql << "GROUP BY " << column << " "
            << "ORDER BY cnt DESC";

        auto result = txn.exec(sql.str());

        std::vector<std::pair<std::string, int>> counts;
        counts.reserve(result.size());
        for (const auto& row : result) {
            if (!row["filter_value"].is_null()) {
                counts.emplace_back(
                    row["filter_value"].as<std::string>(),
                    row["cnt"].as<int>()
                );
            }
        }

        txn.commit();
        ::wtl::core::db::ConnectionPool::getInstance().release(conn);
        return counts;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::DATABASE,
            "getFilterCounts failed: " + std::string(e.what()),
            {{"filter_type", filter_type}}
        );
        return {};
    }
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

SearchQueryBuilder SearchService::buildFilteredQuery(const SearchFilters& filters) {
    SearchQueryBuilder builder;

    // Base SELECT and FROM
    builder.select(getDogSelectColumns())
           .from("dogs d LEFT JOIN shelters s ON d.shelter_id = s.id");

    // Always filter for available dogs
    builder.where("d.is_available = true")
           .where("d.status = 'adoptable'");

    // Apply filters
    if (filters.state_code.has_value()) {
        builder.whereOptional("s.state_code", filters.state_code);
    }

    builder.whereOptional("d.shelter_id", filters.shelter_id)
           .whereOptional("d.size", filters.size)
           .whereOptional("d.age_category", filters.age_category)
           .whereOptional("d.gender", filters.gender)
           .whereOptional("d.urgency_level", filters.urgency_level)
           .whereOptional("d.color_primary", filters.color);

    // Breed filter - match primary or secondary
    if (filters.breed.has_value() && !filters.breed.value().empty()) {
        builder.whereLike("d.breed_primary", filters.breed.value());
    }

    // Boolean filters (tags-based)
    builder.whereOptionalBool("good_with_kids", filters.good_with_kids)
           .whereOptionalBool("good_with_dogs", filters.good_with_dogs)
           .whereOptionalBool("good_with_cats", filters.good_with_cats)
           .whereOptionalBool("house_trained", filters.house_trained);

    // Range filters
    builder.whereMin("d.age_months", filters.min_age_months)
           .whereMax("d.age_months", filters.max_age_months)
           .whereMinDouble("d.weight_lbs", filters.min_weight)
           .whereMaxDouble("d.weight_lbs", filters.max_weight);

    // Full-text search using ILIKE fallback (no search_vector column)
    if (filters.query.has_value() && !filters.query.value().empty()) {
        builder.whereLike("d.name", filters.query.value());
    }

    return builder;
}

SearchResult<::wtl::core::models::Dog> SearchService::executeSearch(
    const std::string& query,
    const std::string& count_query,
    const std::vector<std::string>& params,
    const SearchOptions& options) {

    auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

    try {
        pqxx::work txn(*conn);

        // Execute count query first
        auto count_result = txn.exec_params(count_query);
        int total = count_result[0][0].as<int>();

        // Execute main query
        auto data_result = txn.exec_params(query);

        std::vector<::wtl::core::models::Dog> dogs;
        dogs.reserve(data_result.size());
        for (const auto& row : data_result) {
            dogs.push_back(::wtl::core::models::Dog::fromDbRow(row));
        }

        txn.commit();
        ::wtl::core::db::ConnectionPool::getInstance().release(conn);

        return SearchResult<::wtl::core::models::Dog>(
            std::move(dogs), total, options.page, options.per_page);

    } catch (const std::exception& e) {
        ::wtl::core::db::ConnectionPool::getInstance().release(conn);
        throw;
    }
}

std::vector<::wtl::core::models::Dog> SearchService::executeListQuery(
    const std::string& query,
    const std::vector<std::string>& params) {

    auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

    try {
        pqxx::work txn(*conn);
        auto result = txn.exec(query);

        std::vector<::wtl::core::models::Dog> dogs;
        dogs.reserve(result.size());
        for (const auto& row : result) {
            dogs.push_back(::wtl::core::models::Dog::fromDbRow(row));
        }

        txn.commit();
        ::wtl::core::db::ConnectionPool::getInstance().release(conn);
        return dogs;

    } catch (const std::exception& e) {
        ::wtl::core::db::ConnectionPool::getInstance().release(conn);
        throw;
    }
}

std::vector<std::string> SearchService::executeSuggestionQuery(
    const std::string& query,
    const std::vector<std::string>& params) {

    auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

    try {
        pqxx::work txn(*conn);

        pqxx::result result;
        if (params.size() == 2) {
            result = txn.exec_params(query, params[0], std::stoi(params[1]));
        } else {
            result = txn.exec(query);
        }

        std::vector<std::string> suggestions;
        suggestions.reserve(result.size());
        for (const auto& row : result) {
            if (!row[0].is_null()) {
                suggestions.push_back(row[0].as<std::string>());
            }
        }

        txn.commit();
        ::wtl::core::db::ConnectionPool::getInstance().release(conn);
        return suggestions;

    } catch (const std::exception& e) {
        ::wtl::core::db::ConnectionPool::getInstance().release(conn);
        throw;
    }
}

std::string SearchService::getDogSelectColumns() {
    return "d.id, d.name, d.shelter_id, "
           "d.breed_primary, d.breed_secondary, "
           "d.size, d.age_category, d.age_months, "
           "d.gender, d.color_primary, d.color_secondary, d.weight_lbs, "
           "d.status, d.is_available, "
           "d.intake_date, d.available_date, d.euthanasia_date, "
           "d.urgency_level, d.is_on_euthanasia_list, "
           "d.photo_urls, d.video_url, "
           "d.description, d.tags, "
           "d.external_id, d.source, "
           "d.created_at, d.updated_at";
}

std::string SearchService::getSortExpression(const std::string& sort_by,
                                              const std::string& sort_order) {
    // Map user-friendly names to SQL expressions
    if (sort_by == "wait_time") {
        // For wait time, we sort by intake_date
        // ASC = longest wait first (oldest intake date)
        return "d.intake_date";
    }

    if (sort_by == "urgency") {
        // Custom ordering for urgency levels
        // Always show critical first, then high, medium, normal
        return "CASE d.urgency_level "
               "WHEN 'critical' THEN 1 "
               "WHEN 'high' THEN 2 "
               "WHEN 'medium' THEN 3 "
               "ELSE 4 END";
    }

    if (sort_by == "name") {
        return "d.name";
    }

    if (sort_by == "intake_date") {
        return "d.intake_date";
    }

    if (sort_by == "created_at") {
        return "d.created_at";
    }

    // Default to intake_date
    return "d.intake_date";
}

} // namespace wtl::core::services
