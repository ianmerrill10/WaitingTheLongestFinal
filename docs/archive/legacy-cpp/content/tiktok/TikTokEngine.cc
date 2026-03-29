/**
 * @file TikTokEngine.cc
 * @brief Implementation of TikTok Automation Engine
 * @see TikTokEngine.h for documentation
 */

#include "TikTokEngine.h"

// Standard library includes
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/models/Dog.h"
#include "core/utils/Config.h"

namespace wtl::content::tiktok {

// ============================================================================
// RESULT STRUCTURE IMPLEMENTATIONS
// ============================================================================

Json::Value PostGenerationResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    if (post) {
        json["post"] = post->toJson();
    } else {
        json["post"] = Json::nullValue;
    }
    json["error_message"] = error_message;

    Json::Value warns(Json::arrayValue);
    for (const auto& w : warnings) {
        warns.append(w);
    }
    json["warnings"] = warns;

    return json;
}

Json::Value PostingResult::toJson() const {
    Json::Value json;
    json["success"] = success;

    if (post_id) json["post_id"] = *post_id;
    else json["post_id"] = Json::nullValue;

    if (tiktok_post_id) json["tiktok_post_id"] = *tiktok_post_id;
    else json["tiktok_post_id"] = Json::nullValue;

    if (tiktok_url) json["tiktok_url"] = *tiktok_url;
    else json["tiktok_url"] = Json::nullValue;

    json["error_message"] = error_message;
    json["api_error"] = TikTokClient::errorToString(api_error);

    return json;
}

Json::Value SchedulingResult::toJson() const {
    Json::Value json;
    json["success"] = success;

    if (post_id) json["post_id"] = *post_id;
    else json["post_id"] = Json::nullValue;

    auto time = std::chrono::system_clock::to_time_t(scheduled_at);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    json["scheduled_at"] = oss.str();

    json["error_message"] = error_message;

    return json;
}

Json::Value OptimalPostTime::toJson() const {
    Json::Value json;

    auto time = std::chrono::system_clock::to_time_t(recommended_time);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    json["recommended_time"] = oss.str();

    json["hour"] = hour;
    json["day_of_week"] = day_of_week;
    json["confidence"] = confidence;
    json["reason"] = reason;

    return json;
}

Json::Value EngineStatistics::toJson() const {
    Json::Value json;
    json["posts_generated"] = static_cast<Json::UInt64>(posts_generated);
    json["posts_scheduled"] = static_cast<Json::UInt64>(posts_scheduled);
    json["posts_published"] = static_cast<Json::UInt64>(posts_published);
    json["posts_failed"] = static_cast<Json::UInt64>(posts_failed);
    json["analytics_updated"] = static_cast<Json::UInt64>(analytics_updated);
    json["auto_posts_generated"] = static_cast<Json::UInt64>(auto_posts_generated);
    json["avg_generation_time_ms"] = avg_generation_time_ms;
    json["avg_posting_time_ms"] = avg_posting_time_ms;
    json["total_views"] = static_cast<Json::UInt64>(total_views);
    json["total_likes"] = static_cast<Json::UInt64>(total_likes);
    json["total_shares"] = static_cast<Json::UInt64>(total_shares);
    json["total_comments"] = static_cast<Json::UInt64>(total_comments);
    return json;
}

// ============================================================================
// TIKTOKENGINE SINGLETON
// ============================================================================

TikTokEngine& TikTokEngine::getInstance() {
    static TikTokEngine instance;
    return instance;
}

// ============================================================================
// TIKTOKENGINE INITIALIZATION
// ============================================================================

bool TikTokEngine::initialize() {
    if (initialized_) {
        return true;
    }

    // Initialize template manager
    TemplateManager::getInstance().loadDefaults();

    // Initialize client (credentials may be set later)
    TikTokClient::getInstance().initializeFromConfig();

    initialized_ = true;
    LOG_INFO << "TikTokEngine initialized";

    return true;
}

bool TikTokEngine::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        max_daily_posts_ = config.getInt("tiktok.max_daily_posts", 10);
        max_posts_per_dog_ = config.getInt("tiktok.max_posts_per_dog", 3);
        auto_schedule_ = config.getBool("tiktok.auto_schedule", true);
        urgent_threshold_hours_ = config.getInt("tiktok.urgent_threshold_hours", 72);
        long_wait_threshold_days_ = config.getInt("tiktok.long_wait_threshold_days", 90);

        // Initialize template manager from config
        TemplateManager::getInstance().initializeFromConfig();

        // Initialize client
        TikTokClient::getInstance().initializeFromConfig();

        initialized_ = true;
        LOG_INFO << "TikTokEngine initialized from config";

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize TikTokEngine from config: " + std::string(e.what()),
            {});
        return initialize();  // Fall back to defaults
    }
}

bool TikTokEngine::isInitialized() const {
    return initialized_;
}

void TikTokEngine::shutdown() {
    shutting_down_ = true;
    TikTokClient::getInstance().shutdown();
    initialized_ = false;
    LOG_INFO << "TikTokEngine shutdown";
}

// ============================================================================
// TIKTOKENGINE POST GENERATION
// ============================================================================

PostGenerationResult TikTokEngine::generatePost(
    const wtl::core::models::Dog& dog,
    TikTokTemplateType template_type) {

    PostGenerationResult result;
    auto start = std::chrono::steady_clock::now();

    try {
        // Get template
        auto tmpl_opt = TemplateManager::getInstance().getTemplate(template_type);
        if (!tmpl_opt) {
            result.error_message = "Template not found for type: " +
                TikTokTemplate::typeToString(template_type);
            return result;
        }

        const TikTokTemplate& tmpl = *tmpl_opt;

        // Build placeholders from dog data
        auto placeholders = buildPlaceholders(dog);

        // Validate required placeholders
        auto missing = tmpl.validatePlaceholders(placeholders);
        if (!missing.empty()) {
            for (const auto& m : missing) {
                result.warnings.push_back("Missing placeholder: " + m);
            }
            // Continue anyway - formatCaption handles missing placeholders
        }

        // Create the post
        TikTokPost post;
        post.id = generatePostId();
        post.dog_id = dog.id;
        post.template_type = TikTokTemplate::typeToString(template_type);
        post.shelter_id = dog.shelter_id;

        // Format caption
        post.caption = tmpl.formatCaption(placeholders);

        // Get hashtags
        std::vector<std::string> breed_tags;
        if (!dog.breed_primary.empty()) {
            // Convert breed to hashtag format (remove spaces, add #)
            std::string breed_tag = dog.breed_primary;
            std::replace(breed_tag.begin(), breed_tag.end(), ' ', '_');
            breed_tags.push_back(breed_tag);
        }
        post.hashtags = tmpl.getHashtags(breed_tags);

        // Set overlay from template
        post.overlay_text = tmpl.formatOverlayText(placeholders);
        post.overlay_style = TikTokTemplate::overlayStyleToString(tmpl.overlay_style);
        post.include_countdown = tmpl.include_countdown;
        post.include_wait_time = tmpl.include_wait_time;

        // Set music if template suggests one
        post.music_id = tmpl.suggested_music_id;
        post.music_name = tmpl.suggested_music_name;

        // Determine content type (use photos from dog)
        if (!dog.photo_urls.empty()) {
            if (dog.photo_urls.size() == 1) {
                post.content_type = TikTokContentType::SINGLE_PHOTO;
            } else {
                post.content_type = TikTokContentType::PHOTO_SLIDESHOW;
            }
            post.photo_urls = dog.photo_urls;
            if (!post.photo_urls.empty()) {
                post.thumbnail_url = post.photo_urls[0];
            }
        }

        // Use video if available
        if (dog.video_url) {
            post.content_type = TikTokContentType::VIDEO;
            post.video_url = dog.video_url;
        }

        // Set initial status
        post.status = TikTokPostStatus::DRAFT;
        post.created_at = std::chrono::system_clock::now();
        post.updated_at = post.created_at;

        result.success = true;
        result.post = post;

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.posts_generated++;

            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            double total_time = stats_.avg_generation_time_ms * (stats_.posts_generated - 1) +
                               duration.count();
            stats_.avg_generation_time_ms = total_time / stats_.posts_generated;
        }

    } catch (const std::exception& e) {
        result.error_message = e.what();
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "generatePost"}, {"dog_id", dog.id}}));
    }

    return result;
}

PostGenerationResult TikTokEngine::generatePostWithTemplate(
    const wtl::core::models::Dog& dog,
    const std::string& template_id) {

    auto tmpl = TemplateManager::getInstance().getTemplateById(template_id);
    if (!tmpl) {
        PostGenerationResult result;
        result.error_message = "Template not found: " + template_id;
        return result;
    }

    return generatePost(dog, tmpl->type);
}

PostGenerationResult TikTokEngine::generateCustomPost(
    const wtl::core::models::Dog& dog,
    const std::string& caption,
    const std::vector<std::string>& hashtags) {

    PostGenerationResult result;

    try {
        TikTokPost post;
        post.id = generatePostId();
        post.dog_id = dog.id;
        post.template_type = "custom";
        post.shelter_id = dog.shelter_id;
        post.caption = caption;
        post.hashtags = hashtags;

        // Set media from dog
        if (dog.video_url) {
            post.content_type = TikTokContentType::VIDEO;
            post.video_url = dog.video_url;
        } else if (!dog.photo_urls.empty()) {
            post.content_type = dog.photo_urls.size() == 1 ?
                TikTokContentType::SINGLE_PHOTO : TikTokContentType::PHOTO_SLIDESHOW;
            post.photo_urls = dog.photo_urls;
        }

        post.status = TikTokPostStatus::DRAFT;
        post.created_at = std::chrono::system_clock::now();
        post.updated_at = post.created_at;

        result.success = true;
        result.post = post;

    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    return result;
}

TikTokTemplateType TikTokEngine::selectBestTemplate(const wtl::core::models::Dog& dog) {
    // Calculate days until euthanasia
    int days_until_euthanasia = -1;
    if (dog.euthanasia_date) {
        auto now = std::chrono::system_clock::now();
        auto diff = *dog.euthanasia_date - now;
        auto hours = std::chrono::duration_cast<std::chrono::hours>(diff).count();
        days_until_euthanasia = static_cast<int>(hours / 24);
    }

    // Calculate wait time in days
    auto now = std::chrono::system_clock::now();
    auto wait_diff = now - dog.intake_date;
    int wait_days = static_cast<int>(
        std::chrono::duration_cast<std::chrono::hours>(wait_diff).count() / 24);

    // Check if overlooked
    bool is_overlooked = isDogOverlooked(dog);

    return TemplateManager::getInstance().recommendTemplateType(
        days_until_euthanasia, wait_days, is_overlooked);
}

// ============================================================================
// TIKTOKENGINE SCHEDULING
// ============================================================================

SchedulingResult TikTokEngine::schedulePost(const TikTokPost& post) {
    // Get optimal time and schedule at that time
    auto optimal = getOptimalPostTime(
        TikTokTemplate::stringToType(post.template_type));

    return schedulePostAt(post, optimal.recommended_time);
}

SchedulingResult TikTokEngine::schedulePostAt(
    const TikTokPost& post,
    std::chrono::system_clock::time_point scheduled_time) {

    SchedulingResult result;

    try {
        // Save post first if not already saved
        TikTokPost post_to_save = post;
        post_to_save.scheduled_at = scheduled_time;
        post_to_save.status = TikTokPostStatus::SCHEDULED;
        post_to_save.updated_at = std::chrono::system_clock::now();

        auto saved_id = savePost(post_to_save);
        if (!saved_id) {
            result.error_message = "Failed to save post to database";
            return result;
        }

        result.success = true;
        result.post_id = *saved_id;
        result.scheduled_at = scheduled_time;

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.posts_scheduled++;
        }

    } catch (const std::exception& e) {
        result.error_message = e.what();
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "schedulePost"}}));
    }

    return result;
}

OptimalPostTime TikTokEngine::getOptimalPostTime(TikTokTemplateType template_type) {
    OptimalPostTime result;

    // Get template-specific optimal hours
    auto optimal_hours = TemplateManager::getInstance().getOptimalPostHours(template_type);
    if (optimal_hours.empty()) {
        optimal_hours = {9, 12, 17, 20};  // Defaults
    }

    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time);

    int current_hour = now_tm->tm_hour;
    int current_day = now_tm->tm_wday;

    // Find next optimal hour
    int best_hour = -1;
    int days_ahead = 0;

    for (int day_offset = 0; day_offset < 7 && best_hour == -1; ++day_offset) {
        for (int hour : optimal_hours) {
            if (day_offset == 0 && hour <= current_hour) {
                continue;  // Already passed today
            }
            best_hour = hour;
            days_ahead = day_offset;
            break;
        }
    }

    if (best_hour == -1) {
        // Fallback to first optimal hour tomorrow
        best_hour = optimal_hours[0];
        days_ahead = 1;
    }

    // Calculate the recommended time
    std::tm scheduled_tm = *now_tm;
    scheduled_tm.tm_hour = best_hour;
    scheduled_tm.tm_min = 0;
    scheduled_tm.tm_sec = 0;
    scheduled_tm.tm_mday += days_ahead;

    // Normalize the time
    auto scheduled_time_t = std::mktime(&scheduled_tm);
    result.recommended_time = std::chrono::system_clock::from_time_t(scheduled_time_t);
    result.hour = best_hour;
    result.day_of_week = (current_day + days_ahead) % 7;

    // Confidence based on template type
    // Urgent posts have higher confidence for immediate times
    if (template_type == TikTokTemplateType::URGENT_COUNTDOWN) {
        result.confidence = 0.9;
        result.reason = "High-engagement time for urgent content";
    } else if (template_type == TikTokTemplateType::HAPPY_UPDATE) {
        result.confidence = 0.85;
        result.reason = "Good time for feel-good content";
    } else {
        result.confidence = 0.75;
        result.reason = "General high-engagement period";
    }

    return result;
}

std::vector<OptimalPostTime> TikTokEngine::getOptimalPostSlots(
    int count, TikTokTemplateType template_type) {

    std::vector<OptimalPostTime> results;

    auto optimal_hours = TemplateManager::getInstance().getOptimalPostHours(template_type);
    if (optimal_hours.empty()) {
        optimal_hours = {9, 12, 17, 20};
    }

    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_time);

    int current_hour = now_tm.tm_hour;

    for (int day_offset = 0; results.size() < static_cast<size_t>(count); ++day_offset) {
        for (int hour : optimal_hours) {
            if (day_offset == 0 && hour <= current_hour) {
                continue;
            }

            OptimalPostTime slot;
            std::tm slot_tm = now_tm;
            slot_tm.tm_hour = hour;
            slot_tm.tm_min = 0;
            slot_tm.tm_sec = 0;
            slot_tm.tm_mday += day_offset;

            auto slot_time_t = std::mktime(&slot_tm);
            slot.recommended_time = std::chrono::system_clock::from_time_t(slot_time_t);
            slot.hour = hour;
            slot.day_of_week = (now_tm.tm_wday + day_offset) % 7;
            slot.confidence = 0.8 - (day_offset * 0.05);  // Decrease confidence for future days
            slot.reason = "Optimal posting slot";

            results.push_back(slot);

            if (results.size() >= static_cast<size_t>(count)) {
                break;
            }
        }
    }

    return results;
}

// ============================================================================
// TIKTOKENGINE POSTING
// ============================================================================

PostingResult TikTokEngine::postNow(TikTokPost& post) {
    PostingResult result;
    auto start = std::chrono::steady_clock::now();

    try {
        // Validate post is ready
        if (!post.isReadyToPublish()) {
            result.error_message = "Post is not ready to publish";
            return result;
        }

        // Update status to posting
        post.status = TikTokPostStatus::POSTING;
        post.updated_at = std::chrono::system_clock::now();

        // Save/update post if it has an ID
        if (!post.id.empty()) {
            updatePost(post);
        }

        // Build TikTok content
        TikTokPostContent content;
        content.caption = post.caption;
        content.hashtags = post.hashtags;
        content.is_video = (post.content_type == TikTokContentType::VIDEO);
        content.video_url = post.video_url;
        content.photo_urls = post.photo_urls;
        content.music_id = post.music_id;

        // Post to TikTok
        auto api_result = TikTokClient::getInstance().post(content);

        if (api_result.success) {
            post.status = TikTokPostStatus::PUBLISHED;
            post.tiktok_post_id = api_result.post_id;
            post.tiktok_url = api_result.post_url;
            post.posted_at = std::chrono::system_clock::now();
            post.error_message = std::nullopt;

            result.success = true;
            result.post_id = post.id;
            result.tiktok_post_id = post.tiktok_post_id;
            result.tiktok_url = post.tiktok_url;

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.posts_published++;

                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                double total_time = stats_.avg_posting_time_ms * stats_.posts_published +
                                   duration.count();
                stats_.avg_posting_time_ms = total_time / (stats_.posts_published + 1);
            }
        } else {
            post.status = TikTokPostStatus::FAILED;
            post.error_message = api_result.error_message;
            post.retry_count++;

            result.error_message = api_result.error_message;
            result.api_error = api_result.error;
            result.post_id = post.id;

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.posts_failed++;
            }
        }

        post.updated_at = std::chrono::system_clock::now();

        // Update post in database
        if (!post.id.empty()) {
            updatePost(post);
        }

    } catch (const std::exception& e) {
        result.error_message = e.what();
        result.api_error = TikTokApiError::UNKNOWN;

        post.status = TikTokPostStatus::FAILED;
        post.error_message = e.what();
        post.updated_at = std::chrono::system_clock::now();

        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::EXTERNAL_API,
            (std::unordered_map<std::string, std::string>{{"operation", "postNow"}, {"post_id", post.id}}));
    }

    return result;
}

PostingResult TikTokEngine::postById(const std::string& post_id) {
    auto post_opt = getPost(post_id);
    if (!post_opt) {
        PostingResult result;
        result.error_message = "Post not found: " + post_id;
        return result;
    }

    TikTokPost post = *post_opt;
    return postNow(post);
}

int TikTokEngine::processScheduledPosts() {
    int processed = 0;

    try {
        auto scheduled = getPostsByStatus(TikTokPostStatus::SCHEDULED, 100);
        auto now = std::chrono::system_clock::now();

        for (auto& post : scheduled) {
            if (post.scheduled_at && *post.scheduled_at <= now) {
                auto result = postNow(post);
                if (result.success) {
                    processed++;
                }
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "processScheduledPosts"}}));
    }

    return processed;
}

PostingResult TikTokEngine::retryFailedPost(const std::string& post_id) {
    auto post_opt = getPost(post_id);
    if (!post_opt) {
        PostingResult result;
        result.error_message = "Post not found: " + post_id;
        return result;
    }

    if (post_opt->status != TikTokPostStatus::FAILED) {
        PostingResult result;
        result.error_message = "Post is not in failed status";
        return result;
    }

    TikTokPost post = *post_opt;
    return postNow(post);
}

// ============================================================================
// TIKTOKENGINE ANALYTICS
// ============================================================================

bool TikTokEngine::updateAnalytics(const std::string& post_id) {
    try {
        auto post_opt = getPost(post_id);
        if (!post_opt || !post_opt->tiktok_post_id) {
            return false;
        }

        auto api_result = TikTokClient::getInstance().getPostAnalytics(*post_opt->tiktok_post_id);
        if (!api_result.success) {
            return false;
        }

        // Parse analytics from response
        if (api_result.data.isMember("data") && api_result.data["data"].isMember("videos")) {
            const auto& videos = api_result.data["data"]["videos"];
            if (videos.isArray() && videos.size() > 0) {
                const auto& video = videos[0];

                TikTokPost post = *post_opt;
                post.analytics.views = video.get("view_count", 0).asUInt64();
                post.analytics.likes = video.get("like_count", 0).asUInt64();
                post.analytics.comments = video.get("comment_count", 0).asUInt64();
                post.analytics.shares = video.get("share_count", 0).asUInt64();
                post.analytics.calculateEngagementRate();
                post.analytics.last_updated = std::chrono::system_clock::now();
                post.updated_at = std::chrono::system_clock::now();

                updatePost(post);

                // Update statistics
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.analytics_updated++;
                }

                return true;
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::EXTERNAL_API,
            (std::unordered_map<std::string, std::string>{{"operation", "updateAnalytics"}, {"post_id", post_id}}));
    }

    return false;
}

int TikTokEngine::updateAllAnalytics(int max_posts) {
    int updated = 0;

    try {
        auto published = getPostsByStatus(TikTokPostStatus::PUBLISHED,
            max_posts > 0 ? max_posts : 1000);

        for (const auto& post : published) {
            if (updateAnalytics(post.id)) {
                updated++;
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "updateAllAnalytics"}}));
    }

    return updated;
}

Json::Value TikTokEngine::getAnalyticsSummary() {
    Json::Value summary;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT "
            "COUNT(*) as total_posts, "
            "SUM((analytics->>'views')::bigint) as total_views, "
            "SUM((analytics->>'likes')::bigint) as total_likes, "
            "SUM((analytics->>'shares')::bigint) as total_shares, "
            "SUM((analytics->>'comments')::bigint) as total_comments, "
            "AVG((analytics->>'engagement_rate')::float) as avg_engagement "
            "FROM tiktok_posts "
            "WHERE status = 'published'"
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (!result.empty()) {
            summary["total_posts"] = result[0]["total_posts"].as<int>(0);
            summary["total_views"] = result[0]["total_views"].is_null() ? 0 :
                static_cast<Json::Int64>(result[0]["total_views"].as<int64_t>());
            summary["total_likes"] = result[0]["total_likes"].is_null() ? 0 :
                static_cast<Json::Int64>(result[0]["total_likes"].as<int64_t>());
            summary["total_shares"] = result[0]["total_shares"].is_null() ? 0 :
                static_cast<Json::Int64>(result[0]["total_shares"].as<int64_t>());
            summary["total_comments"] = result[0]["total_comments"].is_null() ? 0 :
                static_cast<Json::Int64>(result[0]["total_comments"].as<int64_t>());
            summary["avg_engagement_rate"] = result[0]["avg_engagement"].is_null() ? 0.0 :
                result[0]["avg_engagement"].as<double>();
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "getAnalyticsSummary"}}));
        summary["error"] = e.what();
    }

    return summary;
}

std::vector<TikTokPost> TikTokEngine::getTopPerformingPosts(
    int count, const std::string& metric) {

    std::vector<TikTokPost> posts;

    try {
        std::string order_by = "analytics->>'views'";
        if (metric == "likes") order_by = "analytics->>'likes'";
        else if (metric == "shares") order_by = "analytics->>'shares'";
        else if (metric == "engagement") order_by = "analytics->>'engagement_rate'";

        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM tiktok_posts "
            "WHERE status = 'published' "
            "ORDER BY (" + order_by + ")::float DESC NULLS LAST "
            "LIMIT $1",
            count
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            posts.push_back(TikTokPost::fromDbRow(row));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "getTopPerformingPosts"}}));
    }

    return posts;
}

// ============================================================================
// TIKTOKENGINE POST MANAGEMENT
// ============================================================================

std::optional<std::string> TikTokEngine::savePost(const TikTokPost& post) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string post_id = post.id.empty() ? generatePostId() : post.id;

        // Convert analytics to JSON string
        Json::StreamWriterBuilder writer;
        std::string analytics_json = Json::writeString(writer, post.analytics.toJson());

        // Convert arrays to PostgreSQL format
        std::string hashtags_arr = TikTokPost::formatPostgresArray(post.hashtags);
        std::string photos_arr = TikTokPost::formatPostgresArray(post.photo_urls);

        txn.exec_params(
            "INSERT INTO tiktok_posts "
            "(id, dog_id, shelter_id, template_type, caption, hashtags, "
            "music_id, music_name, content_type, video_url, photo_urls, "
            "thumbnail_url, overlay_text, overlay_style, include_countdown, "
            "include_wait_time, scheduled_at, posted_at, auto_generated, "
            "status, tiktok_post_id, tiktok_url, error_message, retry_count, "
            "analytics, created_by, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6::text[], $7, $8, $9, $10, $11::text[], "
            "$12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22, $23, $24, "
            "$25::jsonb, $26, $27, $28) "
            "ON CONFLICT (id) DO UPDATE SET "
            "caption = EXCLUDED.caption, "
            "hashtags = EXCLUDED.hashtags, "
            "scheduled_at = EXCLUDED.scheduled_at, "
            "status = EXCLUDED.status, "
            "tiktok_post_id = EXCLUDED.tiktok_post_id, "
            "tiktok_url = EXCLUDED.tiktok_url, "
            "error_message = EXCLUDED.error_message, "
            "retry_count = EXCLUDED.retry_count, "
            "analytics = EXCLUDED.analytics, "
            "updated_at = EXCLUDED.updated_at",
            post_id,
            post.dog_id,
            post.shelter_id,
            post.template_type,
            post.caption,
            hashtags_arr,
            post.music_id ? *post.music_id : "",
            post.music_name ? *post.music_name : "",
            TikTokPost::contentTypeToString(post.content_type),
            post.video_url ? *post.video_url : "",
            photos_arr,
            post.thumbnail_url ? *post.thumbnail_url : "",
            post.overlay_text ? *post.overlay_text : "",
            post.overlay_style ? *post.overlay_style : "",
            post.include_countdown,
            post.include_wait_time,
            post.scheduled_at ? TikTokPost::formatTimestamp(*post.scheduled_at) : "",
            post.posted_at ? TikTokPost::formatTimestamp(*post.posted_at) : "",
            post.auto_generated,
            TikTokPost::statusToString(post.status),
            post.tiktok_post_id ? *post.tiktok_post_id : "",
            post.tiktok_url ? *post.tiktok_url : "",
            post.error_message ? *post.error_message : "",
            post.retry_count,
            analytics_json,
            post.created_by ? *post.created_by : "",
            TikTokPost::formatTimestamp(post.created_at),
            TikTokPost::formatTimestamp(post.updated_at)
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return post_id;

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "savePost"}}));
        return std::nullopt;
    }
}

std::optional<TikTokPost> TikTokEngine::getPost(const std::string& post_id) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM tiktok_posts WHERE id = $1",
            post_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        if (!result.empty()) {
            return TikTokPost::fromDbRow(result[0]);
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "getPost"}, {"post_id", post_id}}));
    }

    return std::nullopt;
}

bool TikTokEngine::updatePost(const TikTokPost& post) {
    auto result = savePost(post);
    return result.has_value();
}

bool TikTokEngine::deletePost(const std::string& post_id, bool delete_from_tiktok) {
    try {
        auto post = getPost(post_id);
        if (!post) {
            return false;
        }

        // Delete from TikTok if requested and published
        if (delete_from_tiktok && post->tiktok_post_id) {
            TikTokClient::getInstance().deletePost(*post->tiktok_post_id);
            // Continue even if TikTok deletion fails
        }

        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "DELETE FROM tiktok_posts WHERE id = $1",
            post_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "deletePost"}, {"post_id", post_id}}));
        return false;
    }
}

std::vector<TikTokPost> TikTokEngine::getPostsByStatus(
    TikTokPostStatus status, int limit) {

    std::vector<TikTokPost> posts;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM tiktok_posts WHERE status = $1 "
            "ORDER BY scheduled_at ASC NULLS LAST, created_at DESC "
            "LIMIT $2",
            TikTokPost::statusToString(status),
            limit
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            posts.push_back(TikTokPost::fromDbRow(row));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "getPostsByStatus"}}));
    }

    return posts;
}

std::vector<TikTokPost> TikTokEngine::getPostsByDog(const std::string& dog_id) {
    std::vector<TikTokPost> posts;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM tiktok_posts WHERE dog_id = $1 "
            "ORDER BY created_at DESC",
            dog_id
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            posts.push_back(TikTokPost::fromDbRow(row));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "getPostsByDog"}, {"dog_id", dog_id}}));
    }

    return posts;
}

std::vector<TikTokPost> TikTokEngine::getScheduledPosts() {
    return getPostsByStatus(TikTokPostStatus::SCHEDULED, 100);
}

// ============================================================================
// TIKTOKENGINE AUTOMATION
// ============================================================================

int TikTokEngine::generateUrgentDogPosts(int max_posts) {
    int generated = 0;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find urgent dogs without recent TikTok posts
        auto result = txn.exec_params(
            "SELECT d.* FROM dogs d "
            "WHERE d.urgency_level IN ('critical', 'high') "
            "AND d.is_available = true "
            "AND NOT EXISTS ("
            "  SELECT 1 FROM tiktok_posts tp "
            "  WHERE tp.dog_id = d.id "
            "  AND tp.created_at > NOW() - INTERVAL '7 days' "
            "  AND tp.status NOT IN ('deleted', 'expired')"
            ") "
            "ORDER BY d.urgency_level DESC, d.euthanasia_date ASC NULLS LAST "
            "LIMIT $1",
            max_posts
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            wtl::core::models::Dog dog = wtl::core::models::Dog::fromDbRow(row);

            auto gen_result = generatePost(dog, TikTokTemplateType::URGENT_COUNTDOWN);
            if (gen_result.success && gen_result.post) {
                TikTokPost post = *gen_result.post;
                post.auto_generated = true;

                if (auto_schedule_) {
                    schedulePost(post);
                } else {
                    savePost(post);
                }

                generated++;

                // Update statistics
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.auto_posts_generated++;
                }
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "generateUrgentDogPosts"}}));
    }

    return generated;
}

int TikTokEngine::generateLongestWaitingPosts(int max_posts) {
    int generated = 0;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find longest waiting dogs without recent TikTok posts
        auto result = txn.exec_params(
            "SELECT d.* FROM dogs d "
            "WHERE d.is_available = true "
            "AND d.intake_date < NOW() - INTERVAL '$1 days' "
            "AND NOT EXISTS ("
            "  SELECT 1 FROM tiktok_posts tp "
            "  WHERE tp.dog_id = d.id "
            "  AND tp.created_at > NOW() - INTERVAL '14 days' "
            "  AND tp.status NOT IN ('deleted', 'expired')"
            ") "
            "ORDER BY d.intake_date ASC "
            "LIMIT $2",
            long_wait_threshold_days_,
            max_posts
        );

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            wtl::core::models::Dog dog = wtl::core::models::Dog::fromDbRow(row);

            auto gen_result = generatePost(dog, TikTokTemplateType::LONGEST_WAITING);
            if (gen_result.success && gen_result.post) {
                TikTokPost post = *gen_result.post;
                post.auto_generated = true;

                if (auto_schedule_) {
                    schedulePost(post);
                } else {
                    savePost(post);
                }

                generated++;

                // Update statistics
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.auto_posts_generated++;
                }
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "generateLongestWaitingPosts"}}));
    }

    return generated;
}

// ============================================================================
// TIKTOKENGINE STATISTICS
// ============================================================================

EngineStatistics TikTokEngine::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void TikTokEngine::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = EngineStatistics();
}

// ============================================================================
// TIKTOKENGINE PRIVATE METHODS
// ============================================================================

std::unordered_map<std::string, std::string> TikTokEngine::buildPlaceholders(
    const wtl::core::models::Dog& dog) {

    std::unordered_map<std::string, std::string> placeholders;

    placeholders["dog_name"] = dog.name;
    placeholders["breed"] = dog.breed_primary;

    if (dog.breed_secondary) {
        placeholders["breed"] += " / " + *dog.breed_secondary;
    }

    placeholders["age"] = dog.age_category;
    placeholders["size"] = dog.size;
    placeholders["gender"] = dog.gender;

    // Wait time
    placeholders["wait_time"] = calculateWaitTimeString(dog);

    // Urgency
    placeholders["urgency"] = calculateUrgencyString(dog);

    // Shelter - would need to fetch from shelter service
    placeholders["shelter_name"] = "Local Shelter";  // Placeholder
    placeholders["location"] = "";  // Would need shelter data

    // Special traits from tags
    std::string traits;
    for (const auto& tag : dog.tags) {
        if (!traits.empty()) traits += ", ";
        traits += tag;
    }
    placeholders["personality"] = traits;
    placeholders["special_traits"] = traits;

    return placeholders;
}

std::string TikTokEngine::calculateWaitTimeString(const wtl::core::models::Dog& dog) {
    auto now = std::chrono::system_clock::now();
    auto wait = now - dog.intake_date;

    auto days = std::chrono::duration_cast<std::chrono::hours>(wait).count() / 24;
    auto months = days / 30;
    auto years = days / 365;

    if (years > 0) {
        return std::to_string(years) + " year" + (years > 1 ? "s" : "");
    } else if (months > 0) {
        return std::to_string(months) + " month" + (months > 1 ? "s" : "");
    } else if (days > 0) {
        return std::to_string(days) + " day" + (days > 1 ? "s" : "");
    } else {
        return "just arrived";
    }
}

std::string TikTokEngine::calculateUrgencyString(const wtl::core::models::Dog& dog) {
    if (!dog.euthanasia_date) {
        return "";
    }

    auto now = std::chrono::system_clock::now();
    auto diff = *dog.euthanasia_date - now;
    auto hours = std::chrono::duration_cast<std::chrono::hours>(diff).count();

    if (hours < 0) {
        return "TIME'S UP";
    } else if (hours < 24) {
        return std::to_string(hours) + " HOURS";
    } else {
        auto days = hours / 24;
        return std::to_string(days) + " DAY" + (days > 1 ? "S" : "");
    }
}

bool TikTokEngine::isDogOverlooked(const wtl::core::models::Dog& dog) {
    // Senior dogs
    if (dog.age_category == "senior") {
        return true;
    }

    // Black dogs (often overlooked - "Black Dog Syndrome")
    std::string color_lower = dog.color_primary;
    std::transform(color_lower.begin(), color_lower.end(), color_lower.begin(), ::tolower);
    if (color_lower == "black") {
        return true;
    }

    // Pit bulls and similar breeds (often face discrimination)
    std::string breed_lower = dog.breed_primary;
    std::transform(breed_lower.begin(), breed_lower.end(), breed_lower.begin(), ::tolower);
    if (breed_lower.find("pit") != std::string::npos ||
        breed_lower.find("bull") != std::string::npos ||
        breed_lower.find("staff") != std::string::npos) {
        return true;
    }

    // Large dogs
    if (dog.size == "xlarge") {
        return true;
    }

    return false;
}

std::string TikTokEngine::generatePostId() {
    // Generate UUID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream uuid;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            uuid << '-';
        }
        uuid << std::hex << dis(gen);
    }

    return uuid.str();
}

void TikTokEngine::updatePostStatus(const std::string& post_id,
                                   TikTokPostStatus status,
                                   const std::string& error_message) {
    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        if (error_message.empty()) {
            txn.exec_params(
                "UPDATE tiktok_posts SET status = $1, updated_at = NOW() WHERE id = $2",
                TikTokPost::statusToString(status),
                post_id
            );
        } else {
            txn.exec_params(
                "UPDATE tiktok_posts SET status = $1, error_message = $2, updated_at = NOW() "
                "WHERE id = $3",
                TikTokPost::statusToString(status),
                error_message,
                post_id
            );
        }

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "updatePostStatus"}, {"post_id", post_id}}));
    }
}

} // namespace wtl::content::tiktok
