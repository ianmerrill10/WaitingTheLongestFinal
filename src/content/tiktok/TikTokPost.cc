/**
 * @file TikTokPost.cc
 * @brief Implementation of TikTokPost data model
 * @see TikTokPost.h for documentation
 */

#include "TikTokPost.h"

// Standard library includes
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>

namespace wtl::content::tiktok {

// ============================================================================
// TIKTOKANALYTICS IMPLEMENTATION
// ============================================================================

Json::Value TikTokAnalytics::toJson() const {
    Json::Value json;
    json["views"] = static_cast<Json::UInt64>(views);
    json["likes"] = static_cast<Json::UInt64>(likes);
    json["shares"] = static_cast<Json::UInt64>(shares);
    json["comments"] = static_cast<Json::UInt64>(comments);
    json["saves"] = static_cast<Json::UInt64>(saves);
    json["engagement_rate"] = engagement_rate;
    json["reach"] = static_cast<Json::UInt64>(reach);
    json["profile_visits"] = static_cast<Json::UInt64>(profile_visits);

    auto last_updated_time = std::chrono::system_clock::to_time_t(last_updated);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&last_updated_time), "%Y-%m-%dT%H:%M:%SZ");
    json["last_updated"] = oss.str();

    return json;
}

TikTokAnalytics TikTokAnalytics::fromJson(const Json::Value& json) {
    TikTokAnalytics analytics;

    analytics.views = json.get("views", 0).asUInt64();
    analytics.likes = json.get("likes", 0).asUInt64();
    analytics.shares = json.get("shares", 0).asUInt64();
    analytics.comments = json.get("comments", 0).asUInt64();
    analytics.saves = json.get("saves", 0).asUInt64();
    analytics.engagement_rate = json.get("engagement_rate", 0.0).asDouble();
    analytics.reach = json.get("reach", 0).asUInt64();
    analytics.profile_visits = json.get("profile_visits", 0).asUInt64();

    if (json.isMember("last_updated") && !json["last_updated"].isNull()) {
        std::string ts = json["last_updated"].asString();
        std::tm tm = {};
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        analytics.last_updated = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    } else {
        analytics.last_updated = std::chrono::system_clock::now();
    }

    return analytics;
}

void TikTokAnalytics::calculateEngagementRate() {
    if (views > 0) {
        // Engagement rate = (likes + comments + shares + saves) / views * 100
        double total_engagement = static_cast<double>(likes + comments + shares + saves);
        engagement_rate = (total_engagement / static_cast<double>(views)) * 100.0;
    } else {
        engagement_rate = 0.0;
    }
}

// ============================================================================
// TIKTOKPOST TIMESTAMP HELPERS
// ============================================================================

std::chrono::system_clock::time_point TikTokPost::parseTimestamp(const std::string& timestamp) {
    if (timestamp.empty()) {
        return std::chrono::system_clock::now();
    }

    std::tm tm = {};
    std::istringstream ss(timestamp);

    // Try ISO 8601 format first: YYYY-MM-DDTHH:MM:SSZ
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        // Try PostgreSQL format: YYYY-MM-DD HH:MM:SS
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    }

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string TikTokPost::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ============================================================================
// TIKTOKPOST POSTGRES ARRAY HELPERS
// ============================================================================

std::vector<std::string> TikTokPost::parsePostgresArray(const std::string& array_str) {
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

    // Parse comma-separated values, handling quoted strings
    bool in_quotes = false;
    std::string current;

    for (size_t i = 0; i < inner.size(); ++i) {
        char c = inner[i];

        if (c == '"' && (i == 0 || inner[i-1] != '\\')) {
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

std::string TikTokPost::formatPostgresArray(const std::vector<std::string>& vec) {
    if (vec.empty()) {
        return "{}";
    }

    std::ostringstream oss;
    oss << "{";

    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";

        // Quote and escape the value
        std::string val = vec[i];
        // Escape quotes
        std::string escaped;
        for (char c : val) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else escaped += c;
        }

        oss << "\"" << escaped << "\"";
    }

    oss << "}";
    return oss.str();
}

// ============================================================================
// TIKTOKPOST ENUM CONVERSIONS
// ============================================================================

std::string TikTokPost::statusToString(TikTokPostStatus status) {
    switch (status) {
        case TikTokPostStatus::DRAFT:            return "draft";
        case TikTokPostStatus::SCHEDULED:        return "scheduled";
        case TikTokPostStatus::PENDING_APPROVAL: return "pending_approval";
        case TikTokPostStatus::POSTING:          return "posting";
        case TikTokPostStatus::PUBLISHED:        return "published";
        case TikTokPostStatus::FAILED:           return "failed";
        case TikTokPostStatus::DELETED:          return "deleted";
        case TikTokPostStatus::EXPIRED:          return "expired";
        default:                                 return "draft";
    }
}

TikTokPostStatus TikTokPost::stringToStatus(const std::string& str) {
    if (str == "draft")            return TikTokPostStatus::DRAFT;
    if (str == "scheduled")        return TikTokPostStatus::SCHEDULED;
    if (str == "pending_approval") return TikTokPostStatus::PENDING_APPROVAL;
    if (str == "posting")          return TikTokPostStatus::POSTING;
    if (str == "published")        return TikTokPostStatus::PUBLISHED;
    if (str == "failed")           return TikTokPostStatus::FAILED;
    if (str == "deleted")          return TikTokPostStatus::DELETED;
    if (str == "expired")          return TikTokPostStatus::EXPIRED;
    return TikTokPostStatus::DRAFT;
}

std::string TikTokPost::contentTypeToString(TikTokContentType type) {
    switch (type) {
        case TikTokContentType::VIDEO:          return "video";
        case TikTokContentType::PHOTO_SLIDESHOW: return "photo_slideshow";
        case TikTokContentType::SINGLE_PHOTO:   return "single_photo";
        default:                                return "video";
    }
}

TikTokContentType TikTokPost::stringToContentType(const std::string& str) {
    if (str == "video")           return TikTokContentType::VIDEO;
    if (str == "photo_slideshow") return TikTokContentType::PHOTO_SLIDESHOW;
    if (str == "single_photo")    return TikTokContentType::SINGLE_PHOTO;
    return TikTokContentType::VIDEO;
}

// ============================================================================
// TIKTOKPOST JSON CONVERSION
// ============================================================================

TikTokPost TikTokPost::fromJson(const Json::Value& json) {
    TikTokPost post;

    // Identity
    post.id = json.get("id", "").asString();
    post.dog_id = json.get("dog_id", "").asString();
    post.template_type = json.get("template_type", "").asString();

    // Content
    post.caption = json.get("caption", "").asString();

    if (json.isMember("hashtags") && json["hashtags"].isArray()) {
        for (const auto& tag : json["hashtags"]) {
            post.hashtags.push_back(tag.asString());
        }
    }

    if (json.isMember("music_id") && !json["music_id"].isNull()) {
        post.music_id = json["music_id"].asString();
    }
    if (json.isMember("music_name") && !json["music_name"].isNull()) {
        post.music_name = json["music_name"].asString();
    }

    // Media
    post.content_type = stringToContentType(json.get("content_type", "video").asString());

    if (json.isMember("video_url") && !json["video_url"].isNull()) {
        post.video_url = json["video_url"].asString();
    }

    if (json.isMember("photo_urls") && json["photo_urls"].isArray()) {
        for (const auto& url : json["photo_urls"]) {
            post.photo_urls.push_back(url.asString());
        }
    }

    if (json.isMember("thumbnail_url") && !json["thumbnail_url"].isNull()) {
        post.thumbnail_url = json["thumbnail_url"].asString();
    }

    // Overlay/Styling
    if (json.isMember("overlay_text") && !json["overlay_text"].isNull()) {
        post.overlay_text = json["overlay_text"].asString();
    }
    if (json.isMember("overlay_style") && !json["overlay_style"].isNull()) {
        post.overlay_style = json["overlay_style"].asString();
    }
    post.include_countdown = json.get("include_countdown", false).asBool();
    post.include_wait_time = json.get("include_wait_time", false).asBool();

    // Scheduling
    if (json.isMember("scheduled_at") && !json["scheduled_at"].isNull()) {
        post.scheduled_at = parseTimestamp(json["scheduled_at"].asString());
    }
    if (json.isMember("posted_at") && !json["posted_at"].isNull()) {
        post.posted_at = parseTimestamp(json["posted_at"].asString());
    }
    post.auto_generated = json.get("auto_generated", false).asBool();

    // Status
    post.status = stringToStatus(json.get("status", "draft").asString());
    if (json.isMember("tiktok_post_id") && !json["tiktok_post_id"].isNull()) {
        post.tiktok_post_id = json["tiktok_post_id"].asString();
    }
    if (json.isMember("tiktok_url") && !json["tiktok_url"].isNull()) {
        post.tiktok_url = json["tiktok_url"].asString();
    }
    if (json.isMember("error_message") && !json["error_message"].isNull()) {
        post.error_message = json["error_message"].asString();
    }
    post.retry_count = json.get("retry_count", 0).asInt();

    // Analytics
    if (json.isMember("analytics") && json["analytics"].isObject()) {
        post.analytics = TikTokAnalytics::fromJson(json["analytics"]);
    }

    // Metadata
    if (json.isMember("created_by") && !json["created_by"].isNull()) {
        post.created_by = json["created_by"].asString();
    }
    if (json.isMember("approved_by") && !json["approved_by"].isNull()) {
        post.approved_by = json["approved_by"].asString();
    }
    post.shelter_id = json.get("shelter_id", "").asString();

    // Timestamps
    if (json.isMember("created_at") && !json["created_at"].isNull()) {
        post.created_at = parseTimestamp(json["created_at"].asString());
    } else {
        post.created_at = std::chrono::system_clock::now();
    }

    if (json.isMember("updated_at") && !json["updated_at"].isNull()) {
        post.updated_at = parseTimestamp(json["updated_at"].asString());
    } else {
        post.updated_at = std::chrono::system_clock::now();
    }

    return post;
}

TikTokPost TikTokPost::fromDbRow(const pqxx::row& row) {
    TikTokPost post;

    // Identity
    post.id = row["id"].as<std::string>();
    post.dog_id = row["dog_id"].as<std::string>();
    post.template_type = row["template_type"].as<std::string>("");

    // Content
    post.caption = row["caption"].as<std::string>("");

    if (!row["hashtags"].is_null()) {
        post.hashtags = parsePostgresArray(row["hashtags"].as<std::string>());
    }

    if (!row["music_id"].is_null()) {
        post.music_id = row["music_id"].as<std::string>();
    }
    if (!row["music_name"].is_null()) {
        post.music_name = row["music_name"].as<std::string>();
    }

    // Media
    post.content_type = stringToContentType(row["content_type"].as<std::string>("video"));

    if (!row["video_url"].is_null()) {
        post.video_url = row["video_url"].as<std::string>();
    }

    if (!row["photo_urls"].is_null()) {
        post.photo_urls = parsePostgresArray(row["photo_urls"].as<std::string>());
    }

    if (!row["thumbnail_url"].is_null()) {
        post.thumbnail_url = row["thumbnail_url"].as<std::string>();
    }

    // Overlay/Styling
    if (!row["overlay_text"].is_null()) {
        post.overlay_text = row["overlay_text"].as<std::string>();
    }
    if (!row["overlay_style"].is_null()) {
        post.overlay_style = row["overlay_style"].as<std::string>();
    }
    post.include_countdown = row["include_countdown"].as<bool>(false);
    post.include_wait_time = row["include_wait_time"].as<bool>(false);

    // Scheduling
    if (!row["scheduled_at"].is_null()) {
        post.scheduled_at = parseTimestamp(row["scheduled_at"].as<std::string>());
    }
    if (!row["posted_at"].is_null()) {
        post.posted_at = parseTimestamp(row["posted_at"].as<std::string>());
    }
    post.auto_generated = row["auto_generated"].as<bool>(false);

    // Status
    post.status = stringToStatus(row["status"].as<std::string>("draft"));

    if (!row["tiktok_post_id"].is_null()) {
        post.tiktok_post_id = row["tiktok_post_id"].as<std::string>();
    }
    if (!row["tiktok_url"].is_null()) {
        post.tiktok_url = row["tiktok_url"].as<std::string>();
    }
    if (!row["error_message"].is_null()) {
        post.error_message = row["error_message"].as<std::string>();
    }
    post.retry_count = row["retry_count"].as<int>(0);

    // Analytics - stored as JSONB in database
    if (!row["analytics"].is_null()) {
        std::string analytics_json = row["analytics"].as<std::string>();
        Json::Value analytics_val;
        Json::CharReaderBuilder builder;
        std::istringstream stream(analytics_json);
        std::string errors;
        if (Json::parseFromStream(builder, stream, &analytics_val, &errors)) {
            post.analytics = TikTokAnalytics::fromJson(analytics_val);
        }
    }

    // Metadata
    if (!row["created_by"].is_null()) {
        post.created_by = row["created_by"].as<std::string>();
    }
    if (!row["approved_by"].is_null()) {
        post.approved_by = row["approved_by"].as<std::string>();
    }
    post.shelter_id = row["shelter_id"].as<std::string>("");

    // Timestamps
    post.created_at = parseTimestamp(row["created_at"].as<std::string>());
    post.updated_at = parseTimestamp(row["updated_at"].as<std::string>());

    return post;
}

Json::Value TikTokPost::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    json["dog_id"] = dog_id;
    json["template_type"] = template_type;

    // Content
    json["caption"] = caption;

    Json::Value tags(Json::arrayValue);
    for (const auto& tag : hashtags) {
        tags.append(tag);
    }
    json["hashtags"] = tags;

    if (music_id) json["music_id"] = *music_id;
    else json["music_id"] = Json::nullValue;

    if (music_name) json["music_name"] = *music_name;
    else json["music_name"] = Json::nullValue;

    // Media
    json["content_type"] = contentTypeToString(content_type);

    if (video_url) json["video_url"] = *video_url;
    else json["video_url"] = Json::nullValue;

    Json::Value photos(Json::arrayValue);
    for (const auto& url : photo_urls) {
        photos.append(url);
    }
    json["photo_urls"] = photos;

    if (thumbnail_url) json["thumbnail_url"] = *thumbnail_url;
    else json["thumbnail_url"] = Json::nullValue;

    // Overlay/Styling
    if (overlay_text) json["overlay_text"] = *overlay_text;
    else json["overlay_text"] = Json::nullValue;

    if (overlay_style) json["overlay_style"] = *overlay_style;
    else json["overlay_style"] = Json::nullValue;

    json["include_countdown"] = include_countdown;
    json["include_wait_time"] = include_wait_time;

    // Scheduling
    if (scheduled_at) json["scheduled_at"] = formatTimestamp(*scheduled_at);
    else json["scheduled_at"] = Json::nullValue;

    if (posted_at) json["posted_at"] = formatTimestamp(*posted_at);
    else json["posted_at"] = Json::nullValue;

    json["auto_generated"] = auto_generated;

    // Status
    json["status"] = statusToString(status);

    if (tiktok_post_id) json["tiktok_post_id"] = *tiktok_post_id;
    else json["tiktok_post_id"] = Json::nullValue;

    if (tiktok_url) json["tiktok_url"] = *tiktok_url;
    else json["tiktok_url"] = Json::nullValue;

    if (error_message) json["error_message"] = *error_message;
    else json["error_message"] = Json::nullValue;

    json["retry_count"] = retry_count;

    // Analytics
    json["analytics"] = analytics.toJson();

    // Metadata
    if (created_by) json["created_by"] = *created_by;
    else json["created_by"] = Json::nullValue;

    if (approved_by) json["approved_by"] = *approved_by;
    else json["approved_by"] = Json::nullValue;

    json["shelter_id"] = shelter_id;

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    // Computed fields
    json["full_caption"] = getFullCaption();

    return json;
}

Json::Value TikTokPost::toDbJson() const {
    // Same as toJson but formatted for database storage
    return toJson();
}

// ============================================================================
// TIKTOKPOST UTILITY METHODS
// ============================================================================

bool TikTokPost::isEditable() const {
    return status == TikTokPostStatus::DRAFT ||
           status == TikTokPostStatus::SCHEDULED ||
           status == TikTokPostStatus::PENDING_APPROVAL ||
           status == TikTokPostStatus::FAILED;
}

bool TikTokPost::isDeletable() const {
    // Can delete draft, scheduled, pending approval, failed, or expired posts
    // Cannot delete while posting
    return status != TikTokPostStatus::POSTING;
}

bool TikTokPost::isReadyToPublish() const {
    // Must have dog_id
    if (dog_id.empty()) return false;

    // Must have caption
    if (caption.empty()) return false;

    // Must have media content
    if (content_type == TikTokContentType::VIDEO) {
        if (!video_url || video_url->empty()) return false;
    } else {
        if (photo_urls.empty()) return false;
    }

    // Status must be appropriate
    if (status != TikTokPostStatus::DRAFT &&
        status != TikTokPostStatus::SCHEDULED &&
        status != TikTokPostStatus::FAILED) {
        return false;
    }

    return true;
}

std::string TikTokPost::getFullCaption() const {
    std::ostringstream oss;
    oss << caption;

    if (!hashtags.empty()) {
        oss << "\n\n";
        for (const auto& tag : hashtags) {
            // Ensure tag starts with #
            if (!tag.empty() && tag[0] != '#') {
                oss << "#";
            }
            oss << tag << " ";
        }
    }

    return oss.str();
}

std::chrono::seconds TikTokPost::getTimeUntilScheduled() const {
    if (!scheduled_at) {
        return std::chrono::seconds(0);
    }

    auto now = std::chrono::system_clock::now();
    auto diff = *scheduled_at - now;

    return std::chrono::duration_cast<std::chrono::seconds>(diff);
}

} // namespace wtl::content::tiktok
