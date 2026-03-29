/**
 * @file SearchResult.h
 * @brief Paginated search result container template
 *
 * PURPOSE:
 * Defines a generic SearchResult template that wraps search results
 * with pagination metadata. Supports JSON serialization for API responses.
 *
 * USAGE:
 * // Create a search result
 * SearchResult<Dog> result;
 * result.items = dogs;
 * result.total = 150;
 * result.page = 1;
 * result.per_page = 20;
 * result.total_pages = 8;
 *
 * // Convert to JSON for API response
 * Json::Value json = result.toJson([](const Dog& dog) {
 *     return dog.toJson();
 * });
 *
 * DEPENDENCIES:
 * - jsoncpp for JSON serialization
 *
 * MODIFICATION GUIDE:
 * - This is a generic template; no modification should be needed
 * - To add metadata fields, add to the toJson() output
 *
 * @author Agent 7 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <cmath>
#include <functional>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::core::services {

/**
 * @class SearchResult
 * @brief Generic container for paginated search results
 *
 * @tparam T The type of items in the result (e.g., Dog, Shelter)
 *
 * Wraps a vector of items with pagination metadata including
 * total count, current page, items per page, and total pages.
 */
template<typename T>
struct SearchResult {
    std::vector<T> items;    ///< The items on the current page
    int total = 0;           ///< Total number of matching items across all pages
    int page = 1;            ///< Current page number (1-based)
    int per_page = 20;       ///< Number of items per page
    int total_pages = 0;     ///< Total number of pages

    /**
     * @brief Default constructor
     */
    SearchResult() = default;

    /**
     * @brief Constructor with all fields
     *
     * @param items Vector of items for current page
     * @param total Total count across all pages
     * @param page Current page number (1-based)
     * @param per_page Items per page
     */
    SearchResult(std::vector<T> items, int total, int page, int per_page)
        : items(std::move(items))
        , total(total)
        , page(page)
        , per_page(per_page)
        , total_pages(calculateTotalPages(total, per_page)) {}

    /**
     * @brief Convert to JSON with custom item serializer
     *
     * Creates a JSON object containing the items array and pagination
     * metadata. Uses the provided function to serialize each item.
     *
     * Output format:
     * {
     *     "items": [ ... ],
     *     "pagination": {
     *         "total": 150,
     *         "page": 1,
     *         "per_page": 20,
     *         "total_pages": 8,
     *         "has_next": true,
     *         "has_prev": false
     *     }
     * }
     *
     * @param itemToJson Function to convert each item to Json::Value
     * @return Json::Value containing the complete result
     *
     * @example
     * auto json = result.toJson([](const Dog& dog) {
     *     return dog.toJson();
     * });
     */
    Json::Value toJson(std::function<Json::Value(const T&)> itemToJson) const {
        Json::Value result;

        // Items array
        Json::Value items_json(Json::arrayValue);
        for (const auto& item : items) {
            items_json.append(itemToJson(item));
        }
        result["items"] = items_json;

        // Pagination metadata
        Json::Value pagination;
        pagination["total"] = total;
        pagination["page"] = page;
        pagination["per_page"] = per_page;
        pagination["total_pages"] = total_pages;
        pagination["has_next"] = hasNextPage();
        pagination["has_prev"] = hasPrevPage();
        result["pagination"] = pagination;

        return result;
    }

    /**
     * @brief Convert to JSON using the item's toJson() method
     *
     * Convenience overload that assumes T has a toJson() method.
     * Falls back to toJson(itemToJson) with T::toJson as the converter.
     *
     * @return Json::Value containing the complete result
     */
    Json::Value toJson() const {
        return toJson([](const T& item) { return item.toJson(); });
    }

    /**
     * @brief Check if there is a next page
     *
     * @return true if current page is not the last page
     */
    bool hasNextPage() const {
        return page < total_pages;
    }

    /**
     * @brief Check if there is a previous page
     *
     * @return true if current page is not the first page
     */
    bool hasPrevPage() const {
        return page > 1;
    }

    /**
     * @brief Get the count of items on current page
     *
     * @return Number of items in the items vector
     */
    size_t count() const {
        return items.size();
    }

    /**
     * @brief Check if result is empty
     *
     * @return true if no items in result
     */
    bool empty() const {
        return items.empty();
    }

    /**
     * @brief Calculate total pages from total count and per_page
     *
     * @param total Total count of items
     * @param per_page Items per page
     * @return Number of pages needed
     */
    static int calculateTotalPages(int total, int per_page) {
        if (per_page <= 0) return 0;
        if (total <= 0) return 0;
        return static_cast<int>(std::ceil(static_cast<double>(total) / per_page));
    }

    /**
     * @brief Create an empty result
     *
     * Convenience factory for creating an empty result with specified
     * pagination parameters.
     *
     * @param page Page number
     * @param per_page Items per page
     * @return Empty SearchResult with pagination set
     */
    static SearchResult<T> empty(int page = 1, int per_page = 20) {
        SearchResult<T> result;
        result.items = {};
        result.total = 0;
        result.page = page;
        result.per_page = per_page;
        result.total_pages = 0;
        return result;
    }

    /**
     * @brief Update pagination metadata based on total count
     *
     * Recalculates total_pages based on current total and per_page values.
     * Call after setting total to ensure consistent metadata.
     */
    void updatePagination() {
        total_pages = calculateTotalPages(total, per_page);
    }
};

/**
 * @brief Convenience type alias for dog search results
 *
 * Note: Requires Dog model to be included where used.
 * This forward declaration allows the alias without including Dog.h here.
 */
// Forward declare Dog model - actual definition comes from Agent 3
namespace wtl::core::models {
    struct Dog;
}

} // namespace wtl::core::services
