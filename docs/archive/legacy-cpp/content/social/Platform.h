/**
 * @file Platform.h
 * @brief Social media platform enumeration and configuration for cross-posting
 *
 * PURPOSE:
 * Defines the supported social media platforms and their specific configurations
 * for the cross-posting engine. Each platform has different requirements for
 * content length, media support, and API endpoints.
 *
 * USAGE:
 * Platform platform = Platform::INSTAGRAM;
 * auto config = PlatformConfig::getConfig(platform);
 * if (config.supports_video) {
 *     // Post video content
 * }
 *
 * DEPENDENCIES:
 * - Standard library (string, vector)
 * - jsoncpp (JSON serialization)
 *
 * MODIFICATION GUIDE:
 * - Add new platforms to the Platform enum
 * - Add configuration in PlatformConfig::getConfig()
 * - Update platformToString() and stringToPlatform()
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

namespace wtl::content::social {

// ============================================================================
// PLATFORM ENUMERATION
// ============================================================================

/**
 * @enum Platform
 * @brief Supported social media platforms for cross-posting
 */
enum class Platform {
    TIKTOK = 0,           ///< TikTok - short-form video
    FACEBOOK,             ///< Facebook - general social
    INSTAGRAM,            ///< Instagram - photo/video sharing
    TWITTER,              ///< Twitter/X - microblogging
    THREADS,              ///< Threads by Meta - text-focused
    YOUTUBE,              ///< YouTube - long-form video
    PINTEREST,            ///< Pinterest - image sharing
    LINKEDIN              ///< LinkedIn - professional network
};

// ============================================================================
// PLATFORM CONFIGURATION
// ============================================================================

/**
 * @struct PlatformConfig
 * @brief Configuration and capabilities for a social media platform
 */
struct PlatformConfig {
    // ========================================================================
    // IDENTITY
    // ========================================================================

    Platform platform;                    ///< The platform this config is for
    std::string name;                     ///< Human-readable platform name
    std::string code;                     ///< Short code (fb, ig, tw, etc.)

    // ========================================================================
    // CONTENT LIMITS
    // ========================================================================

    int max_text_length;                  ///< Maximum text/caption length
    int max_hashtags;                     ///< Maximum number of hashtags
    int max_images;                       ///< Maximum images per post
    int max_video_length_seconds;         ///< Maximum video duration in seconds
    int max_video_size_mb;                ///< Maximum video file size in MB

    // ========================================================================
    // CAPABILITIES
    // ========================================================================

    bool supports_video;                  ///< Whether platform supports video
    bool supports_images;                 ///< Whether platform supports images
    bool supports_carousel;               ///< Whether platform supports multi-image carousel
    bool supports_stories;                ///< Whether platform supports stories
    bool supports_reels;                  ///< Whether platform supports short video (reels/shorts)
    bool supports_threads;                ///< Whether platform supports threaded posts
    bool supports_scheduling;             ///< Whether platform API supports scheduling
    bool supports_analytics;              ///< Whether platform provides analytics API

    // ========================================================================
    // API CONFIGURATION
    // ========================================================================

    std::string api_endpoint;             ///< Base API endpoint URL
    std::string api_version;              ///< API version (e.g., "v18.0" for Facebook)
    int rate_limit_per_hour;              ///< API rate limit (requests per hour)
    int rate_limit_per_day;               ///< API rate limit (requests per day)

    // ========================================================================
    // OPTIMAL CONTENT SETTINGS
    // ========================================================================

    int optimal_image_width;              ///< Recommended image width
    int optimal_image_height;             ///< Recommended image height
    std::string optimal_aspect_ratio;     ///< Recommended aspect ratio (e.g., "1:1", "9:16")

    // ========================================================================
    // METHODS
    // ========================================================================

    /**
     * @brief Get configuration for a specific platform
     * @param platform The platform to get config for
     * @return PlatformConfig The configuration
     */
    static PlatformConfig getConfig(Platform platform);

    /**
     * @brief Get all platform configurations
     * @return Vector of all platform configs
     */
    static std::vector<PlatformConfig> getAllConfigs();

    /**
     * @brief Get configurations for platforms that support a feature
     * @param supports_video Filter for video support
     * @param supports_images Filter for image support
     * @return Vector of matching platform configs
     */
    static std::vector<PlatformConfig> getConfigsWithCapabilities(
        bool supports_video,
        bool supports_images);

    /**
     * @brief Convert to JSON representation
     * @return Json::Value The config as JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Create from JSON
     * @param json The JSON data
     * @return PlatformConfig The parsed config
     */
    static PlatformConfig fromJson(const Json::Value& json);
};

// ============================================================================
// PLATFORM STATUS
// ============================================================================

/**
 * @enum PlatformStatus
 * @brief Status of a post on a specific platform
 */
enum class PlatformStatus {
    PENDING = 0,          ///< Not yet posted
    SCHEDULED,            ///< Scheduled for future posting
    POSTING,              ///< Currently being posted
    POSTED,               ///< Successfully posted
    FAILED,               ///< Posting failed
    DELETED,              ///< Deleted from platform
    RATE_LIMITED          ///< Waiting due to rate limiting
};

// ============================================================================
// PLATFORM CONNECTION
// ============================================================================

/**
 * @struct PlatformConnection
 * @brief OAuth connection to a social media platform
 */
struct PlatformConnection {
    std::string id;                       ///< UUID
    std::string user_id;                  ///< User who owns this connection
    Platform platform;                    ///< The connected platform

    // OAuth tokens
    std::string access_token;             ///< Current access token
    std::string refresh_token;            ///< Refresh token (if applicable)
    std::chrono::system_clock::time_point token_expires_at;  ///< Token expiration

    // Platform-specific IDs
    std::string platform_user_id;         ///< User ID on the platform
    std::string platform_username;        ///< Username on the platform
    std::optional<std::string> page_id;   ///< Page/business ID (for Facebook/Instagram)

    // Status
    bool is_active;                       ///< Whether connection is active
    bool needs_refresh;                   ///< Whether token needs refresh
    std::chrono::system_clock::time_point last_used;  ///< Last time used

    // Timestamps
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Convert to JSON (excludes sensitive tokens)
     * @return Json::Value The connection as JSON
     */
    Json::Value toJson() const;

    /**
     * @brief Convert to JSON with tokens (for internal use)
     * @return Json::Value The full connection as JSON
     */
    Json::Value toJsonWithTokens() const;

    /**
     * @brief Create from database row
     * @param row The database row
     * @return PlatformConnection The parsed connection
     */
    static PlatformConnection fromDbRow(const pqxx::row& row);

    /**
     * @brief Check if token is expired or about to expire
     * @param margin_seconds Margin before actual expiration (default 5 minutes)
     * @return bool True if token needs refresh
     */
    bool isTokenExpired(int margin_seconds = 300) const;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Convert Platform enum to string
 * @param platform The platform
 * @return String representation
 */
std::string platformToString(Platform platform);

/**
 * @brief Convert string to Platform enum
 * @param str The string (case-insensitive)
 * @return Platform The parsed platform
 * @throws std::invalid_argument If string is not a valid platform
 */
Platform stringToPlatform(const std::string& str);

/**
 * @brief Convert PlatformStatus enum to string
 * @param status The status
 * @return String representation
 */
std::string platformStatusToString(PlatformStatus status);

/**
 * @brief Convert string to PlatformStatus enum
 * @param str The string
 * @return PlatformStatus The parsed status
 */
PlatformStatus stringToPlatformStatus(const std::string& str);

/**
 * @brief Get all supported platforms as strings
 * @return Vector of platform names
 */
std::vector<std::string> getAllPlatformStrings();

/**
 * @brief Get all supported platforms
 * @return Vector of Platform enums
 */
std::vector<Platform> getAllPlatforms();

} // namespace wtl::content::social
