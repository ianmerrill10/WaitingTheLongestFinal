/**
 * @file SearchOptions.h
 * @brief Pagination and sorting options for search operations
 *
 * PURPOSE:
 * Defines the SearchOptions structure that encapsulates pagination
 * and sorting parameters for search queries. Includes validation
 * to ensure values are within acceptable ranges.
 *
 * USAGE:
 * // Create options from HTTP request parameters
 * auto options = SearchOptions::fromQueryParams(req);
 * options.validate();  // Clamp values to valid ranges
 *
 * // Use with search service
 * auto results = SearchService::getInstance().searchDogs(filters, options);
 *
 * DEPENDENCIES:
 * - Drogon HTTP framework (for request parsing)
 *
 * MODIFICATION GUIDE:
 * - To add new sort fields, update the validate() method's allowed_sorts list
 * - To change limits, modify max_per_page and validate() method
 *
 * @author Agent 7 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <algorithm>
#include <string>
#include <vector>

// Third-party includes
#include <drogon/drogon.h>

namespace wtl::core::services {

/**
 * @struct SearchOptions
 * @brief Container for pagination and sorting options
 *
 * Provides pagination (page, per_page) and sorting (sort_by, sort_order)
 * for search results. Values are validated and clamped to safe ranges.
 *
 * Allowed sort_by values:
 * - wait_time: Sort by time waiting for adoption
 * - urgency: Sort by urgency level (critical first)
 * - name: Sort alphabetically by dog name
 * - intake_date: Sort by date entered shelter
 * - created_at: Sort by record creation date
 *
 * Allowed sort_order values:
 * - asc: Ascending order
 * - desc: Descending order
 */
struct SearchOptions {
    int page = 1;                            ///< Current page number (1-based)
    int per_page = 20;                       ///< Results per page
    std::string sort_by = "wait_time";       ///< Field to sort by
    std::string sort_order = "desc";         ///< Sort direction: asc or desc

    static constexpr int max_per_page = 100; ///< Maximum allowed per_page value
    static constexpr int min_per_page = 1;   ///< Minimum allowed per_page value
    static constexpr int min_page = 1;       ///< Minimum page number

    /**
     * @brief Parse options from HTTP request query parameters
     *
     * Extracts pagination and sorting from URL query parameters.
     * Invalid values use defaults.
     *
     * Query parameter names:
     * - page: Page number (1-based, default: 1)
     * - per_page or limit: Results per page (default: 20, max: 100)
     * - sort_by or sort: Field to sort by (default: wait_time)
     * - sort_order or order: Sort direction (default: desc)
     *
     * @param req The HTTP request containing query parameters
     * @return SearchOptions populated from request parameters
     *
     * @example
     * // Request: GET /api/search/dogs?page=2&per_page=50&sort_by=name&sort_order=asc
     * auto options = SearchOptions::fromQueryParams(req);
     * // options.page == 2
     * // options.per_page == 50
     * // options.sort_by == "name"
     * // options.sort_order == "asc"
     */
    static SearchOptions fromQueryParams(const drogon::HttpRequestPtr& req);

    /**
     * @brief Validate and clamp values to valid ranges
     *
     * Ensures all values are within acceptable limits:
     * - page >= 1
     * - 1 <= per_page <= 100
     * - sort_by is in allowed list (defaults to wait_time)
     * - sort_order is asc or desc (defaults to desc)
     *
     * Call this after fromQueryParams() to ensure safe values.
     */
    void validate();

    /**
     * @brief Calculate SQL OFFSET from page and per_page
     *
     * @return Offset value for SQL LIMIT/OFFSET clause
     */
    int getOffset() const;

    /**
     * @brief Get the actual database column name for sorting
     *
     * Maps user-facing sort field names to database column names.
     *
     * @return Database column name for ORDER BY clause
     */
    std::string getSortColumn() const;

    /**
     * @brief Check if sort_by value is valid
     *
     * @param field The sort field to validate
     * @return true if valid, false otherwise
     */
    static bool isValidSortField(const std::string& field);

    /**
     * @brief Get list of allowed sort field names
     *
     * @return Vector of allowed sort field names
     */
    static std::vector<std::string> getAllowedSortFields();
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline SearchOptions SearchOptions::fromQueryParams(const drogon::HttpRequestPtr& req) {
    SearchOptions options;

    // Page number
    auto page_str = req->getParameter("page");
    if (!page_str.empty()) {
        try {
            options.page = std::stoi(page_str);
        } catch (...) {
            // Invalid value, use default
        }
    }

    // Results per page (support both 'per_page' and 'limit')
    auto per_page_str = req->getParameter("per_page");
    if (per_page_str.empty()) {
        per_page_str = req->getParameter("limit");
    }
    if (!per_page_str.empty()) {
        try {
            options.per_page = std::stoi(per_page_str);
        } catch (...) {
            // Invalid value, use default
        }
    }

    // Sort field (support both 'sort_by' and 'sort')
    auto sort_by = req->getParameter("sort_by");
    if (sort_by.empty()) {
        sort_by = req->getParameter("sort");
    }
    if (!sort_by.empty()) {
        options.sort_by = sort_by;
    }

    // Sort order (support both 'sort_order' and 'order')
    auto sort_order = req->getParameter("sort_order");
    if (sort_order.empty()) {
        sort_order = req->getParameter("order");
    }
    if (!sort_order.empty()) {
        options.sort_order = sort_order;
    }

    return options;
}

inline void SearchOptions::validate() {
    // Clamp page to minimum
    if (page < min_page) {
        page = min_page;
    }

    // Clamp per_page to valid range
    if (per_page < min_per_page) {
        per_page = min_per_page;
    }
    if (per_page > max_per_page) {
        per_page = max_per_page;
    }

    // Validate sort_by field
    if (!isValidSortField(sort_by)) {
        sort_by = "wait_time";  // Default to wait_time
    }

    // Validate sort_order
    if (sort_order != "asc" && sort_order != "desc") {
        sort_order = "desc";  // Default to descending
    }
}

inline int SearchOptions::getOffset() const {
    // Calculate offset for SQL query (0-based)
    return (page - 1) * per_page;
}

inline std::string SearchOptions::getSortColumn() const {
    // Map user-facing names to database column names
    if (sort_by == "wait_time") {
        return "intake_date";  // Older intake = longer wait
    }
    if (sort_by == "urgency") {
        return "urgency_level";
    }
    if (sort_by == "name") {
        return "name";
    }
    if (sort_by == "intake_date") {
        return "intake_date";
    }
    if (sort_by == "created_at") {
        return "created_at";
    }

    // Default
    return "intake_date";
}

inline bool SearchOptions::isValidSortField(const std::string& field) {
    auto allowed = getAllowedSortFields();
    return std::find(allowed.begin(), allowed.end(), field) != allowed.end();
}

inline std::vector<std::string> SearchOptions::getAllowedSortFields() {
    return {"wait_time", "urgency", "name", "intake_date", "created_at"};
}

} // namespace wtl::core::services
