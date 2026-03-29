/**
 * @file BlogService.h
 * @brief CRUD service for blog posts
 *
 * PURPOSE:
 * Provides database CRUD operations for blog posts including
 * search, filtering, pagination, and view tracking.
 *
 * USAGE:
 * auto& service = BlogService::getInstance();
 * auto post = service.findById(id);
 * auto posts = service.findByCategory(BlogCategory::URGENT);
 * service.incrementViewCount(post_id);
 *
 * DEPENDENCIES:
 * - ConnectionPool (database access)
 * - ErrorCapture (error logging)
 * - SelfHealing (retry logic)
 * - BlogPost model
 *
 * MODIFICATION GUIDE:
 * - Add new query methods following findById pattern
 * - All database operations use connection pool
 * - All errors use WTL_CAPTURE_ERROR macro
 *
 * @author Agent 12 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "content/blog/BlogPost.h"
#include "content/blog/BlogCategory.h"
#include "content/blog/EducationalContent.h"

namespace wtl::content::blog {

/**
 * @struct BlogSearchFilters
 * @brief Filters for blog post searches
 */
struct BlogSearchFilters {
    std::optional<BlogCategory> category;        ///< Filter by category
    std::optional<std::string> dog_id;           ///< Filter by featured dog
    std::optional<std::string> shelter_id;       ///< Filter by shelter
    std::optional<std::string> tag;              ///< Filter by tag
    std::optional<std::string> query;            ///< Full-text search query
    std::optional<PostStatus> status;            ///< Filter by status
    bool only_published = true;                  ///< Only show published posts

    /**
     * @brief Create filters from HTTP query parameters
     */
    static BlogSearchFilters fromQueryParams(const std::map<std::string, std::string>& params);
};

/**
 * @struct BlogSearchOptions
 * @brief Pagination and sorting options
 */
struct BlogSearchOptions {
    int page = 1;                                ///< Page number (1-indexed)
    int per_page = 20;                           ///< Items per page
    std::string sort_by = "published_at";        ///< Sort field
    std::string sort_order = "DESC";             ///< ASC or DESC

    /**
     * @brief Get offset for pagination
     */
    int getOffset() const { return (page - 1) * per_page; }

    /**
     * @brief Validate and clamp values
     */
    void validate() {
        if (page < 1) page = 1;
        if (per_page < 1) per_page = 1;
        if (per_page > 100) per_page = 100;

        // Allowed sort fields
        if (sort_by != "published_at" && sort_by != "created_at" &&
            sort_by != "view_count" && sort_by != "title") {
            sort_by = "published_at";
        }

        if (sort_order != "ASC" && sort_order != "DESC") {
            sort_order = "DESC";
        }
    }
};

/**
 * @struct BlogSearchResult
 * @brief Paginated search result
 */
struct BlogSearchResult {
    std::vector<BlogPost> posts;                 ///< Posts on current page
    int total;                                   ///< Total matching posts
    int page;                                    ///< Current page
    int per_page;                                ///< Items per page
    int total_pages;                             ///< Total number of pages

    /**
     * @brief Convert to JSON
     */
    Json::Value toJson() const;
};

/**
 * @class BlogService
 * @brief Singleton service for blog post CRUD operations
 */
class BlogService {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get singleton instance
     */
    static BlogService& getInstance();

    // Prevent copying
    BlogService(const BlogService&) = delete;
    BlogService& operator=(const BlogService&) = delete;

    // ========================================================================
    // CRUD OPERATIONS
    // ========================================================================

    /**
     * @brief Find post by ID
     * @param id Post UUID
     * @return Post if found
     */
    std::optional<BlogPost> findById(const std::string& id);

    /**
     * @brief Find post by slug
     * @param slug URL-friendly slug
     * @return Post if found
     */
    std::optional<BlogPost> findBySlug(const std::string& slug);

    /**
     * @brief Search posts with filters and pagination
     * @param filters Search filters
     * @param options Pagination options
     * @return Paginated results
     */
    BlogSearchResult search(
        const BlogSearchFilters& filters,
        const BlogSearchOptions& options
    );

    /**
     * @brief Find posts by category
     * @param category Blog category
     * @param limit Maximum results
     * @param offset Skip count
     * @return Matching posts
     */
    std::vector<BlogPost> findByCategory(
        BlogCategory category,
        int limit = 20,
        int offset = 0
    );

    /**
     * @brief Find posts for a dog
     * @param dog_id Dog UUID
     * @param limit Maximum results
     * @return Posts featuring the dog
     */
    std::vector<BlogPost> findByDogId(
        const std::string& dog_id,
        int limit = 10
    );

    /**
     * @brief Find posts for a shelter
     * @param shelter_id Shelter UUID
     * @param limit Maximum results
     * @return Posts related to shelter
     */
    std::vector<BlogPost> findByShelterId(
        const std::string& shelter_id,
        int limit = 10
    );

    /**
     * @brief Find posts by tag
     * @param tag Tag to search
     * @param limit Maximum results
     * @return Posts with tag
     */
    std::vector<BlogPost> findByTag(
        const std::string& tag,
        int limit = 20
    );

    /**
     * @brief Get recently published posts
     * @param limit Maximum results
     * @return Recent posts
     */
    std::vector<BlogPost> getRecentPosts(int limit = 10);

    /**
     * @brief Get most viewed posts
     * @param limit Maximum results
     * @param days_back Only consider posts from last N days
     * @return Popular posts
     */
    std::vector<BlogPost> getPopularPosts(int limit = 10, int days_back = 30);

    /**
     * @brief Get related posts for a post
     * @param post_id Post UUID
     * @param limit Maximum results
     * @return Related posts
     */
    std::vector<BlogPost> getRelatedPosts(
        const std::string& post_id,
        int limit = 5
    );

    /**
     * @brief Save a new post
     * @param post Post to save
     * @return Saved post with generated ID
     */
    BlogPost save(const BlogPost& post);

    /**
     * @brief Update an existing post
     * @param post Post with updated fields
     * @return Updated post
     */
    BlogPost update(const BlogPost& post);

    /**
     * @brief Delete a post
     * @param id Post UUID
     * @return True if deleted
     */
    bool deleteById(const std::string& id);

    // ========================================================================
    // STATUS OPERATIONS
    // ========================================================================

    /**
     * @brief Publish a post
     * @param id Post UUID
     * @return True if published
     */
    bool publish(const std::string& id);

    /**
     * @brief Unpublish a post (return to draft)
     * @param id Post UUID
     * @return True if unpublished
     */
    bool unpublish(const std::string& id);

    /**
     * @brief Schedule a post for future publication
     * @param id Post UUID
     * @param scheduled_at Scheduled time
     * @return True if scheduled
     */
    bool schedule(
        const std::string& id,
        const std::chrono::system_clock::time_point& scheduled_at
    );

    /**
     * @brief Archive a post
     * @param id Post UUID
     * @return True if archived
     */
    bool archive(const std::string& id);

    // ========================================================================
    // ENGAGEMENT TRACKING
    // ========================================================================

    /**
     * @brief Increment view count
     * @param id Post UUID
     */
    void incrementViewCount(const std::string& id);

    /**
     * @brief Increment share count
     * @param id Post UUID
     * @param platform Social platform name
     */
    void incrementShareCount(const std::string& id, const std::string& platform = "");

    /**
     * @brief Increment like count
     * @param id Post UUID
     */
    void incrementLikeCount(const std::string& id);

    // ========================================================================
    // EDUCATIONAL CONTENT OPERATIONS
    // ========================================================================

    /**
     * @brief Get educational content by ID
     */
    std::optional<EducationalContent> getEducationalById(const std::string& id);

    /**
     * @brief Get educational content by topic
     */
    std::vector<EducationalContent> getEducationalByTopic(
        EducationalTopic topic,
        int limit = 20
    );

    /**
     * @brief Get educational content by life stage
     */
    std::vector<EducationalContent> getEducationalByLifeStage(
        LifeStage stage,
        int limit = 20
    );

    /**
     * @brief Search educational content
     */
    std::vector<EducationalContent> searchEducational(
        const std::string& query,
        int limit = 20
    );

    /**
     * @brief Get featured educational content
     */
    std::vector<EducationalContent> getFeaturedEducational(int limit = 10);

    /**
     * @brief Record helpful/not helpful vote
     */
    void voteEducationalHelpful(const std::string& id, bool helpful);

    // ========================================================================
    // STATISTICS
    // ========================================================================

    /**
     * @brief Get post count by category
     */
    std::map<BlogCategory, int> getPostCountByCategory();

    /**
     * @brief Get total published post count
     */
    int getTotalPublishedCount();

    /**
     * @brief Get total view count
     */
    int getTotalViewCount();

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    BlogService();
    ~BlogService();

    // ========================================================================
    // PRIVATE HELPERS
    // ========================================================================

    /**
     * @brief Build SQL WHERE clause from filters
     */
    std::string buildWhereClause(
        const BlogSearchFilters& filters,
        std::vector<std::string>& params
    );

    /**
     * @brief Execute query with self-healing
     */
    template<typename T>
    T executeWithHealing(
        const std::string& operation_name,
        std::function<T()> operation
    );

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    mutable std::mutex mutex_;
};

} // namespace wtl::content::blog
