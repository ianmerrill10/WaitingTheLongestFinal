/**
 * @file SearchService.h
 * @brief Service for dog search operations with filtering, pagination, and full-text search
 *
 * PURPOSE:
 * Provides comprehensive search functionality for finding dogs based on
 * various criteria including location, breed, size, age, temperament,
 * and urgency level. Supports full-text search, autocomplete suggestions,
 * and faceted search for filter UI.
 *
 * USAGE:
 * auto& service = SearchService::getInstance();
 *
 * // Full search with filters
 * SearchFilters filters;
 * filters.state_code = "TX";
 * filters.breed = "labrador";
 * SearchOptions options;
 * options.page = 1;
 * options.per_page = 20;
 * auto results = service.searchDogs(filters, options);
 *
 * // Quick searches
 * auto longest_waiting = service.getLongestWaiting(10);
 * auto urgent = service.getMostUrgent(10);
 *
 * // Autocomplete
 * auto breed_suggestions = service.suggestBreeds("lab", 10);
 *
 * DEPENDENCIES:
 * - ConnectionPool (Agent 1) - Database connections
 * - Dog model (Agent 3) - Dog data structure
 * - ErrorCapture (Agent 1) - Error logging
 * - SelfHealing (Agent 1) - Retry logic
 *
 * MODIFICATION GUIDE:
 * - To add new search methods, follow the pattern of existing quick searches
 * - All database access must use the connection pool
 * - All errors must be captured with WTL_CAPTURE_ERROR macro
 * - Consider adding indexes for new search patterns
 *
 * PERFORMANCE NOTES:
 * - Uses parameterized queries via SearchQueryBuilder
 * - Recommends indexes on frequently filtered columns
 * - Consider caching popular search results (see comments)
 *
 * RECOMMENDED INDEXES:
 * CREATE INDEX idx_dogs_state ON dogs(shelter_id) JOIN shelters USING (id) ON state_code;
 * CREATE INDEX idx_dogs_breed ON dogs(breed_primary);
 * CREATE INDEX idx_dogs_size ON dogs(size);
 * CREATE INDEX idx_dogs_age ON dogs(age_category);
 * CREATE INDEX idx_dogs_urgency ON dogs(urgency_level);
 * CREATE INDEX idx_dogs_intake ON dogs(intake_date);
 * CREATE INDEX idx_dogs_available ON dogs(is_available, status);
 * CREATE INDEX idx_dogs_search_vector ON dogs USING gin(search_vector);
 *
 * @author Agent 7 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "core/services/SearchFilters.h"
#include "core/services/SearchOptions.h"
#include "core/services/SearchResult.h"
#include "core/services/SearchQueryBuilder.h"
#include "core/services/FullTextSearch.h"

// Model includes (needed for SearchResult<Dog> template instantiation)
#include "core/models/Dog.h"
#include "core/debug/SelfHealing.h"

namespace wtl::core::services {

/**
 * @class SearchService
 * @brief Singleton service for dog search operations
 *
 * Thread-safe singleton that provides comprehensive search functionality
 * for dogs. Uses connection pooling for database access and supports
 * various filtering, sorting, and pagination options.
 */
class SearchService {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     *
     * @return Reference to the SearchService singleton
     */
    static SearchService& getInstance();

    // Delete copy/move constructors for singleton
    SearchService(const SearchService&) = delete;
    SearchService& operator=(const SearchService&) = delete;
    SearchService(SearchService&&) = delete;
    SearchService& operator=(SearchService&&) = delete;

    // ========================================================================
    // MAIN SEARCH
    // ========================================================================

    /**
     * @brief Search dogs with filters and pagination
     *
     * Performs a comprehensive search based on the provided filters and
     * returns paginated results with total count.
     *
     * @param filters Search filter criteria
     * @param options Pagination and sorting options
     * @return SearchResult containing matching dogs and pagination info
     *
     * @example
     * SearchFilters filters;
     * filters.state_code = "TX";
     * filters.size = "large";
     * filters.good_with_kids = true;
     *
     * SearchOptions options;
     * options.page = 2;
     * options.per_page = 20;
     * options.sort_by = "urgency";
     *
     * auto result = SearchService::getInstance().searchDogs(filters, options);
     * // result.items = vector of dogs
     * // result.total = total matching (for pagination)
     */
    SearchResult<::wtl::core::models::Dog> searchDogs(const SearchFilters& filters,
                                                     const SearchOptions& options);

    // ========================================================================
    // QUICK SEARCHES
    // ========================================================================

    /**
     * @brief Get dogs waiting the longest for adoption
     *
     * Returns dogs ordered by intake_date ascending (oldest first).
     * Only includes available dogs.
     *
     * @param limit Maximum number of dogs to return (default: 10)
     * @return Vector of dogs waiting longest
     */
    std::vector<::wtl::core::models::Dog> getLongestWaiting(int limit = 10);

    /**
     * @brief Get most urgent dogs (highest priority for adoption)
     *
     * Returns dogs ordered by urgency level (critical first),
     * then by euthanasia date proximity.
     *
     * @param limit Maximum number of dogs to return (default: 10)
     * @return Vector of most urgent dogs
     */
    std::vector<::wtl::core::models::Dog> getMostUrgent(int limit = 10);

    /**
     * @brief Get recently added dogs
     *
     * Returns dogs ordered by created_at descending (newest first).
     * Good for "New Arrivals" section.
     *
     * @param limit Maximum number of dogs to return (default: 10)
     * @return Vector of recently added dogs
     */
    std::vector<::wtl::core::models::Dog> getRecentlyAdded(int limit = 10);

    /**
     * @brief Get dogs by breed
     *
     * Returns dogs matching the specified breed (primary or secondary).
     * Case-insensitive matching with partial match support.
     *
     * @param breed Breed name to search for
     * @param limit Maximum number of dogs to return (default: 20)
     * @return Vector of dogs matching the breed
     */
    std::vector<::wtl::core::models::Dog> getByBreed(const std::string& breed, int limit = 20);

    /**
     * @brief Get dogs similar to a specific dog
     *
     * Finds dogs with similar characteristics (breed, size, age category)
     * for "You might also like" suggestions.
     *
     * @param dog_id UUID of the dog to find similar dogs for
     * @param limit Maximum number of dogs to return (default: 5)
     * @return Vector of similar dogs (excludes the original dog)
     */
    std::vector<::wtl::core::models::Dog> getSimilarDogs(const std::string& dog_id, int limit = 5);

    /**
     * @brief Get dogs near a geographic location
     *
     * Finds dogs within a specified radius of the given coordinates.
     * Uses shelter location for distance calculation.
     *
     * @param lat Latitude of center point
     * @param lon Longitude of center point
     * @param radius_miles Search radius in miles
     * @param limit Maximum number of dogs to return (default: 20)
     * @return Vector of nearby dogs, ordered by distance
     */
    std::vector<::wtl::core::models::Dog> getNearby(double lat, double lon,
                                                   int radius_miles, int limit = 20);

    // ========================================================================
    // AUTOCOMPLETE SUGGESTIONS
    // ========================================================================

    /**
     * @brief Suggest breeds matching a query prefix
     *
     * For breed autocomplete in search UI. Returns distinct breed names
     * that start with or contain the query string.
     *
     * @param query Partial breed name to search for
     * @param limit Maximum number of suggestions (default: 10)
     * @return Vector of matching breed names
     */
    std::vector<std::string> suggestBreeds(const std::string& query, int limit = 10);

    /**
     * @brief Suggest locations (cities/states) matching a query
     *
     * For location autocomplete in search UI. Returns formatted
     * location strings like "Austin, TX".
     *
     * @param query Partial location to search for
     * @param limit Maximum number of suggestions (default: 10)
     * @return Vector of matching location strings
     */
    std::vector<std::string> suggestLocations(const std::string& query, int limit = 10);

    /**
     * @brief Suggest dog names matching a query prefix
     *
     * For name search autocomplete. Returns distinct dog names
     * that start with the query string.
     *
     * @param query Partial name to search for
     * @param limit Maximum number of suggestions (default: 10)
     * @return Vector of matching dog names
     */
    std::vector<std::string> suggestNames(const std::string& query, int limit = 10);

    // ========================================================================
    // FACETED SEARCH
    // ========================================================================

    /**
     * @brief Get available filter options with counts
     *
     * Returns the available options for each filter type along with
     * the count of matching dogs for each option. Used to populate
     * filter dropdowns and show counts in the UI.
     *
     * @param current_filters Currently applied filters
     * @return JSON object with filter options and counts
     *
     * @example
     * // Returns:
     * // {
     * //     "breeds": [{"name": "Labrador", "count": 45}, ...],
     * //     "sizes": [{"name": "large", "count": 120}, ...],
     * //     "states": [{"code": "TX", "name": "Texas", "count": 89}, ...],
     * //     ...
     * // }
     */
    Json::Value getAvailableFilters(const SearchFilters& current_filters);

    /**
     * @brief Get counts for each filter value
     *
     * Returns how many dogs match each filter value, respecting
     * currently applied filters. Useful for showing "(45)" next
     * to filter options.
     *
     * @param filter_type Type of filter (breed, size, state, etc.)
     * @param current_filters Currently applied filters
     * @return Map of filter values to counts
     */
    std::vector<std::pair<std::string, int>> getFilterCounts(
        const std::string& filter_type,
        const SearchFilters& current_filters);

private:
    // ========================================================================
    // CONSTRUCTION (Private for singleton)
    // ========================================================================

    /**
     * @brief Private constructor for singleton
     */
    SearchService() = default;

    /**
     * @brief Private destructor
     */
    ~SearchService() = default;

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    /**
     * @brief Build base query with applied filters
     *
     * Creates a SearchQueryBuilder with the given filters applied.
     * Used as the base for both data and count queries.
     *
     * @param filters Search filters to apply
     * @return Configured SearchQueryBuilder
     */
    SearchQueryBuilder buildFilteredQuery(const SearchFilters& filters);

    /**
     * @brief Execute search query and return results
     *
     * Executes the data query and count query, then builds the
     * SearchResult with pagination information.
     *
     * @param query Main SELECT query
     * @param count_query COUNT query for pagination
     * @param params Query parameters
     * @param options Pagination options
     * @return SearchResult with dogs and pagination
     */
    SearchResult<::wtl::core::models::Dog> executeSearch(
        const std::string& query,
        const std::string& count_query,
        const std::vector<std::string>& params,
        const SearchOptions& options);

    /**
     * @brief Execute a simple list query
     *
     * Executes a query that returns a list of dogs without pagination.
     *
     * @param query SQL query to execute
     * @param params Query parameters
     * @return Vector of dogs
     */
    std::vector<::wtl::core::models::Dog> executeListQuery(
        const std::string& query,
        const std::vector<std::string>& params);

    /**
     * @brief Execute an autocomplete query
     *
     * Executes a query that returns string suggestions.
     *
     * @param query SQL query to execute
     * @param params Query parameters
     * @return Vector of suggestion strings
     */
    std::vector<std::string> executeSuggestionQuery(
        const std::string& query,
        const std::vector<std::string>& params);

    /**
     * @brief Get the base columns for dog select
     *
     * Returns the standard list of columns to select for dog queries.
     *
     * @return Column list string
     */
    static std::string getDogSelectColumns();

    /**
     * @brief Map sort_by value to SQL expression
     *
     * Maps user-friendly sort names to SQL ORDER BY expressions.
     * Handles special cases like urgency ordering.
     *
     * @param sort_by Sort field name
     * @param sort_order Sort direction
     * @return SQL ORDER BY expression
     */
    static std::string getSortExpression(const std::string& sort_by,
                                          const std::string& sort_order);

    /**
     * @brief Execute operation with self-healing wrapper
     * @tparam T Return type
     * @param operation_name Name for tracking
     * @param operation Primary operation
     * @param fallback Optional fallback operation
     * @return T Result value
     */
    template<typename T>
    T executeWithHealing(const std::string& operation_name,
                         std::function<T()> operation,
                         std::function<T()> fallback = nullptr);
};

// Template implementation (must be in header)
template<typename T>
T SearchService::executeWithHealing(const std::string& operation_name,
                                     std::function<T()> operation,
                                     std::function<T()> fallback) {
    auto result = ::wtl::core::debug::SelfHealing::getInstance()
        .executeWithHealing<T>(operation_name, std::move(operation), std::move(fallback));
    if (result.value) {
        return std::move(*result.value);
    }
    if (fallback) {
        return fallback();
    }
    return T{};
}

} // namespace wtl::core::services
