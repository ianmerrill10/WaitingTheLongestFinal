/**
 * @file AnalyticsService.cc
 * @brief Implementation of AnalyticsService
 * @see AnalyticsService.h for documentation
 */

#include "analytics/AnalyticsService.h"

#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::analytics {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// SINGLETON
// ============================================================================

AnalyticsService& AnalyticsService::getInstance() {
    static AnalyticsService instance;
    return instance;
}

AnalyticsService::AnalyticsService() {
    startBackgroundThread();
}

AnalyticsService::~AnalyticsService() {
    shutdown();
}

// ============================================================================
// EVENT TRACKING
// ============================================================================

std::string AnalyticsService::trackEvent(
    AnalyticsEventType event_type,
    const std::string& entity_type,
    const std::string& entity_id,
    const Json::Value& data,
    const std::optional<std::string>& user_id,
    const std::string& session_id)
{
    AnalyticsEvent event(event_type, entity_type, entity_id, user_id, session_id);
    event.data = data;
    event.source = "web";

    return trackEvent(event);
}

std::string AnalyticsService::trackEvent(const AnalyticsEvent& event) {
    AnalyticsEvent mutable_event = event;

    if (mutable_event.id.empty()) {
        mutable_event.generateId();
    }

    if (mutable_event.timestamp == std::chrono::system_clock::time_point{}) {
        mutable_event.setTimestampNow();
    }

    mutable_event.received_at = std::chrono::system_clock::now();

    if (async_tracking_) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (event_queue_.size() < max_queue_size_) {
            event_queue_.push(mutable_event);
            queue_cv_.notify_one();
        } else {
            // Queue full, save synchronously
            saveEvent(mutable_event);
        }
    } else {
        saveEvent(mutable_event);
    }

    return mutable_event.id;
}

std::string AnalyticsService::trackPageView(
    const std::string& page,
    const std::optional<std::string>& user_id,
    const std::string& session_id,
    const std::optional<std::string>& referrer,
    const Json::Value& data)
{
    AnalyticsEvent event;
    event.generateId();
    event.event_type = AnalyticsEventType::PAGE_VIEW;
    event.entity_type = "page";
    event.entity_id = page;
    event.user_id = user_id;
    event.session_id = session_id;
    event.referrer = referrer;
    event.data = data;
    event.data["page"] = page;
    event.setTimestampNow();

    return trackEvent(event);
}

std::string AnalyticsService::trackDogView(
    const std::string& dog_id,
    const std::optional<std::string>& user_id,
    const std::string& source,
    const std::string& session_id)
{
    Json::Value data;
    data["source"] = source;

    return trackEvent(
        AnalyticsEventType::DOG_VIEW,
        "dog",
        dog_id,
        data,
        user_id,
        session_id
    );
}

std::string AnalyticsService::trackSearch(
    const std::string& query,
    const Json::Value& filters,
    int result_count,
    const std::optional<std::string>& user_id,
    const std::string& session_id)
{
    Json::Value data;
    data["query"] = query;
    data["filters"] = filters;
    data["result_count"] = result_count;

    return trackEvent(
        AnalyticsEventType::SEARCH,
        "search",
        "",
        data,
        user_id,
        session_id
    );
}

std::string AnalyticsService::trackAdoption(
    const std::string& dog_id,
    const std::string& user_id,
    const std::string& source)
{
    Json::Value data;
    data["source"] = source;

    auto event_id = trackEvent(
        AnalyticsEventType::ADOPTION_COMPLETED,
        "dog",
        dog_id,
        data,
        user_id,
        ""
    );

    // Also record in impact tracker
    Attribution attr;
    attr.source = source;
    attr.user_id = user_id;
    ImpactTracker::getInstance().recordAdoption(dog_id, attr);

    return event_id;
}

std::string AnalyticsService::trackFosterPlacement(
    const std::string& dog_id,
    const std::string& foster_id,
    const std::string& user_id,
    const std::string& source)
{
    Json::Value data;
    data["foster_id"] = foster_id;
    data["source"] = source;

    auto event_id = trackEvent(
        AnalyticsEventType::FOSTER_STARTED,
        "dog",
        dog_id,
        data,
        user_id,
        ""
    );

    // Also record in impact tracker
    Attribution attr;
    attr.source = source;
    attr.user_id = user_id;
    ImpactTracker::getInstance().recordFosterPlacement(dog_id, foster_id, attr);

    return event_id;
}

std::string AnalyticsService::trackShare(
    const std::string& dog_id,
    const std::string& platform,
    const std::optional<std::string>& user_id,
    const std::string& session_id)
{
    Json::Value data;
    data["platform"] = platform;

    return trackEvent(
        AnalyticsEventType::DOG_SHARED,
        "dog",
        dog_id,
        data,
        user_id,
        session_id
    );
}

std::string AnalyticsService::trackFavorite(
    const std::string& dog_id,
    const std::string& user_id,
    bool added)
{
    return trackEvent(
        added ? AnalyticsEventType::DOG_FAVORITED : AnalyticsEventType::DOG_UNFAVORITED,
        "dog",
        dog_id,
        Json::Value(Json::objectValue),
        user_id,
        ""
    );
}

std::string AnalyticsService::trackSocialReferral(
    const std::string& platform,
    const std::string& content_id,
    const std::string& session_id)
{
    Json::Value data;
    data["platform"] = platform;
    data["content_id"] = content_id;

    return trackEvent(
        AnalyticsEventType::SOCIAL_REFERRAL,
        "referral",
        content_id,
        data,
        std::nullopt,
        session_id
    );
}

// ============================================================================
// DASHBOARD STATISTICS
// ============================================================================

DashboardStats AnalyticsService::getDashboardStats(const DateRange& range) {
    DashboardStats stats;
    stats.date_range = range;
    stats.generated_at = std::chrono::system_clock::now();

    stats.real_time = getRealTimeStats();
    stats.engagement = getEngagementStats(range);
    stats.conversions = getConversionStats(range);
    stats.trends = getTrendStats(range);
    stats.impact = getImpactStats(range);

    // Get top performers
    stats.top_performers.most_viewed_dogs = getMostViewedDogs(range, 5);
    stats.top_performers.most_shared_dogs = getMostSharedDogs(range, 5);
    stats.top_performers.top_shelters_by_adoptions = getTopSheltersByAdoptions(range, 5);

    // Social stats
    auto social_overview = SocialAnalytics::getInstance().getSocialOverview(range);
    stats.social.total_social_views = social_overview.total_views;
    stats.social.total_social_engagement = social_overview.total_engagement;
    stats.social.total_social_shares = 0;  // Sum from overview
    stats.social.social_attributed_adoptions = social_overview.total_attributed_adoptions;
    stats.social.top_platform = socialPlatformToString(social_overview.best_platform_by_views);

    return stats;
}

RealTimeStats AnalyticsService::getRealTimeStats() {
    RealTimeStats stats;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Active dogs
        auto dogs_result = txn.exec1(
            "SELECT COUNT(*) as count FROM dogs WHERE status = 'available'");
        stats.active_dogs = dogs_result["count"].as<int>(0);

        // Urgent dogs
        auto urgent_result = txn.exec1(
            "SELECT COUNT(*) as count FROM dogs WHERE urgency_level IN ('high', 'critical')");
        stats.urgent_dogs = urgent_result["count"].as<int>(0);

        // Critical dogs
        auto critical_result = txn.exec1(
            "SELECT COUNT(*) as count FROM dogs WHERE urgency_level = 'critical'");
        stats.critical_dogs = critical_result["count"].as<int>(0);

        // Active users today
        auto users_result = txn.exec1(
            R"(SELECT COUNT(DISTINCT COALESCE(user_id::text, session_id)) as count
               FROM analytics_events
               WHERE timestamp >= CURRENT_DATE)");
        stats.active_users_today = users_result["count"].as<int>(0);

        // Pending applications
        auto apps_result = txn.exec1(
            "SELECT COUNT(*) as count FROM foster_placements WHERE status = 'pending'");
        stats.pending_applications = apps_result["count"].as<int>(0);

        // Fosters today
        auto foster_result = txn.exec1(
            R"(SELECT COUNT(*) as count FROM foster_placements
               WHERE start_date = CURRENT_DATE)");
        stats.dogs_fostered_today = foster_result["count"].as<int>(0);

        // Adoptions today
        auto adopt_result = txn.exec1(
            R"(SELECT COUNT(*) as count FROM dogs
               WHERE status = 'adopted' AND updated_at >= CURRENT_DATE)");
        stats.dogs_adopted_today = adopt_result["count"].as<int>(0);

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get real-time stats: " + std::string(e.what()),
            {}
        );
    }

    return stats;
}

EngagementStats AnalyticsService::getEngagementStats(const DateRange& range) {
    EngagementStats stats;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT
                COUNT(*) FILTER (WHERE event_type = 'PAGE_VIEW') as page_views,
                COUNT(DISTINCT COALESCE(user_id::text, session_id)) as unique_visitors,
                COUNT(*) FILTER (WHERE event_type = 'DOG_VIEW') as dog_views,
                COUNT(*) FILTER (WHERE event_type = 'SEARCH') as searches,
                COUNT(*) FILTER (WHERE event_type = 'DOG_FAVORITED') as favorites,
                COUNT(*) FILTER (WHERE event_type = 'DOG_SHARED') as shares
            FROM analytics_events
            WHERE timestamp >= $1 AND timestamp <= $2
        )";

        auto result = txn.exec_params1(query, range.getStartISO(), range.getEndISO());

        stats.total_page_views = result["page_views"].as<int>(0);
        stats.unique_visitors = result["unique_visitors"].as<int>(0);
        stats.dog_views = result["dog_views"].as<int>(0);
        stats.searches = result["searches"].as<int>(0);
        stats.favorites_added = result["favorites"].as<int>(0);
        stats.shares = result["shares"].as<int>(0);

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get engagement stats: " + std::string(e.what()),
            {}
        );
    }

    return stats;
}

ConversionStats AnalyticsService::getConversionStats(const DateRange& range) {
    ConversionStats stats;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT
                COUNT(*) FILTER (WHERE event_type = 'FOSTER_APPLICATION') as foster_apps,
                COUNT(*) FILTER (WHERE event_type = 'FOSTER_APPROVED') as foster_approved,
                COUNT(*) FILTER (WHERE event_type = 'FOSTER_STARTED') as foster_started,
                COUNT(*) FILTER (WHERE event_type = 'ADOPTION_APPLICATION') as adopt_apps,
                COUNT(*) FILTER (WHERE event_type = 'ADOPTION_COMPLETED') as adoptions,
                COUNT(*) FILTER (WHERE event_type = 'DOG_RESCUED') as rescues
            FROM analytics_events
            WHERE timestamp >= $1 AND timestamp <= $2
        )";

        auto result = txn.exec_params1(query, range.getStartISO(), range.getEndISO());

        stats.foster_applications = result["foster_apps"].as<int>(0);
        stats.foster_approved = result["foster_approved"].as<int>(0);
        stats.foster_started = result["foster_started"].as<int>(0);
        stats.adoption_applications = result["adopt_apps"].as<int>(0);
        stats.adoptions_completed = result["adoptions"].as<int>(0);
        stats.dogs_rescued = result["rescues"].as<int>(0);

        // Calculate conversion rates
        auto engagement = getEngagementStats(range);
        if (engagement.dog_views > 0) {
            stats.view_to_application = (static_cast<double>(
                stats.foster_applications + stats.adoption_applications) /
                engagement.dog_views) * 100.0;
        }

        if (stats.adoption_applications > 0) {
            stats.application_to_adoption = (static_cast<double>(stats.adoptions_completed) /
                stats.adoption_applications) * 100.0;
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get conversion stats: " + std::string(e.what()),
            {}
        );
    }

    return stats;
}

TrendStats AnalyticsService::getTrendStats(const DateRange& range) {
    TrendStats stats;

    auto& aggregator = MetricsAggregator::getInstance();

    stats.dog_views = aggregator.getTrendData("dog_views", range);
    stats.adoptions = aggregator.getTrendData("adoptions", range);
    stats.fosters = aggregator.getTrendData("fosters", range);
    stats.new_users = aggregator.getTrendData("new_users", range);

    // Calculate week-over-week changes
    std::string end_date = range.getEndISO().substr(0, 10);
    stats.views_wow_change = aggregator.getWeekOverWeekChange("views", end_date);
    stats.adoptions_wow_change = aggregator.getWeekOverWeekChange("adoptions", end_date);
    stats.fosters_wow_change = aggregator.getWeekOverWeekChange("fosters", end_date);

    return stats;
}

// ============================================================================
// ENTITY-SPECIFIC STATISTICS
// ============================================================================

DogStats AnalyticsService::getDogStats(const std::string& dog_id, const DateRange& range) {
    DogStats stats;
    stats.dog_id = dog_id;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get dog name
        auto dog_result = txn.exec_params1(
            "SELECT name, intake_date, status FROM dogs WHERE id = $1",
            dog_id
        );
        stats.dog_name = dog_result["name"].as<std::string>("");

        // Calculate days listed
        if (!dog_result["intake_date"].is_null()) {
            auto intake = parseTimestamp(dog_result["intake_date"].as<std::string>());
            auto now = std::chrono::system_clock::now();
            stats.days_listed = std::chrono::duration_cast<std::chrono::hours>(
                now - intake).count() / 24;
        }

        stats.is_adopted = (dog_result["status"].as<std::string>("") == "adopted");

        // Get event counts
        auto events_query = R"(
            SELECT
                COUNT(*) FILTER (WHERE event_type = 'DOG_VIEW') as views,
                COUNT(DISTINCT COALESCE(user_id::text, session_id)) FILTER (WHERE event_type = 'DOG_VIEW') as unique_viewers,
                COUNT(*) FILTER (WHERE event_type = 'DOG_FAVORITED') as favorites,
                COUNT(*) FILTER (WHERE event_type = 'DOG_SHARED') as shares,
                COUNT(*) FILTER (WHERE event_type = 'DOG_CONTACT_CLICKED') as contact_clicks,
                COUNT(*) FILTER (WHERE event_type = 'DOG_DIRECTIONS_CLICKED') as direction_clicks,
                COUNT(*) FILTER (WHERE event_type IN ('FOSTER_APPLICATION', 'ADOPTION_APPLICATION')) as applications,
                MIN(timestamp) as first_view,
                MAX(timestamp) as last_view
            FROM analytics_events
            WHERE entity_id = $1 AND entity_type = 'dog'
              AND timestamp >= $2 AND timestamp <= $3
        )";

        auto events_result = txn.exec_params1(events_query, dog_id, range.getStartISO(), range.getEndISO());

        stats.total_views = events_result["views"].as<int>(0);
        stats.unique_viewers = events_result["unique_viewers"].as<int>(0);
        stats.favorites_count = events_result["favorites"].as<int>(0);
        stats.shares_count = events_result["shares"].as<int>(0);
        stats.contact_clicks = events_result["contact_clicks"].as<int>(0);
        stats.direction_clicks = events_result["direction_clicks"].as<int>(0);
        stats.applications_submitted = events_result["applications"].as<int>(0);

        if (!events_result["first_view"].is_null()) {
            stats.first_view_date = events_result["first_view"].as<std::string>().substr(0, 10);
        }
        if (!events_result["last_view"].is_null()) {
            stats.last_view_date = events_result["last_view"].as<std::string>().substr(0, 10);
        }

        // Get social metrics
        auto social_metrics = SocialAnalytics::getInstance().getDogSocialMetrics(dog_id, range);
        stats.social_views = social_metrics.views;
        stats.social_shares = social_metrics.shares;
        stats.social_engagement = social_metrics.likes + social_metrics.comments + social_metrics.shares;

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get dog stats: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return stats;
}

ShelterStats AnalyticsService::getShelterStats(const std::string& shelter_id, const DateRange& range) {
    ShelterStats stats;
    stats.shelter_id = shelter_id;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get shelter info
        auto shelter_result = txn.exec_params1(
            "SELECT name FROM shelters WHERE id = $1",
            shelter_id
        );
        stats.shelter_name = shelter_result["name"].as<std::string>("");

        // Get dog counts
        auto dogs_query = R"(
            SELECT
                COUNT(*) FILTER (WHERE status = 'available') as active,
                COUNT(*) FILTER (WHERE urgency_level IN ('high', 'critical')) as urgent,
                COUNT(*) as total
            FROM dogs WHERE shelter_id = $1
        )";

        auto dogs_result = txn.exec_params1(dogs_query, shelter_id);
        stats.active_dogs = dogs_result["active"].as<int>(0);
        stats.urgent_dogs = dogs_result["urgent"].as<int>(0);
        stats.total_dogs_listed = dogs_result["total"].as<int>(0);

        // Get outcomes
        auto outcomes_query = R"(
            SELECT
                COUNT(*) FILTER (WHERE status = 'adopted') as adoptions,
                COUNT(*) FILTER (WHERE status = 'fostered') as fosters
            FROM dogs
            WHERE shelter_id = $1
              AND updated_at >= $2 AND updated_at <= $3
        )";

        auto outcomes_result = txn.exec_params1(outcomes_query, shelter_id, range.getStartISO(), range.getEndISO());
        stats.adoptions = outcomes_result["adoptions"].as<int>(0);
        stats.fosters = outcomes_result["fosters"].as<int>(0);

        // Get engagement
        auto events_query = R"(
            SELECT
                COUNT(*) FILTER (WHERE event_type = 'SHELTER_VIEW') as page_views,
                COUNT(*) FILTER (WHERE event_type = 'DOG_VIEW') as dog_views,
                COUNT(*) FILTER (WHERE event_type IN ('FOSTER_APPLICATION', 'ADOPTION_APPLICATION')) as applications
            FROM analytics_events ae
            JOIN dogs d ON ae.entity_id = d.id::text
            WHERE (ae.entity_id = $1 OR d.shelter_id = $1)
              AND ae.timestamp >= $2 AND ae.timestamp <= $3
        )";

        auto events_result = txn.exec_params1(events_query, shelter_id, range.getStartISO(), range.getEndISO());
        stats.page_views = events_result["page_views"].as<int>(0);
        stats.dog_views = events_result["dog_views"].as<int>(0);
        stats.applications = events_result["applications"].as<int>(0);

        // Calculate rates
        if (stats.total_dogs_listed > 0) {
            stats.adoption_rate = (static_cast<double>(stats.adoptions) / stats.total_dogs_listed) * 100.0;
            stats.foster_rate = (static_cast<double>(stats.fosters) / stats.total_dogs_listed) * 100.0;
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get shelter stats: " + std::string(e.what()),
            {{"shelter_id", shelter_id}}
        );
    }

    return stats;
}

Json::Value AnalyticsService::getSocialStats(const std::string& platform, const DateRange& range) {
    auto& social = SocialAnalytics::getInstance();

    if (platform == "all" || platform.empty()) {
        return social.getSocialOverview(range).toJson();
    }

    return social.getPlatformStats(platform, range).toJson();
}

ImpactSummary AnalyticsService::getImpactStats(const DateRange& range) {
    return ImpactTracker::getInstance().getImpactSummary(range);
}

// ============================================================================
// TOP PERFORMERS
// ============================================================================

std::vector<TopPerformer> AnalyticsService::getMostViewedDogs(const DateRange& range, int limit) {
    std::vector<TopPerformer> performers;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT ae.entity_id as dog_id, d.name, COUNT(*) as views
            FROM analytics_events ae
            JOIN dogs d ON ae.entity_id = d.id::text
            WHERE ae.event_type = 'DOG_VIEW'
              AND ae.timestamp >= $1 AND ae.timestamp <= $2
            GROUP BY ae.entity_id, d.name
            ORDER BY views DESC
            LIMIT $3
        )";

        auto result = txn.exec_params(query, range.getStartISO(), range.getEndISO(), limit);

        for (const auto& row : result) {
            TopPerformer p;
            p.id = row["dog_id"].as<std::string>();
            p.name = row["name"].as<std::string>();
            p.type = "dog";
            p.metric_value = row["views"].as<int>(0);
            p.metric_name = "views";
            performers.push_back(p);
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get most viewed dogs: " + std::string(e.what()),
            {}
        );
    }

    return performers;
}

std::vector<TopPerformer> AnalyticsService::getMostSharedDogs(const DateRange& range, int limit) {
    std::vector<TopPerformer> performers;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT ae.entity_id as dog_id, d.name, COUNT(*) as shares
            FROM analytics_events ae
            JOIN dogs d ON ae.entity_id = d.id::text
            WHERE ae.event_type = 'DOG_SHARED'
              AND ae.timestamp >= $1 AND ae.timestamp <= $2
            GROUP BY ae.entity_id, d.name
            ORDER BY shares DESC
            LIMIT $3
        )";

        auto result = txn.exec_params(query, range.getStartISO(), range.getEndISO(), limit);

        for (const auto& row : result) {
            TopPerformer p;
            p.id = row["dog_id"].as<std::string>();
            p.name = row["name"].as<std::string>();
            p.type = "dog";
            p.metric_value = row["shares"].as<int>(0);
            p.metric_name = "shares";
            performers.push_back(p);
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get most shared dogs: " + std::string(e.what()),
            {}
        );
    }

    return performers;
}

std::vector<TopPerformer> AnalyticsService::getTopSheltersByAdoptions(const DateRange& range, int limit) {
    std::vector<TopPerformer> performers;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT s.id as shelter_id, s.name, COUNT(*) as adoptions
            FROM dogs d
            JOIN shelters s ON d.shelter_id = s.id
            WHERE d.status = 'adopted'
              AND d.updated_at >= $1 AND d.updated_at <= $2
            GROUP BY s.id, s.name
            ORDER BY adoptions DESC
            LIMIT $3
        )";

        auto result = txn.exec_params(query, range.getStartISO(), range.getEndISO(), limit);

        for (const auto& row : result) {
            TopPerformer p;
            p.id = row["shelter_id"].as<std::string>();
            p.name = row["name"].as<std::string>();
            p.type = "shelter";
            p.metric_value = row["adoptions"].as<int>(0);
            p.metric_name = "adoptions";
            performers.push_back(p);
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get top shelters by adoptions: " + std::string(e.what()),
            {}
        );
    }

    return performers;
}

// ============================================================================
// EVENT QUERYING
// ============================================================================

std::vector<AnalyticsEvent> AnalyticsService::getRecentEvents(AnalyticsEventType event_type, int limit) {
    std::vector<AnalyticsEvent> events;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(SELECT * FROM analytics_events
               WHERE event_type = $1
               ORDER BY timestamp DESC
               LIMIT $2)",
            analyticsEventTypeToString(event_type),
            limit
        );

        for (const auto& row : result) {
            events.push_back(AnalyticsEvent::fromDbRow(row));
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get recent events: " + std::string(e.what()),
            {}
        );
    }

    return events;
}

std::vector<AnalyticsEvent> AnalyticsService::getEventsByEntity(
    const std::string& entity_type,
    const std::string& entity_id,
    const DateRange& range)
{
    std::vector<AnalyticsEvent> events;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(SELECT * FROM analytics_events
               WHERE entity_type = $1 AND entity_id = $2
                 AND timestamp >= $3 AND timestamp <= $4
               ORDER BY timestamp DESC)",
            entity_type, entity_id, range.getStartISO(), range.getEndISO()
        );

        for (const auto& row : result) {
            events.push_back(AnalyticsEvent::fromDbRow(row));
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get events by entity: " + std::string(e.what()),
            {{"entity_type", entity_type}, {"entity_id", entity_id}}
        );
    }

    return events;
}

std::vector<AnalyticsEvent> AnalyticsService::getEventsByUser(
    const std::string& user_id,
    const DateRange& range)
{
    std::vector<AnalyticsEvent> events;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(SELECT * FROM analytics_events
               WHERE user_id = $1
                 AND timestamp >= $2 AND timestamp <= $3
               ORDER BY timestamp DESC)",
            user_id, range.getStartISO(), range.getEndISO()
        );

        for (const auto& row : result) {
            events.push_back(AnalyticsEvent::fromDbRow(row));
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get events by user: " + std::string(e.what()),
            {{"user_id", user_id}}
        );
    }

    return events;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void AnalyticsService::setAsyncTracking(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    async_tracking_ = enabled;
}

void AnalyticsService::setMaxQueueSize(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_queue_size_ = size;
}

int AnalyticsService::flushEvents() {
    int count = 0;
    std::queue<AnalyticsEvent> events_to_process;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(events_to_process, event_queue_);
    }

    while (!events_to_process.empty()) {
        if (saveEvent(events_to_process.front())) {
            count++;
        }
        events_to_process.pop();
    }

    return count;
}

size_t AnalyticsService::getQueueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return event_queue_.size();
}

void AnalyticsService::shutdown() {
    stopBackgroundThread();
    flushEvents();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool AnalyticsService::saveEvent(const AnalyticsEvent& event) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        Json::StreamWriterBuilder writer;
        std::string data_json = Json::writeString(writer, event.data);

        auto query = R"(
            INSERT INTO analytics_events (
                id, event_type, entity_type, entity_id,
                user_id, session_id, visitor_id,
                source, referrer, utm_campaign, utm_source, utm_medium, page_url,
                device_type, browser, os, ip_hash, country_code, region_code, city,
                data, timestamp, received_at, is_processed
            ) VALUES (
                $1, $2, $3, $4,
                $5, $6, $7,
                $8, $9, $10, $11, $12, $13,
                $14, $15, $16, $17, $18, $19, $20,
                $21::jsonb, $22, $23, FALSE
            )
        )";

        txn.exec_params(query,
            event.id,
            analyticsEventTypeToString(event.event_type),
            event.entity_type,
            event.entity_id,
            event.user_id.value_or(""),
            event.session_id,
            event.visitor_id.value_or(""),
            event.source,
            event.referrer.value_or(""),
            event.utm_campaign.value_or(""),
            event.utm_source.value_or(""),
            event.utm_medium.value_or(""),
            event.page_url.value_or(""),
            event.device_type.value_or(""),
            event.browser.value_or(""),
            event.os.value_or(""),
            event.ip_hash.value_or(""),
            event.country_code.value_or(""),
            event.region_code.value_or(""),
            event.city.value_or(""),
            data_json,
            event.getTimestampISO(),
            event.received_at ? formatTimestampISO(*event.received_at) : formatTimestampISO(std::chrono::system_clock::now())
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to save analytics event: " + std::string(e.what()),
            {{"event_id", event.id}, {"event_type", analyticsEventTypeToString(event.event_type)}}
        );
        return false;
    }
}

void AnalyticsService::processQueue() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);

        queue_cv_.wait_for(lock, std::chrono::seconds(1), [this] {
            return !event_queue_.empty() || !running_;
        });

        if (!running_ && event_queue_.empty()) {
            break;
        }

        // Process batch of events
        std::vector<AnalyticsEvent> batch;
        while (!event_queue_.empty() && batch.size() < 100) {
            batch.push_back(std::move(event_queue_.front()));
            event_queue_.pop();
        }

        lock.unlock();

        for (const auto& event : batch) {
            saveEvent(event);
        }
    }
}

void AnalyticsService::startBackgroundThread() {
    if (!running_) {
        running_ = true;
        background_thread_ = std::make_unique<std::thread>(&AnalyticsService::processQueue, this);
    }
}

void AnalyticsService::stopBackgroundThread() {
    if (running_) {
        running_ = false;
        queue_cv_.notify_all();

        if (background_thread_ && background_thread_->joinable()) {
            background_thread_->join();
        }
    }
}

} // namespace wtl::analytics
