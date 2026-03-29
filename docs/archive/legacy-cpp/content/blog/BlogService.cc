/**
 * @file BlogService.cc
 * @brief Implementation of BlogService
 * @see BlogService.h for documentation
 */

#include "content/blog/BlogService.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::content::blog {

// ============================================================================
// BLOG SEARCH FILTERS
// ============================================================================

BlogSearchFilters BlogSearchFilters::fromQueryParams(
    const std::map<std::string, std::string>& params
) {
    BlogSearchFilters filters;

    auto it = params.find("category");
    if (it != params.end() && !it->second.empty()) {
        try {
            filters.category = blogCategoryFromString(it->second);
        } catch (...) {
            // Invalid category, ignore
        }
    }

    it = params.find("dog_id");
    if (it != params.end() && !it->second.empty()) {
        filters.dog_id = it->second;
    }

    it = params.find("shelter_id");
    if (it != params.end() && !it->second.empty()) {
        filters.shelter_id = it->second;
    }

    it = params.find("tag");
    if (it != params.end() && !it->second.empty()) {
        filters.tag = it->second;
    }

    it = params.find("query");
    if (it != params.end() && !it->second.empty()) {
        filters.query = it->second;
    }

    it = params.find("status");
    if (it != params.end() && !it->second.empty()) {
        filters.status = postStatusFromString(it->second);
        filters.only_published = false;  // If status specified, don't filter to published
    }

    return filters;
}

// ============================================================================
// BLOG SEARCH RESULT
// ============================================================================

Json::Value BlogSearchResult::toJson() const {
    Json::Value json;

    Json::Value posts_array(Json::arrayValue);
    for (const auto& post : posts) {
        posts_array.append(post.toListJson());
    }
    json["posts"] = posts_array;

    Json::Value meta;
    meta["total"] = total;
    meta["page"] = page;
    meta["per_page"] = per_page;
    meta["total_pages"] = total_pages;
    json["meta"] = meta;

    return json;
}

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

BlogService& BlogService::getInstance() {
    static BlogService instance;
    return instance;
}

BlogService::BlogService() = default;
BlogService::~BlogService() = default;

// ============================================================================
// CRUD OPERATIONS
// ============================================================================

std::optional<BlogPost> BlogService::findById(const std::string& id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE id = $1",
            id
        );

        std::optional<BlogPost> result;
        if (!res.empty()) {
            result = BlogPost::fromDbRow(res[0]);
        }

        txn.commit();
        pool.release(conn);

        return result;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find blog post by ID: " + std::string(e.what()),
            {{"post_id", id}}
        );
        return std::nullopt;
    }
}

std::optional<BlogPost> BlogService::findBySlug(const std::string& slug) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE slug = $1",
            slug
        );

        std::optional<BlogPost> result;
        if (!res.empty()) {
            result = BlogPost::fromDbRow(res[0]);
        }

        txn.commit();
        pool.release(conn);

        return result;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find blog post by slug: " + std::string(e.what()),
            {{"slug", slug}}
        );
        return std::nullopt;
    }
}

BlogSearchResult BlogService::search(
    const BlogSearchFilters& filters,
    const BlogSearchOptions& options
) {
    BlogSearchResult result;
    result.page = options.page;
    result.per_page = options.per_page;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        // Build WHERE clause using txn.quote() for safe value escaping
        std::vector<std::string> conditions;

        if (filters.only_published) {
            conditions.push_back("status = 'published'");
        } else if (filters.status) {
            conditions.push_back("status = " + txn.quote(postStatusToString(*filters.status)));
        }

        if (filters.category) {
            conditions.push_back("category = " + txn.quote(blogCategoryToString(*filters.category)));
        }

        if (filters.dog_id) {
            conditions.push_back("dog_id = " + txn.quote(*filters.dog_id));
        }

        if (filters.shelter_id) {
            conditions.push_back("shelter_id = " + txn.quote(*filters.shelter_id));
        }

        if (filters.tag) {
            conditions.push_back(txn.quote(*filters.tag) + " = ANY(tags)");
        }

        if (filters.query) {
            std::string quoted_pattern = txn.quote("%" + *filters.query + "%");
            conditions.push_back("(title ILIKE " + quoted_pattern +
                " OR content ILIKE " + quoted_pattern +
                " OR excerpt ILIKE " + quoted_pattern + ")");
        }

        std::string where_clause;
        if (!conditions.empty()) {
            where_clause = "WHERE ";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i > 0) where_clause += " AND ";
                where_clause += conditions[i];
            }
        }

        // Count query
        std::string count_query = "SELECT COUNT(*) FROM blog_posts " + where_clause;
        auto count_res = txn.exec(count_query);

        result.total = count_res[0][0].as<int>();
        result.total_pages = (result.total + result.per_page - 1) / result.per_page;

        // Data query
        std::string data_query = "SELECT * FROM blog_posts " + where_clause +
            " ORDER BY " + options.sort_by + " " + options.sort_order +
            " LIMIT " + std::to_string(options.per_page) +
            " OFFSET " + std::to_string(options.getOffset());

        auto data_res = txn.exec(data_query);

        for (const auto& row : data_res) {
            result.posts.push_back(BlogPost::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to search blog posts: " + std::string(e.what()),
            {}
        );
    }

    return result;
}

std::vector<BlogPost> BlogService::findByCategory(
    BlogCategory category,
    int limit,
    int offset
) {
    std::vector<BlogPost> posts;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE status = 'published' AND category = $1 "
            "ORDER BY published_at DESC LIMIT $2 OFFSET $3",
            blogCategoryToString(category), limit, offset
        );

        for (const auto& row : res) {
            posts.push_back(BlogPost::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find posts by category: " + std::string(e.what()),
            {{"category", blogCategoryToString(category)}}
        );
    }

    return posts;
}

std::vector<BlogPost> BlogService::findByDogId(
    const std::string& dog_id,
    int limit
) {
    std::vector<BlogPost> posts;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE status = 'published' AND dog_id = $1 "
            "ORDER BY published_at DESC LIMIT $2",
            dog_id, limit
        );

        for (const auto& row : res) {
            posts.push_back(BlogPost::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find posts by dog ID: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return posts;
}

std::vector<BlogPost> BlogService::findByShelterId(
    const std::string& shelter_id,
    int limit
) {
    std::vector<BlogPost> posts;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE status = 'published' AND shelter_id = $1 "
            "ORDER BY published_at DESC LIMIT $2",
            shelter_id, limit
        );

        for (const auto& row : res) {
            posts.push_back(BlogPost::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find posts by shelter ID: " + std::string(e.what()),
            {{"shelter_id", shelter_id}}
        );
    }

    return posts;
}

std::vector<BlogPost> BlogService::findByTag(
    const std::string& tag,
    int limit
) {
    std::vector<BlogPost> posts;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE status = 'published' AND $1 = ANY(tags) "
            "ORDER BY published_at DESC LIMIT $2",
            tag, limit
        );

        for (const auto& row : res) {
            posts.push_back(BlogPost::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to find posts by tag: " + std::string(e.what()),
            {{"tag", tag}}
        );
    }

    return posts;
}

std::vector<BlogPost> BlogService::getRecentPosts(int limit) {
    std::vector<BlogPost> posts;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE status = 'published' "
            "ORDER BY published_at DESC LIMIT $1",
            limit
        );

        for (const auto& row : res) {
            posts.push_back(BlogPost::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get recent posts: " + std::string(e.what()),
            {}
        );
    }

    return posts;
}

std::vector<BlogPost> BlogService::getPopularPosts(int limit, int days_back) {
    std::vector<BlogPost> posts;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE status = 'published' "
            "AND published_at > NOW() - INTERVAL '$1 days' "
            "ORDER BY view_count DESC LIMIT $2",
            days_back, limit
        );

        for (const auto& row : res) {
            posts.push_back(BlogPost::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get popular posts: " + std::string(e.what()),
            {}
        );
    }

    return posts;
}

std::vector<BlogPost> BlogService::getRelatedPosts(
    const std::string& post_id,
    int limit
) {
    std::vector<BlogPost> posts;

    try {
        // Get the source post
        auto source = findById(post_id);
        if (!source) return posts;

        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        // Find posts with same category or same dog
        auto res = txn.exec_params(
            "SELECT * FROM blog_posts WHERE status = 'published' AND id != $1 "
            "AND (category = $2 OR dog_id = $3) "
            "ORDER BY published_at DESC LIMIT $4",
            post_id,
            blogCategoryToString(source->category),
            source->dog_id.value_or(""),
            limit
        );

        for (const auto& row : res) {
            posts.push_back(BlogPost::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get related posts: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
    }

    return posts;
}

BlogPost BlogService::save(const BlogPost& post) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        // Generate UUID
        auto id_res = txn.exec("SELECT gen_random_uuid()::text");
        std::string post_id = id_res[0][0].as<std::string>();

        // Format timestamps
        auto now = std::chrono::system_clock::now();
        std::time_t now_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        gmtime_s(&tm, &now_t);
#else
        gmtime_r(&now_t, &tm);
#endif
        std::ostringstream now_ss;
        now_ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        std::string now_str = now_ss.str();

        // Format tags array
        std::ostringstream tags_ss;
        tags_ss << "{";
        for (size_t i = 0; i < post.tags.size(); ++i) {
            if (i > 0) tags_ss << ",";
            tags_ss << "\"" << post.tags[i] << "\"";
        }
        tags_ss << "}";

        // Format gallery images array
        std::ostringstream gallery_ss;
        gallery_ss << "{";
        for (size_t i = 0; i < post.gallery_images.size(); ++i) {
            if (i > 0) gallery_ss << ",";
            gallery_ss << "\"" << post.gallery_images[i] << "\"";
        }
        gallery_ss << "}";

        txn.exec_params(
            "INSERT INTO blog_posts (id, title, slug, content, excerpt, category, tags, "
            "featured_image, gallery_images, dog_id, adoption_id, shelter_id, "
            "status, meta_description, og_title, og_description, og_image, "
            "author_id, author_name, is_auto_generated, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $21)",
            post_id,
            post.title,
            post.slug,
            post.content,
            post.excerpt,
            blogCategoryToString(post.category),
            tags_ss.str(),
            post.featured_image.value_or(""),
            gallery_ss.str(),
            post.dog_id.value_or(""),
            post.adoption_id.value_or(""),
            post.shelter_id.value_or(""),
            postStatusToString(post.status),
            post.meta_description,
            post.og_title.value_or(""),
            post.og_description.value_or(""),
            post.og_image.value_or(""),
            post.author_id.value_or(""),
            post.author_name,
            post.is_auto_generated,
            now_str
        );

        txn.commit();
        pool.release(conn);

        BlogPost saved = post;
        saved.id = post_id;
        saved.created_at = now;
        saved.updated_at = now;

        return saved;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to save blog post: " + std::string(e.what()),
            {{"title", post.title}}
        );
        throw;
    }
}

BlogPost BlogService::update(const BlogPost& post) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        // Format tags array
        std::ostringstream tags_ss;
        tags_ss << "{";
        for (size_t i = 0; i < post.tags.size(); ++i) {
            if (i > 0) tags_ss << ",";
            tags_ss << "\"" << post.tags[i] << "\"";
        }
        tags_ss << "}";

        // Format gallery images array
        std::ostringstream gallery_ss;
        gallery_ss << "{";
        for (size_t i = 0; i < post.gallery_images.size(); ++i) {
            if (i > 0) gallery_ss << ",";
            gallery_ss << "\"" << post.gallery_images[i] << "\"";
        }
        gallery_ss << "}";

        txn.exec_params(
            "UPDATE blog_posts SET "
            "title = $2, slug = $3, content = $4, excerpt = $5, category = $6, tags = $7, "
            "featured_image = $8, gallery_images = $9, dog_id = $10, adoption_id = $11, shelter_id = $12, "
            "status = $13, meta_description = $14, og_title = $15, og_description = $16, og_image = $17, "
            "updated_at = NOW() WHERE id = $1",
            post.id,
            post.title,
            post.slug,
            post.content,
            post.excerpt,
            blogCategoryToString(post.category),
            tags_ss.str(),
            post.featured_image.value_or(""),
            gallery_ss.str(),
            post.dog_id.value_or(""),
            post.adoption_id.value_or(""),
            post.shelter_id.value_or(""),
            postStatusToString(post.status),
            post.meta_description,
            post.og_title.value_or(""),
            post.og_description.value_or(""),
            post.og_image.value_or("")
        );

        txn.commit();
        pool.release(conn);

        BlogPost updated = post;
        updated.updated_at = std::chrono::system_clock::now();

        return updated;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to update blog post: " + std::string(e.what()),
            {{"post_id", post.id}}
        );
        throw;
    }
}

bool BlogService::deleteById(const std::string& id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "DELETE FROM blog_posts WHERE id = $1",
            id
        );

        txn.commit();
        pool.release(conn);

        return res.affected_rows() > 0;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to delete blog post: " + std::string(e.what()),
            {{"post_id", id}}
        );
        return false;
    }
}

// ============================================================================
// STATUS OPERATIONS
// ============================================================================

bool BlogService::publish(const std::string& id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        txn.exec_params(
            "UPDATE blog_posts SET status = 'published', published_at = NOW(), updated_at = NOW() WHERE id = $1",
            id
        );

        txn.commit();
        pool.release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to publish blog post: " + std::string(e.what()),
            {{"post_id", id}}
        );
        return false;
    }
}

bool BlogService::unpublish(const std::string& id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        txn.exec_params(
            "UPDATE blog_posts SET status = 'draft', updated_at = NOW() WHERE id = $1",
            id
        );

        txn.commit();
        pool.release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to unpublish blog post: " + std::string(e.what()),
            {{"post_id", id}}
        );
        return false;
    }
}

bool BlogService::schedule(
    const std::string& id,
    const std::chrono::system_clock::time_point& scheduled_at
) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        std::time_t time = std::chrono::system_clock::to_time_t(scheduled_at);
        std::tm tm;
#ifdef _WIN32
        gmtime_s(&tm, &time);
#else
        gmtime_r(&time, &tm);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

        pqxx::work txn(*conn);
        txn.exec_params(
            "UPDATE blog_posts SET status = 'scheduled', scheduled_at = $2, updated_at = NOW() WHERE id = $1",
            id, ss.str()
        );

        txn.commit();
        pool.release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to schedule blog post: " + std::string(e.what()),
            {{"post_id", id}}
        );
        return false;
    }
}

bool BlogService::archive(const std::string& id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        txn.exec_params(
            "UPDATE blog_posts SET status = 'archived', updated_at = NOW() WHERE id = $1",
            id
        );

        txn.commit();
        pool.release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to archive blog post: " + std::string(e.what()),
            {{"post_id", id}}
        );
        return false;
    }
}

// ============================================================================
// ENGAGEMENT TRACKING
// ============================================================================

void BlogService::incrementViewCount(const std::string& id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        txn.exec_params(
            "UPDATE blog_posts SET view_count = view_count + 1 WHERE id = $1",
            id
        );

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        // Don't capture error for view counting - not critical
    }
}

void BlogService::incrementShareCount(const std::string& id, const std::string& platform) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        txn.exec_params(
            "UPDATE blog_posts SET share_count = share_count + 1 WHERE id = $1",
            id
        );

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        // Don't capture error for share counting - not critical
    }
}

void BlogService::incrementLikeCount(const std::string& id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        txn.exec_params(
            "UPDATE blog_posts SET like_count = like_count + 1 WHERE id = $1",
            id
        );

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        // Don't capture error for like counting - not critical
    }
}

// ============================================================================
// EDUCATIONAL CONTENT OPERATIONS
// ============================================================================

std::optional<EducationalContent> BlogService::getEducationalById(const std::string& id) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "SELECT * FROM educational_content WHERE id = $1",
            id
        );

        std::optional<EducationalContent> result;
        if (!res.empty()) {
            result = EducationalContent::fromDbRow(res[0]);
        }

        txn.commit();
        pool.release(conn);

        return result;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get educational content: " + std::string(e.what()),
            {{"id", id}}
        );
        return std::nullopt;
    }
}

std::vector<EducationalContent> BlogService::getEducationalByTopic(
    EducationalTopic topic,
    int limit
) {
    return EducationalContentRepository::getByTopic(topic, limit);
}

std::vector<EducationalContent> BlogService::getEducationalByLifeStage(
    LifeStage stage,
    int limit
) {
    return EducationalContentRepository::getByLifeStage(stage, limit);
}

std::vector<EducationalContent> BlogService::searchEducational(
    const std::string& query,
    int limit
) {
    return EducationalContentRepository::searchByKeyword(query, limit);
}

std::vector<EducationalContent> BlogService::getFeaturedEducational(int limit) {
    return EducationalContentRepository::getFeatured(limit);
}

void BlogService::voteEducationalHelpful(const std::string& id, bool helpful) {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        if (helpful) {
            txn.exec_params(
                "UPDATE educational_content SET helpful_count = helpful_count + 1, "
                "helpfulness_score = helpful_count::float / NULLIF(helpful_count + not_helpful_count, 0) "
                "WHERE id = $1",
                id
            );
        } else {
            txn.exec_params(
                "UPDATE educational_content SET not_helpful_count = not_helpful_count + 1, "
                "helpfulness_score = helpful_count::float / NULLIF(helpful_count + not_helpful_count, 0) "
                "WHERE id = $1",
                id
            );
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        // Non-critical, don't capture error
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

std::map<BlogCategory, int> BlogService::getPostCountByCategory() {
    std::map<BlogCategory, int> counts;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec(
            "SELECT category, COUNT(*) FROM blog_posts WHERE status = 'published' GROUP BY category"
        );

        for (const auto& row : res) {
            std::string cat_str = row[0].as<std::string>();
            int count = row[1].as<int>();
            try {
                counts[blogCategoryFromString(cat_str)] = count;
            } catch (...) {
                // Skip invalid category
            }
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get post count by category: " + std::string(e.what()),
            {}
        );
    }

    return counts;
}

int BlogService::getTotalPublishedCount() {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec(
            "SELECT COUNT(*) FROM blog_posts WHERE status = 'published'"
        );

        int count = res[0][0].as<int>();

        txn.commit();
        pool.release(conn);

        return count;

    } catch (const std::exception& e) {
        return 0;
    }
}

int BlogService::getTotalViewCount() {
    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);
        auto res = txn.exec(
            "SELECT COALESCE(SUM(view_count), 0) FROM blog_posts"
        );

        int count = res[0][0].as<int>();

        txn.commit();
        pool.release(conn);

        return count;

    } catch (const std::exception& e) {
        return 0;
    }
}

} // namespace wtl::content::blog
