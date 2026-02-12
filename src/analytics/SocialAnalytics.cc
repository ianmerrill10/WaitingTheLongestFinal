/**
 * @file SocialAnalytics.cc
 * @brief Implementation of SocialAnalytics
 * @see SocialAnalytics.h for documentation
 */

#include "analytics/SocialAnalytics.h"

#include "analytics/AnalyticsEvent.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

#include <unordered_set>

namespace wtl::analytics {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// SOCIAL CONTENT
// ============================================================================

Json::Value SocialContent::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["platform"] = socialPlatformToString(platform);
    json["platform_content_id"] = platform_content_id;
    json["dog_id"] = dog_id;
    json["content_type"] = content_type;
    json["url"] = url;
    if (caption) json["caption"] = *caption;
    json["posted_at"] = formatTimestampISO(posted_at);
    json["created_at"] = formatTimestampISO(created_at);
    return json;
}

SocialContent SocialContent::fromDbRow(const pqxx::row& row) {
    SocialContent content;
    content.id = row["id"].as<std::string>();
    content.platform = socialPlatformFromString(row["platform"].as<std::string>());
    content.platform_content_id = row["platform_content_id"].as<std::string>("");
    content.dog_id = row["dog_id"].as<std::string>("");
    content.content_type = row["content_type"].as<std::string>("");
    content.url = row["url"].as<std::string>("");
    if (!row["caption"].is_null()) {
        content.caption = row["caption"].as<std::string>();
    }
    content.posted_at = parseTimestamp(row["posted_at"].as<std::string>());
    content.created_at = parseTimestamp(row["created_at"].as<std::string>());
    return content;
}

// ============================================================================
// CONTENT METRICS
// ============================================================================

Json::Value ContentMetrics::toJson() const {
    Json::Value json;
    json["content_id"] = content_id;
    json["platform"] = socialPlatformToString(platform);

    json["views"] = views;
    json["likes"] = likes;
    json["comments"] = comments;
    json["shares"] = shares;
    json["saves"] = saves;
    json["follows"] = follows;

    json["engagement_rate"] = engagement_rate;
    json["share_rate"] = share_rate;
    json["save_rate"] = save_rate;

    json["watch_time_seconds"] = watch_time_seconds;
    json["average_watch_percent"] = average_watch_percent;
    json["profile_visits"] = profile_visits;
    json["link_clicks"] = link_clicks;

    json["site_visits"] = site_visits;
    json["dog_views"] = dog_views;
    json["applications"] = applications;
    json["adoptions"] = adoptions;

    return json;
}

ContentMetrics ContentMetrics::fromDbRow(const pqxx::row& row) {
    ContentMetrics metrics;
    metrics.content_id = row["content_id"].as<std::string>();
    metrics.platform = socialPlatformFromString(row["platform"].as<std::string>("unknown"));

    metrics.views = row["views"].as<int>(0);
    metrics.likes = row["likes"].as<int>(0);
    metrics.comments = row["comments"].as<int>(0);
    metrics.shares = row["shares"].as<int>(0);
    metrics.saves = row["saves"].as<int>(0);
    metrics.follows = row["follows"].as<int>(0);

    metrics.engagement_rate = row["engagement_rate"].as<double>(0);
    metrics.share_rate = row["share_rate"].as<double>(0);
    metrics.save_rate = row["save_rate"].as<double>(0);

    metrics.watch_time_seconds = row["watch_time_seconds"].as<int>(0);
    metrics.average_watch_percent = row["average_watch_percent"].as<int>(0);
    metrics.profile_visits = row["profile_visits"].as<int>(0);
    metrics.link_clicks = row["link_clicks"].as<int>(0);

    metrics.site_visits = row["site_visits"].as<int>(0);
    metrics.dog_views = row["dog_views"].as<int>(0);
    metrics.applications = row["applications"].as<int>(0);
    metrics.adoptions = row["adoptions"].as<int>(0);

    return metrics;
}

// ============================================================================
// PLATFORM STATS
// ============================================================================

Json::Value PlatformStats::toJson() const {
    Json::Value json;
    json["platform"] = socialPlatformToString(platform);

    json["total_content"] = total_content;
    json["total_views"] = total_views;
    json["total_likes"] = total_likes;
    json["total_comments"] = total_comments;
    json["total_shares"] = total_shares;
    json["total_saves"] = total_saves;

    json["avg_views_per_content"] = avg_views_per_content;
    json["avg_engagement_rate"] = avg_engagement_rate;
    json["avg_share_rate"] = avg_share_rate;

    json["top_content_id"] = top_content_id;
    json["top_content_views"] = top_content_views;
    json["most_engaging_content_id"] = most_engaging_content_id;
    json["highest_engagement_rate"] = highest_engagement_rate;

    json["total_site_visits"] = total_site_visits;
    json["total_dog_views"] = total_dog_views;
    json["total_applications"] = total_applications;
    json["total_adoptions"] = total_adoptions;
    json["adoption_conversion_rate"] = adoption_conversion_rate;

    json["views_week_over_week"] = views_week_over_week;
    json["engagement_week_over_week"] = engagement_week_over_week;

    return json;
}

// ============================================================================
// CONTENT PERFORMANCE RANKING
// ============================================================================

Json::Value ContentPerformanceRanking::toJson() const {
    Json::Value json;
    json["content_id"] = content_id;
    json["platform"] = platform;
    json["dog_id"] = dog_id;
    json["dog_name"] = dog_name;
    json["views"] = views;
    json["engagement_rate"] = engagement_rate;
    json["adoptions"] = adoptions;
    json["score"] = score;
    return json;
}

// ============================================================================
// SOCIAL OVERVIEW
// ============================================================================

Json::Value SocialOverview::toJson() const {
    Json::Value json;

    json["total_views"] = total_views;
    json["total_engagement"] = total_engagement;
    json["total_content_pieces"] = total_content_pieces;
    json["total_attributed_adoptions"] = total_attributed_adoptions;

    Json::Value platforms_json(Json::objectValue);
    for (const auto& [platform, stats] : by_platform) {
        platforms_json[socialPlatformToString(platform)] = stats.toJson();
    }
    json["by_platform"] = platforms_json;

    Json::Value top_views_arr(Json::arrayValue);
    for (const auto& r : top_by_views) {
        top_views_arr.append(r.toJson());
    }
    json["top_by_views"] = top_views_arr;

    Json::Value top_eng_arr(Json::arrayValue);
    for (const auto& r : top_by_engagement) {
        top_eng_arr.append(r.toJson());
    }
    json["top_by_engagement"] = top_eng_arr;

    Json::Value top_adopt_arr(Json::arrayValue);
    for (const auto& r : top_by_adoptions) {
        top_adopt_arr.append(r.toJson());
    }
    json["top_by_adoptions"] = top_adopt_arr;

    json["best_platform_by_views"] = socialPlatformToString(best_platform_by_views);
    json["best_platform_by_engagement"] = socialPlatformToString(best_platform_by_engagement);
    json["best_platform_by_adoptions"] = socialPlatformToString(best_platform_by_adoptions);

    Json::Value views_trend(Json::arrayValue);
    for (const auto& t : daily_views) {
        views_trend.append(t.toJson());
    }
    json["daily_views"] = views_trend;

    Json::Value eng_trend(Json::arrayValue);
    for (const auto& t : daily_engagement) {
        eng_trend.append(t.toJson());
    }
    json["daily_engagement"] = eng_trend;

    return json;
}

// ============================================================================
// SINGLETON
// ============================================================================

SocialAnalytics& SocialAnalytics::getInstance() {
    static SocialAnalytics instance;
    return instance;
}

SocialAnalytics::SocialAnalytics() {
}

// ============================================================================
// CONTENT MANAGEMENT
// ============================================================================

std::string SocialAnalytics::registerContent(
    SocialPlatform platform,
    const std::string& platform_content_id,
    const std::string& dog_id,
    const std::string& content_type,
    const std::string& url)
{
    std::string content_id = generateUUID();

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            R"(INSERT INTO social_content
               (id, platform, platform_content_id, dog_id, content_type, url, posted_at, created_at)
               VALUES ($1, $2, $3, $4, $5, $6, NOW(), NOW()))",
            content_id,
            socialPlatformToString(platform),
            platform_content_id,
            dog_id,
            content_type,
            url
        );

        // Initialize metrics record
        txn.exec_params(
            R"(INSERT INTO social_performance (content_id, platform, created_at, updated_at)
               VALUES ($1, $2, NOW(), NOW()))",
            content_id,
            socialPlatformToString(platform)
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return content_id;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to register social content: " + std::string(e.what()),
            {{"platform", socialPlatformToString(platform)}, {"dog_id", dog_id}}
        );
        return "";
    }
}

std::optional<SocialContent> SocialAnalytics::getContent(const std::string& content_id) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM social_content WHERE id = $1",
            content_id
        );

        ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return SocialContent::fromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get social content: " + std::string(e.what()),
            {{"content_id", content_id}}
        );
        return std::nullopt;
    }
}

std::optional<SocialContent> SocialAnalytics::getContentByPlatformId(
    SocialPlatform platform,
    const std::string& platform_content_id)
{
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM social_content WHERE platform = $1 AND platform_content_id = $2",
            socialPlatformToString(platform),
            platform_content_id
        );

        ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return SocialContent::fromDbRow(result[0]);

    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

std::vector<SocialContent> SocialAnalytics::getContentByDog(const std::string& dog_id) {
    std::vector<SocialContent> contents;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM social_content WHERE dog_id = $1 ORDER BY posted_at DESC",
            dog_id
        );

        for (const auto& row : result) {
            contents.push_back(SocialContent::fromDbRow(row));
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get content by dog: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return contents;
}

// ============================================================================
// RECORDING METRICS
// ============================================================================

void SocialAnalytics::recordView(const std::string& content_id, SocialPlatform platform) {
    incrementMetric(resolveContentId(content_id, platform), "views");
}

void SocialAnalytics::recordLike(const std::string& content_id, SocialPlatform platform) {
    incrementMetric(resolveContentId(content_id, platform), "likes");
}

void SocialAnalytics::recordComment(const std::string& content_id, SocialPlatform platform) {
    incrementMetric(resolveContentId(content_id, platform), "comments");
}

void SocialAnalytics::recordShare(const std::string& content_id, SocialPlatform platform) {
    incrementMetric(resolveContentId(content_id, platform), "shares");
}

void SocialAnalytics::recordSave(const std::string& content_id, SocialPlatform platform) {
    incrementMetric(resolveContentId(content_id, platform), "saves");
}

void SocialAnalytics::recordFollow(const std::string& content_id, SocialPlatform platform) {
    incrementMetric(resolveContentId(content_id, platform), "follows");
}

void SocialAnalytics::updateMetrics(const std::string& content_id, const ContentMetrics& metrics) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        double eng_rate = calculateEngagementRate(
            metrics.views, metrics.likes, metrics.comments, metrics.shares);
        double share_rate = metrics.views > 0 ?
            (static_cast<double>(metrics.shares) / metrics.views * 100.0) : 0;
        double save_rate = metrics.views > 0 ?
            (static_cast<double>(metrics.saves) / metrics.views * 100.0) : 0;

        txn.exec_params(
            R"(UPDATE social_performance SET
               views = $2, likes = $3, comments = $4, shares = $5, saves = $6, follows = $7,
               engagement_rate = $8, share_rate = $9, save_rate = $10,
               watch_time_seconds = $11, average_watch_percent = $12,
               profile_visits = $13, link_clicks = $14,
               updated_at = NOW()
               WHERE content_id = $1)",
            content_id,
            metrics.views, metrics.likes, metrics.comments, metrics.shares, metrics.saves, metrics.follows,
            eng_rate, share_rate, save_rate,
            metrics.watch_time_seconds, metrics.average_watch_percent,
            metrics.profile_visits, metrics.link_clicks
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to update social metrics: " + std::string(e.what()),
            {{"content_id", content_id}}
        );
    }
}

void SocialAnalytics::recordSiteVisit(const std::string& content_id, const std::string& session_id) {
    incrementMetric(content_id, "site_visits");
}

void SocialAnalytics::recordDogView(const std::string& content_id, const std::string& dog_id) {
    incrementMetric(content_id, "dog_views");
}

void SocialAnalytics::recordApplication(const std::string& content_id, const std::string& dog_id) {
    incrementMetric(content_id, "applications");
}

void SocialAnalytics::recordAdoption(const std::string& content_id, const std::string& dog_id) {
    incrementMetric(content_id, "adoptions");
}

// ============================================================================
// QUERYING METRICS
// ============================================================================

ContentMetrics SocialAnalytics::getContentMetrics(const std::string& content_id) {
    ContentMetrics metrics;
    metrics.content_id = content_id;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM social_performance WHERE content_id = $1",
            content_id
        );

        ConnectionPool::getInstance().release(conn);

        if (!result.empty()) {
            return ContentMetrics::fromDbRow(result[0]);
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get content metrics: " + std::string(e.what()),
            {{"content_id", content_id}}
        );
    }

    return metrics;
}

PlatformStats SocialAnalytics::getPlatformStats(SocialPlatform platform, const DateRange& range) {
    PlatformStats stats;
    stats.platform = platform;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string platform_str = socialPlatformToString(platform);

        // Get aggregate stats
        auto agg_query = R"(
            SELECT
                COUNT(DISTINCT sp.content_id) as total_content,
                SUM(sp.views) as total_views,
                SUM(sp.likes) as total_likes,
                SUM(sp.comments) as total_comments,
                SUM(sp.shares) as total_shares,
                SUM(sp.saves) as total_saves,
                AVG(sp.views) as avg_views,
                AVG(sp.engagement_rate) as avg_engagement,
                AVG(sp.share_rate) as avg_share,
                SUM(sp.site_visits) as total_site_visits,
                SUM(sp.dog_views) as total_dog_views,
                SUM(sp.applications) as total_applications,
                SUM(sp.adoptions) as total_adoptions
            FROM social_performance sp
            JOIN social_content sc ON sp.content_id = sc.id
            WHERE sp.platform = $1
              AND sc.posted_at >= $2 AND sc.posted_at <= $3
        )";

        auto agg_result = txn.exec_params1(
            agg_query, platform_str, range.getStartISO(), range.getEndISO());

        stats.total_content = agg_result["total_content"].as<int>(0);
        stats.total_views = agg_result["total_views"].as<int>(0);
        stats.total_likes = agg_result["total_likes"].as<int>(0);
        stats.total_comments = agg_result["total_comments"].as<int>(0);
        stats.total_shares = agg_result["total_shares"].as<int>(0);
        stats.total_saves = agg_result["total_saves"].as<int>(0);
        stats.avg_views_per_content = agg_result["avg_views"].as<double>(0);
        stats.avg_engagement_rate = agg_result["avg_engagement"].as<double>(0);
        stats.avg_share_rate = agg_result["avg_share"].as<double>(0);
        stats.total_site_visits = agg_result["total_site_visits"].as<int>(0);
        stats.total_dog_views = agg_result["total_dog_views"].as<int>(0);
        stats.total_applications = agg_result["total_applications"].as<int>(0);
        stats.total_adoptions = agg_result["total_adoptions"].as<int>(0);

        if (stats.total_views > 0) {
            stats.adoption_conversion_rate =
                (static_cast<double>(stats.total_adoptions) / stats.total_views) * 100.0;
        }

        // Get top content by views
        auto top_views_query = R"(
            SELECT sp.content_id, sp.views
            FROM social_performance sp
            JOIN social_content sc ON sp.content_id = sc.id
            WHERE sp.platform = $1
              AND sc.posted_at >= $2 AND sc.posted_at <= $3
            ORDER BY sp.views DESC
            LIMIT 1
        )";

        auto top_result = txn.exec_params(
            top_views_query, platform_str, range.getStartISO(), range.getEndISO());

        if (!top_result.empty()) {
            stats.top_content_id = top_result[0]["content_id"].as<std::string>();
            stats.top_content_views = top_result[0]["views"].as<int>(0);
        }

        // Get most engaging content
        auto top_eng_query = R"(
            SELECT sp.content_id, sp.engagement_rate
            FROM social_performance sp
            JOIN social_content sc ON sp.content_id = sc.id
            WHERE sp.platform = $1
              AND sc.posted_at >= $2 AND sc.posted_at <= $3
              AND sp.views >= 100
            ORDER BY sp.engagement_rate DESC
            LIMIT 1
        )";

        auto eng_result = txn.exec_params(
            top_eng_query, platform_str, range.getStartISO(), range.getEndISO());

        if (!eng_result.empty()) {
            stats.most_engaging_content_id = eng_result[0]["content_id"].as<std::string>();
            stats.highest_engagement_rate = eng_result[0]["engagement_rate"].as<double>(0);
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get platform stats: " + std::string(e.what()),
            {{"platform", socialPlatformToString(platform)}}
        );
    }

    return stats;
}

PlatformStats SocialAnalytics::getPlatformStats(const std::string& platform, const DateRange& range) {
    return getPlatformStats(socialPlatformFromString(platform), range);
}

SocialOverview SocialAnalytics::getSocialOverview(const DateRange& range) {
    SocialOverview overview;

    // Get stats for each platform
    std::vector<SocialPlatform> platforms = {
        SocialPlatform::TIKTOK,
        SocialPlatform::INSTAGRAM,
        SocialPlatform::FACEBOOK,
        SocialPlatform::TWITTER
    };

    int max_views = 0;
    int max_adoptions = 0;
    double max_engagement = 0;

    for (auto platform : platforms) {
        auto stats = getPlatformStats(platform, range);
        overview.by_platform[platform] = stats;

        overview.total_views += stats.total_views;
        overview.total_engagement += stats.total_likes + stats.total_comments + stats.total_shares;
        overview.total_content_pieces += stats.total_content;
        overview.total_attributed_adoptions += stats.total_adoptions;

        if (stats.total_views > max_views) {
            max_views = stats.total_views;
            overview.best_platform_by_views = platform;
        }
        if (stats.total_adoptions > max_adoptions) {
            max_adoptions = stats.total_adoptions;
            overview.best_platform_by_adoptions = platform;
        }
        if (stats.avg_engagement_rate > max_engagement) {
            max_engagement = stats.avg_engagement_rate;
            overview.best_platform_by_engagement = platform;
        }
    }

    // Get top content
    overview.top_by_views = getTopByViews(range, 5);
    overview.top_by_engagement = getTopByEngagement(range, 5);
    overview.top_by_adoptions = getTopByAdoptions(range, 5);

    return overview;
}

std::vector<ContentPerformanceRanking> SocialAnalytics::getTopByViews(const DateRange& range, int limit) {
    std::vector<ContentPerformanceRanking> rankings;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT sp.content_id, sp.platform, sc.dog_id, d.name as dog_name,
                   sp.views, sp.engagement_rate, sp.adoptions
            FROM social_performance sp
            JOIN social_content sc ON sp.content_id = sc.id
            LEFT JOIN dogs d ON sc.dog_id = d.id
            WHERE sc.posted_at >= $1 AND sc.posted_at <= $2
            ORDER BY sp.views DESC
            LIMIT $3
        )";

        auto result = txn.exec_params(query, range.getStartISO(), range.getEndISO(), limit);

        for (const auto& row : result) {
            ContentPerformanceRanking r;
            r.content_id = row["content_id"].as<std::string>();
            r.platform = row["platform"].as<std::string>();
            r.dog_id = row["dog_id"].as<std::string>("");
            r.dog_name = row["dog_name"].as<std::string>("");
            r.views = row["views"].as<int>(0);
            r.engagement_rate = row["engagement_rate"].as<double>(0);
            r.adoptions = row["adoptions"].as<int>(0);
            r.score = static_cast<double>(r.views);
            rankings.push_back(r);
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get top content by views: " + std::string(e.what()),
            {}
        );
    }

    return rankings;
}

std::vector<ContentPerformanceRanking> SocialAnalytics::getTopByEngagement(const DateRange& range, int limit) {
    std::vector<ContentPerformanceRanking> rankings;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT sp.content_id, sp.platform, sc.dog_id, d.name as dog_name,
                   sp.views, sp.engagement_rate, sp.adoptions
            FROM social_performance sp
            JOIN social_content sc ON sp.content_id = sc.id
            LEFT JOIN dogs d ON sc.dog_id = d.id
            WHERE sc.posted_at >= $1 AND sc.posted_at <= $2
              AND sp.views >= 100
            ORDER BY sp.engagement_rate DESC
            LIMIT $3
        )";

        auto result = txn.exec_params(query, range.getStartISO(), range.getEndISO(), limit);

        for (const auto& row : result) {
            ContentPerformanceRanking r;
            r.content_id = row["content_id"].as<std::string>();
            r.platform = row["platform"].as<std::string>();
            r.dog_id = row["dog_id"].as<std::string>("");
            r.dog_name = row["dog_name"].as<std::string>("");
            r.views = row["views"].as<int>(0);
            r.engagement_rate = row["engagement_rate"].as<double>(0);
            r.adoptions = row["adoptions"].as<int>(0);
            r.score = r.engagement_rate;
            rankings.push_back(r);
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get top content by engagement: " + std::string(e.what()),
            {}
        );
    }

    return rankings;
}

std::vector<ContentPerformanceRanking> SocialAnalytics::getTopByAdoptions(const DateRange& range, int limit) {
    std::vector<ContentPerformanceRanking> rankings;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT sp.content_id, sp.platform, sc.dog_id, d.name as dog_name,
                   sp.views, sp.engagement_rate, sp.adoptions
            FROM social_performance sp
            JOIN social_content sc ON sp.content_id = sc.id
            LEFT JOIN dogs d ON sc.dog_id = d.id
            WHERE sc.posted_at >= $1 AND sc.posted_at <= $2
              AND sp.adoptions > 0
            ORDER BY sp.adoptions DESC, sp.views DESC
            LIMIT $3
        )";

        auto result = txn.exec_params(query, range.getStartISO(), range.getEndISO(), limit);

        for (const auto& row : result) {
            ContentPerformanceRanking r;
            r.content_id = row["content_id"].as<std::string>();
            r.platform = row["platform"].as<std::string>();
            r.dog_id = row["dog_id"].as<std::string>("");
            r.dog_name = row["dog_name"].as<std::string>("");
            r.views = row["views"].as<int>(0);
            r.engagement_rate = row["engagement_rate"].as<double>(0);
            r.adoptions = row["adoptions"].as<int>(0);
            r.score = static_cast<double>(r.adoptions);
            rankings.push_back(r);
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get top content by adoptions: " + std::string(e.what()),
            {}
        );
    }

    return rankings;
}

ContentMetrics SocialAnalytics::getDogSocialMetrics(const std::string& dog_id, const DateRange& range) {
    ContentMetrics metrics;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT
                SUM(sp.views) as views,
                SUM(sp.likes) as likes,
                SUM(sp.comments) as comments,
                SUM(sp.shares) as shares,
                SUM(sp.saves) as saves,
                SUM(sp.site_visits) as site_visits,
                SUM(sp.dog_views) as dog_views,
                SUM(sp.applications) as applications,
                SUM(sp.adoptions) as adoptions
            FROM social_performance sp
            JOIN social_content sc ON sp.content_id = sc.id
            WHERE sc.dog_id = $1
              AND sc.posted_at >= $2 AND sc.posted_at <= $3
        )";

        auto result = txn.exec_params1(query, dog_id, range.getStartISO(), range.getEndISO());

        metrics.views = result["views"].as<int>(0);
        metrics.likes = result["likes"].as<int>(0);
        metrics.comments = result["comments"].as<int>(0);
        metrics.shares = result["shares"].as<int>(0);
        metrics.saves = result["saves"].as<int>(0);
        metrics.site_visits = result["site_visits"].as<int>(0);
        metrics.dog_views = result["dog_views"].as<int>(0);
        metrics.applications = result["applications"].as<int>(0);
        metrics.adoptions = result["adoptions"].as<int>(0);

        metrics.engagement_rate = calculateEngagementRate(
            metrics.views, metrics.likes, metrics.comments, metrics.shares);

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get dog social metrics: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return metrics;
}

// ============================================================================
// CORRELATION ANALYSIS
// ============================================================================

double SocialAnalytics::getViewsToAdoptionsCorrelation(SocialPlatform platform, const DateRange& range) {
    // Simplified correlation - would need proper statistical analysis
    auto stats = getPlatformStats(platform, range);

    if (stats.total_views == 0) return 0;

    // Simple correlation approximation based on adoption rate
    return stats.adoption_conversion_rate / 100.0;
}

double SocialAnalytics::getAverageViewToAdoptionTime(SocialPlatform platform, const DateRange& range) {
    // Would need to track first view timestamps per user/dog combination
    // Returning placeholder
    return 7.0;  // Average 7 days from first social view to adoption
}

Json::Value SocialAnalytics::analyzeSuccessfulContent(const DateRange& range) {
    Json::Value analysis;

    auto top_adoptions = getTopByAdoptions(range, 20);

    // Analyze common traits
    std::map<std::string, int> platform_counts;
    int total_views = 0;
    double total_engagement = 0;

    for (const auto& content : top_adoptions) {
        platform_counts[content.platform]++;
        total_views += content.views;
        total_engagement += content.engagement_rate;
    }

    // Find best platform
    std::string best_platform;
    int max_count = 0;
    for (const auto& [platform, count] : platform_counts) {
        if (count > max_count) {
            max_count = count;
            best_platform = platform;
        }
    }

    analysis["best_platform"] = best_platform;
    analysis["avg_views"] = top_adoptions.empty() ? 0 :
        total_views / static_cast<int>(top_adoptions.size());
    analysis["avg_engagement"] = top_adoptions.empty() ? 0 :
        total_engagement / static_cast<double>(top_adoptions.size());
    analysis["sample_size"] = static_cast<int>(top_adoptions.size());

    Json::Value platforms(Json::objectValue);
    for (const auto& [platform, count] : platform_counts) {
        platforms[platform] = count;
    }
    analysis["by_platform"] = platforms;

    return analysis;
}

// ============================================================================
// SYNC FROM PLATFORMS
// ============================================================================

int SocialAnalytics::syncTikTokMetrics() {
    // Would integrate with TikTok API to pull latest metrics
    // For now, return 0 as this requires API credentials
    return 0;
}

int SocialAnalytics::syncInstagramMetrics() {
    // Would integrate with Instagram API
    return 0;
}

int SocialAnalytics::syncAllPlatforms() {
    int total = 0;
    total += syncTikTokMetrics();
    total += syncInstagramMetrics();
    return total;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void SocialAnalytics::incrementMetric(const std::string& content_id, const std::string& metric) {
    if (content_id.empty()) return;

    // Whitelist allowed metric column names to prevent SQL injection
    static const std::unordered_set<std::string> allowed_metrics = {
        "likes", "shares", "comments", "views", "clicks",
        "impressions", "engagements", "saves", "reach"
    };
    if (allowed_metrics.find(metric) == allowed_metrics.end()) {
        LOG_WARN << "SocialAnalytics: rejected invalid metric name: " << metric;
        return;
    }

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string query = "UPDATE social_performance SET " + metric + " = " + metric +
            " + 1, updated_at = NOW() WHERE content_id = $1";

        txn.exec_params(query, content_id);
        txn.commit();

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to increment social metric: " + std::string(e.what()),
            {{"content_id", content_id}, {"metric", metric}}
        );
    }
}

std::string SocialAnalytics::resolveContentId(const std::string& id, SocialPlatform platform) {
    // Check if this is already our internal ID
    auto content = getContent(id);
    if (content) {
        return content->id;
    }

    // Try to find by platform ID
    auto by_platform = getContentByPlatformId(platform, id);
    if (by_platform) {
        return by_platform->id;
    }

    return id;  // Return as-is if we can't resolve it
}

double SocialAnalytics::calculateEngagementRate(int views, int likes, int comments, int shares) {
    if (views == 0) return 0;
    return (static_cast<double>(likes + comments + shares) / views) * 100.0;
}

} // namespace wtl::analytics
