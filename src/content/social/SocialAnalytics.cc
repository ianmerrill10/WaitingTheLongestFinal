/**
 * @file SocialAnalytics.cc
 * @brief Implementation of social media analytics tracking
 * @see SocialAnalytics.h for documentation
 */

#include "content/social/SocialAnalytics.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <sstream>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::content::social {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// PLATFORM ANALYTICS
// ============================================================================

Json::Value PlatformAnalytics::toJson() const {
    Json::Value json;
    json["platform"] = platformToString(platform);
    json["impressions"] = impressions;
    json["reach"] = reach;
    json["profile_visits"] = profile_visits;
    json["likes"] = likes;
    json["comments"] = comments;
    json["shares"] = shares;
    json["saves"] = saves;
    json["link_clicks"] = link_clicks;
    json["video_views"] = video_views;
    json["video_watch_time_seconds"] = video_watch_time_seconds;
    json["video_completion_rate"] = video_completion_rate;
    json["new_followers"] = new_followers;
    json["engagement_rate"] = engagement_rate;
    json["total_engagements"] = getTotalEngagements();

    // Format timestamp
    auto time_t_val = std::chrono::system_clock::to_time_t(fetched_at);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
    json["fetched_at"] = oss.str();

    return json;
}

PlatformAnalytics PlatformAnalytics::fromJson(const Json::Value& json) {
    PlatformAnalytics analytics;
    analytics.platform = stringToPlatform(json["platform"].asString());
    analytics.impressions = json.get("impressions", 0).asInt();
    analytics.reach = json.get("reach", 0).asInt();
    analytics.profile_visits = json.get("profile_visits", 0).asInt();
    analytics.likes = json.get("likes", 0).asInt();
    analytics.comments = json.get("comments", 0).asInt();
    analytics.shares = json.get("shares", 0).asInt();
    analytics.saves = json.get("saves", 0).asInt();
    analytics.link_clicks = json.get("link_clicks", 0).asInt();
    analytics.video_views = json.get("video_views", 0).asInt();
    analytics.video_watch_time_seconds = json.get("video_watch_time_seconds", 0).asInt();
    analytics.video_completion_rate = json.get("video_completion_rate", 0.0).asDouble();
    analytics.new_followers = json.get("new_followers", 0).asInt();
    analytics.engagement_rate = json.get("engagement_rate", 0.0).asDouble();

    if (json.isMember("fetched_at")) {
        std::string ts = json["fetched_at"].asString();
        std::tm tm = {};
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        analytics.fetched_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    } else {
        analytics.fetched_at = std::chrono::system_clock::now();
    }

    return analytics;
}

PlatformAnalytics PlatformAnalytics::fromDbRow(const pqxx::row& row) {
    PlatformAnalytics analytics;
    analytics.platform = stringToPlatform(row["platform"].as<std::string>());
    analytics.impressions = row["impressions"].as<int>();
    analytics.reach = row["reach"].as<int>();
    analytics.profile_visits = row["profile_visits"].as<int>();
    analytics.likes = row["likes"].as<int>();
    analytics.comments = row["comments"].as<int>();
    analytics.shares = row["shares"].as<int>();
    analytics.saves = row["saves"].as<int>();
    analytics.link_clicks = row["link_clicks"].as<int>();
    analytics.video_views = row["video_views"].as<int>();
    analytics.video_watch_time_seconds = row["video_watch_time_seconds"].as<int>();
    analytics.video_completion_rate = row["video_completion_rate"].as<double>();
    analytics.new_followers = row["new_followers"].as<int>();
    analytics.engagement_rate = row["engagement_rate"].as<double>();

    std::string ts = row["fetched_at"].as<std::string>();
    std::tm tm = {};
    std::istringstream ss(ts);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    analytics.fetched_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    return analytics;
}

int PlatformAnalytics::getTotalEngagements() const {
    return likes + comments + shares + saves;
}

void PlatformAnalytics::calculateEngagementRate() {
    if (impressions > 0) {
        engagement_rate = static_cast<double>(getTotalEngagements()) / impressions * 100.0;
    } else {
        engagement_rate = 0.0;
    }
}

// ============================================================================
// POST ANALYTICS
// ============================================================================

Json::Value PostAnalytics::toJson() const {
    Json::Value json;
    json["post_id"] = post_id;
    if (dog_id) json["dog_id"] = *dog_id;
    if (shelter_id) json["shelter_id"] = *shelter_id;

    // Platform analytics
    Json::Value platform_data;
    for (const auto& [platform, analytics] : platform_analytics) {
        platform_data[platformToString(platform)] = analytics.toJson();
    }
    json["platform_analytics"] = platform_data;

    // Totals
    json["total_impressions"] = total_impressions;
    json["total_reach"] = total_reach;
    json["total_engagements"] = total_engagements;
    json["total_link_clicks"] = total_link_clicks;
    json["total_video_views"] = total_video_views;
    json["total_new_followers"] = total_new_followers;

    // Best performing
    json["best_performing_platform"] = platformToString(best_performing_platform);
    json["best_engagement_rate"] = best_engagement_rate;

    // Impact
    json["resulted_in_adoption"] = resulted_in_adoption;
    json["resulted_in_foster"] = resulted_in_foster;
    json["resulted_in_application"] = resulted_in_application;
    json["application_count"] = application_count;

    // Cost metrics
    json["amount_spent"] = amount_spent;
    json["cost_per_engagement"] = cost_per_engagement;
    json["cost_per_click"] = cost_per_click;

    return json;
}

PostAnalytics PostAnalytics::fromDbRow(const pqxx::row& row) {
    PostAnalytics analytics;
    analytics.post_id = row["post_id"].as<std::string>();

    if (!row["dog_id"].is_null()) {
        analytics.dog_id = row["dog_id"].as<std::string>();
    }
    if (!row["shelter_id"].is_null()) {
        analytics.shelter_id = row["shelter_id"].as<std::string>();
    }

    analytics.total_impressions = row["total_impressions"].as<int>();
    analytics.total_reach = row["total_reach"].as<int>();
    analytics.total_engagements = row["total_engagements"].as<int>();
    analytics.total_link_clicks = row["total_link_clicks"].as<int>();
    analytics.total_video_views = row["total_video_views"].as<int>();
    analytics.total_new_followers = row["total_new_followers"].as<int>();

    analytics.resulted_in_adoption = row["resulted_in_adoption"].as<bool>();
    analytics.resulted_in_foster = row["resulted_in_foster"].as<bool>();
    analytics.resulted_in_application = row["resulted_in_application"].as<bool>();
    analytics.application_count = row["application_count"].as<int>();

    analytics.amount_spent = row["amount_spent"].as<double>();
    analytics.cost_per_engagement = row["cost_per_engagement"].as<double>();
    analytics.cost_per_click = row["cost_per_click"].as<double>();

    return analytics;
}

void PostAnalytics::aggregate() {
    total_impressions = 0;
    total_reach = 0;
    total_engagements = 0;
    total_link_clicks = 0;
    total_video_views = 0;
    total_new_followers = 0;
    best_engagement_rate = 0.0;

    for (const auto& [platform, analytics] : platform_analytics) {
        total_impressions += analytics.impressions;
        total_reach += analytics.reach;
        total_engagements += analytics.getTotalEngagements();
        total_link_clicks += analytics.link_clicks;
        total_video_views += analytics.video_views;
        total_new_followers += analytics.new_followers;

        if (analytics.engagement_rate > best_engagement_rate) {
            best_engagement_rate = analytics.engagement_rate;
            best_performing_platform = platform;
        }
    }

    // Calculate cost metrics if money was spent
    if (amount_spent > 0) {
        cost_per_engagement = total_engagements > 0 ?
            amount_spent / total_engagements : 0.0;
        cost_per_click = total_link_clicks > 0 ?
            amount_spent / total_link_clicks : 0.0;
    }
}

// ============================================================================
// ADOPTION IMPACT
// ============================================================================

Json::Value AdoptionImpact::toJson() const {
    Json::Value json;
    json["dog_id"] = dog_id;
    json["dog_name"] = dog_name;
    json["shelter_name"] = shelter_name;

    Json::Value posts_json(Json::arrayValue);
    for (const auto& id : post_ids) {
        posts_json.append(id);
    }
    json["post_ids"] = posts_json;

    json["total_social_impressions"] = total_social_impressions;
    json["total_social_engagements"] = total_social_engagements;
    json["days_from_first_post"] = days_from_first_post;

    if (attributed_post_id) {
        json["attributed_post_id"] = *attributed_post_id;
    }
    if (attributed_platform) {
        json["attributed_platform"] = platformToString(*attributed_platform);
    }

    // Format timestamps
    auto time_t_first = std::chrono::system_clock::to_time_t(first_social_post);
    auto time_t_adopt = std::chrono::system_clock::to_time_t(adoption_date);
    std::ostringstream oss_first, oss_adopt;
    oss_first << std::put_time(std::gmtime(&time_t_first), "%Y-%m-%dT%H:%M:%SZ");
    oss_adopt << std::put_time(std::gmtime(&time_t_adopt), "%Y-%m-%dT%H:%M:%SZ");
    json["first_social_post"] = oss_first.str();
    json["adoption_date"] = oss_adopt.str();

    return json;
}

// ============================================================================
// ANALYTICS SUMMARY
// ============================================================================

Json::Value AnalyticsSummary::toJson() const {
    Json::Value json;

    // Time period
    auto time_t_start = std::chrono::system_clock::to_time_t(start_date);
    auto time_t_end = std::chrono::system_clock::to_time_t(end_date);
    std::ostringstream oss_start, oss_end;
    oss_start << std::put_time(std::gmtime(&time_t_start), "%Y-%m-%dT%H:%M:%SZ");
    oss_end << std::put_time(std::gmtime(&time_t_end), "%Y-%m-%dT%H:%M:%SZ");
    json["start_date"] = oss_start.str();
    json["end_date"] = oss_end.str();

    // Post counts
    json["total_posts"] = total_posts;
    json["posts_with_video"] = posts_with_video;
    json["posts_with_images"] = posts_with_images;

    Json::Value platform_counts;
    for (const auto& [platform, count] : posts_per_platform) {
        platform_counts[platformToString(platform)] = count;
    }
    json["posts_per_platform"] = platform_counts;

    // Aggregate metrics
    json["total_impressions"] = total_impressions;
    json["total_reach"] = total_reach;
    json["total_engagements"] = total_engagements;
    json["total_link_clicks"] = total_link_clicks;
    json["average_engagement_rate"] = average_engagement_rate;

    // Impact
    json["adoptions_attributed"] = adoptions_attributed;
    json["fosters_attributed"] = fosters_attributed;
    json["applications_received"] = applications_received;

    // Best content
    json["top_post_id"] = top_post_id;
    json["top_post_engagements"] = top_post_engagements;
    json["top_platform"] = platformToString(top_platform);

    // Trends
    json["impressions_growth_pct"] = impressions_growth_pct;
    json["engagements_growth_pct"] = engagements_growth_pct;

    return json;
}

// ============================================================================
// SOCIAL ANALYTICS SERVICE - SINGLETON
// ============================================================================

SocialAnalytics& SocialAnalytics::getInstance() {
    static SocialAnalytics instance;
    return instance;
}

// ============================================================================
// RECORDING ANALYTICS
// ============================================================================

void SocialAnalytics::recordAnalytics(const std::string& post_id,
                                       const PlatformAnalytics& analytics) {
    try {
        saveToDatabase(post_id, analytics);

        // Update cache
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto it = analytics_cache_.find(post_id);
            if (it != analytics_cache_.end()) {
                it->second.platform_analytics[analytics.platform] = analytics;
                it->second.aggregate();
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to record analytics: " + std::string(e.what()),
            {{"post_id", post_id}, {"platform", platformToString(analytics.platform)}}
        );
    }
}

void SocialAnalytics::recordAdoption(const std::string& dog_id,
                                      const std::optional<std::string>& post_id,
                                      const std::optional<Platform>& platform) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Update all posts for this dog
        txn.exec_params(
            "UPDATE social_posts SET resulted_in_adoption = true, "
            "updated_at = NOW() WHERE dog_id = $1",
            dog_id
        );

        // Record specific attribution if provided
        if (post_id) {
            txn.exec_params(
                "INSERT INTO social_adoption_attributions "
                "(dog_id, post_id, platform, adoption_date) "
                "VALUES ($1, $2, $3, NOW())",
                dog_id,
                *post_id,
                platform ? platformToString(*platform) : ""
            );
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        // Invalidate cache
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            // Remove all entries for this dog from cache
            for (auto it = analytics_cache_.begin(); it != analytics_cache_.end();) {
                if (it->second.dog_id && *it->second.dog_id == dog_id) {
                    it = analytics_cache_.erase(it);
                } else {
                    ++it;
                }
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to record adoption: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }
}

void SocialAnalytics::recordFoster(const std::string& dog_id,
                                    const std::optional<std::string>& post_id) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Update all posts for this dog
        txn.exec_params(
            "UPDATE social_posts SET resulted_in_foster = true, "
            "updated_at = NOW() WHERE dog_id = $1",
            dog_id
        );

        if (post_id) {
            txn.exec_params(
                "INSERT INTO social_foster_attributions "
                "(dog_id, post_id, foster_date) "
                "VALUES ($1, $2, NOW())",
                dog_id, *post_id
            );
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to record foster: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }
}

void SocialAnalytics::recordApplication(const std::string& dog_id,
                                         const std::optional<std::string>& source_post_id) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        if (source_post_id) {
            txn.exec_params(
                "UPDATE social_posts SET "
                "resulted_in_application = true, "
                "application_count = COALESCE(application_count, 0) + 1, "
                "updated_at = NOW() "
                "WHERE id = $1",
                *source_post_id
            );
        }

        // Record the application
        txn.exec_params(
            "INSERT INTO social_application_attributions "
            "(dog_id, post_id, application_date) "
            "VALUES ($1, $2, NOW())",
            dog_id,
            source_post_id ? *source_post_id : ""
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to record application: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }
}

// ============================================================================
// RETRIEVING ANALYTICS
// ============================================================================

std::optional<PostAnalytics> SocialAnalytics::getPostAnalytics(const std::string& post_id) {
    // Check cache first
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = analytics_cache_.find(post_id);
        if (it != analytics_cache_.end()) {
            auto age = std::chrono::system_clock::now() - it->second.last_updated_at;
            if (age < std::chrono::minutes(CACHE_TTL_MINUTES)) {
                return it->second;
            }
        }
    }

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get base post analytics
        auto result = txn.exec_params(
            "SELECT sp.id as post_id, sp.dog_id, sp.shelter_id, "
            "sp.total_impressions, sp.total_engagements, sp.total_clicks as total_link_clicks, "
            "0 as total_reach, 0 as total_video_views, 0 as total_new_followers, "
            "sp.resulted_in_adoption, sp.resulted_in_foster, "
            "false as resulted_in_application, 0 as application_count, "
            "0.0 as amount_spent, 0.0 as cost_per_engagement, 0.0 as cost_per_click "
            "FROM social_posts sp WHERE sp.id = $1",
            post_id
        );

        if (result.empty()) {
            ConnectionPool::getInstance().release(conn);
            return std::nullopt;
        }

        PostAnalytics analytics = PostAnalytics::fromDbRow(result[0]);

        // Get platform-specific analytics
        auto platform_result = txn.exec_params(
            "SELECT * FROM social_analytics WHERE post_id = $1",
            post_id
        );

        for (const auto& row : platform_result) {
            PlatformAnalytics platform_analytics = PlatformAnalytics::fromDbRow(row);
            analytics.platform_analytics[platform_analytics.platform] = platform_analytics;
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        analytics.aggregate();
        analytics.last_updated_at = std::chrono::system_clock::now();

        // Update cache
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            analytics_cache_[post_id] = analytics;
        }

        return analytics;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get post analytics: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
        return std::nullopt;
    }
}

std::vector<PostAnalytics> SocialAnalytics::getDogAnalytics(const std::string& dog_id) {
    std::vector<PostAnalytics> result;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto posts_result = txn.exec_params(
            "SELECT id FROM social_posts WHERE dog_id = $1 ORDER BY created_at DESC",
            dog_id
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        for (const auto& row : posts_result) {
            auto analytics = getPostAnalytics(row["id"].as<std::string>());
            if (analytics) {
                result.push_back(*analytics);
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get dog analytics: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return result;
}

AnalyticsSummary SocialAnalytics::getSummary(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) {

    AnalyticsSummary summary;
    summary.start_date = start;
    summary.end_date = end;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Format timestamps for query
        auto time_t_start = std::chrono::system_clock::to_time_t(start);
        auto time_t_end = std::chrono::system_clock::to_time_t(end);
        std::ostringstream oss_start, oss_end;
        oss_start << std::put_time(std::gmtime(&time_t_start), "%Y-%m-%d %H:%M:%S");
        oss_end << std::put_time(std::gmtime(&time_t_end), "%Y-%m-%d %H:%M:%S");

        // Get aggregate stats
        auto result = txn.exec_params(
            "SELECT "
            "COUNT(*) as total_posts, "
            "SUM(CASE WHEN video_url IS NOT NULL THEN 1 ELSE 0 END) as posts_with_video, "
            "SUM(CASE WHEN array_length(images, 1) > 0 THEN 1 ELSE 0 END) as posts_with_images, "
            "SUM(total_impressions) as total_impressions, "
            "SUM(total_engagements) as total_engagements, "
            "SUM(total_clicks) as total_link_clicks, "
            "SUM(CASE WHEN resulted_in_adoption THEN 1 ELSE 0 END) as adoptions, "
            "SUM(CASE WHEN resulted_in_foster THEN 1 ELSE 0 END) as fosters "
            "FROM social_posts "
            "WHERE created_at BETWEEN $1 AND $2",
            oss_start.str(), oss_end.str()
        );

        if (!result.empty()) {
            summary.total_posts = result[0]["total_posts"].as<int>();
            summary.posts_with_video = result[0]["posts_with_video"].as<int>();
            summary.posts_with_images = result[0]["posts_with_images"].as<int>();
            summary.total_impressions = result[0]["total_impressions"].as<int>();
            summary.total_engagements = result[0]["total_engagements"].as<int>();
            summary.total_link_clicks = result[0]["total_link_clicks"].as<int>();
            summary.adoptions_attributed = result[0]["adoptions"].as<int>();
            summary.fosters_attributed = result[0]["fosters"].as<int>();
        }

        // Calculate average engagement rate
        if (summary.total_impressions > 0) {
            summary.average_engagement_rate =
                static_cast<double>(summary.total_engagements) /
                summary.total_impressions * 100.0;
        }

        // Get top post
        auto top_result = txn.exec_params(
            "SELECT id, total_engagements FROM social_posts "
            "WHERE created_at BETWEEN $1 AND $2 "
            "ORDER BY total_engagements DESC LIMIT 1",
            oss_start.str(), oss_end.str()
        );

        if (!top_result.empty()) {
            summary.top_post_id = top_result[0]["id"].as<std::string>();
            summary.top_post_engagements = top_result[0]["total_engagements"].as<int>();
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get analytics summary: " + std::string(e.what()),
            {}
        );
    }

    return summary;
}

std::vector<PostAnalytics> SocialAnalytics::getByPlatform(Platform platform, int limit) {
    std::vector<PostAnalytics> result;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto posts_result = txn.exec_params(
            "SELECT DISTINCT sp.id FROM social_posts sp "
            "WHERE $1 = ANY(sp.platforms) "
            "ORDER BY sp.created_at DESC LIMIT $2",
            platformToString(platform), limit
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        for (const auto& row : posts_result) {
            auto analytics = getPostAnalytics(row["id"].as<std::string>());
            if (analytics) {
                result.push_back(*analytics);
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get platform analytics: " + std::string(e.what()),
            {{"platform", platformToString(platform)}}
        );
    }

    return result;
}

std::vector<PostAnalytics> SocialAnalytics::getTopPosts(const std::string& metric, int limit) {
    std::vector<PostAnalytics> result;

    std::string order_column = "total_engagements";
    if (metric == "impressions") {
        order_column = "total_impressions";
    } else if (metric == "clicks") {
        order_column = "total_clicks";
    }

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto posts_result = txn.exec_params(
            "SELECT id FROM social_posts ORDER BY " + order_column + " DESC LIMIT $1",
            limit
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        for (const auto& row : posts_result) {
            auto analytics = getPostAnalytics(row["id"].as<std::string>());
            if (analytics) {
                result.push_back(*analytics);
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get top posts: " + std::string(e.what()),
            {{"metric", metric}}
        );
    }

    return result;
}

// ============================================================================
// ADOPTION IMPACT
// ============================================================================

std::vector<AdoptionImpact> SocialAnalytics::getAdoptionImpact(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) {

    std::vector<AdoptionImpact> result;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto time_t_start = std::chrono::system_clock::to_time_t(start);
        auto time_t_end = std::chrono::system_clock::to_time_t(end);
        std::ostringstream oss_start, oss_end;
        oss_start << std::put_time(std::gmtime(&time_t_start), "%Y-%m-%d %H:%M:%S");
        oss_end << std::put_time(std::gmtime(&time_t_end), "%Y-%m-%d %H:%M:%S");

        auto adoptions = txn.exec_params(
            "SELECT saa.dog_id, saa.post_id, saa.platform, saa.adoption_date, "
            "d.name as dog_name, s.name as shelter_name "
            "FROM social_adoption_attributions saa "
            "JOIN dogs d ON d.id = saa.dog_id "
            "JOIN shelters s ON s.id = d.shelter_id "
            "WHERE saa.adoption_date BETWEEN $1 AND $2",
            oss_start.str(), oss_end.str()
        );

        for (const auto& row : adoptions) {
            AdoptionImpact impact;
            impact.dog_id = row["dog_id"].as<std::string>();
            impact.dog_name = row["dog_name"].as<std::string>();
            impact.shelter_name = row["shelter_name"].as<std::string>();

            // Parse adoption date
            std::string adopt_ts = row["adoption_date"].as<std::string>();
            std::tm tm = {};
            std::istringstream ss(adopt_ts);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            impact.adoption_date = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            if (!row["post_id"].is_null()) {
                impact.attributed_post_id = row["post_id"].as<std::string>();
            }
            if (!row["platform"].is_null() && !row["platform"].as<std::string>().empty()) {
                impact.attributed_platform = stringToPlatform(row["platform"].as<std::string>());
            }

            // Get all posts for this dog
            auto posts = txn.exec_params(
                "SELECT id, total_impressions, total_engagements, created_at "
                "FROM social_posts WHERE dog_id = $1 ORDER BY created_at ASC",
                impact.dog_id
            );

            impact.total_social_impressions = 0;
            impact.total_social_engagements = 0;

            for (const auto& post_row : posts) {
                impact.post_ids.push_back(post_row["id"].as<std::string>());
                impact.total_social_impressions += post_row["total_impressions"].as<int>();
                impact.total_social_engagements += post_row["total_engagements"].as<int>();
            }

            if (!posts.empty()) {
                std::string first_ts = posts[0]["created_at"].as<std::string>();
                std::tm tm_first = {};
                std::istringstream ss_first(first_ts);
                ss_first >> std::get_time(&tm_first, "%Y-%m-%d %H:%M:%S");
                impact.first_social_post = std::chrono::system_clock::from_time_t(std::mktime(&tm_first));

                auto days = std::chrono::duration_cast<std::chrono::hours>(
                    impact.adoption_date - impact.first_social_post).count() / 24;
                impact.days_from_first_post = static_cast<int>(days);
            }

            result.push_back(impact);
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get adoption impact: " + std::string(e.what()),
            {}
        );
    }

    return result;
}

Json::Value SocialAnalytics::calculateImpactMetrics(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) {

    Json::Value metrics;

    auto impacts = getAdoptionImpact(start, end);
    auto summary = getSummary(start, end);

    metrics["total_adoptions_with_social"] = static_cast<int>(impacts.size());
    metrics["total_posts_in_period"] = summary.total_posts;
    metrics["total_impressions"] = summary.total_impressions;
    metrics["total_engagements"] = summary.total_engagements;

    // Calculate average days to adoption
    int total_days = 0;
    for (const auto& impact : impacts) {
        total_days += impact.days_from_first_post;
    }
    metrics["average_days_to_adoption"] = impacts.empty() ?
        0 : total_days / static_cast<int>(impacts.size());

    // Calculate adoption rate
    if (summary.total_posts > 0) {
        metrics["adoption_rate_per_post"] =
            static_cast<double>(impacts.size()) / summary.total_posts * 100.0;
    } else {
        metrics["adoption_rate_per_post"] = 0.0;
    }

    // Impressions per adoption
    if (!impacts.empty()) {
        metrics["impressions_per_adoption"] =
            summary.total_impressions / static_cast<int>(impacts.size());
    } else {
        metrics["impressions_per_adoption"] = 0;
    }

    return metrics;
}

// ============================================================================
// SYNC AND REFRESH
// ============================================================================

bool SocialAnalytics::syncPostAnalytics(const std::string& post_id) {
    // This would integrate with platform APIs to fetch latest analytics
    // For now, just refresh from database
    refreshAggregates(post_id);
    return true;
}

int SocialAnalytics::syncRecentAnalytics(int hours_back) {
    int synced = 0;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(hours_back);
        auto time_t_cutoff = std::chrono::system_clock::to_time_t(cutoff);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t_cutoff), "%Y-%m-%d %H:%M:%S");

        auto posts = txn.exec_params(
            "SELECT id FROM social_posts WHERE updated_at > $1",
            oss.str()
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        for (const auto& row : posts) {
            if (syncPostAnalytics(row["id"].as<std::string>())) {
                synced++;
            }
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to sync recent analytics: " + std::string(e.what()),
            {{"hours_back", std::to_string(hours_back)}}
        );
    }

    return synced;
}

void SocialAnalytics::refreshAggregates(const std::string& post_id) {
    // Invalidate cache to force fresh fetch
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        analytics_cache_.erase(post_id);
    }

    // Fetch fresh data
    getPostAnalytics(post_id);
}

// ============================================================================
// OPTIMAL TIMING
// ============================================================================

Json::Value SocialAnalytics::getOptimalPostTimes(Platform platform) {
    Json::Value result;
    result["platform"] = platformToString(platform);

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Analyze engagement by hour and day of week
        auto analysis = txn.exec_params(
            "SELECT "
            "EXTRACT(DOW FROM created_at) as day_of_week, "
            "EXTRACT(HOUR FROM created_at) as hour, "
            "AVG(total_engagements) as avg_engagements, "
            "AVG(total_impressions) as avg_impressions, "
            "COUNT(*) as post_count "
            "FROM social_posts "
            "WHERE $1 = ANY(platforms) "
            "GROUP BY EXTRACT(DOW FROM created_at), EXTRACT(HOUR FROM created_at) "
            "ORDER BY avg_engagements DESC",
            platformToString(platform)
        );

        Json::Value by_day(Json::arrayValue);
        std::map<int, std::map<int, double>> engagement_by_day_hour;

        for (const auto& row : analysis) {
            int day = row["day_of_week"].as<int>();
            int hour = row["hour"].as<int>();
            double avg_engagement = row["avg_engagements"].as<double>();
            engagement_by_day_hour[day][hour] = avg_engagement;
        }

        // Format as readable data
        std::vector<std::string> day_names = {
            "Sunday", "Monday", "Tuesday", "Wednesday",
            "Thursday", "Friday", "Saturday"
        };

        for (int day = 0; day < 7; day++) {
            Json::Value day_data;
            day_data["day"] = day_names[day];

            // Find best hour for this day
            int best_hour = 12;  // Default
            double best_engagement = 0;

            for (int hour = 0; hour < 24; hour++) {
                auto it = engagement_by_day_hour.find(day);
                if (it != engagement_by_day_hour.end()) {
                    auto hour_it = it->second.find(hour);
                    if (hour_it != it->second.end() &&
                        hour_it->second > best_engagement) {
                        best_engagement = hour_it->second;
                        best_hour = hour;
                    }
                }
            }

            day_data["best_hour"] = best_hour;
            day_data["best_engagement_avg"] = best_engagement;
            by_day.append(day_data);
        }

        result["by_day"] = by_day;

        // Get overall best times
        Json::Value best_times(Json::arrayValue);
        int count = 0;
        for (const auto& row : analysis) {
            if (count >= 5) break;  // Top 5 time slots
            Json::Value slot;
            slot["day_of_week"] = static_cast<int>(row["day_of_week"].as<int>());
            slot["day_name"] = day_names[static_cast<int>(row["day_of_week"].as<int>())];
            slot["hour"] = static_cast<int>(row["hour"].as<int>());
            slot["avg_engagements"] = row["avg_engagements"].as<double>();
            slot["post_count"] = row["post_count"].as<int>();
            best_times.append(slot);
            count++;
        }
        result["best_times"] = best_times;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get optimal post times: " + std::string(e.what()),
            {{"platform", platformToString(platform)}}
        );
    }

    return result;
}

Json::Value SocialAnalytics::analyzeContentPerformance(Platform platform) {
    Json::Value result;
    result["platform"] = platformToString(platform);

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Analyze by post type
        auto type_analysis = txn.exec_params(
            "SELECT post_type, "
            "COUNT(*) as post_count, "
            "AVG(total_engagements) as avg_engagements, "
            "AVG(total_impressions) as avg_impressions, "
            "SUM(CASE WHEN resulted_in_adoption THEN 1 ELSE 0 END) as adoptions "
            "FROM social_posts "
            "WHERE $1 = ANY(platforms) "
            "GROUP BY post_type "
            "ORDER BY avg_engagements DESC",
            platformToString(platform)
        );

        Json::Value by_type(Json::arrayValue);
        for (const auto& row : type_analysis) {
            Json::Value type_data;
            type_data["post_type"] = row["post_type"].as<std::string>();
            type_data["post_count"] = row["post_count"].as<int>();
            type_data["avg_engagements"] = row["avg_engagements"].as<double>();
            type_data["avg_impressions"] = row["avg_impressions"].as<double>();
            type_data["adoptions"] = row["adoptions"].as<int>();
            by_type.append(type_data);
        }
        result["by_type"] = by_type;

        // Analyze video vs image performance
        auto media_analysis = txn.exec_params(
            "SELECT "
            "CASE WHEN video_url IS NOT NULL THEN 'video' "
            "     WHEN array_length(images, 1) > 0 THEN 'image' "
            "     ELSE 'text_only' END as media_type, "
            "COUNT(*) as post_count, "
            "AVG(total_engagements) as avg_engagements, "
            "AVG(total_impressions) as avg_impressions "
            "FROM social_posts "
            "WHERE $1 = ANY(platforms) "
            "GROUP BY 1 "
            "ORDER BY avg_engagements DESC",
            platformToString(platform)
        );

        Json::Value by_media(Json::arrayValue);
        for (const auto& row : media_analysis) {
            Json::Value media_data;
            media_data["media_type"] = row["media_type"].as<std::string>();
            media_data["post_count"] = row["post_count"].as<int>();
            media_data["avg_engagements"] = row["avg_engagements"].as<double>();
            media_data["avg_impressions"] = row["avg_impressions"].as<double>();
            by_media.append(media_data);
        }
        result["by_media_type"] = by_media;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to analyze content performance: " + std::string(e.what()),
            {{"platform", platformToString(platform)}}
        );
    }

    return result;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void SocialAnalytics::saveToDatabase(const std::string& post_id,
                                      const PlatformAnalytics& analytics) {
    auto conn = ConnectionPool::getInstance().acquire();

    try {
        pqxx::work txn(*conn);

        // Upsert analytics record
        txn.exec_params(
            "INSERT INTO social_analytics "
            "(post_id, platform, impressions, reach, profile_visits, "
            "likes, comments, shares, saves, link_clicks, "
            "video_views, video_watch_time_seconds, video_completion_rate, "
            "new_followers, engagement_rate, fetched_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, NOW()) "
            "ON CONFLICT (post_id, platform) DO UPDATE SET "
            "impressions = EXCLUDED.impressions, "
            "reach = EXCLUDED.reach, "
            "profile_visits = EXCLUDED.profile_visits, "
            "likes = EXCLUDED.likes, "
            "comments = EXCLUDED.comments, "
            "shares = EXCLUDED.shares, "
            "saves = EXCLUDED.saves, "
            "link_clicks = EXCLUDED.link_clicks, "
            "video_views = EXCLUDED.video_views, "
            "video_watch_time_seconds = EXCLUDED.video_watch_time_seconds, "
            "video_completion_rate = EXCLUDED.video_completion_rate, "
            "new_followers = EXCLUDED.new_followers, "
            "engagement_rate = EXCLUDED.engagement_rate, "
            "fetched_at = NOW()",
            post_id,
            platformToString(analytics.platform),
            analytics.impressions,
            analytics.reach,
            analytics.profile_visits,
            analytics.likes,
            analytics.comments,
            analytics.shares,
            analytics.saves,
            analytics.link_clicks,
            analytics.video_views,
            analytics.video_watch_time_seconds,
            analytics.video_completion_rate,
            analytics.new_followers,
            analytics.engagement_rate
        );

        // Update post totals
        txn.exec_params(
            "UPDATE social_posts SET "
            "total_impressions = (SELECT COALESCE(SUM(impressions), 0) FROM social_analytics WHERE post_id = $1), "
            "total_engagements = (SELECT COALESCE(SUM(likes + comments + shares + saves), 0) FROM social_analytics WHERE post_id = $1), "
            "total_clicks = (SELECT COALESCE(SUM(link_clicks), 0) FROM social_analytics WHERE post_id = $1), "
            "updated_at = NOW() "
            "WHERE id = $1",
            post_id
        );

        txn.commit();

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to save analytics to database: " + std::string(e.what()),
            {{"post_id", post_id}}
        );
        throw;
    }

    ConnectionPool::getInstance().release(conn);
}

std::optional<PlatformAnalytics> SocialAnalytics::loadFromDatabase(
    const std::string& post_id, Platform platform) {

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM social_analytics WHERE post_id = $1 AND platform = $2",
            post_id, platformToString(platform)
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return PlatformAnalytics::fromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to load analytics from database: " + std::string(e.what()),
            {{"post_id", post_id}, {"platform", platformToString(platform)}}
        );
        return std::nullopt;
    }
}

} // namespace wtl::content::social
