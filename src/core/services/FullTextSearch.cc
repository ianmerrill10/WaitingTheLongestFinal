/**
 * @file FullTextSearch.cc
 * @brief Implementation of FullTextSearch utilities
 * @see FullTextSearch.h for documentation
 */

#include "core/services/FullTextSearch.h"

// Standard library includes
#include <algorithm>
#include <cctype>
#include <sstream>

namespace wtl::core::services {

// ============================================================================
// QUERY CONVERSION
// ============================================================================

std::string FullTextSearch::toTsQuery(const std::string& query, bool prefix_match) {
    if (query.empty()) {
        return "";
    }

    std::string escaped = escapeQuery(query);
    std::string normalized = normalizeQuery(escaped);

    if (normalized.empty()) {
        return "";
    }

    // Split by whitespace and join with &
    std::istringstream iss(normalized);
    std::ostringstream result;
    std::string word;
    bool first = true;

    while (iss >> word) {
        if (!first) {
            result << " & ";
        }
        first = false;

        result << word;
        if (prefix_match) {
            result << ":*";
        }
    }

    return result.str();
}

std::string FullTextSearch::toPlainTsQuery(const std::string& query) {
    if (query.empty()) {
        return "";
    }

    std::string normalized = normalizeQuery(query);
    if (normalized.empty()) {
        return "";
    }

    // Use plainto_tsquery for simple conversion
    std::ostringstream sql;
    sql << "plainto_tsquery('" << DEFAULT_CONFIG << "', '" << normalized << "')";
    return sql.str();
}

std::string FullTextSearch::toWebsearchTsQuery(const std::string& query) {
    if (query.empty()) {
        return "";
    }

    // websearch_to_tsquery handles special syntax like:
    // "exact phrase"  - matches exact phrase
    // -word          - excludes word
    // or             - OR instead of AND
    std::string normalized = normalizeQuery(query);
    if (normalized.empty()) {
        return "";
    }

    std::ostringstream sql;
    sql << "websearch_to_tsquery('" << DEFAULT_CONFIG << "', '" << normalized << "')";
    return sql.str();
}

std::string FullTextSearch::escapeQuery(const std::string& query) {
    std::string result;
    result.reserve(query.size() * 2);

    for (char c : query) {
        // Escape special tsquery characters
        switch (c) {
            case '&':
            case '|':
            case '!':
            case '(':
            case ')':
            case ':':
            case '*':
            case '\\':
            case '\'':
                // Skip these characters (don't include them)
                // Or replace with space
                result += ' ';
                break;
            default:
                result += c;
                break;
        }
    }

    return result;
}

// ============================================================================
// TSVECTOR BUILDING
// ============================================================================

std::string FullTextSearch::buildTsVector(const std::vector<std::string>& columns,
                                           const std::string& config) {
    if (columns.empty()) {
        return "";
    }

    std::ostringstream sql;
    sql << "to_tsvector('" << config << "', ";

    // Concatenate columns with COALESCE to handle NULLs
    bool first = true;
    for (const auto& column : columns) {
        if (!first) {
            sql << " || ' ' || ";
        }
        first = false;
        sql << "COALESCE(" << column << ", '')";
    }

    sql << ")";
    return sql.str();
}

std::string FullTextSearch::buildWeightedTsVector(
    const std::vector<std::pair<std::string, std::string>>& weighted_columns,
    const std::string& config) {

    if (weighted_columns.empty()) {
        return "";
    }

    std::ostringstream sql;
    bool first = true;

    for (const auto& [column, weight] : weighted_columns) {
        if (!first) {
            sql << " || ";
        }
        first = false;

        // setweight(to_tsvector('config', column), 'weight')
        sql << "setweight(to_tsvector('" << config << "', COALESCE("
            << column << ", '')), '" << weight << "')";
    }

    return sql.str();
}

// ============================================================================
// RANKING
// ============================================================================

std::string FullTextSearch::rankExpression(const std::string& tsvector_column,
                                            const std::string& query,
                                            int normalization) {
    if (query.empty()) {
        return "0";  // No ranking if no query
    }

    std::string tsquery = toTsQuery(query);
    if (tsquery.empty()) {
        return "0";
    }

    std::ostringstream sql;
    sql << "ts_rank(" << tsvector_column
        << ", to_tsquery('" << DEFAULT_CONFIG << "', '" << tsquery << "')"
        << ", " << normalization << ")";

    return sql.str();
}

std::string FullTextSearch::rankCdExpression(const std::string& tsvector_column,
                                              const std::string& query) {
    if (query.empty()) {
        return "0";
    }

    std::string tsquery = toTsQuery(query);
    if (tsquery.empty()) {
        return "0";
    }

    std::ostringstream sql;
    sql << "ts_rank_cd(" << tsvector_column
        << ", to_tsquery('" << DEFAULT_CONFIG << "', '" << tsquery << "'))";

    return sql.str();
}

// ============================================================================
// HIGHLIGHTING
// ============================================================================

std::string FullTextSearch::highlightMatches(const std::string& column,
                                              const std::string& query,
                                              const std::string& start_tag,
                                              const std::string& stop_tag,
                                              int max_words) {
    if (query.empty()) {
        return column;  // Return original column if no query
    }

    std::string tsquery = toTsQuery(query);
    if (tsquery.empty()) {
        return column;
    }

    std::ostringstream sql;
    sql << "ts_headline('" << DEFAULT_CONFIG << "', " << column
        << ", to_tsquery('" << DEFAULT_CONFIG << "', '" << tsquery << "')"
        << ", 'StartSel=" << start_tag << ", StopSel=" << stop_tag
        << ", MaxWords=" << max_words << "')";

    return sql.str();
}

std::string FullTextSearch::snippetExpression(const std::string& column,
                                               const std::string& query,
                                               int min_words,
                                               int max_words,
                                               int short_word) {
    if (query.empty()) {
        return column;
    }

    std::string tsquery = toTsQuery(query);
    if (tsquery.empty()) {
        return column;
    }

    std::ostringstream sql;
    sql << "ts_headline('" << DEFAULT_CONFIG << "', " << column
        << ", to_tsquery('" << DEFAULT_CONFIG << "', '" << tsquery << "')"
        << ", 'MinWords=" << min_words
        << ", MaxWords=" << max_words
        << ", ShortWord=" << short_word
        << ", StartSel=<mark>, StopSel=</mark>"
        << ", HighlightAll=false')";

    return sql.str();
}

// ============================================================================
// SIMILARITY SEARCH (Trigram)
// ============================================================================

std::string FullTextSearch::similarityExpression(const std::string& column,
                                                  const std::string& value) {
    if (value.empty()) {
        return "0";
    }

    std::string normalized = normalizeQuery(value);
    if (normalized.empty()) {
        return "0";
    }

    std::ostringstream sql;
    sql << "similarity(" << column << ", '" << normalized << "')";
    return sql.str();
}

std::string FullTextSearch::similarityCondition(const std::string& column,
                                                 const std::string& value,
                                                 double threshold) {
    if (value.empty()) {
        return "true";  // No condition if no value
    }

    std::string normalized = normalizeQuery(value);
    if (normalized.empty()) {
        return "true";
    }

    std::ostringstream sql;
    sql << "similarity(" << column << ", '" << normalized << "') > " << threshold;
    return sql.str();
}

// ============================================================================
// UTILITY
// ============================================================================

bool FullTextSearch::isValidQuery(const std::string& query) {
    if (query.empty()) {
        return false;
    }

    std::string normalized = normalizeQuery(query);
    if (normalized.empty()) {
        return false;
    }

    // Check if there's at least one alphanumeric character
    for (char c : normalized) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            return true;
        }
    }

    return false;
}

std::string FullTextSearch::normalizeQuery(const std::string& query) {
    std::string result;
    result.reserve(query.size());

    bool last_was_space = true;  // Start true to trim leading spaces

    for (char c : query) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            // Collapse multiple spaces into one
            if (!last_was_space) {
                result += ' ';
                last_was_space = true;
            }
        } else {
            // Convert to lowercase
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            last_was_space = false;
        }
    }

    // Trim trailing space
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result;
}

} // namespace wtl::core::services
