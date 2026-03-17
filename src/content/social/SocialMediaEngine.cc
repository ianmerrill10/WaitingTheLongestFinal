/**
 * @file SocialMediaEngine.cc
 * @brief Implementation of the Social Media Cross-Posting Engine
 * @see SocialMediaEngine.h for documentation
 */

#include "content/social/SocialMediaEngine.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

// Project includes
#include "content/social/clients/FacebookClient.h"
#include "content/social/clients/InstagramClient.h"
#include "content/social/clients/TwitterClient.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::content::social {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;
using namespace ::wtl::core::utils;
using namespace clients;

// ============================================================================
// CROSS POST RESULT
// ============================================================================

Json::Value CrossPostResult::toJson() const {
    Json::Value json;
    json["post_id"] = post_id;
    json["overall_success"] = overall_success;
    json["all_succeeded"] = all_succeeded;
    json["platforms_attempted"] = platforms_attempted;
    json["platforms_succeeded"] = platforms_succeeded;
    json["platforms_failed"] = platforms_failed;

    Json::Value platforms_json;
    for (const auto& [platform, success] : platform_success) {
        Json::Value p;
        p["success"] = success;
        if (platform_post_ids.count(platform)) {
            p["post_id"] = platform_post_ids.at(platform);
        }
        if (platform_urls.count(platform)) {
            p["url"] = platform_urls.at(platform);
        }
        if (platform_errors.count(platform)) {
            p["error"] = platform_errors.at(platform);
        }
        platforms_json[platformToString(platform)] = p;
    }
    json["platforms"] = platforms_json;

    return json;
}

// ============================================================================
// SOCIAL MEDIA ENGINE - SINGLETON
// ============================================================================

SocialMediaEngine& SocialMediaEngine::getInstance() {
    static SocialMediaEngine instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void SocialMediaEngine::initialize() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Initialize API clients
    FacebookClient::getInstance().initializeFromConfig();
    InstagramClient::getInstance().initializeFromConfig();
    TwitterClient::getInstance().initializeFromConfig();

    // Set up default hashtags
    default_hashtags_[PostType::ADOPTION_SPOTLIGHT] = {
        "AdoptDontShop", "ShelterDog", "RescueDog",
        "AdoptMe", "AdoptAShelterDog", "ShelterPets"
    };
    default_hashtags_[PostType::URGENT_APPEAL] = {
        "UrgentDog", "RescueNeeded", "HelpSaveThem",
        "AdoptDontShop", "ShelterDog", "TimeIsRunningOut"
    };
    default_hashtags_[PostType::WAITING_MILESTONE] = {
        "LongestWaiting", "WaitingForHome", "OverlookedDogs",
        "AdoptMe", "ShelterDog", "EveryDogDeservesAHome"
    };
    default_hashtags_[PostType::SUCCESS_STORY] = {
        "AdoptionSuccess", "HappyTails", "RescuedDog",
        "AdoptDontShop", "GotAdopted", "FureverHome"
    };
    default_hashtags_[PostType::FOSTER_NEEDED] = {
        "FosterNeeded", "FosterSavesLives", "FosterDog",
        "BeAFoster", "ShelterDog", "TemporaryLovePermanentImpact"
    };

    initialized_ = true;
}

void SocialMediaEngine::initializeFromConfig() {
    initialize();
}

bool SocialMediaEngine::isInitialized() const {
    return initialized_;
}

void SocialMediaEngine::shutdown() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (!initialized_) {
        return;
    }
    initialized_ = false;
    default_hashtags_.clear();
}

// ============================================================================
// CROSS-POSTING
// ============================================================================

CrossPostResult SocialMediaEngine::crossPost(
    const SocialPost& post,
    const std::vector<Platform>& platforms) {

    CrossPostResult result;
    result.post_id = post.id.empty() ? generateUUID() : post.id;
    result.overall_success = false;
    result.all_succeeded = true;
    result.platforms_attempted = 0;
    result.platforms_succeeded = 0;
    result.platforms_failed = 0;

    // Use provided platforms or post's platforms
    std::vector<Platform> target_platforms = platforms.empty() ? post.platforms : platforms;

    if (target_platforms.empty()) {
        WTL_CAPTURE_WARNING(
            ErrorCategory::BUSINESS_LOGIC,
            "No target platforms specified for cross-post",
            {{"post_id", result.post_id}}
        );
        return result;
    }

    // Save post first
    SocialPost saved_post = post;
    saved_post.id = result.post_id;
    saved_post.platforms = target_platforms;
    savePost(saved_post);

    // Post to each platform
    for (const auto& platform : target_platforms) {
        result.platforms_attempted++;

        // Get connection for this platform
        auto connection = getDefaultConnection(platform);
        if (!connection) {
            result.platform_success[platform] = false;
            result.platform_errors[platform] = "No connection configured for platform";
            result.platforms_failed++;
            result.all_succeeded = false;
            continue;
        }

        // Format post for platform
        SocialPost formatted = formatForPlatform(saved_post, platform);

        // Post to platform
        auto [success, post_id, url, error] = postToPlatform(formatted, platform, *connection);

        result.platform_success[platform] = success;
        if (success) {
            result.platform_post_ids[platform] = post_id;
            result.platform_urls[platform] = url;
            result.platforms_succeeded++;
            result.overall_success = true;

            // Update saved post with platform info
            saved_post.updatePlatformStatus(platform, PlatformStatus::POSTED, post_id);
        } else {
            result.platform_errors[platform] = error;
            result.platforms_failed++;
            result.all_succeeded = false;

            saved_post.updatePlatformStatus(platform, PlatformStatus::FAILED, "", error);
        }
    }

    // Update post in database
    updatePost(saved_post);

    return result;
}

void SocialMediaEngine::crossPostAsync(
    const SocialPost& post,
    const std::vector<Platform>& platforms,
    std::function<void(const CrossPostResult&)> callback) {

    std::thread([this, post, platforms, callback]() {
        auto result = this->crossPost(post, platforms);
        if (callback) {
            callback(result);
        }
    }).detach();
}

CrossPostResult SocialMediaEngine::crossPostTikTok(
    const std::string& tiktok_url,
    const std::string& dog_id,
    const std::vector<Platform>& platforms) {

    // Create post from TikTok content
    SocialPost post = SocialPostBuilder::forDog(dog_id)
        .withType(PostType::REPOST_TIKTOK)
        .withVideo(tiktok_url)
        .toPlatforms(platforms.empty() ?
            std::vector<Platform>{Platform::FACEBOOK, Platform::INSTAGRAM, Platform::TWITTER} :
            platforms)
        .build();

    // Get dog info for caption
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT name, breed_primary, shelter_id FROM dogs WHERE id = $1",
            dog_id
        );

        if (!result.empty()) {
            std::string name = result[0]["name"].as<std::string>();
            std::string breed = result[0]["breed_primary"].as<std::string>();

            post.primary_text = "Meet " + name + "! This adorable " + breed +
                " is looking for a forever home. "
                "Can you share their story?";
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to fetch dog info for TikTok cross-post: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    // Add hashtags
    post.hashtags = getRecommendedHashtags(dog_id, Platform::INSTAGRAM);

    return crossPost(post);
}

// ============================================================================
// CONTENT FORMATTING
// ============================================================================

SocialPost SocialMediaEngine::formatForPlatform(const SocialPost& post, Platform platform) const {
    SocialPost formatted = post;
    auto config = PlatformConfig::getConfig(platform);

    // Truncate text if needed
    std::string text = formatted.getTextForPlatform(platform);
    if (static_cast<int>(text.length()) > config.max_text_length) {
        text = text.substr(0, config.max_text_length - 3) + "...";
    }
    formatted.platform_text[platform] = text;

    // Limit images
    if (formatted.images.size() > static_cast<size_t>(config.max_images)) {
        formatted.images.resize(config.max_images);
    }

    // Platform-specific formatting
    switch (platform) {
        case Platform::TWITTER:
            // Twitter: 280 chars, keep it punchy
            if (text.length() > 250) {
                // Make room for link if present
                text = text.substr(0, 250) + "...";
                formatted.platform_text[platform] = text;
            }
            break;

        case Platform::INSTAGRAM:
            // Instagram: Add line breaks before hashtags
            {
                std::string hashtags = formatted.getHashtagsForPlatform(platform);
                if (!hashtags.empty() && text.find(hashtags) == std::string::npos) {
                    formatted.platform_text[platform] = text + "\n.\n.\n.\n" + hashtags;
                }
            }
            break;

        case Platform::FACEBOOK:
            // Facebook: Can use longer captions, add call to action
            {
                std::string cta = "\n\nLearn more about adoption at WaitingTheLongest.com";
                formatted.platform_text[platform] = text + cta;
            }
            break;

        case Platform::THREADS:
            // Threads: 500 char limit
            if (text.length() > 480) {
                text = text.substr(0, 480) + "...";
                formatted.platform_text[platform] = text;
            }
            break;

        default:
            break;
    }

    return formatted;
}

std::string SocialMediaEngine::generateCaption(const SocialPost& post, Platform platform) const {
    std::ostringstream caption;

    // Base text
    caption << post.getTextForPlatform(platform);

    // Platform-specific additions
    switch (platform) {
        case Platform::INSTAGRAM:
        case Platform::TIKTOK:
            // Add hashtags
            {
                std::string hashtags = post.getHashtagsForPlatform(platform);
                if (!hashtags.empty()) {
                    caption << "\n.\n.\n.\n" << hashtags;
                }
            }
            break;

        case Platform::TWITTER:
            // Keep it short, add key hashtags inline
            {
                std::vector<std::string> top_hashtags;
                int chars_left = 280 - static_cast<int>(caption.str().length());
                for (const auto& tag : post.hashtags) {
                    if (chars_left > static_cast<int>(tag.length()) + 2) {
                        top_hashtags.push_back(tag);
                        chars_left -= static_cast<int>(tag.length()) + 2;
                        if (top_hashtags.size() >= 3) break;
                    }
                }
                if (!top_hashtags.empty()) {
                    caption << " ";
                    for (const auto& tag : top_hashtags) {
                        caption << "#" << tag << " ";
                    }
                }
            }
            break;

        case Platform::FACEBOOK:
            caption << "\n\nAdopt at WaitingTheLongest.com";
            break;

        default:
            break;
    }

    return caption.str();
}

std::vector<std::string> SocialMediaEngine::getRecommendedHashtags(
    const std::string& dog_id,
    Platform platform) const {

    std::vector<std::string> hashtags;
    auto config = PlatformConfig::getConfig(platform);

    // Start with post type defaults
    hashtags.push_back("AdoptDontShop");
    hashtags.push_back("ShelterDog");
    hashtags.push_back("RescueDog");

    // Get dog info for specific hashtags
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT d.breed_primary, d.size, d.age_category, d.urgency_level, "
            "s.state_code, s.city FROM dogs d "
            "JOIN shelters s ON s.id = d.shelter_id WHERE d.id = $1",
            dog_id
        );

        if (!result.empty()) {
            std::string breed = result[0]["breed_primary"].as<std::string>();
            std::string size = result[0]["size"].as<std::string>();
            std::string age = result[0]["age_category"].as<std::string>();
            std::string urgency = result[0]["urgency_level"].as<std::string>();
            std::string state = result[0]["state_code"].as<std::string>();
            std::string city = result[0]["city"].as<std::string>();

            // Breed hashtag
            std::string breed_tag = breed;
            std::replace(breed_tag.begin(), breed_tag.end(), ' ', '_');
            hashtags.push_back(breed_tag);
            hashtags.push_back(breed_tag + "sOfInstagram");

            // Size
            hashtags.push_back(size + "Dog");

            // Age
            if (age == "puppy") {
                hashtags.push_back("Puppies");
                hashtags.push_back("AdoptAPuppy");
            } else if (age == "senior") {
                hashtags.push_back("SeniorDog");
                hashtags.push_back("SeniorDogAdoption");
            }

            // Urgency
            if (urgency == "critical" || urgency == "high") {
                hashtags.push_back("UrgentDog");
                hashtags.push_back("RescueNeeded");
            }

            // Location
            hashtags.push_back(state + "Dogs");
            std::string city_tag = city;
            std::replace(city_tag.begin(), city_tag.end(), ' ', '_');
            hashtags.push_back(city_tag + "Dogs");
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get dog info for hashtags: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    // Limit to platform max
    if (hashtags.size() > static_cast<size_t>(config.max_hashtags)) {
        hashtags.resize(config.max_hashtags);
    }

    return hashtags;
}

// ============================================================================
// SCHEDULING
// ============================================================================

std::string SocialMediaEngine::schedulePost(
    const SocialPost& post,
    std::chrono::system_clock::time_point scheduled_time) {

    SocialPost scheduled = post;
    scheduled.id = scheduled.id.empty() ? generateUUID() : scheduled.id;
    scheduled.scheduled_at = scheduled_time;
    scheduled.is_immediate = false;

    // Initialize platform status as SCHEDULED
    for (const auto& platform : scheduled.platforms) {
        scheduled.updatePlatformStatus(platform, PlatformStatus::SCHEDULED);
    }

    savePost(scheduled);
    return scheduled.id;
}

std::map<Platform, std::chrono::system_clock::time_point> SocialMediaEngine::scheduleForOptimalTimes(
    const SocialPost& post) {

    std::map<Platform, std::chrono::system_clock::time_point> scheduled_times;

    for (const auto& platform : post.platforms) {
        auto optimal_time = getNextOptimalTime(platform);
        scheduled_times[platform] = optimal_time;
    }

    // Create separate posts for different times if needed
    if (scheduled_times.size() > 1) {
        // For simplicity, use earliest time for all
        auto earliest = std::min_element(
            scheduled_times.begin(), scheduled_times.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; }
        );
        schedulePost(post, earliest->second);
    }

    return scheduled_times;
}

bool SocialMediaEngine::cancelScheduledPost(const std::string& post_id) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "DELETE FROM social_posts WHERE id = $1 AND is_immediate = false AND all_posted = false",
            post_id
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to cancel scheduled post: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
        return false;
    }
}

std::vector<SocialPost> SocialMediaEngine::getScheduledPosts() const {
    std::vector<SocialPost> posts;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT * FROM social_posts "
            "WHERE is_immediate = false AND all_posted = false AND scheduled_at IS NOT NULL "
            "ORDER BY scheduled_at ASC"
        );

        for (const auto& row : result) {
            posts.push_back(SocialPost::fromDbRow(row));
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get scheduled posts: " + std::string(e.what()),
            {}
        );
    }

    return posts;
}

int SocialMediaEngine::processScheduledPosts() {
    int processed = 0;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get posts that are due
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%d %H:%M:%S");

        auto result = txn.exec_params(
            "SELECT * FROM social_posts "
            "WHERE is_immediate = false AND all_posted = false "
            "AND scheduled_at <= $1 "
            "ORDER BY scheduled_at ASC LIMIT 10",
            oss.str()
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            SocialPost post = SocialPost::fromDbRow(row);
            post.is_immediate = true;  // Process now

            auto cross_result = crossPost(post);
            if (cross_result.overall_success) {
                processed++;
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to process scheduled posts: " + std::string(e.what()),
            {}
        );
    }

    return processed;
}

// ============================================================================
// OPTIMAL TIMING
// ============================================================================

std::vector<OptimalPostTime> SocialMediaEngine::getOptimalPostTimes(Platform platform) const {
    std::vector<OptimalPostTime> times;

    // Based on general social media best practices
    // In production, this would use actual analytics data

    switch (platform) {
        case Platform::INSTAGRAM:
            times.push_back({platform, 1, 11, 0.95, "Monday lunch hour"});
            times.push_back({platform, 2, 11, 0.93, "Tuesday lunch hour"});
            times.push_back({platform, 3, 11, 0.92, "Wednesday lunch hour"});
            times.push_back({platform, 0, 10, 0.90, "Sunday morning"});
            times.push_back({platform, 4, 14, 0.88, "Thursday afternoon"});
            break;

        case Platform::FACEBOOK:
            times.push_back({platform, 3, 13, 0.95, "Wednesday afternoon"});
            times.push_back({platform, 4, 13, 0.93, "Thursday afternoon"});
            times.push_back({platform, 5, 12, 0.90, "Friday lunch"});
            times.push_back({platform, 0, 9, 0.88, "Sunday morning"});
            break;

        case Platform::TWITTER:
            times.push_back({platform, 3, 9, 0.95, "Wednesday morning"});
            times.push_back({platform, 2, 10, 0.93, "Tuesday mid-morning"});
            times.push_back({platform, 4, 8, 0.90, "Thursday early morning"});
            break;

        case Platform::TIKTOK:
            times.push_back({platform, 2, 19, 0.95, "Tuesday evening"});
            times.push_back({platform, 4, 19, 0.93, "Thursday evening"});
            times.push_back({platform, 5, 17, 0.90, "Friday late afternoon"});
            break;

        default:
            times.push_back({platform, 3, 12, 0.90, "Wednesday noon"});
            break;
    }

    return times;
}

std::chrono::system_clock::time_point SocialMediaEngine::getNextOptimalTime(Platform platform) const {
    auto optimal_times = getOptimalPostTimes(platform);
    if (optimal_times.empty()) {
        // Default to 24 hours from now
        return std::chrono::system_clock::now() + std::chrono::hours(24);
    }

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&time_t_now);

    int current_dow = now_tm->tm_wday;
    int current_hour = now_tm->tm_hour;

    // Find next optimal time
    for (const auto& opt : optimal_times) {
        int days_until = (opt.day_of_week - current_dow + 7) % 7;
        if (days_until == 0 && opt.hour <= current_hour) {
            days_until = 7;  // Same day but hour passed, next week
        }

        auto target_time = now + std::chrono::hours(days_until * 24);

        // Set to the optimal hour
        auto target_time_t = std::chrono::system_clock::to_time_t(target_time);
        std::tm* target_tm = std::localtime(&target_time_t);
        target_tm->tm_hour = opt.hour;
        target_tm->tm_min = 0;
        target_tm->tm_sec = 0;

        return std::chrono::system_clock::from_time_t(std::mktime(target_tm));
    }

    return std::chrono::system_clock::now() + std::chrono::hours(24);
}

// ============================================================================
// ANALYTICS SYNC
// ============================================================================

int SocialMediaEngine::syncAnalytics(int hours_back) {
    return SocialAnalytics::getInstance().syncRecentAnalytics(hours_back);
}

bool SocialMediaEngine::syncPostAnalytics(const std::string& post_id) {
    return SocialAnalytics::getInstance().syncPostAnalytics(post_id);
}

// ============================================================================
// POST MANAGEMENT
// ============================================================================

std::string SocialMediaEngine::savePost(const SocialPost& post) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string id = post.id.empty() ? generateUUID() : post.id;

        // Format arrays
        std::ostringstream images_arr, platforms_arr, hashtags_arr;
        images_arr << "{";
        for (size_t i = 0; i < post.images.size(); ++i) {
            if (i > 0) images_arr << ",";
            images_arr << "\"" << post.images[i] << "\"";
        }
        images_arr << "}";

        platforms_arr << "{";
        for (size_t i = 0; i < post.platforms.size(); ++i) {
            if (i > 0) platforms_arr << ",";
            platforms_arr << "\"" << platformToString(post.platforms[i]) << "\"";
        }
        platforms_arr << "}";

        hashtags_arr << "{";
        for (size_t i = 0; i < post.hashtags.size(); ++i) {
            if (i > 0) hashtags_arr << ",";
            hashtags_arr << "\"" << post.hashtags[i] << "\"";
        }
        hashtags_arr << "}";

        // Format platform_info as JSON
        Json::Value platform_info_json;
        for (const auto& [platform, info] : post.platform_info) {
            platform_info_json[platformToString(platform)] = info.toJson();
        }
        Json::StreamWriterBuilder writer;
        std::string platform_info_str = Json::writeString(writer, platform_info_json);

        // Format scheduled_at
        std::string scheduled_str;
        if (post.scheduled_at) {
            auto time_t_sched = std::chrono::system_clock::to_time_t(*post.scheduled_at);
            std::ostringstream oss;
            oss << std::put_time(std::gmtime(&time_t_sched), "%Y-%m-%d %H:%M:%S");
            scheduled_str = oss.str();
        }

        txn.exec_params(
            "INSERT INTO social_posts "
            "(id, dog_id, shelter_id, created_by, post_type, primary_text, "
            "images, video_url, thumbnail_url, share_card_url, platforms, hashtags, "
            "scheduled_at, is_immediate, platform_info, all_posted, any_failed, "
            "total_impressions, total_engagements, total_clicks, "
            "resulted_in_adoption, resulted_in_foster, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, "
            "$13, $14, $15, $16, $17, $18, $19, $20, $21, $22, NOW(), NOW()) "
            "ON CONFLICT (id) DO UPDATE SET "
            "primary_text = EXCLUDED.primary_text, "
            "images = EXCLUDED.images, "
            "platforms = EXCLUDED.platforms, "
            "hashtags = EXCLUDED.hashtags, "
            "scheduled_at = EXCLUDED.scheduled_at, "
            "is_immediate = EXCLUDED.is_immediate, "
            "platform_info = EXCLUDED.platform_info, "
            "all_posted = EXCLUDED.all_posted, "
            "any_failed = EXCLUDED.any_failed, "
            "total_impressions = EXCLUDED.total_impressions, "
            "total_engagements = EXCLUDED.total_engagements, "
            "total_clicks = EXCLUDED.total_clicks, "
            "resulted_in_adoption = EXCLUDED.resulted_in_adoption, "
            "resulted_in_foster = EXCLUDED.resulted_in_foster, "
            "updated_at = NOW()",
            id,
            post.dog_id ? *post.dog_id : "",
            post.shelter_id ? *post.shelter_id : "",
            post.created_by,
            postTypeToString(post.post_type),
            post.primary_text,
            images_arr.str(),
            post.video_url ? *post.video_url : "",
            post.thumbnail_url ? *post.thumbnail_url : "",
            post.share_card_url ? *post.share_card_url : "",
            platforms_arr.str(),
            hashtags_arr.str(),
            scheduled_str.empty() ? "" : scheduled_str,
            post.is_immediate,
            platform_info_str,
            post.all_posted,
            post.any_failed,
            post.total_impressions,
            post.total_engagements,
            post.total_clicks,
            post.resulted_in_adoption,
            post.resulted_in_foster
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return id;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to save social post: " + std::string(e.what()),
            {{"post_id", post.id}}
        );
        return "";
    }
}

std::optional<SocialPost> SocialMediaEngine::getPost(const std::string& post_id) const {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM social_posts WHERE id = $1",
            post_id
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return SocialPost::fromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get social post: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
        return std::nullopt;
    }
}

bool SocialMediaEngine::updatePost(const SocialPost& post) {
    return !savePost(post).empty();
}

bool SocialMediaEngine::deletePost(const std::string& post_id) {
    // Get post first
    auto post = getPost(post_id);
    if (!post) {
        return false;
    }

    // Delete from each platform
    for (const auto& [platform, info] : post->platform_info) {
        if (info.status == PlatformStatus::POSTED && !info.platform_post_id.empty()) {
            auto connection = getDefaultConnection(platform);
            if (connection) {
                switch (platform) {
                    case Platform::FACEBOOK:
                        FacebookClient::getInstance().deletePost(
                            info.platform_post_id, connection->access_token);
                        break;
                    case Platform::TWITTER:
                        TwitterClient::getInstance().deleteTweet(
                            info.platform_post_id, connection->access_token);
                        break;
                    // Instagram doesn't support deletion via API
                    default:
                        break;
                }
            }
        }
    }

    // Delete from database
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params("DELETE FROM social_posts WHERE id = $1", post_id);
        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to delete social post: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
        return false;
    }
}

std::vector<SocialPost> SocialMediaEngine::getPostsForDog(const std::string& dog_id) const {
    std::vector<SocialPost> posts;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM social_posts WHERE dog_id = $1 ORDER BY created_at DESC",
            dog_id
        );

        for (const auto& row : result) {
            posts.push_back(SocialPost::fromDbRow(row));
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get posts for dog: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return posts;
}

std::vector<SocialPost> SocialMediaEngine::getRecentPosts(int limit) const {
    std::vector<SocialPost> posts;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM social_posts ORDER BY created_at DESC LIMIT $1",
            limit
        );

        for (const auto& row : result) {
            posts.push_back(SocialPost::fromDbRow(row));
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get recent posts: " + std::string(e.what()),
            {}
        );
    }

    return posts;
}

// ============================================================================
// PLATFORM CONNECTIONS
// ============================================================================

std::optional<PlatformConnection> SocialMediaEngine::getConnection(
    const std::string& user_id,
    Platform platform) const {

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM platform_connections "
            "WHERE user_id = $1 AND platform = $2 AND is_active = true",
            user_id, platformToString(platform)
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return PlatformConnection::fromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get platform connection: " + std::string(e.what()),
            {{"user_id", user_id}, {"platform", platformToString(platform)}}
        );
        return std::nullopt;
    }
}

bool SocialMediaEngine::saveConnection(const PlatformConnection& connection) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto time_t_exp = std::chrono::system_clock::to_time_t(connection.token_expires_at);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t_exp), "%Y-%m-%d %H:%M:%S");

        txn.exec_params(
            "INSERT INTO platform_connections "
            "(id, user_id, platform, access_token, refresh_token, token_expires_at, "
            "platform_user_id, platform_username, page_id, is_active, needs_refresh, "
            "last_used, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, NOW(), NOW(), NOW()) "
            "ON CONFLICT (id) DO UPDATE SET "
            "access_token = EXCLUDED.access_token, "
            "refresh_token = EXCLUDED.refresh_token, "
            "token_expires_at = EXCLUDED.token_expires_at, "
            "is_active = EXCLUDED.is_active, "
            "needs_refresh = EXCLUDED.needs_refresh, "
            "updated_at = NOW()",
            connection.id,
            connection.user_id,
            platformToString(connection.platform),
            connection.access_token,
            connection.refresh_token,
            oss.str(),
            connection.platform_user_id,
            connection.platform_username,
            connection.page_id ? *connection.page_id : "",
            connection.is_active,
            connection.needs_refresh
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to save platform connection: " + std::string(e.what()),
            {}
        );
        return false;
    }
}

std::vector<PlatformConnection> SocialMediaEngine::getUserConnections(
    const std::string& user_id) const {

    std::vector<PlatformConnection> connections;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM platform_connections WHERE user_id = $1 AND is_active = true",
            user_id
        );

        for (const auto& row : result) {
            connections.push_back(PlatformConnection::fromDbRow(row));
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get user connections: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }

    return connections;
}

bool SocialMediaEngine::refreshToken(const std::string& connection_id) {
    // Implementation depends on the platform
    // Would need to call the appropriate client's token refresh method
    return false;
}

// ============================================================================
// AUTO-GENERATION
// ============================================================================

std::optional<SocialPost> SocialMediaEngine::generateUrgentDogPost(const std::string& dog_id) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT d.*, s.name as shelter_name, s.city, s.state_code "
            "FROM dogs d JOIN shelters s ON s.id = d.shelter_id "
            "WHERE d.id = $1",
            dog_id
        );

        if (result.empty()) {
            ConnectionPool::getInstance().release(conn);
            return std::nullopt;
        }

        auto& row = result[0];
        std::string name = row["name"].as<std::string>();
        std::string breed = row["breed_primary"].as<std::string>();
        std::string urgency = row["urgency_level"].as<std::string>();
        std::string shelter = row["shelter_name"].as<std::string>();
        std::string city = row["city"].as<std::string>();
        std::string state = row["state_code"].as<std::string>();

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        // Generate caption based on urgency
        std::ostringstream caption;
        if (urgency == "critical") {
            caption << "URGENT! " << name << " has only hours left! ";
        } else if (urgency == "high") {
            caption << "TIME IS RUNNING OUT for " << name << "! ";
        } else {
            caption << "Please help " << name << " find a home! ";
        }

        caption << "This beautiful " << breed << " is at " << shelter
                << " in " << city << ", " << state << ". ";
        caption << "Can you share their story and help save a life?";

        auto post = SocialPostBuilder::forDog(dog_id)
            .withType(PostType::URGENT_APPEAL)
            .withText(caption.str())
            .toPlatforms({Platform::FACEBOOK, Platform::INSTAGRAM, Platform::TWITTER})
            .withHashtags(getRecommendedHashtags(dog_id, Platform::INSTAGRAM))
            .postImmediately()
            .build();

        return post;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to generate urgent dog post: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
        return std::nullopt;
    }
}

std::optional<SocialPost> SocialMediaEngine::generateMilestonePost(
    const std::string& dog_id,
    const std::string& milestone_type) {

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT d.*, s.name as shelter_name "
            "FROM dogs d JOIN shelters s ON s.id = d.shelter_id "
            "WHERE d.id = $1",
            dog_id
        );

        if (result.empty()) {
            ConnectionPool::getInstance().release(conn);
            return std::nullopt;
        }

        auto& row = result[0];
        std::string name = row["name"].as<std::string>();
        std::string breed = row["breed_primary"].as<std::string>();
        std::string shelter = row["shelter_name"].as<std::string>();

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        std::ostringstream caption;
        if (milestone_type == "1_year") {
            caption << "365 days. One whole year. " << name
                    << " has been waiting at " << shelter
                    << " for someone to choose them. "
                    << "Will today be the day?";
        } else if (milestone_type == "2_years") {
            caption << "TWO YEARS. " << name << " has spent 730 days hoping. "
                    << "This " << breed << " deserves a family. "
                    << "Please share their story.";
        } else {
            caption << name << " has been waiting and waiting. "
                    << "Every share brings them closer to finding home.";
        }

        auto post = SocialPostBuilder::forDog(dog_id)
            .withType(PostType::WAITING_MILESTONE)
            .withText(caption.str())
            .toPlatforms({Platform::FACEBOOK, Platform::INSTAGRAM, Platform::TWITTER})
            .withHashtags(getRecommendedHashtags(dog_id, Platform::INSTAGRAM))
            .build();

        return post;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to generate milestone post: " + std::string(e.what()),
            {{"dog_id", dog_id}, {"milestone", milestone_type}}
        );
        return std::nullopt;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::tuple<bool, std::string, std::string, std::string> SocialMediaEngine::postToPlatform(
    const SocialPost& post,
    Platform platform,
    const PlatformConnection& connection) {

    try {
        switch (platform) {
            case Platform::FACEBOOK: {
                auto& client = FacebookClient::getInstance();
                std::string page_id = connection.page_id.value_or(connection.platform_user_id);

                FacebookPostData data = FacebookPostData::fromSocialPost(post);
                auto result = client.post(page_id, connection.access_token, data);

                return {result.success, result.post_id, result.permalink, result.error_message};
            }

            case Platform::INSTAGRAM: {
                auto& client = InstagramClient::getInstance();

                InstagramPostData data = InstagramPostData::fromSocialPost(post);
                auto result = client.post(connection.platform_user_id, connection.access_token, data);

                return {result.success, result.media_id, result.permalink, result.error_message};
            }

            case Platform::TWITTER: {
                auto& client = TwitterClient::getInstance();

                TwitterPostData data = TwitterPostData::fromSocialPost(post);
                auto result = client.post(connection.access_token, data);

                return {result.success, result.tweet_id, result.tweet_url, result.error_detail};
            }

            default:
                return {false, "", "", "Platform not supported"};
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::EXTERNAL_API,
            "Exception posting to platform: " + std::string(e.what()),
            {{"platform", platformToString(platform)}}
        );
        return {false, "", "", e.what()};
    }
}

std::optional<PlatformConnection> SocialMediaEngine::getDefaultConnection(Platform platform) const {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get the default connection for this platform (e.g., organization's account)
        auto result = txn.exec_params(
            "SELECT * FROM platform_connections "
            "WHERE platform = $1 AND is_active = true "
            "ORDER BY created_at ASC LIMIT 1",
            platformToString(platform)
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return PlatformConnection::fromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get default connection: " + std::string(e.what()),
            {{"platform", platformToString(platform)}}
        );
        return std::nullopt;
    }
}

std::string SocialMediaEngine::generateUUID() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t ab = dis(gen);
    uint64_t cd = dis(gen);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (ab >> 32) << "-";
    oss << std::setw(4) << ((ab >> 16) & 0xFFFF) << "-";
    oss << std::setw(4) << ((ab & 0xFFFF) | 0x4000) << "-";  // Version 4
    oss << std::setw(4) << (((cd >> 48) & 0x3FFF) | 0x8000) << "-";  // Variant
    oss << std::setw(12) << (cd & 0xFFFFFFFFFFFF);

    return oss.str();
}

} // namespace wtl::content::social
