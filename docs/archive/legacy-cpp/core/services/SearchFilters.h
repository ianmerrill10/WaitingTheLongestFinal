/**
 * @file SearchFilters.h
 * @brief Filter criteria for dog search operations
 *
 * PURPOSE:
 * Defines the SearchFilters structure that encapsulates all possible
 * filter criteria for searching dogs. Supports filtering by location,
 * breed, size, age, temperament, urgency, and full-text search.
 *
 * USAGE:
 * // Create filters from HTTP request parameters
 * auto filters = SearchFilters::fromQueryParams(req);
 *
 * // Check if any filters are applied
 * if (!filters.isEmpty()) {
 *     auto results = SearchService::getInstance().searchDogs(filters, options);
 * }
 *
 * DEPENDENCIES:
 * - Drogon HTTP framework (for request parsing)
 *
 * MODIFICATION GUIDE:
 * - To add new filter fields, add the optional member and update
 *   fromQueryParams() and isEmpty() methods
 * - Ensure corresponding SQL conditions are added in SearchQueryBuilder
 *
 * @author Agent 7 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <optional>
#include <string>

// Third-party includes
#include <drogon/drogon.h>

namespace wtl::core::services {

/**
 * @struct SearchFilters
 * @brief Container for all search filter criteria
 *
 * All fields are optional to allow flexible search combinations.
 * Empty optionals are ignored when building SQL queries.
 *
 * Size values: small, medium, large, xlarge
 * Age category values: puppy, young, adult, senior
 * Gender values: male, female
 * Urgency level values: normal, medium, high, critical
 */
struct SearchFilters {
    // Location filters
    std::optional<std::string> state_code;     ///< Two-letter state code (TX, CA, etc.)
    std::optional<std::string> shelter_id;     ///< Specific shelter UUID

    // Basic characteristics
    std::optional<std::string> breed;          ///< Primary or secondary breed match
    std::optional<std::string> size;           ///< Size category: small, medium, large, xlarge
    std::optional<std::string> age_category;   ///< Age category: puppy, young, adult, senior
    std::optional<std::string> gender;         ///< Gender: male, female
    std::optional<std::string> color;          ///< Primary or secondary color

    // Temperament/compatibility filters
    std::optional<bool> good_with_kids;        ///< Safe around children
    std::optional<bool> good_with_dogs;        ///< Good with other dogs
    std::optional<bool> good_with_cats;        ///< Good with cats
    std::optional<bool> house_trained;         ///< House/potty trained

    // Urgency filter
    std::optional<std::string> urgency_level;  ///< Urgency: normal, medium, high, critical

    // Range filters
    std::optional<int> min_age_months;         ///< Minimum age in months
    std::optional<int> max_age_months;         ///< Maximum age in months
    std::optional<double> min_weight;          ///< Minimum weight in lbs
    std::optional<double> max_weight;          ///< Maximum weight in lbs

    // Full-text search
    std::optional<std::string> query;          ///< Full-text search query string

    /**
     * @brief Parse filters from HTTP request query parameters
     *
     * Extracts filter values from URL query parameters. Invalid or
     * empty values result in empty optionals (filter not applied).
     *
     * Query parameter names:
     * - state, shelter_id, breed, size, age_category, gender, color
     * - good_with_kids, good_with_dogs, good_with_cats, house_trained (true/false)
     * - urgency_level
     * - min_age, max_age (months)
     * - min_weight, max_weight (lbs)
     * - q or query (full-text search)
     *
     * @param req The HTTP request containing query parameters
     * @return SearchFilters populated from request parameters
     *
     * @example
     * // Request: GET /api/search/dogs?state=TX&breed=labrador&size=large&good_with_kids=true
     * auto filters = SearchFilters::fromQueryParams(req);
     * // filters.state_code == "TX"
     * // filters.breed == "labrador"
     * // filters.size == "large"
     * // filters.good_with_kids == true
     */
    static SearchFilters fromQueryParams(const drogon::HttpRequestPtr& req);

    /**
     * @brief Check if no filters are applied
     *
     * Returns true if all filter fields are empty (std::nullopt).
     * Useful to determine if a full table scan is needed.
     *
     * @return true if all fields are empty, false otherwise
     */
    bool isEmpty() const;

    /**
     * @brief Validate filter values
     *
     * Checks that filter values are within allowed ranges and formats.
     * Returns empty string if valid, or error message if invalid.
     *
     * @return Empty string if valid, error message if invalid
     */
    std::string validate() const;
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline SearchFilters SearchFilters::fromQueryParams(const drogon::HttpRequestPtr& req) {
    SearchFilters filters;

    // Location filters
    auto state = req->getParameter("state");
    if (!state.empty()) {
        filters.state_code = state;
    }

    auto shelter = req->getParameter("shelter_id");
    if (!shelter.empty()) {
        filters.shelter_id = shelter;
    }

    // Basic characteristics
    auto breed = req->getParameter("breed");
    if (!breed.empty()) {
        filters.breed = breed;
    }

    auto size = req->getParameter("size");
    if (!size.empty()) {
        filters.size = size;
    }

    auto age_category = req->getParameter("age_category");
    if (!age_category.empty()) {
        filters.age_category = age_category;
    }

    auto gender = req->getParameter("gender");
    if (!gender.empty()) {
        filters.gender = gender;
    }

    auto color = req->getParameter("color");
    if (!color.empty()) {
        filters.color = color;
    }

    // Temperament filters (boolean)
    auto good_with_kids = req->getParameter("good_with_kids");
    if (!good_with_kids.empty()) {
        filters.good_with_kids = (good_with_kids == "true" || good_with_kids == "1");
    }

    auto good_with_dogs = req->getParameter("good_with_dogs");
    if (!good_with_dogs.empty()) {
        filters.good_with_dogs = (good_with_dogs == "true" || good_with_dogs == "1");
    }

    auto good_with_cats = req->getParameter("good_with_cats");
    if (!good_with_cats.empty()) {
        filters.good_with_cats = (good_with_cats == "true" || good_with_cats == "1");
    }

    auto house_trained = req->getParameter("house_trained");
    if (!house_trained.empty()) {
        filters.house_trained = (house_trained == "true" || house_trained == "1");
    }

    // Urgency filter
    auto urgency = req->getParameter("urgency_level");
    if (!urgency.empty()) {
        filters.urgency_level = urgency;
    }

    // Range filters
    auto min_age = req->getParameter("min_age");
    if (!min_age.empty()) {
        try {
            filters.min_age_months = std::stoi(min_age);
        } catch (...) {
            // Invalid value, leave as nullopt
        }
    }

    auto max_age = req->getParameter("max_age");
    if (!max_age.empty()) {
        try {
            filters.max_age_months = std::stoi(max_age);
        } catch (...) {
            // Invalid value, leave as nullopt
        }
    }

    auto min_weight = req->getParameter("min_weight");
    if (!min_weight.empty()) {
        try {
            filters.min_weight = std::stod(min_weight);
        } catch (...) {
            // Invalid value, leave as nullopt
        }
    }

    auto max_weight = req->getParameter("max_weight");
    if (!max_weight.empty()) {
        try {
            filters.max_weight = std::stod(max_weight);
        } catch (...) {
            // Invalid value, leave as nullopt
        }
    }

    // Full-text search (support both 'q' and 'query' parameters)
    auto query = req->getParameter("q");
    if (query.empty()) {
        query = req->getParameter("query");
    }
    if (!query.empty()) {
        filters.query = query;
    }

    return filters;
}

inline bool SearchFilters::isEmpty() const {
    return !state_code.has_value() &&
           !shelter_id.has_value() &&
           !breed.has_value() &&
           !size.has_value() &&
           !age_category.has_value() &&
           !gender.has_value() &&
           !color.has_value() &&
           !good_with_kids.has_value() &&
           !good_with_dogs.has_value() &&
           !good_with_cats.has_value() &&
           !house_trained.has_value() &&
           !urgency_level.has_value() &&
           !min_age_months.has_value() &&
           !max_age_months.has_value() &&
           !min_weight.has_value() &&
           !max_weight.has_value() &&
           !query.has_value();
}

inline std::string SearchFilters::validate() const {
    // Validate size values
    if (size.has_value()) {
        const auto& s = size.value();
        if (s != "small" && s != "medium" && s != "large" && s != "xlarge") {
            return "Invalid size value. Must be: small, medium, large, or xlarge";
        }
    }

    // Validate age_category values
    if (age_category.has_value()) {
        const auto& ac = age_category.value();
        if (ac != "puppy" && ac != "young" && ac != "adult" && ac != "senior") {
            return "Invalid age_category value. Must be: puppy, young, adult, or senior";
        }
    }

    // Validate gender values
    if (gender.has_value()) {
        const auto& g = gender.value();
        if (g != "male" && g != "female") {
            return "Invalid gender value. Must be: male or female";
        }
    }

    // Validate urgency_level values
    if (urgency_level.has_value()) {
        const auto& ul = urgency_level.value();
        if (ul != "normal" && ul != "medium" && ul != "high" && ul != "critical") {
            return "Invalid urgency_level value. Must be: normal, medium, high, or critical";
        }
    }

    // Validate age range
    if (min_age_months.has_value() && min_age_months.value() < 0) {
        return "min_age must be non-negative";
    }
    if (max_age_months.has_value() && max_age_months.value() < 0) {
        return "max_age must be non-negative";
    }
    if (min_age_months.has_value() && max_age_months.has_value() &&
        min_age_months.value() > max_age_months.value()) {
        return "min_age cannot be greater than max_age";
    }

    // Validate weight range
    if (min_weight.has_value() && min_weight.value() < 0) {
        return "min_weight must be non-negative";
    }
    if (max_weight.has_value() && max_weight.value() < 0) {
        return "max_weight must be non-negative";
    }
    if (min_weight.has_value() && max_weight.has_value() &&
        min_weight.value() > max_weight.value()) {
        return "min_weight cannot be greater than max_weight";
    }

    return "";  // Valid
}

} // namespace wtl::core::services
