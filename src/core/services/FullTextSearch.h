/**
 * @file FullTextSearch.h
 * @brief PostgreSQL full-text search utilities
 *
 * PURPOSE:
 * Provides utilities for working with PostgreSQL's full-text search
 * capabilities, including query conversion, vector building, ranking,
 * and match highlighting.
 *
 * USAGE:
 * // Convert user query to PostgreSQL tsquery format
 * std::string tsquery = FullTextSearch::toTsQuery("labrador retriever");
 *
 * // Build tsvector from multiple columns
 * std::string tsvector = FullTextSearch::buildTsVector({"name", "description", "breed_primary"});
 *
 * // Get rank expression for ordering by relevance
 * std::string rank = FullTextSearch::rankExpression("search_vector", "labrador");
 *
 * DEPENDENCIES:
 * - None (generates SQL fragments)
 *
 * MODIFICATION GUIDE:
 * - To change the text search configuration, modify DEFAULT_CONFIG
 * - To add new query formats, update toTsQuery() method
 *
 * RECOMMENDED INDEXES:
 * CREATE INDEX idx_dogs_search_vector ON dogs USING gin(search_vector);
 * CREATE INDEX idx_dogs_name_trgm ON dogs USING gin(name gin_trgm_ops);
 * CREATE INDEX idx_dogs_breed_trgm ON dogs USING gin(breed_primary gin_trgm_ops);
 *
 * @author Agent 7 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <sstream>
#include <string>
#include <vector>

namespace wtl::core::services {

/**
 * @class FullTextSearch
 * @brief Static utilities for PostgreSQL full-text search
 *
 * Provides methods for building PostgreSQL full-text search queries,
 * including ts_vector construction, ts_query conversion, ranking,
 * and result highlighting.
 */
class FullTextSearch {
public:
    /// Default PostgreSQL text search configuration
    static constexpr const char* DEFAULT_CONFIG = "english";

    // ========================================================================
    // QUERY CONVERSION
    // ========================================================================

    /**
     * @brief Convert user search query to PostgreSQL tsquery format
     *
     * Converts a space-separated user query into a PostgreSQL tsquery
     * format suitable for full-text search. Handles special characters
     * and optional prefix matching.
     *
     * @param query User's search query string
     * @param prefix_match If true, add :* for prefix matching (default: true)
     * @return tsquery-compatible string
     *
     * @example
     * toTsQuery("labrador retriever")
     * // Returns: "labrador:* & retriever:*"
     *
     * toTsQuery("golden retriever", false)
     * // Returns: "golden & retriever"
     */
    static std::string toTsQuery(const std::string& query, bool prefix_match = true);

    /**
     * @brief Convert query to plainto_tsquery format
     *
     * Simpler conversion that uses PostgreSQL's plainto_tsquery function,
     * which handles the parsing automatically.
     *
     * @param query User's search query
     * @return SQL fragment using plainto_tsquery
     */
    static std::string toPlainTsQuery(const std::string& query);

    /**
     * @brief Convert query to websearch_to_tsquery format
     *
     * Uses PostgreSQL 11+ websearch_to_tsquery for natural language
     * search with support for quotes and minus signs.
     *
     * @param query User's search query (supports quotes and - for exclusion)
     * @return SQL fragment using websearch_to_tsquery
     */
    static std::string toWebsearchTsQuery(const std::string& query);

    /**
     * @brief Escape special characters in search query
     *
     * Escapes characters that have special meaning in tsquery:
     * & | ! ( ) : * \
     *
     * @param query Raw user input
     * @return Escaped string safe for tsquery
     */
    static std::string escapeQuery(const std::string& query);

    // ========================================================================
    // TSVECTOR BUILDING
    // ========================================================================

    /**
     * @brief Build tsvector expression from multiple columns
     *
     * Creates a tsvector expression that combines multiple columns,
     * optionally with different weights for ranking.
     *
     * @param columns List of column names to include
     * @param config Text search configuration (default: english)
     * @return SQL expression for tsvector
     *
     * @example
     * buildTsVector({"name", "description"})
     * // Returns: "to_tsvector('english', COALESCE(name, '') || ' ' || COALESCE(description, ''))"
     */
    static std::string buildTsVector(const std::vector<std::string>& columns,
                                      const std::string& config = DEFAULT_CONFIG);

    /**
     * @brief Build weighted tsvector expression
     *
     * Creates a tsvector with different weights for different columns.
     * Weights are A (highest) to D (lowest).
     *
     * @param weighted_columns List of {column, weight} pairs
     * @param config Text search configuration
     * @return SQL expression for weighted tsvector
     *
     * @example
     * buildWeightedTsVector({{"name", "A"}, {"breed_primary", "B"}, {"description", "C"}})
     * // Returns: setweight(to_tsvector('english', name), 'A') || ...
     */
    static std::string buildWeightedTsVector(
        const std::vector<std::pair<std::string, std::string>>& weighted_columns,
        const std::string& config = DEFAULT_CONFIG);

    // ========================================================================
    // RANKING
    // ========================================================================

    /**
     * @brief Generate rank expression for ordering by relevance
     *
     * Uses ts_rank or ts_rank_cd to calculate relevance score
     * for ordering search results.
     *
     * @param tsvector_column Column containing the tsvector
     * @param query User's search query
     * @param normalization Normalization option (default: 32 for rank normalization)
     * @return SQL expression for ranking
     *
     * @example
     * rankExpression("search_vector", "labrador")
     * // Returns: "ts_rank(search_vector, to_tsquery('english', 'labrador:*'), 32)"
     */
    static std::string rankExpression(const std::string& tsvector_column,
                                       const std::string& query,
                                       int normalization = 32);

    /**
     * @brief Generate rank expression using ts_rank_cd (cover density)
     *
     * Uses cover density ranking which considers proximity of matching
     * terms. Better for longer documents.
     *
     * @param tsvector_column Column containing the tsvector
     * @param query User's search query
     * @return SQL expression for cover density ranking
     */
    static std::string rankCdExpression(const std::string& tsvector_column,
                                         const std::string& query);

    // ========================================================================
    // HIGHLIGHTING
    // ========================================================================

    /**
     * @brief Generate SQL for highlighting matches in text
     *
     * Uses ts_headline to wrap matching terms with HTML tags
     * for visual highlighting in search results.
     *
     * @param column Text column to highlight
     * @param query Search query
     * @param start_tag Opening HTML tag (default: <b>)
     * @param stop_tag Closing HTML tag (default: </b>)
     * @param max_words Maximum words in result (default: 50)
     * @return SQL expression for highlighted text
     *
     * @example
     * highlightMatches("description", "labrador")
     * // Returns: "ts_headline('english', description, to_tsquery('english', 'labrador:*'),
     * //           'StartSel=<b>, StopSel=</b>, MaxWords=50')"
     */
    static std::string highlightMatches(const std::string& column,
                                         const std::string& query,
                                         const std::string& start_tag = "<b>",
                                         const std::string& stop_tag = "</b>",
                                         int max_words = 50);

    /**
     * @brief Generate SQL for highlighted snippet
     *
     * Similar to highlightMatches but with additional options for
     * generating search result snippets.
     *
     * @param column Text column to highlight
     * @param query Search query
     * @param min_words Minimum words to show
     * @param max_words Maximum words to show
     * @param short_word Minimum word length to include
     * @return SQL expression for snippet
     */
    static std::string snippetExpression(const std::string& column,
                                          const std::string& query,
                                          int min_words = 15,
                                          int max_words = 35,
                                          int short_word = 3);

    // ========================================================================
    // SIMILARITY SEARCH (Trigram)
    // ========================================================================

    /**
     * @brief Generate similarity expression for fuzzy matching
     *
     * Uses pg_trgm extension for fuzzy string matching.
     * Useful for typo tolerance.
     *
     * @param column Column to compare
     * @param value Value to match
     * @return SQL expression for similarity score
     *
     * @example
     * similarityExpression("breed_primary", "labrador")
     * // Returns: "similarity(breed_primary, 'labrador')"
     */
    static std::string similarityExpression(const std::string& column,
                                             const std::string& value);

    /**
     * @brief Generate trigram similarity condition
     *
     * Uses the % operator for trigram matching with a threshold.
     *
     * @param column Column to compare
     * @param value Value to match
     * @param threshold Similarity threshold (default: 0.3)
     * @return SQL WHERE condition for similarity
     */
    static std::string similarityCondition(const std::string& column,
                                            const std::string& value,
                                            double threshold = 0.3);

    // ========================================================================
    // UTILITY
    // ========================================================================

    /**
     * @brief Check if a search query is valid
     *
     * Validates that the query is not empty and contains searchable terms.
     *
     * @param query Search query to validate
     * @return true if query is valid for searching
     */
    static bool isValidQuery(const std::string& query);

    /**
     * @brief Normalize search query
     *
     * Trims whitespace, converts to lowercase, and removes
     * extra spaces from search query.
     *
     * @param query Raw query string
     * @return Normalized query
     */
    static std::string normalizeQuery(const std::string& query);
};

} // namespace wtl::core::services
