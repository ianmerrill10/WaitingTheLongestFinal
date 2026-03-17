/**
 * @file EducationalContent.cc
 * @brief Implementation of EducationalContent model and repository
 * @see EducationalContent.h for documentation
 */

#include "content/blog/EducationalContent.h"

// Standard library includes
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <regex>
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::content::blog {

// ============================================================================
// JSON CONVERSION
// ============================================================================

EducationalContent EducationalContent::fromJson(const Json::Value& json) {
    EducationalContent content;

    // Identity
    content.id = json.get("id", "").asString();
    content.title = json.get("title", "").asString();
    content.slug = json.get("slug", "").asString();

    if (content.slug.empty() && !content.title.empty()) {
        content.slug = generateSlug(content.title);
    }

    // Content
    content.content = json.get("content", "").asString();
    content.summary = json.get("summary", "").asString();

    if (json.isMember("key_points") && json["key_points"].isArray()) {
        for (const auto& point : json["key_points"]) {
            content.key_points.push_back(point.asString());
        }
    }

    // Classification
    std::string topic_str = json.get("topic", "health").asString();
    content.topic = educationalTopicFromString(topic_str);

    if (json.isMember("secondary_topics") && json["secondary_topics"].isArray()) {
        for (const auto& t : json["secondary_topics"]) {
            content.secondary_topics.push_back(educationalTopicFromString(t.asString()));
        }
    }

    std::string life_stage_str = json.get("life_stage", "all").asString();
    content.life_stage = lifeStageFromString(life_stage_str);

    if (json.isMember("tags") && json["tags"].isArray()) {
        for (const auto& tag : json["tags"]) {
            content.tags.push_back(tag.asString());
        }
    }

    // Media
    if (json.isMember("featured_image") && !json["featured_image"].isNull()) {
        content.featured_image = json["featured_image"].asString();
    }

    if (json.isMember("images") && json["images"].isArray()) {
        for (const auto& img : json["images"]) {
            content.images.push_back(img.asString());
        }
    }

    if (json.isMember("video_url") && !json["video_url"].isNull()) {
        content.video_url = json["video_url"].asString();
    }

    // SEO
    content.meta_description = json.get("meta_description", "").asString();
    if (json.isMember("meta_keywords") && !json["meta_keywords"].isNull()) {
        content.meta_keywords = json["meta_keywords"].asString();
    }

    // Engagement
    content.view_count = json.get("view_count", 0).asInt();
    content.helpful_count = json.get("helpful_count", 0).asInt();
    content.not_helpful_count = json.get("not_helpful_count", 0).asInt();
    content.updateHelpfulnessScore();

    // Status
    content.is_published = json.get("is_published", false).asBool();
    content.is_featured = json.get("is_featured", false).asBool();
    content.display_order = json.get("display_order", 0).asInt();

    // Author
    if (json.isMember("author_id") && !json["author_id"].isNull()) {
        content.author_id = json["author_id"].asString();
    }
    content.author_name = json.get("author_name", "WTL Team").asString();

    if (json.isMember("reviewer_id") && !json["reviewer_id"].isNull()) {
        content.reviewer_id = json["reviewer_id"].asString();
    }
    if (json.isMember("reviewer_name") && !json["reviewer_name"].isNull()) {
        content.reviewer_name = json["reviewer_name"].asString();
    }

    // Timestamps
    if (json.isMember("created_at")) {
        content.created_at = parseTimestamp(json["created_at"].asString());
    } else {
        content.created_at = std::chrono::system_clock::now();
    }
    if (json.isMember("updated_at")) {
        content.updated_at = parseTimestamp(json["updated_at"].asString());
    } else {
        content.updated_at = content.created_at;
    }
    if (json.isMember("published_at") && !json["published_at"].isNull()) {
        content.published_at = parseTimestamp(json["published_at"].asString());
    }
    if (json.isMember("last_reviewed_at") && !json["last_reviewed_at"].isNull()) {
        content.last_reviewed_at = parseTimestamp(json["last_reviewed_at"].asString());
    }

    return content;
}

EducationalContent EducationalContent::fromDbRow(const pqxx::row& row) {
    EducationalContent content;

    // Identity
    content.id = row["id"].as<std::string>();
    content.title = row["title"].as<std::string>();
    content.slug = row["slug"].as<std::string>();

    // Content
    content.content = row["content"].as<std::string>();
    content.summary = row["summary"].as<std::string>("");

    if (!row["key_points"].is_null()) {
        content.key_points = parsePostgresArray(row["key_points"].as<std::string>());
    }

    // Classification
    std::string topic_str = row["topic"].as<std::string>();
    content.topic = educationalTopicFromString(topic_str);

    if (!row["secondary_topics"].is_null()) {
        auto secondary_strs = parsePostgresArray(row["secondary_topics"].as<std::string>());
        for (const auto& s : secondary_strs) {
            content.secondary_topics.push_back(educationalTopicFromString(s));
        }
    }

    std::string life_stage_str = row["life_stage"].as<std::string>();
    content.life_stage = lifeStageFromString(life_stage_str);

    if (!row["tags"].is_null()) {
        content.tags = parsePostgresArray(row["tags"].as<std::string>());
    }

    // Media
    if (!row["featured_image"].is_null()) {
        content.featured_image = row["featured_image"].as<std::string>();
    }
    if (!row["images"].is_null()) {
        content.images = parsePostgresArray(row["images"].as<std::string>());
    }
    if (!row["video_url"].is_null()) {
        content.video_url = row["video_url"].as<std::string>();
    }

    // SEO
    content.meta_description = row["meta_description"].as<std::string>("");
    if (!row["meta_keywords"].is_null()) {
        content.meta_keywords = row["meta_keywords"].as<std::string>();
    }

    // Engagement
    content.view_count = row["view_count"].as<int>(0);
    content.helpful_count = row["helpful_count"].as<int>(0);
    content.not_helpful_count = row["not_helpful_count"].as<int>(0);
    content.helpfulness_score = row["helpfulness_score"].as<double>(0.0);

    // Status
    content.is_published = row["is_published"].as<bool>(false);
    content.is_featured = row["is_featured"].as<bool>(false);
    content.display_order = row["display_order"].as<int>(0);

    // Author
    if (!row["author_id"].is_null()) {
        content.author_id = row["author_id"].as<std::string>();
    }
    content.author_name = row["author_name"].as<std::string>("WTL Team");
    if (!row["reviewer_id"].is_null()) {
        content.reviewer_id = row["reviewer_id"].as<std::string>();
    }
    if (!row["reviewer_name"].is_null()) {
        content.reviewer_name = row["reviewer_name"].as<std::string>();
    }

    // Timestamps
    content.created_at = parseTimestamp(row["created_at"].as<std::string>());
    content.updated_at = parseTimestamp(row["updated_at"].as<std::string>());
    if (!row["published_at"].is_null()) {
        content.published_at = parseTimestamp(row["published_at"].as<std::string>());
    }
    if (!row["last_reviewed_at"].is_null()) {
        content.last_reviewed_at = parseTimestamp(row["last_reviewed_at"].as<std::string>());
    }

    return content;
}

Json::Value EducationalContent::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    json["title"] = title;
    json["slug"] = slug;

    // Content
    json["content"] = content;
    json["summary"] = summary;

    Json::Value key_points_array(Json::arrayValue);
    for (const auto& point : key_points) {
        key_points_array.append(point);
    }
    json["key_points"] = key_points_array;

    // Classification
    json["topic"] = educationalTopicToString(topic);
    json["topic_display"] = educationalTopicDisplayName(topic);
    json["topic_icon"] = educationalTopicIcon(topic);

    Json::Value secondary_array(Json::arrayValue);
    for (const auto& t : secondary_topics) {
        Json::Value topic_obj;
        topic_obj["id"] = educationalTopicToString(t);
        topic_obj["display"] = educationalTopicDisplayName(t);
        secondary_array.append(topic_obj);
    }
    json["secondary_topics"] = secondary_array;

    json["life_stage"] = lifeStageToString(life_stage);
    json["life_stage_display"] = lifeStageDisplayName(life_stage);

    Json::Value tags_array(Json::arrayValue);
    for (const auto& tag : tags) {
        tags_array.append(tag);
    }
    json["tags"] = tags_array;

    // Media
    if (featured_image) {
        json["featured_image"] = *featured_image;
    } else {
        json["featured_image"] = Json::nullValue;
    }

    Json::Value images_array(Json::arrayValue);
    for (const auto& img : images) {
        images_array.append(img);
    }
    json["images"] = images_array;

    if (video_url) {
        json["video_url"] = *video_url;
    } else {
        json["video_url"] = Json::nullValue;
    }

    // SEO
    json["meta_description"] = meta_description;
    if (meta_keywords) {
        json["meta_keywords"] = *meta_keywords;
    } else {
        json["meta_keywords"] = Json::nullValue;
    }

    // Engagement
    json["view_count"] = view_count;
    json["helpful_count"] = helpful_count;
    json["not_helpful_count"] = not_helpful_count;
    json["helpfulness_score"] = helpfulness_score;

    // Status
    json["is_published"] = is_published;
    json["is_featured"] = is_featured;
    json["display_order"] = display_order;

    // Author
    if (author_id) {
        json["author_id"] = *author_id;
    } else {
        json["author_id"] = Json::nullValue;
    }
    json["author_name"] = author_name;

    if (reviewer_id) {
        json["reviewer_id"] = *reviewer_id;
    } else {
        json["reviewer_id"] = Json::nullValue;
    }
    if (reviewer_name) {
        json["reviewer_name"] = *reviewer_name;
    } else {
        json["reviewer_name"] = Json::nullValue;
    }

    // Computed fields
    json["reading_time_minutes"] = getReadingTimeMinutes();
    json["word_count"] = getWordCount();

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);
    if (published_at) {
        json["published_at"] = formatTimestamp(*published_at);
    } else {
        json["published_at"] = Json::nullValue;
    }
    if (last_reviewed_at) {
        json["last_reviewed_at"] = formatTimestamp(*last_reviewed_at);
    } else {
        json["last_reviewed_at"] = Json::nullValue;
    }

    return json;
}

Json::Value EducationalContent::toListJson() const {
    Json::Value json;

    json["id"] = id;
    json["title"] = title;
    json["slug"] = slug;
    json["summary"] = summary;

    json["topic"] = educationalTopicToString(topic);
    json["topic_display"] = educationalTopicDisplayName(topic);
    json["topic_icon"] = educationalTopicIcon(topic);

    json["life_stage"] = lifeStageToString(life_stage);
    json["life_stage_display"] = lifeStageDisplayName(life_stage);

    if (featured_image) {
        json["featured_image"] = *featured_image;
    }

    json["view_count"] = view_count;
    json["helpfulness_score"] = helpfulness_score;
    json["is_featured"] = is_featured;
    json["reading_time_minutes"] = getReadingTimeMinutes();

    if (published_at) {
        json["published_at"] = formatTimestamp(*published_at);
    }

    return json;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

std::string EducationalContent::generateSlug(const std::string& title) {
    std::string slug;
    slug.reserve(title.size());

    bool prev_dash = false;

    for (char c : title) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            slug += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            prev_dash = false;
        } else if (c == ' ' || c == '-' || c == '_') {
            if (!prev_dash && !slug.empty()) {
                slug += '-';
                prev_dash = true;
            }
        }
    }

    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }

    if (slug.size() > 80) {
        slug = slug.substr(0, 80);
        size_t last_dash = slug.rfind('-');
        if (last_dash != std::string::npos && last_dash > 20) {
            slug = slug.substr(0, last_dash);
        }
    }

    return slug;
}

int EducationalContent::getReadingTimeMinutes() const {
    int words = getWordCount();
    int minutes = (words + 199) / 200;
    return std::max(1, minutes);
}

int EducationalContent::getWordCount() const {
    if (content.empty()) return 0;

    int count = 0;
    bool in_word = false;

    for (char c : content) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            if (!in_word) {
                in_word = true;
                count++;
            }
        } else {
            in_word = false;
        }
    }

    return count;
}

void EducationalContent::updateHelpfulnessScore() {
    int total = helpful_count + not_helpful_count;
    if (total > 0) {
        helpfulness_score = static_cast<double>(helpful_count) / static_cast<double>(total);
    } else {
        helpfulness_score = 0.0;
    }
}

bool EducationalContent::isPublishable() const {
    if (title.empty() || content.empty()) {
        return false;
    }
    if (summary.empty()) {
        return false;
    }
    if (meta_description.empty()) {
        return false;
    }
    return true;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::chrono::system_clock::time_point EducationalContent::parseTimestamp(const std::string& timestamp) {
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    }

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    std::time_t time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

std::string EducationalContent::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;

#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::vector<std::string> EducationalContent::parsePostgresArray(const std::string& array_str) {
    std::vector<std::string> result;

    if (array_str.empty() || array_str == "{}") {
        return result;
    }

    std::string inner = array_str;
    if (inner.front() == '{') inner = inner.substr(1);
    if (inner.back() == '}') inner = inner.substr(0, inner.size() - 1);

    if (inner.empty()) {
        return result;
    }

    std::string current;
    bool in_quotes = false;
    bool escape_next = false;

    for (char c : inner) {
        if (escape_next) {
            current += c;
            escape_next = false;
            continue;
        }

        if (c == '\\') {
            escape_next = true;
            continue;
        }

        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }

        if (c == ',' && !in_quotes) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

// ============================================================================
// REPOSITORY IMPLEMENTATION
// ============================================================================

std::vector<EducationalContent> EducationalContentRepository::getByLifeStage(LifeStage stage, int limit) {
    std::vector<EducationalContent> results;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        std::string query;
        if (stage == LifeStage::ALL) {
            query = "SELECT * FROM educational_content WHERE is_published = true "
                    "ORDER BY display_order, created_at DESC LIMIT $1";
        } else {
            query = "SELECT * FROM educational_content WHERE is_published = true "
                    "AND (life_stage = $2 OR life_stage = 'all') "
                    "ORDER BY display_order, created_at DESC LIMIT $1";
        }

        pqxx::result res;
        if (stage == LifeStage::ALL) {
            res = txn.exec_params(query, limit);
        } else {
            res = txn.exec_params(query, limit, lifeStageToString(stage));
        }

        for (const auto& row : res) {
            results.push_back(EducationalContent::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get educational content by life stage: " + std::string(e.what()),
            {{"life_stage", lifeStageToString(stage)}}
        );
    }

    return results;
}

std::vector<EducationalContent> EducationalContentRepository::getByTopic(EducationalTopic topic, int limit) {
    std::vector<EducationalContent> results;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        std::string topic_str = educationalTopicToString(topic);

        pqxx::result res = txn.exec_params(
            "SELECT * FROM educational_content WHERE is_published = true "
            "AND (topic = $1 OR $1 = ANY(secondary_topics)) "
            "ORDER BY display_order, created_at DESC LIMIT $2",
            topic_str, limit
        );

        for (const auto& row : res) {
            results.push_back(EducationalContent::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get educational content by topic: " + std::string(e.what()),
            {{"topic", educationalTopicToString(topic)}}
        );
    }

    return results;
}

std::vector<EducationalContent> EducationalContentRepository::searchByKeyword(const std::string& keyword, int limit) {
    std::vector<EducationalContent> results;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        // Use full-text search with trigram similarity fallback
        std::string search_term = "%" + keyword + "%";

        pqxx::result res = txn.exec_params(
            "SELECT * FROM educational_content WHERE is_published = true "
            "AND (title ILIKE $1 OR content ILIKE $1 OR summary ILIKE $1 "
            "OR $2 = ANY(tags)) "
            "ORDER BY "
            "CASE WHEN title ILIKE $1 THEN 0 ELSE 1 END, "
            "created_at DESC LIMIT $3",
            search_term, keyword, limit
        );

        for (const auto& row : res) {
            results.push_back(EducationalContent::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to search educational content: " + std::string(e.what()),
            {{"keyword", keyword}}
        );
    }

    return results;
}

std::vector<EducationalContent> EducationalContentRepository::getFeatured(int limit) {
    std::vector<EducationalContent> results;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        pqxx::result res = txn.exec_params(
            "SELECT * FROM educational_content WHERE is_published = true AND is_featured = true "
            "ORDER BY display_order, created_at DESC LIMIT $1",
            limit
        );

        for (const auto& row : res) {
            results.push_back(EducationalContent::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get featured educational content: " + std::string(e.what()),
            {}
        );
    }

    return results;
}

std::vector<EducationalContent> EducationalContentRepository::getMostHelpful(int limit) {
    std::vector<EducationalContent> results;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        pqxx::result res = txn.exec_params(
            "SELECT * FROM educational_content WHERE is_published = true "
            "AND (helpful_count + not_helpful_count) >= 5 "
            "ORDER BY helpfulness_score DESC, helpful_count DESC LIMIT $1",
            limit
        );

        for (const auto& row : res) {
            results.push_back(EducationalContent::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get most helpful educational content: " + std::string(e.what()),
            {}
        );
    }

    return results;
}

std::vector<EducationalContent> EducationalContentRepository::getFirstTimeOwnerGuides() {
    std::vector<EducationalContent> results;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        pqxx::result res = txn.exec(
            "SELECT * FROM educational_content WHERE is_published = true "
            "AND (topic = 'first_time_owner' OR topic = 'adoption_prep' OR topic = 'home_safety') "
            "ORDER BY display_order, topic, created_at DESC"
        );

        for (const auto& row : res) {
            results.push_back(EducationalContent::fromDbRow(row));
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get first-time owner guides: " + std::string(e.what()),
            {}
        );
    }

    return results;
}

std::vector<std::pair<EducationalTopic, int>> EducationalContentRepository::getTopicSummary() {
    std::vector<std::pair<EducationalTopic, int>> results;

    try {
        auto& pool = wtl::core::db::ConnectionPool::getInstance();
        auto conn = pool.acquire();

        pqxx::work txn(*conn);

        pqxx::result res = txn.exec(
            "SELECT topic, COUNT(*) as count FROM educational_content "
            "WHERE is_published = true GROUP BY topic ORDER BY count DESC"
        );

        for (const auto& row : res) {
            std::string topic_str = row["topic"].as<std::string>();
            int count = row["count"].as<int>();
            results.emplace_back(educationalTopicFromString(topic_str), count);
        }

        txn.commit();
        pool.release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::DATABASE,
            "Failed to get topic summary: " + std::string(e.what()),
            {}
        );
    }

    return results;
}

} // namespace wtl::content::blog
