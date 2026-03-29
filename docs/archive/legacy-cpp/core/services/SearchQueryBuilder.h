/**
 * @file SearchQueryBuilder.h
 * @brief SQL query builder with parameterized queries for SQL injection prevention
 *
 * PURPOSE:
 * Provides a fluent interface for building SQL SELECT queries with proper
 * parameterization to prevent SQL injection attacks. Supports optional
 * conditions for flexible filter-based queries.
 *
 * USAGE:
 * SearchQueryBuilder builder;
 * builder.select("id, name, breed")
 *        .from("dogs")
 *        .where("status = 'adoptable'")
 *        .whereOptional("state_code", filters.state_code)
 *        .whereOptional("breed_primary", filters.breed)
 *        .orderBy("intake_date", "asc")
 *        .limit(20)
 *        .offset(40);
 *
 * std::string query = builder.build();
 * auto params = builder.getParams();
 *
 * DEPENDENCIES:
 * - None (standalone utility)
 *
 * MODIFICATION GUIDE:
 * - To add new condition types, add a new whereXxx() method
 * - All parameters should be added to params_ and use $N placeholders
 * - Maintain the fluent interface by returning *this from methods
 *
 * @author Agent 7 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace wtl::core::services {

/**
 * @class SearchQueryBuilder
 * @brief Fluent SQL query builder with parameterized query support
 *
 * Builds SELECT queries safely using parameterized queries to prevent
 * SQL injection. Parameters are collected and returned for use with
 * pqxx::work::exec_params().
 *
 * PostgreSQL parameter placeholders ($1, $2, etc.) are automatically
 * generated based on parameter order.
 */
class SearchQueryBuilder {
public:
    /**
     * @brief Default constructor
     */
    SearchQueryBuilder() = default;

    // ========================================================================
    // FLUENT BUILDER METHODS
    // ========================================================================

    /**
     * @brief Set the SELECT columns
     *
     * @param columns Comma-separated list of columns to select
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.select("id, name, breed_primary");
     * builder.select("*");
     * builder.select("COUNT(*) as total");
     */
    SearchQueryBuilder& select(const std::string& columns);

    /**
     * @brief Set the FROM table
     *
     * @param table Table name (can include aliases and joins)
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.from("dogs");
     * builder.from("dogs d JOIN shelters s ON d.shelter_id = s.id");
     */
    SearchQueryBuilder& from(const std::string& table);

    /**
     * @brief Add a WHERE condition (always applied)
     *
     * @param condition SQL condition (without WHERE keyword)
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.where("status = 'adoptable'");
     * builder.where("is_available = true");
     */
    SearchQueryBuilder& where(const std::string& condition);

    /**
     * @brief Add a WHERE condition with parameter (always applied)
     *
     * @param column Column name
     * @param value Parameter value
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.whereEquals("state_code", "TX");
     */
    SearchQueryBuilder& whereEquals(const std::string& column, const std::string& value);

    /**
     * @brief Add an optional WHERE condition (only if value has value)
     *
     * @param column Column name
     * @param value Optional parameter value
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.whereOptional("state_code", filters.state_code);
     * // Only adds condition if state_code has a value
     */
    SearchQueryBuilder& whereOptional(const std::string& column,
                                       const std::optional<std::string>& value);

    /**
     * @brief Add an optional WHERE condition for integers
     *
     * @param column Column name
     * @param value Optional integer value
     * @return Reference to this builder for chaining
     */
    SearchQueryBuilder& whereOptionalInt(const std::string& column,
                                          const std::optional<int>& value);

    /**
     * @brief Add an optional WHERE condition for doubles
     *
     * @param column Column name
     * @param value Optional double value
     * @return Reference to this builder for chaining
     */
    SearchQueryBuilder& whereOptionalDouble(const std::string& column,
                                             const std::optional<double>& value);

    /**
     * @brief Add an optional WHERE condition for booleans
     *
     * @param column Column name (or tag name to check in tags array)
     * @param value Optional boolean value
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.whereOptionalBool("good_with_kids", filters.good_with_kids);
     * // Checks if tag exists in tags array
     */
    SearchQueryBuilder& whereOptionalBool(const std::string& column,
                                           const std::optional<bool>& value);

    /**
     * @brief Add a WHERE IN condition
     *
     * @param column Column name
     * @param values Vector of values for IN clause
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.whereIn("urgency_level", {"high", "critical"});
     */
    SearchQueryBuilder& whereIn(const std::string& column,
                                 const std::vector<std::string>& values);

    /**
     * @brief Add a WHERE LIKE condition
     *
     * @param column Column name
     * @param pattern LIKE pattern (without wildcards, they're added)
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.whereLike("name", "Max");
     * // Generates: name ILIKE '%Max%'
     */
    SearchQueryBuilder& whereLike(const std::string& column,
                                   const std::string& pattern);

    /**
     * @brief Add a WHERE condition for full-text search
     *
     * Uses PostgreSQL full-text search with ts_vector and ts_query.
     *
     * @param column Column name (or expression with tsvector)
     * @param query Search query string
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.whereFullText("search_vector", "labrador retriever");
     */
    SearchQueryBuilder& whereFullText(const std::string& column,
                                       const std::string& query);

    /**
     * @brief Add a WHERE condition for range (>=)
     *
     * @param column Column name
     * @param min_value Minimum value
     * @return Reference to this builder for chaining
     */
    SearchQueryBuilder& whereMin(const std::string& column,
                                  const std::optional<int>& min_value);

    /**
     * @brief Add a WHERE condition for range (<=)
     *
     * @param column Column name
     * @param max_value Maximum value
     * @return Reference to this builder for chaining
     */
    SearchQueryBuilder& whereMax(const std::string& column,
                                  const std::optional<int>& max_value);

    /**
     * @brief Add a WHERE condition for double range (>=)
     *
     * @param column Column name
     * @param min_value Minimum value
     * @return Reference to this builder for chaining
     */
    SearchQueryBuilder& whereMinDouble(const std::string& column,
                                        const std::optional<double>& min_value);

    /**
     * @brief Add a WHERE condition for double range (<=)
     *
     * @param column Column name
     * @param max_value Maximum value
     * @return Reference to this builder for chaining
     */
    SearchQueryBuilder& whereMaxDouble(const std::string& column,
                                        const std::optional<double>& max_value);

    /**
     * @brief Add ORDER BY clause
     *
     * @param column Column to sort by
     * @param direction Sort direction: "asc" or "desc"
     * @return Reference to this builder for chaining
     *
     * @example
     * builder.orderBy("intake_date", "asc");
     * builder.orderBy("urgency_level", "desc");
     */
    SearchQueryBuilder& orderBy(const std::string& column,
                                 const std::string& direction = "asc");

    /**
     * @brief Add LIMIT clause
     *
     * @param limit Maximum number of rows to return
     * @return Reference to this builder for chaining
     */
    SearchQueryBuilder& limit(int limit);

    /**
     * @brief Add OFFSET clause
     *
     * @param offset Number of rows to skip
     * @return Reference to this builder for chaining
     */
    SearchQueryBuilder& offset(int offset);

    /**
     * @brief Add GROUP BY clause
     *
     * @param columns Comma-separated list of columns to group by
     * @return Reference to this builder for chaining
     */
    SearchQueryBuilder& groupBy(const std::string& columns);

    // ========================================================================
    // BUILD METHODS
    // ========================================================================

    /**
     * @brief Build the complete SELECT query
     *
     * Combines all parts into a complete SQL query with
     * parameterized placeholders ($1, $2, etc.).
     *
     * @return Complete SQL query string
     */
    std::string build() const;

    /**
     * @brief Build a COUNT query with the same conditions
     *
     * Generates a SELECT COUNT(*) query with the same WHERE
     * conditions for pagination total count.
     *
     * @return COUNT query string
     */
    std::string buildCount() const;

    /**
     * @brief Get the parameter values in order
     *
     * Returns the parameters in the order they were added,
     * matching the $N placeholders in the query.
     *
     * @return Vector of parameter values as strings
     */
    std::vector<std::string> getParams() const;

    /**
     * @brief Get the number of parameters
     *
     * @return Number of parameters added
     */
    size_t getParamCount() const;

    /**
     * @brief Clear the builder for reuse
     *
     * Resets all fields to initial state.
     */
    void clear();

private:
    std::string select_;                   ///< SELECT columns
    std::string from_;                     ///< FROM table
    std::vector<std::string> conditions_;  ///< WHERE conditions
    std::vector<std::string> params_;      ///< Parameter values
    std::string order_by_;                 ///< ORDER BY clause
    std::string group_by_;                 ///< GROUP BY clause
    int limit_ = 0;                        ///< LIMIT value (0 = no limit)
    int offset_ = 0;                       ///< OFFSET value (0 = no offset)

    /**
     * @brief Get the next parameter placeholder ($N)
     *
     * @return Next placeholder string (e.g., "$1", "$2")
     */
    std::string nextPlaceholder() const;

    /**
     * @brief Add a parameter and return its placeholder
     *
     * @param value Parameter value
     * @return Placeholder string for the parameter
     */
    std::string addParam(const std::string& value);

    /**
     * @brief Sanitize column name to prevent injection
     *
     * Removes any potentially dangerous characters from column names.
     * Column names should only contain alphanumeric, underscore, and dot.
     *
     * @param column Column name to sanitize
     * @return Sanitized column name
     */
    static std::string sanitizeColumn(const std::string& column);

    /**
     * @brief Validate sort direction
     *
     * @param direction Direction string
     * @return "ASC" or "DESC"
     */
    static std::string validateDirection(const std::string& direction);
};

} // namespace wtl::core::services
