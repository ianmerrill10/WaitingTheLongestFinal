/**
 * @file SocialPost.cc
 * @brief Implementation of SocialPost model and builder
 * @see SocialPost.h for documentation
 */

#include "content/social/SocialPost.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace wtl::content::social {

// ============================================================================
// POST TYPE UTILITIES
// ============================================================================

std::string postTypeToString(PostType type) {
    switch (type) {
        case PostType::ADOPTION_SPOTLIGHT:  return "adoption_spotlight";
        case PostType::URGENT_APPEAL:       return "urgent_appeal";
        case PostType::WAITING_MILESTONE:   return "waiting_milestone";
        case PostType::SUCCESS_STORY:       return "success_story";
        case PostType::FOSTER_NEEDED:       return "foster_needed";
        case PostType::EVENT_PROMOTION:     return "event_promotion";
        case PostType::GENERAL_AWARENESS:   return "general_awareness";
        case PostType::REPOST_TIKTOK:       return "repost_tiktok";
        default:                            return "general_awareness";
    }
}

PostType stringToPostType(const std::string& str) {
    if (str == "adoption_spotlight") return PostType::ADOPTION_SPOTLIGHT;
    if (str == "urgent_appeal") return PostType::URGENT_APPEAL;
    if (str == "waiting_milestone") return PostType::WAITING_MILESTONE;
    if (str == "success_story") return PostType::SUCCESS_STORY;
    if (str == "foster_needed") return PostType::FOSTER_NEEDED;
    if (str == "event_promotion") return PostType::EVENT_PROMOTION;
    if (str == "general_awareness") return PostType::GENERAL_AWARENESS;
    if (str == "repost_tiktok") return PostType::REPOST_TIKTOK;
    return PostType::GENERAL_AWARENESS;  // Default
}

// ============================================================================
// PLATFORM POST INFO
// ============================================================================

Json::Value PlatformPostInfo::toJson() const {
    Json::Value json;
    json["platform"] = platformToString(platform);
    json["status"] = platformStatusToString(status);
    json["platform_post_id"] = platform_post_id;
    json["platform_url"] = platform_url;
    json["error_message"] = error_message;
    json["retry_count"] = retry_count;

    // Format timestamps
    if (posted_at.time_since_epoch().count() > 0) {
        auto time_t_posted = std::chrono::system_clock::to_time_t(posted_at);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t_posted), "%Y-%m-%dT%H:%M:%SZ");
        json["posted_at"] = oss.str();
    }

    if (last_attempt.time_since_epoch().count() > 0) {
        auto time_t_attempt = std::chrono::system_clock::to_time_t(last_attempt);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t_attempt), "%Y-%m-%dT%H:%M:%SZ");
        json["last_attempt"] = oss.str();
    }

    return json;
}

PlatformPostInfo PlatformPostInfo::fromJson(const Json::Value& json) {
    PlatformPostInfo info;
    info.platform = stringToPlatform(json["platform"].asString());
    info.status = stringToPlatformStatus(json["status"].asString());
    info.platform_post_id = json.get("platform_post_id", "").asString();
    info.platform_url = json.get("platform_url", "").asString();
    info.error_message = json.get("error_message", "").asString();
    info.retry_count = json.get("retry_count", 0).asInt();

    // Parse timestamps
    if (json.isMember("posted_at") && !json["posted_at"].asString().empty()) {
        std::string ts = json["posted_at"].asString();
        std::tm tm = {};
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        info.posted_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    if (json.isMember("last_attempt") && !json["last_attempt"].asString().empty()) {
        std::string ts = json["last_attempt"].asString();
        std::tm tm = {};
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        info.last_attempt = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    return info;
}

// ============================================================================
// SOCIAL POST - CONVERSION METHODS
// ============================================================================

Json::Value SocialPost::toJson() const {
    Json::Value json;

    // Identity
    json["id"] = id;
    if (dog_id) json["dog_id"] = *dog_id;
    if (shelter_id) json["shelter_id"] = *shelter_id;
    json["created_by"] = created_by;

    // Post type and content
    json["post_type"] = postTypeToString(post_type);
    json["primary_text"] = primary_text;

    // Platform-specific text
    if (!platform_text.empty()) {
        Json::Value platform_text_json;
        for (const auto& [platform, text] : platform_text) {
            platform_text_json[platformToString(platform)] = text;
        }
        json["platform_text"] = platform_text_json;
    }

    // Media
    Json::Value images_json(Json::arrayValue);
    for (const auto& img : images) {
        images_json.append(img);
    }
    json["images"] = images_json;

    if (video_url) json["video_url"] = *video_url;
    if (thumbnail_url) json["thumbnail_url"] = *thumbnail_url;
    if (share_card_url) json["share_card_url"] = *share_card_url;

    // Targeting
    Json::Value platforms_json(Json::arrayValue);
    for (const auto& p : platforms) {
        platforms_json.append(platformToString(p));
    }
    json["platforms"] = platforms_json;

    Json::Value hashtags_json(Json::arrayValue);
    for (const auto& h : hashtags) {
        hashtags_json.append(h);
    }
    json["hashtags"] = hashtags_json;

    // Scheduling
    if (scheduled_at) {
        json["scheduled_at"] = formatTimestamp(*scheduled_at);
    }
    json["is_immediate"] = is_immediate;

    // Platform info
    Json::Value platform_info_json;
    for (const auto& [platform, info] : platform_info) {
        platform_info_json[platformToString(platform)] = info.toJson();
    }
    json["platform_info"] = platform_info_json;

    // Status
    json["all_posted"] = all_posted;
    json["any_failed"] = any_failed;

    // Analytics
    json["total_impressions"] = total_impressions;
    json["total_engagements"] = total_engagements;
    json["total_clicks"] = total_clicks;
    json["resulted_in_adoption"] = resulted_in_adoption;
    json["resulted_in_foster"] = resulted_in_foster;

    // Metadata
    json["metadata"] = metadata;

    // Timestamps
    json["created_at"] = formatTimestamp(created_at);
    json["updated_at"] = formatTimestamp(updated_at);

    return json;
}

SocialPost SocialPost::fromJson(const Json::Value& json) {
    SocialPost post;

    // Identity
    post.id = json.get("id", "").asString();
    if (json.isMember("dog_id") && !json["dog_id"].isNull()) {
        post.dog_id = json["dog_id"].asString();
    }
    if (json.isMember("shelter_id") && !json["shelter_id"].isNull()) {
        post.shelter_id = json["shelter_id"].asString();
    }
    post.created_by = json.get("created_by", "").asString();

    // Post type and content
    post.post_type = stringToPostType(json.get("post_type", "general_awareness").asString());
    post.primary_text = json.get("primary_text", "").asString();

    // Platform-specific text
    if (json.isMember("platform_text") && json["platform_text"].isObject()) {
        for (const auto& key : json["platform_text"].getMemberNames()) {
            Platform platform = stringToPlatform(key);
            post.platform_text[platform] = json["platform_text"][key].asString();
        }
    }

    // Media
    if (json.isMember("images") && json["images"].isArray()) {
        for (const auto& img : json["images"]) {
            post.images.push_back(img.asString());
        }
    }

    if (json.isMember("video_url") && !json["video_url"].isNull()) {
        post.video_url = json["video_url"].asString();
    }
    if (json.isMember("thumbnail_url") && !json["thumbnail_url"].isNull()) {
        post.thumbnail_url = json["thumbnail_url"].asString();
    }
    if (json.isMember("share_card_url") && !json["share_card_url"].isNull()) {
        post.share_card_url = json["share_card_url"].asString();
    }

    // Targeting
    if (json.isMember("platforms") && json["platforms"].isArray()) {
        for (const auto& p : json["platforms"]) {
            post.platforms.push_back(stringToPlatform(p.asString()));
        }
    }

    if (json.isMember("hashtags") && json["hashtags"].isArray()) {
        for (const auto& h : json["hashtags"]) {
            post.hashtags.push_back(h.asString());
        }
    }

    // Scheduling
    if (json.isMember("scheduled_at") && !json["scheduled_at"].isNull()) {
        post.scheduled_at = parseTimestamp(json["scheduled_at"].asString());
    }
    post.is_immediate = json.get("is_immediate", true).asBool();

    // Platform info
    if (json.isMember("platform_info") && json["platform_info"].isObject()) {
        for (const auto& key : json["platform_info"].getMemberNames()) {
            Platform platform = stringToPlatform(key);
            post.platform_info[platform] = PlatformPostInfo::fromJson(json["platform_info"][key]);
        }
    }

    // Status
    post.all_posted = json.get("all_posted", false).asBool();
    post.any_failed = json.get("any_failed", false).asBool();

    // Analytics
    post.total_impressions = json.get("total_impressions", 0).asInt();
    post.total_engagements = json.get("total_engagements", 0).asInt();
    post.total_clicks = json.get("total_clicks", 0).asInt();
    post.resulted_in_adoption = json.get("resulted_in_adoption", false).asBool();
    post.resulted_in_foster = json.get("resulted_in_foster", false).asBool();

    // Metadata
    if (json.isMember("metadata")) {
        post.metadata = json["metadata"];
    }

    // Timestamps
    if (json.isMember("created_at")) {
        post.created_at = parseTimestamp(json["created_at"].asString());
    } else {
        post.created_at = std::chrono::system_clock::now();
    }

    if (json.isMember("updated_at")) {
        post.updated_at = parseTimestamp(json["updated_at"].asString());
    } else {
        post.updated_at = std::chrono::system_clock::now();
    }

    return post;
}

SocialPost SocialPost::fromDbRow(const pqxx::row& row) {
    SocialPost post;

    post.id = row["id"].as<std::string>();

    if (!row["dog_id"].is_null()) {
        post.dog_id = row["dog_id"].as<std::string>();
    }
    if (!row["shelter_id"].is_null()) {
        post.shelter_id = row["shelter_id"].as<std::string>();
    }

    post.created_by = row["created_by"].as<std::string>();
    post.post_type = stringToPostType(row["post_type"].as<std::string>());
    post.primary_text = row["primary_text"].as<std::string>();

    // Parse JSON columns
    if (!row["platform_text"].is_null()) {
        std::string platform_text_str = row["platform_text"].as<std::string>();
        Json::Reader reader;
        Json::Value platform_text_json;
        if (reader.parse(platform_text_str, platform_text_json)) {
            for (const auto& key : platform_text_json.getMemberNames()) {
                Platform platform = stringToPlatform(key);
                post.platform_text[platform] = platform_text_json[key].asString();
            }
        }
    }

    // Parse arrays
    if (!row["images"].is_null()) {
        post.images = parsePostgresArray(row["images"].as<std::string>());
    }

    if (!row["video_url"].is_null()) {
        post.video_url = row["video_url"].as<std::string>();
    }
    if (!row["thumbnail_url"].is_null()) {
        post.thumbnail_url = row["thumbnail_url"].as<std::string>();
    }
    if (!row["share_card_url"].is_null()) {
        post.share_card_url = row["share_card_url"].as<std::string>();
    }

    // Parse platforms array
    if (!row["platforms"].is_null()) {
        auto platform_strings = parsePostgresArray(row["platforms"].as<std::string>());
        for (const auto& p : platform_strings) {
            post.platforms.push_back(stringToPlatform(p));
        }
    }

    // Parse hashtags
    if (!row["hashtags"].is_null()) {
        post.hashtags = parsePostgresArray(row["hashtags"].as<std::string>());
    }

    // Scheduling
    if (!row["scheduled_at"].is_null()) {
        post.scheduled_at = parseTimestamp(row["scheduled_at"].as<std::string>());
    }
    post.is_immediate = row["is_immediate"].as<bool>();

    // Platform info (stored as JSONB)
    if (!row["platform_info"].is_null()) {
        std::string platform_info_str = row["platform_info"].as<std::string>();
        Json::Reader reader;
        Json::Value platform_info_json;
        if (reader.parse(platform_info_str, platform_info_json)) {
            for (const auto& key : platform_info_json.getMemberNames()) {
                Platform platform = stringToPlatform(key);
                post.platform_info[platform] = PlatformPostInfo::fromJson(platform_info_json[key]);
            }
        }
    }

    // Status
    post.all_posted = row["all_posted"].as<bool>();
    post.any_failed = row["any_failed"].as<bool>();

    // Analytics
    post.total_impressions = row["total_impressions"].as<int>();
    post.total_engagements = row["total_engagements"].as<int>();
    post.total_clicks = row["total_clicks"].as<int>();
    post.resulted_in_adoption = row["resulted_in_adoption"].as<bool>();
    post.resulted_in_foster = row["resulted_in_foster"].as<bool>();

    // Metadata
    if (!row["metadata"].is_null()) {
        std::string metadata_str = row["metadata"].as<std::string>();
        Json::Reader reader;
        reader.parse(metadata_str, post.metadata);
    }

    // Timestamps
    post.created_at = parseTimestamp(row["created_at"].as<std::string>());
    post.updated_at = parseTimestamp(row["updated_at"].as<std::string>());

    return post;
}

// ============================================================================
// SOCIAL POST - HELPER METHODS
// ============================================================================

std::string SocialPost::getTextForPlatform(Platform platform) const {
    // Check for platform-specific override
    auto it = platform_text.find(platform);
    if (it != platform_text.end()) {
        return truncateText(it->second, PlatformConfig::getConfig(platform).max_text_length);
    }

    // Use primary text with platform constraints
    auto config = PlatformConfig::getConfig(platform);
    return truncateText(primary_text, config.max_text_length);
}

std::string SocialPost::getHashtagsForPlatform(Platform platform) const {
    auto config = PlatformConfig::getConfig(platform);
    int max_hashtags = config.max_hashtags;

    std::string result;
    int count = 0;

    for (const auto& hashtag : hashtags) {
        if (count >= max_hashtags) break;

        if (!result.empty()) {
            result += " ";
        }

        // Ensure hashtag starts with #
        if (hashtag[0] != '#') {
            result += "#";
        }
        result += hashtag;
        count++;
    }

    return result;
}

bool SocialPost::isValidForPlatform(Platform platform) const {
    auto config = PlatformConfig::getConfig(platform);

    // Check if platform needs video but we don't have one
    if (platform == Platform::TIKTOK && !video_url) {
        return false;  // TikTok requires video
    }

    // Check if we have required content
    if (primary_text.empty() && platform_text.find(platform) == platform_text.end()) {
        // Some platforms require text
        if (platform == Platform::TWITTER || platform == Platform::THREADS) {
            return images.empty();  // Need either text or images
        }
    }

    // Check image count
    if (!images.empty() && !config.supports_images) {
        return false;
    }

    if (images.size() > static_cast<size_t>(config.max_images) && config.max_images > 0) {
        return false;
    }

    // Video checks
    if (video_url && !config.supports_video) {
        return false;
    }

    return true;
}

Json::Value SocialPost::getStatusSummary() const {
    Json::Value summary;

    int posted_count = 0;
    int pending_count = 0;
    int failed_count = 0;

    for (const auto& [platform, info] : platform_info) {
        switch (info.status) {
            case PlatformStatus::POSTED:
                posted_count++;
                break;
            case PlatformStatus::FAILED:
                failed_count++;
                break;
            default:
                pending_count++;
                break;
        }
    }

    summary["total_platforms"] = static_cast<int>(platforms.size());
    summary["posted_count"] = posted_count;
    summary["pending_count"] = pending_count;
    summary["failed_count"] = failed_count;
    summary["all_posted"] = all_posted;
    summary["any_failed"] = any_failed;

    return summary;
}

void SocialPost::updatePlatformStatus(Platform platform, PlatformStatus status,
                                       const std::string& post_id,
                                       const std::string& error_msg) {
    PlatformPostInfo& info = platform_info[platform];
    info.platform = platform;
    info.status = status;
    info.last_attempt = std::chrono::system_clock::now();

    if (status == PlatformStatus::POSTED) {
        info.platform_post_id = post_id;
        info.posted_at = std::chrono::system_clock::now();
        info.error_message.clear();
    } else if (status == PlatformStatus::FAILED) {
        info.error_message = error_msg;
        info.retry_count++;
    }

    // Update overall status
    all_posted = true;
    any_failed = false;

    for (const auto& p : platforms) {
        auto it = platform_info.find(p);
        if (it == platform_info.end() || it->second.status != PlatformStatus::POSTED) {
            all_posted = false;
        }
        if (it != platform_info.end() && it->second.status == PlatformStatus::FAILED) {
            any_failed = true;
        }
    }

    updated_at = std::chrono::system_clock::now();
}

void SocialPost::addAnalytics(Platform platform, int impressions, int engagements, int clicks) {
    total_impressions += impressions;
    total_engagements += engagements;
    total_clicks += clicks;
    updated_at = std::chrono::system_clock::now();
}

// ============================================================================
// SOCIAL POST - PRIVATE HELPER METHODS
// ============================================================================

std::string SocialPost::truncateText(const std::string& text, int max_length) {
    if (static_cast<int>(text.length()) <= max_length) {
        return text;
    }

    // Truncate and add ellipsis
    return text.substr(0, max_length - 3) + "...";
}

std::chrono::system_clock::time_point SocialPost::parseTimestamp(const std::string& timestamp) {
    std::tm tm = {};
    std::istringstream ss(timestamp);

    // Try ISO 8601 format first
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) {
        // Try PostgreSQL format
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string SocialPost::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::vector<std::string> SocialPost::parsePostgresArray(const std::string& array_str) {
    std::vector<std::string> result;

    if (array_str.empty() || array_str == "{}") {
        return result;
    }

    // Remove braces
    std::string content = array_str;
    if (content.front() == '{') content = content.substr(1);
    if (content.back() == '}') content = content.substr(0, content.length() - 1);

    // Split by comma
    std::istringstream iss(content);
    std::string item;
    while (std::getline(iss, item, ',')) {
        // Trim whitespace and quotes
        while (!item.empty() && (item.front() == ' ' || item.front() == '"')) {
            item = item.substr(1);
        }
        while (!item.empty() && (item.back() == ' ' || item.back() == '"')) {
            item = item.substr(0, item.length() - 1);
        }
        if (!item.empty()) {
            result.push_back(item);
        }
    }

    return result;
}

std::string SocialPost::formatPostgresArray(const std::vector<std::string>& vec) {
    if (vec.empty()) {
        return "{}";
    }

    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << vec[i] << "\"";
    }
    oss << "}";
    return oss.str();
}

// ============================================================================
// SOCIAL POST BUILDER
// ============================================================================

SocialPostBuilder SocialPostBuilder::forDog(const std::string& dog_id) {
    SocialPostBuilder builder;
    builder.post_.dog_id = dog_id;
    builder.post_.post_type = PostType::ADOPTION_SPOTLIGHT;
    builder.post_.is_immediate = true;
    builder.post_.all_posted = false;
    builder.post_.any_failed = false;
    builder.post_.total_impressions = 0;
    builder.post_.total_engagements = 0;
    builder.post_.total_clicks = 0;
    builder.post_.resulted_in_adoption = false;
    builder.post_.resulted_in_foster = false;
    builder.post_.created_at = std::chrono::system_clock::now();
    builder.post_.updated_at = std::chrono::system_clock::now();
    return builder;
}

SocialPostBuilder SocialPostBuilder::forShelter(const std::string& shelter_id) {
    SocialPostBuilder builder;
    builder.post_.shelter_id = shelter_id;
    builder.post_.post_type = PostType::GENERAL_AWARENESS;
    builder.post_.is_immediate = true;
    builder.post_.all_posted = false;
    builder.post_.any_failed = false;
    builder.post_.total_impressions = 0;
    builder.post_.total_engagements = 0;
    builder.post_.total_clicks = 0;
    builder.post_.resulted_in_adoption = false;
    builder.post_.resulted_in_foster = false;
    builder.post_.created_at = std::chrono::system_clock::now();
    builder.post_.updated_at = std::chrono::system_clock::now();
    return builder;
}

SocialPostBuilder& SocialPostBuilder::withType(PostType type) {
    post_.post_type = type;
    return *this;
}

SocialPostBuilder& SocialPostBuilder::withText(const std::string& text) {
    post_.primary_text = text;
    return *this;
}

SocialPostBuilder& SocialPostBuilder::withPlatformText(Platform platform, const std::string& text) {
    post_.platform_text[platform] = text;
    return *this;
}

SocialPostBuilder& SocialPostBuilder::withImage(const std::string& image_url) {
    post_.images.push_back(image_url);
    return *this;
}

SocialPostBuilder& SocialPostBuilder::withImages(const std::vector<std::string>& image_urls) {
    post_.images.insert(post_.images.end(), image_urls.begin(), image_urls.end());
    return *this;
}

SocialPostBuilder& SocialPostBuilder::withVideo(const std::string& video_url) {
    post_.video_url = video_url;
    return *this;
}

SocialPostBuilder& SocialPostBuilder::withShareCard(const std::string& card_url) {
    post_.share_card_url = card_url;
    return *this;
}

SocialPostBuilder& SocialPostBuilder::toplatform(Platform platform) {
    post_.platforms.push_back(platform);
    return *this;
}

SocialPostBuilder& SocialPostBuilder::toPlatforms(const std::vector<Platform>& platforms) {
    post_.platforms.insert(post_.platforms.end(), platforms.begin(), platforms.end());
    return *this;
}

SocialPostBuilder& SocialPostBuilder::withHashtag(const std::string& hashtag) {
    post_.hashtags.push_back(hashtag);
    return *this;
}

SocialPostBuilder& SocialPostBuilder::withHashtags(const std::vector<std::string>& hashtags) {
    post_.hashtags.insert(post_.hashtags.end(), hashtags.begin(), hashtags.end());
    return *this;
}

SocialPostBuilder& SocialPostBuilder::scheduleAt(std::chrono::system_clock::time_point time) {
    post_.scheduled_at = time;
    post_.is_immediate = false;
    return *this;
}

SocialPostBuilder& SocialPostBuilder::postImmediately() {
    post_.scheduled_at = std::nullopt;
    post_.is_immediate = true;
    return *this;
}

SocialPostBuilder& SocialPostBuilder::createdBy(const std::string& user_id) {
    post_.created_by = user_id;
    return *this;
}

SocialPostBuilder& SocialPostBuilder::withMetadata(const Json::Value& metadata) {
    post_.metadata = metadata;
    return *this;
}

SocialPost SocialPostBuilder::build() {
    // Initialize platform info for all target platforms
    for (const auto& platform : post_.platforms) {
        if (post_.platform_info.find(platform) == post_.platform_info.end()) {
            PlatformPostInfo info;
            info.platform = platform;
            info.status = PlatformStatus::PENDING;
            info.retry_count = 0;
            post_.platform_info[platform] = info;
        }
    }

    return post_;
}

} // namespace wtl::content::social
