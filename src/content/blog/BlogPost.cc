/**
 * @file BlogPost.cc
 * @brief Implementation of BlogPost model
 * @see BlogPost.h for documentation
 */

#include "content/blog/BlogPost.h"

// Standard library includes
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <regex>
#include <sstream>

namespace wtl::content::blog {

// ============================================================================
// JSON CONVERSION
// ============================================================================

BlogPost BlogPost::fromJson(const Json::Value& json) {
    BlogPost post;

    // Identity
    post.id = json.get("id", "").asString();
    post.title = json.get("title", "").asString();
    post.slug = json.get("slug", "").asString();

    // If no slug provided, generate from title
    if (post.slug.empty() && !post.title.empty()) {
        post.slug = generateSlug(post.title);
    }

    // Content
    post.content = json.get("content", "").asString();
    post.excerpt = json.get("excerpt", "").asString();

    // If no excerpt provided, generate from content
    if (post.excerpt.empty() && !post.content.empty()) {
        post.excerpt = generateExcerpt(post.content);
    }

    // Classification
    std::string category_str = json.get("category", "educational").asString();
    try {
        post.category = blogCategoryFromString(category_str);
    } catch (...) {
        post.category = BlogCategory::EDUCATIONAL;
    }

    // Parse tags array
    if (json.isMember("tags") && json["tags"].isArray()) {
        for (const auto& tag : json["tags"]) {
            post.tags.push_back(tag.asString());
        }
    }

    // Media
    if (json.isMember("featured_image") && !json["featured_image"].isNull()) {
        post.featured_image = json["featured_image"].asString();
    }

    if (json.isMember("gallery_images") && json["gallery_images"].isArray()) {
        for (const auto& img : json["gallery_images"]) {
            post.gallery_images.push_back(img.asString());
        }
    }

    // Relationships
    if (json.isMember("dog_id") && !json["dog_id"].isNull()) {
        post.dog_id = json["dog_id"].asString();
    }
    if (json.isMember("adoption_id") && !json["adoption_id"].isNull()) {
        post.adoption_id = json["adoption_id"].asString();
    }
    if (json.isMember("shelter_id") && !json["shelter_id"].isNull()) {
        post.shelter_id = json["shelter_id"].asString();
    }

    // Status
    std::string status_str = json.get("status", "draft").asString();
    post.status = postStatusFromString(status_str);

    // Timestamps
    if (json.isMember("scheduled_at") && !json["scheduled_at"].isNull()) {
        post.scheduled_at = parseTimestamp(json["scheduled_at"].asString());
    }
    if (json.isMember("published_at") && !json["published_at"].isNull()) {
        post.published_at = parseTimestamp(json["published_at"].asString());
    }

    // SEO
    post.meta_description = json.get("meta_description", "").asString();
    if (json.isMember("og_title") && !json["og_title"].isNull()) {
        post.og_title = json["og_title"].asString();
    }
    if (json.isMember("og_description") && !json["og_description"].isNull()) {
        post.og_description = json["og_description"].asString();
    }
    if (json.isMember("og_image") && !json["og_image"].isNull()) {
        post.og_image = json["og_image"].asString();
    }

    // Engagement
    post.view_count = json.get("view_count", 0).asInt();
    post.share_count = json.get("share_count", 0).asInt();
    post.like_count = json.get("like_count", 0).asInt();

    // Author
    if (json.isMember("author_id") && !json["author_id"].isNull()) {
        post.author_id = json["author_id"].asString();
    }
    post.author_name = json.get("author_name", "WTL Team").asString();
    post.is_auto_generated = json.get("is_auto_generated", false).asBool();

    // Timestamps
    if (json.isMember("created_at")) {
        post.created_at = parseTimestamp(json["created_at"].asString());
    } else {
        post.created_at = std::chrono::system_clock::now();
    }
    if (json.isMember("updated_at")) {
        post.updated_at = parseTimestamp(json["updated_at"].asString());
    } else {
        post.updated_at = post.created_at;
    }

    return post;
}

BlogPost BlogPost::fromDbRow(const pqxx::row& row) {
    BlogPost post;

    // Identity
    post.id = row["id"].as<std::string>();
    post.title = row["title"].as<std::string>();
    post.slug = row["slug"].as<std::string>();

    // Content
    post.content = row["content"].as<std::string>();
    post.excerpt = row["excerpt"].as<std::string>();

    // Classification
    std::string category_str = row["category"].as<std::string>();
    try {
        post.category = blogCategoryFromString(category_str);
    } catch (...) {
        post.category = BlogCategory::EDUCATIONAL;
    }

    // Tags
    if (!row["tags"].is_null()) {
        post.tags = parsePostgresArray(row["tags"].as<std::string>());
    }

    // Media
    if (!row["featured_image"].is_null()) {
        post.featured_image = row["featured_image"].as<std::string>();
    }
    if (!row["gallery_images"].is_null()) {
        post.gallery_images = parsePostgresArray(row["gallery_images"].as<std::string>());
    }

    // Relationships
    if (!row["dog_id"].is_null()) {
        post.dog_id = row["dog_id"].as<std::string>();
    }
    if (!row["adoption_id"].is_null()) {
        post.adoption_id = row["adoption_id"].as<std::string>();
    }
    if (!row["shelter_id"].is_null()) {
        post.shelter_id = row["shelter_id"].as<std::string>();
    }

    // Status
    std::string status_str = row["status"].as<std::string>();
    post.status = postStatusFromString(status_str);

    // Publication timestamps
    if (!row["scheduled_at"].is_null()) {
        post.scheduled_at = parseTimestamp(row["scheduled_at"].as<std::string>());
    }
    if (!row["published_at"].is_null()) {
        post.published_at = parseTimestamp(row["published_at"].as<std::string>());
    }

    // SEO
    post.meta_description = row["meta_description"].as<std::string>("");
    if (!row["og_title"].is_null()) {
        post.og_title = row["og_title"].as<std::string>();
    }
    if (!row["og_description"].is_null()) {
        post.og_description = row["og_description"].as<std::string>();
    }
    if (!row["og_image"].is_null()) {
        post.og_image = row["og_image"].as<std::string>();
    }

    // Engagement
    post.view_count = row["view_count"].as<int>(0);
    post.share_count = row["share_count"].as<int>(0);
    post.like_count = row["like_count"].as<int>(0);

    // Author
    if (!row["author_id"].is_null()) {
        post.author_id = row["author_id"].as<std::string>();
    }
    post.author_name = row["author_name"].as<std::string>("WTL Team");
    post.is_auto_generated = row["is_auto_generated"].as<bool>(false);

    // Timestamps
    post.created_at = parseTimestamp(row["created_at"].as<std::string>());
    post.updated_at = parseTimestamp(row["updated_at"].as<std::string>());

    return post;
}

Json::Value BlogPost::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    json["title"] = title;
    json["slug"] = slug;

    // Content
    json["content"] = content;
    json["excerpt"] = excerpt;

    // Classification
    json["category"] = blogCategoryToString(category);
    json["category_display"] = blogCategoryDisplayName(category);
    json["category_slug"] = blogCategorySlug(category);

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

    Json::Value gallery_array(Json::arrayValue);
    for (const auto& img : gallery_images) {
        gallery_array.append(img);
    }
    json["gallery_images"] = gallery_array;

    // Relationships
    if (dog_id) {
        json["dog_id"] = *dog_id;
    } else {
        json["dog_id"] = Json::nullValue;
    }
    if (adoption_id) {
        json["adoption_id"] = *adoption_id;
    } else {
        json["adoption_id"] = Json::nullValue;
    }
    if (shelter_id) {
        json["shelter_id"] = *shelter_id;
    } else {
        json["shelter_id"] = Json::nullValue;
    }

    // Status
    json["status"] = postStatusToString(status);
    json["is_visible"] = isVisible();

    if (scheduled_at) {
        json["scheduled_at"] = formatTimestamp(*scheduled_at);
    } else {
        json["scheduled_at"] = Json::nullValue;
    }
    if (published_at) {
        json["published_at"] = formatTimestamp(*published_at);
    } else {
        json["published_at"] = Json::nullValue;
    }

    // SEO
    json["meta_description"] = meta_description;
    if (og_title) {
        json["og_title"] = *og_title;
    } else {
        json["og_title"] = title;  // Fallback to title
    }
    if (og_description) {
        json["og_description"] = *og_description;
    } else {
        json["og_description"] = excerpt;  // Fallback to excerpt
    }
    if (og_image) {
        json["og_image"] = *og_image;
    } else if (featured_image) {
        json["og_image"] = *featured_image;  // Fallback to featured image
    } else {
        json["og_image"] = Json::nullValue;
    }

    // Engagement
    json["view_count"] = view_count;
    json["share_count"] = share_count;
    json["like_count"] = like_count;

    // Author
    if (author_id) {
        json["author_id"] = *author_id;
    } else {
        json["author_id"] = Json::nullValue;
    }
    json["author_name"] = author_name;
    json["is_auto_generated"] = is_auto_generated;

    // Computed fields
    json["reading_time_minutes"] = getReadingTimeMinutes();
    json["word_count"] = getWordCount();

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    return json;
}

Json::Value BlogPost::toListJson() const {
    Json::Value json;

    // Basic info for listing
    json["id"] = id;
    json["title"] = title;
    json["slug"] = slug;
    json["excerpt"] = excerpt;

    // Classification
    json["category"] = blogCategoryToString(category);
    json["category_display"] = blogCategoryDisplayName(category);
    json["category_slug"] = blogCategorySlug(category);
    json["category_color"] = blogCategoryColor(category);

    // Media - just featured image
    if (featured_image) {
        json["featured_image"] = *featured_image;
    } else {
        json["featured_image"] = Json::nullValue;
    }

    // Relationships
    if (dog_id) {
        json["dog_id"] = *dog_id;
    }

    // Status
    json["status"] = postStatusToString(status);
    json["is_visible"] = isVisible();

    if (published_at) {
        json["published_at"] = formatTimestamp(*published_at);
    }

    // Engagement
    json["view_count"] = view_count;
    json["reading_time_minutes"] = getReadingTimeMinutes();

    // Author
    json["author_name"] = author_name;

    return json;
}

// ============================================================================
// STATIC HELPER METHODS
// ============================================================================

std::string BlogPost::generateSlug(const std::string& title) {
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
        // Skip other characters
    }

    // Remove trailing dash
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }

    // Limit length
    if (slug.size() > 80) {
        slug = slug.substr(0, 80);
        // Find last complete word
        size_t last_dash = slug.rfind('-');
        if (last_dash != std::string::npos && last_dash > 20) {
            slug = slug.substr(0, last_dash);
        }
    }

    return slug;
}

std::string BlogPost::generateExcerpt(const std::string& content, size_t max_length) {
    // Strip markdown formatting for excerpt
    std::string text = content;

    // Remove headers
    std::regex header_regex("^#+\\s+", std::regex::multiline);
    text = std::regex_replace(text, header_regex, "");

    // Remove links but keep text
    std::regex link_regex("\\[([^\\]]+)\\]\\([^)]+\\)");
    text = std::regex_replace(text, link_regex, "$1");

    // Remove images
    std::regex image_regex("!\\[([^\\]]*)\\]\\([^)]+\\)");
    text = std::regex_replace(text, image_regex, "");

    // Remove bold/italic
    std::regex bold_regex("\\*\\*([^*]+)\\*\\*");
    text = std::regex_replace(text, bold_regex, "$1");
    std::regex italic_regex("\\*([^*]+)\\*");
    text = std::regex_replace(text, italic_regex, "$1");

    // Remove code blocks
    std::regex code_block_regex("```[^`]*```");
    text = std::regex_replace(text, code_block_regex, "");
    std::regex inline_code_regex("`([^`]+)`");
    text = std::regex_replace(text, inline_code_regex, "$1");

    // Collapse whitespace
    std::regex whitespace_regex("\\s+");
    text = std::regex_replace(text, whitespace_regex, " ");

    // Trim
    size_t start = text.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = text.find_last_not_of(" \t\n\r");
    text = text.substr(start, end - start + 1);

    // Truncate
    if (text.size() > max_length) {
        text = text.substr(0, max_length);
        // Find last complete word/sentence
        size_t last_space = text.rfind(' ');
        if (last_space != std::string::npos && last_space > max_length / 2) {
            text = text.substr(0, last_space);
        }
        text += "...";
    }

    return text;
}

// ============================================================================
// INSTANCE HELPER METHODS
// ============================================================================

bool BlogPost::isPublishable() const {
    // Must have title and content
    if (title.empty() || content.empty()) {
        return false;
    }

    // Must have excerpt
    if (excerpt.empty()) {
        return false;
    }

    // Must have meta description for SEO
    if (meta_description.empty()) {
        return false;
    }

    return true;
}

bool BlogPost::isVisible() const {
    if (status != PostStatus::PUBLISHED) {
        return false;
    }

    // Check if publication date has passed
    if (published_at) {
        return *published_at <= std::chrono::system_clock::now();
    }

    return true;
}

std::string BlogPost::getDisplayTitle() const {
    return title.empty() ? "Untitled" : title;
}

int BlogPost::getReadingTimeMinutes() const {
    // Average reading speed: 200 words per minute
    int words = getWordCount();
    int minutes = (words + 199) / 200;  // Round up
    return std::max(1, minutes);  // At least 1 minute
}

int BlogPost::getWordCount() const {
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

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::chrono::system_clock::time_point BlogPost::parseTimestamp(const std::string& timestamp) {
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        // Try alternate format without T
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    }

    if (ss.fail()) {
        // Return current time on parse failure
        return std::chrono::system_clock::now();
    }

    std::time_t time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

std::string BlogPost::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
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

std::vector<std::string> BlogPost::parsePostgresArray(const std::string& array_str) {
    std::vector<std::string> result;

    if (array_str.empty() || array_str == "{}") {
        return result;
    }

    // Remove outer braces
    std::string inner = array_str;
    if (inner.front() == '{') inner = inner.substr(1);
    if (inner.back() == '}') inner = inner.substr(0, inner.size() - 1);

    if (inner.empty()) {
        return result;
    }

    // Parse comma-separated values
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

std::string BlogPost::formatPostgresArray(const std::vector<std::string>& vec) {
    if (vec.empty()) {
        return "{}";
    }

    std::ostringstream ss;
    ss << "{";

    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) ss << ",";

        // Escape and quote the value
        ss << "\"";
        for (char c : vec[i]) {
            if (c == '"' || c == '\\') {
                ss << '\\';
            }
            ss << c;
        }
        ss << "\"";
    }

    ss << "}";
    return ss.str();
}

} // namespace wtl::content::blog
