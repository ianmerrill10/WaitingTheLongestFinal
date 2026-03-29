/**
 * @file Platform.cc
 * @brief Implementation of platform configuration and utilities
 * @see Platform.h for documentation
 */

#include "content/social/Platform.h"

// Standard library includes
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <stdexcept>
#include <sstream>

namespace wtl::content::social {

// ============================================================================
// PLATFORM CONFIG - STATIC CONFIGURATIONS
// ============================================================================

PlatformConfig PlatformConfig::getConfig(Platform platform) {
    PlatformConfig config;
    config.platform = platform;

    switch (platform) {
        case Platform::TIKTOK:
            config.name = "TikTok";
            config.code = "tiktok";
            config.max_text_length = 2200;
            config.max_hashtags = 30;
            config.max_images = 0;  // TikTok is video-only
            config.max_video_length_seconds = 600;  // 10 minutes
            config.max_video_size_mb = 287;
            config.supports_video = true;
            config.supports_images = false;
            config.supports_carousel = false;
            config.supports_stories = true;
            config.supports_reels = true;
            config.supports_threads = false;
            config.supports_scheduling = false;
            config.supports_analytics = true;
            config.api_endpoint = "https://open.tiktokapis.com";
            config.api_version = "v2";
            config.rate_limit_per_hour = 1000;
            config.rate_limit_per_day = 10000;
            config.optimal_image_width = 1080;
            config.optimal_image_height = 1920;
            config.optimal_aspect_ratio = "9:16";
            break;

        case Platform::FACEBOOK:
            config.name = "Facebook";
            config.code = "fb";
            config.max_text_length = 63206;
            config.max_hashtags = 30;
            config.max_images = 10;
            config.max_video_length_seconds = 14400;  // 4 hours
            config.max_video_size_mb = 4000;  // 4GB
            config.supports_video = true;
            config.supports_images = true;
            config.supports_carousel = true;
            config.supports_stories = true;
            config.supports_reels = true;
            config.supports_threads = false;
            config.supports_scheduling = true;
            config.supports_analytics = true;
            config.api_endpoint = "https://graph.facebook.com";
            config.api_version = "v18.0";
            config.rate_limit_per_hour = 200;
            config.rate_limit_per_day = 4800;
            config.optimal_image_width = 1200;
            config.optimal_image_height = 630;
            config.optimal_aspect_ratio = "1.91:1";
            break;

        case Platform::INSTAGRAM:
            config.name = "Instagram";
            config.code = "ig";
            config.max_text_length = 2200;
            config.max_hashtags = 30;
            config.max_images = 10;
            config.max_video_length_seconds = 5400;  // 90 minutes for feed
            config.max_video_size_mb = 3600;  // 3.6GB
            config.supports_video = true;
            config.supports_images = true;
            config.supports_carousel = true;
            config.supports_stories = true;
            config.supports_reels = true;
            config.supports_threads = false;
            config.supports_scheduling = true;
            config.supports_analytics = true;
            config.api_endpoint = "https://graph.facebook.com";  // Uses Facebook Graph API
            config.api_version = "v18.0";
            config.rate_limit_per_hour = 200;
            config.rate_limit_per_day = 4800;
            config.optimal_image_width = 1080;
            config.optimal_image_height = 1080;
            config.optimal_aspect_ratio = "1:1";
            break;

        case Platform::TWITTER:
            config.name = "Twitter/X";
            config.code = "tw";
            config.max_text_length = 280;  // 25000 for Twitter Blue
            config.max_hashtags = 10;
            config.max_images = 4;
            config.max_video_length_seconds = 140;  // 2:20
            config.max_video_size_mb = 512;
            config.supports_video = true;
            config.supports_images = true;
            config.supports_carousel = false;
            config.supports_stories = false;
            config.supports_reels = false;
            config.supports_threads = true;
            config.supports_scheduling = true;
            config.supports_analytics = true;
            config.api_endpoint = "https://api.twitter.com";
            config.api_version = "2";
            config.rate_limit_per_hour = 100;
            config.rate_limit_per_day = 500;
            config.optimal_image_width = 1200;
            config.optimal_image_height = 675;
            config.optimal_aspect_ratio = "16:9";
            break;

        case Platform::THREADS:
            config.name = "Threads";
            config.code = "threads";
            config.max_text_length = 500;
            config.max_hashtags = 5;
            config.max_images = 10;
            config.max_video_length_seconds = 300;  // 5 minutes
            config.max_video_size_mb = 1000;
            config.supports_video = true;
            config.supports_images = true;
            config.supports_carousel = true;
            config.supports_stories = false;
            config.supports_reels = false;
            config.supports_threads = true;
            config.supports_scheduling = false;
            config.supports_analytics = true;
            config.api_endpoint = "https://graph.threads.net";
            config.api_version = "v1.0";
            config.rate_limit_per_hour = 250;
            config.rate_limit_per_day = 1000;
            config.optimal_image_width = 1080;
            config.optimal_image_height = 1080;
            config.optimal_aspect_ratio = "1:1";
            break;

        case Platform::YOUTUBE:
            config.name = "YouTube";
            config.code = "yt";
            config.max_text_length = 5000;  // Description
            config.max_hashtags = 15;
            config.max_images = 0;  // YouTube is video
            config.max_video_length_seconds = 43200;  // 12 hours
            config.max_video_size_mb = 256000;  // 256GB
            config.supports_video = true;
            config.supports_images = false;
            config.supports_carousel = false;
            config.supports_stories = false;
            config.supports_reels = true;  // YouTube Shorts
            config.supports_threads = false;
            config.supports_scheduling = true;
            config.supports_analytics = true;
            config.api_endpoint = "https://www.googleapis.com/youtube";
            config.api_version = "v3";
            config.rate_limit_per_hour = 10000;
            config.rate_limit_per_day = 100000;
            config.optimal_image_width = 1920;
            config.optimal_image_height = 1080;
            config.optimal_aspect_ratio = "16:9";
            break;

        case Platform::PINTEREST:
            config.name = "Pinterest";
            config.code = "pin";
            config.max_text_length = 500;  // Description
            config.max_hashtags = 20;
            config.max_images = 5;
            config.max_video_length_seconds = 900;  // 15 minutes
            config.max_video_size_mb = 2000;
            config.supports_video = true;
            config.supports_images = true;
            config.supports_carousel = true;
            config.supports_stories = true;  // Idea Pins
            config.supports_reels = false;
            config.supports_threads = false;
            config.supports_scheduling = true;
            config.supports_analytics = true;
            config.api_endpoint = "https://api.pinterest.com";
            config.api_version = "v5";
            config.rate_limit_per_hour = 1000;
            config.rate_limit_per_day = 10000;
            config.optimal_image_width = 1000;
            config.optimal_image_height = 1500;
            config.optimal_aspect_ratio = "2:3";
            break;

        case Platform::LINKEDIN:
            config.name = "LinkedIn";
            config.code = "li";
            config.max_text_length = 3000;
            config.max_hashtags = 5;
            config.max_images = 9;
            config.max_video_length_seconds = 600;  // 10 minutes
            config.max_video_size_mb = 5000;  // 5GB
            config.supports_video = true;
            config.supports_images = true;
            config.supports_carousel = true;
            config.supports_stories = false;
            config.supports_reels = false;
            config.supports_threads = false;
            config.supports_scheduling = true;
            config.supports_analytics = true;
            config.api_endpoint = "https://api.linkedin.com";
            config.api_version = "v2";
            config.rate_limit_per_hour = 100;
            config.rate_limit_per_day = 1000;
            config.optimal_image_width = 1200;
            config.optimal_image_height = 627;
            config.optimal_aspect_ratio = "1.91:1";
            break;
    }

    return config;
}

std::vector<PlatformConfig> PlatformConfig::getAllConfigs() {
    std::vector<PlatformConfig> configs;
    configs.push_back(getConfig(Platform::TIKTOK));
    configs.push_back(getConfig(Platform::FACEBOOK));
    configs.push_back(getConfig(Platform::INSTAGRAM));
    configs.push_back(getConfig(Platform::TWITTER));
    configs.push_back(getConfig(Platform::THREADS));
    configs.push_back(getConfig(Platform::YOUTUBE));
    configs.push_back(getConfig(Platform::PINTEREST));
    configs.push_back(getConfig(Platform::LINKEDIN));
    return configs;
}

std::vector<PlatformConfig> PlatformConfig::getConfigsWithCapabilities(
    bool supports_video,
    bool supports_images) {

    std::vector<PlatformConfig> result;
    for (const auto& config : getAllConfigs()) {
        bool matches = true;
        if (supports_video && !config.supports_video) matches = false;
        if (supports_images && !config.supports_images) matches = false;
        if (matches) {
            result.push_back(config);
        }
    }
    return result;
}

Json::Value PlatformConfig::toJson() const {
    Json::Value json;
    json["platform"] = platformToString(platform);
    json["name"] = name;
    json["code"] = code;
    json["max_text_length"] = max_text_length;
    json["max_hashtags"] = max_hashtags;
    json["max_images"] = max_images;
    json["max_video_length_seconds"] = max_video_length_seconds;
    json["max_video_size_mb"] = max_video_size_mb;
    json["supports_video"] = supports_video;
    json["supports_images"] = supports_images;
    json["supports_carousel"] = supports_carousel;
    json["supports_stories"] = supports_stories;
    json["supports_reels"] = supports_reels;
    json["supports_threads"] = supports_threads;
    json["supports_scheduling"] = supports_scheduling;
    json["supports_analytics"] = supports_analytics;
    json["api_endpoint"] = api_endpoint;
    json["api_version"] = api_version;
    json["rate_limit_per_hour"] = rate_limit_per_hour;
    json["rate_limit_per_day"] = rate_limit_per_day;
    json["optimal_image_width"] = optimal_image_width;
    json["optimal_image_height"] = optimal_image_height;
    json["optimal_aspect_ratio"] = optimal_aspect_ratio;
    return json;
}

PlatformConfig PlatformConfig::fromJson(const Json::Value& json) {
    Platform platform = stringToPlatform(json["platform"].asString());
    PlatformConfig config = getConfig(platform);

    // Override with any custom values from JSON
    if (json.isMember("max_text_length")) {
        config.max_text_length = json["max_text_length"].asInt();
    }
    if (json.isMember("rate_limit_per_hour")) {
        config.rate_limit_per_hour = json["rate_limit_per_hour"].asInt();
    }
    if (json.isMember("rate_limit_per_day")) {
        config.rate_limit_per_day = json["rate_limit_per_day"].asInt();
    }

    return config;
}

// ============================================================================
// PLATFORM CONNECTION
// ============================================================================

Json::Value PlatformConnection::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["user_id"] = user_id;
    json["platform"] = platformToString(platform);
    json["platform_user_id"] = platform_user_id;
    json["platform_username"] = platform_username;
    if (page_id) {
        json["page_id"] = *page_id;
    }
    json["is_active"] = is_active;
    json["needs_refresh"] = needs_refresh;
    // Don't include tokens in regular JSON
    return json;
}

Json::Value PlatformConnection::toJsonWithTokens() const {
    Json::Value json = toJson();
    json["access_token"] = access_token;
    json["refresh_token"] = refresh_token;
    // Add token expiration as ISO timestamp
    auto epoch = token_expires_at.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
    json["token_expires_at"] = static_cast<Json::Int64>(seconds);
    return json;
}

PlatformConnection PlatformConnection::fromDbRow(const pqxx::row& row) {
    PlatformConnection conn;

    conn.id = row["id"].as<std::string>();
    conn.user_id = row["user_id"].as<std::string>();
    conn.platform = stringToPlatform(row["platform"].as<std::string>());
    conn.access_token = row["access_token"].as<std::string>();

    if (!row["refresh_token"].is_null()) {
        conn.refresh_token = row["refresh_token"].as<std::string>();
    }

    // Parse timestamp
    std::string expires_str = row["token_expires_at"].as<std::string>();
    // Assuming ISO format, convert to time_point
    std::tm tm = {};
    std::istringstream ss(expires_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    conn.token_expires_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    conn.platform_user_id = row["platform_user_id"].as<std::string>();
    conn.platform_username = row["platform_username"].as<std::string>();

    if (!row["page_id"].is_null()) {
        conn.page_id = row["page_id"].as<std::string>();
    }

    conn.is_active = row["is_active"].as<bool>();
    conn.needs_refresh = row["needs_refresh"].as<bool>();

    // Parse other timestamps similarly
    std::string last_used_str = row["last_used"].as<std::string>();
    std::tm tm_last = {};
    std::istringstream ss_last(last_used_str);
    ss_last >> std::get_time(&tm_last, "%Y-%m-%d %H:%M:%S");
    conn.last_used = std::chrono::system_clock::from_time_t(std::mktime(&tm_last));

    std::string created_str = row["created_at"].as<std::string>();
    std::tm tm_created = {};
    std::istringstream ss_created(created_str);
    ss_created >> std::get_time(&tm_created, "%Y-%m-%d %H:%M:%S");
    conn.created_at = std::chrono::system_clock::from_time_t(std::mktime(&tm_created));

    std::string updated_str = row["updated_at"].as<std::string>();
    std::tm tm_updated = {};
    std::istringstream ss_updated(updated_str);
    ss_updated >> std::get_time(&tm_updated, "%Y-%m-%d %H:%M:%S");
    conn.updated_at = std::chrono::system_clock::from_time_t(std::mktime(&tm_updated));

    return conn;
}

bool PlatformConnection::isTokenExpired(int margin_seconds) const {
    auto now = std::chrono::system_clock::now();
    auto expiry_with_margin = token_expires_at - std::chrono::seconds(margin_seconds);
    return now >= expiry_with_margin;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::string platformToString(Platform platform) {
    switch (platform) {
        case Platform::TIKTOK:    return "tiktok";
        case Platform::FACEBOOK:  return "facebook";
        case Platform::INSTAGRAM: return "instagram";
        case Platform::TWITTER:   return "twitter";
        case Platform::THREADS:   return "threads";
        case Platform::YOUTUBE:   return "youtube";
        case Platform::PINTEREST: return "pinterest";
        case Platform::LINKEDIN:  return "linkedin";
        default:                  return "unknown";
    }
}

Platform stringToPlatform(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "tiktok" || lower == "tk") return Platform::TIKTOK;
    if (lower == "facebook" || lower == "fb") return Platform::FACEBOOK;
    if (lower == "instagram" || lower == "ig" || lower == "insta") return Platform::INSTAGRAM;
    if (lower == "twitter" || lower == "tw" || lower == "x") return Platform::TWITTER;
    if (lower == "threads") return Platform::THREADS;
    if (lower == "youtube" || lower == "yt") return Platform::YOUTUBE;
    if (lower == "pinterest" || lower == "pin") return Platform::PINTEREST;
    if (lower == "linkedin" || lower == "li") return Platform::LINKEDIN;

    throw std::invalid_argument("Unknown platform: " + str);
}

std::string platformStatusToString(PlatformStatus status) {
    switch (status) {
        case PlatformStatus::PENDING:      return "pending";
        case PlatformStatus::SCHEDULED:    return "scheduled";
        case PlatformStatus::POSTING:      return "posting";
        case PlatformStatus::POSTED:       return "posted";
        case PlatformStatus::FAILED:       return "failed";
        case PlatformStatus::DELETED:      return "deleted";
        case PlatformStatus::RATE_LIMITED: return "rate_limited";
        default:                           return "unknown";
    }
}

PlatformStatus stringToPlatformStatus(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "pending") return PlatformStatus::PENDING;
    if (lower == "scheduled") return PlatformStatus::SCHEDULED;
    if (lower == "posting") return PlatformStatus::POSTING;
    if (lower == "posted") return PlatformStatus::POSTED;
    if (lower == "failed") return PlatformStatus::FAILED;
    if (lower == "deleted") return PlatformStatus::DELETED;
    if (lower == "rate_limited") return PlatformStatus::RATE_LIMITED;

    return PlatformStatus::PENDING;  // Default
}

std::vector<std::string> getAllPlatformStrings() {
    return {
        "tiktok", "facebook", "instagram", "twitter",
        "threads", "youtube", "pinterest", "linkedin"
    };
}

std::vector<Platform> getAllPlatforms() {
    return {
        Platform::TIKTOK, Platform::FACEBOOK, Platform::INSTAGRAM,
        Platform::TWITTER, Platform::THREADS, Platform::YOUTUBE,
        Platform::PINTEREST, Platform::LINKEDIN
    };
}

} // namespace wtl::content::social
