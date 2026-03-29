/**
 * @file SearchQueryBuilder.cc
 * @brief Implementation of SearchQueryBuilder
 * @see SearchQueryBuilder.h for documentation
 */

#include "core/services/SearchQueryBuilder.h"

// Standard library includes
#include <algorithm>
#include <cctype>
#include <sstream>

namespace wtl::core::services {

// ============================================================================
// FLUENT BUILDER METHODS
// ============================================================================

SearchQueryBuilder& SearchQueryBuilder::select(const std::string& columns) {
    select_ = columns;
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::from(const std::string& table) {
    from_ = table;
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::where(const std::string& condition) {
    if (!condition.empty()) {
        conditions_.push_back(condition);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereEquals(const std::string& column,
                                                     const std::string& value) {
    std::string placeholder = addParam(value);
    conditions_.push_back(sanitizeColumn(column) + " = " + placeholder);
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereOptional(const std::string& column,
                                                       const std::optional<std::string>& value) {
    if (value.has_value() && !value.value().empty()) {
        whereEquals(column, value.value());
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereOptionalInt(const std::string& column,
                                                          const std::optional<int>& value) {
    if (value.has_value()) {
        std::string placeholder = addParam(std::to_string(value.value()));
        conditions_.push_back(sanitizeColumn(column) + " = " + placeholder);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereOptionalDouble(const std::string& column,
                                                             const std::optional<double>& value) {
    if (value.has_value()) {
        std::string placeholder = addParam(std::to_string(value.value()));
        conditions_.push_back(sanitizeColumn(column) + " = " + placeholder);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereOptionalBool(const std::string& column,
                                                           const std::optional<bool>& value) {
    if (value.has_value()) {
        // For boolean attributes stored in tags array, check if tag exists
        // Tags format: good_with_kids, good_with_dogs, good_with_cats, house_trained
        if (column == "good_with_kids" || column == "good_with_dogs" ||
            column == "good_with_cats" || column == "house_trained") {
            if (value.value()) {
                // Tag should exist in tags array
                std::string placeholder = addParam(column);
                conditions_.push_back(placeholder + " = ANY(tags)");
            } else {
                // Tag should NOT exist in tags array
                std::string placeholder = addParam(column);
                conditions_.push_back("NOT (" + placeholder + " = ANY(tags))");
            }
        } else {
            // Regular boolean column
            std::string placeholder = addParam(value.value() ? "true" : "false");
            conditions_.push_back(sanitizeColumn(column) + " = " + placeholder);
        }
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereIn(const std::string& column,
                                                 const std::vector<std::string>& values) {
    if (!values.empty()) {
        std::string col = sanitizeColumn(column);
        std::string in_clause = col + " IN (";

        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) in_clause += ", ";
            in_clause += addParam(values[i]);
        }

        in_clause += ")";
        conditions_.push_back(in_clause);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereLike(const std::string& column,
                                                   const std::string& pattern) {
    if (!pattern.empty()) {
        // Use ILIKE for case-insensitive search with wildcards
        std::string placeholder = addParam("%" + pattern + "%");
        conditions_.push_back(sanitizeColumn(column) + " ILIKE " + placeholder);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereFullText(const std::string& column,
                                                       const std::string& query) {
    if (!query.empty()) {
        // Convert query to tsquery format
        // Split by spaces and join with &
        std::string tsquery;
        std::istringstream iss(query);
        std::string word;
        bool first = true;

        while (iss >> word) {
            if (!first) tsquery += " & ";
            first = false;
            // Add :* for prefix matching
            tsquery += word + ":*";
        }

        if (!tsquery.empty()) {
            std::string placeholder = addParam(tsquery);
            // Use to_tsquery with websearch_to_tsquery for better handling
            conditions_.push_back(sanitizeColumn(column) + " @@ to_tsquery('english', " + placeholder + ")");
        }
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereMin(const std::string& column,
                                                  const std::optional<int>& min_value) {
    if (min_value.has_value()) {
        std::string placeholder = addParam(std::to_string(min_value.value()));
        conditions_.push_back(sanitizeColumn(column) + " >= " + placeholder);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereMax(const std::string& column,
                                                  const std::optional<int>& max_value) {
    if (max_value.has_value()) {
        std::string placeholder = addParam(std::to_string(max_value.value()));
        conditions_.push_back(sanitizeColumn(column) + " <= " + placeholder);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereMinDouble(const std::string& column,
                                                        const std::optional<double>& min_value) {
    if (min_value.has_value()) {
        std::string placeholder = addParam(std::to_string(min_value.value()));
        conditions_.push_back(sanitizeColumn(column) + " >= " + placeholder);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::whereMaxDouble(const std::string& column,
                                                        const std::optional<double>& max_value) {
    if (max_value.has_value()) {
        std::string placeholder = addParam(std::to_string(max_value.value()));
        conditions_.push_back(sanitizeColumn(column) + " <= " + placeholder);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::orderBy(const std::string& column,
                                                 const std::string& direction) {
    if (!column.empty()) {
        order_by_ = sanitizeColumn(column) + " " + validateDirection(direction);
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::limit(int limit) {
    if (limit > 0) {
        limit_ = limit;
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::offset(int offset) {
    if (offset >= 0) {
        offset_ = offset;
    }
    return *this;
}

SearchQueryBuilder& SearchQueryBuilder::groupBy(const std::string& columns) {
    if (!columns.empty()) {
        group_by_ = columns;
    }
    return *this;
}

// ============================================================================
// BUILD METHODS
// ============================================================================

std::string SearchQueryBuilder::build() const {
    std::ostringstream sql;

    // SELECT clause
    sql << "SELECT " << (select_.empty() ? "*" : select_);

    // FROM clause
    if (!from_.empty()) {
        sql << " FROM " << from_;
    }

    // WHERE clause
    if (!conditions_.empty()) {
        sql << " WHERE ";
        for (size_t i = 0; i < conditions_.size(); ++i) {
            if (i > 0) sql << " AND ";
            sql << "(" << conditions_[i] << ")";
        }
    }

    // GROUP BY clause
    if (!group_by_.empty()) {
        sql << " GROUP BY " << group_by_;
    }

    // ORDER BY clause
    if (!order_by_.empty()) {
        sql << " ORDER BY " << order_by_;
    }

    // LIMIT clause
    if (limit_ > 0) {
        sql << " LIMIT " << limit_;
    }

    // OFFSET clause
    if (offset_ > 0) {
        sql << " OFFSET " << offset_;
    }

    return sql.str();
}

std::string SearchQueryBuilder::buildCount() const {
    std::ostringstream sql;

    // SELECT COUNT(*)
    sql << "SELECT COUNT(*)";

    // FROM clause
    if (!from_.empty()) {
        sql << " FROM " << from_;
    }

    // WHERE clause (same as regular query)
    if (!conditions_.empty()) {
        sql << " WHERE ";
        for (size_t i = 0; i < conditions_.size(); ++i) {
            if (i > 0) sql << " AND ";
            sql << "(" << conditions_[i] << ")";
        }
    }

    // Note: No ORDER BY, LIMIT, or OFFSET for count queries

    return sql.str();
}

std::vector<std::string> SearchQueryBuilder::getParams() const {
    return params_;
}

size_t SearchQueryBuilder::getParamCount() const {
    return params_.size();
}

void SearchQueryBuilder::clear() {
    select_.clear();
    from_.clear();
    conditions_.clear();
    params_.clear();
    order_by_.clear();
    group_by_.clear();
    limit_ = 0;
    offset_ = 0;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::string SearchQueryBuilder::nextPlaceholder() const {
    return "$" + std::to_string(params_.size() + 1);
}

std::string SearchQueryBuilder::addParam(const std::string& value) {
    std::string placeholder = nextPlaceholder();
    params_.push_back(value);
    return placeholder;
}

std::string SearchQueryBuilder::sanitizeColumn(const std::string& column) {
    // Remove any potentially dangerous characters
    // Allow: alphanumeric, underscore, dot (for table.column), spaces (for expressions)
    std::string sanitized;
    sanitized.reserve(column.size());

    for (char c : column) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '_' || c == '.' || c == ' ' || c == '(' || c == ')' || c == ',') {
            sanitized += c;
        }
    }

    return sanitized;
}

std::string SearchQueryBuilder::validateDirection(const std::string& direction) {
    // Convert to lowercase for comparison
    std::string lower;
    lower.reserve(direction.size());
    for (char c : direction) {
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower == "desc" || lower == "descending") {
        return "DESC";
    }

    // Default to ASC for any other value
    return "ASC";
}

} // namespace wtl::core::services
