/**
 * @file MetricsAggregator.cc
 * @brief Implementation of MetricsAggregator
 * @see MetricsAggregator.h for documentation
 */

#include "analytics/MetricsAggregator.h"

#include <ctime>
#include <iomanip>
#include <sstream>

#include "analytics/AnalyticsEvent.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::analytics {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// AGGREGATED METRICS
// ============================================================================

Json::Value AggregatedMetrics::toJson() const {
    Json::Value json;

    json["period"] = period;
    json["period_type"] = period_type;

    if (!dimension_type.empty()) {
        json["dimension_type"] = dimension_type;
        json["dimension_id"] = dimension_id;
    }

    // Views
    json["page_views"] = page_views;
    json["unique_visitors"] = unique_visitors;
    json["dog_views"] = dog_views;
    json["shelter_views"] = shelter_views;
    json["search_count"] = search_count;

    // Engagement
    json["favorites_added"] = favorites_added;
    json["favorites_removed"] = favorites_removed;
    json["shares"] = shares;
    json["contact_clicks"] = contact_clicks;
    json["direction_clicks"] = direction_clicks;

    // Applications
    json["foster_applications"] = foster_applications;
    json["foster_approved"] = foster_approved;
    json["foster_started"] = foster_started;
    json["foster_ended"] = foster_ended;
    json["adoption_applications"] = adoption_applications;

    // Outcomes
    json["adoptions"] = adoptions;
    json["rescues"] = rescues;
    json["transports"] = transports;

    // Users
    json["new_users"] = new_users;
    json["active_users"] = active_users;
    json["returning_users"] = returning_users;

    // Social
    json["social_views"] = social_views;
    json["social_shares"] = social_shares;
    json["social_engagement"] = social_engagement;
    json["social_referrals"] = social_referrals;

    // Notifications
    json["notifications_sent"] = notifications_sent;
    json["notifications_opened"] = notifications_opened;
    json["emails_sent"] = emails_sent;
    json["emails_opened"] = emails_opened;

    json["created_at"] = formatTimestampISO(created_at);
    json["updated_at"] = formatTimestampISO(updated_at);

    return json;
}

AggregatedMetrics AggregatedMetrics::fromDbRow(const pqxx::row& row) {
    AggregatedMetrics metrics;

    metrics.period = row["period"].as<std::string>("");
    metrics.period_type = row["period_type"].as<std::string>("");
    metrics.dimension_type = row["dimension_type"].as<std::string>("");
    metrics.dimension_id = row["dimension_id"].as<std::string>("");

    metrics.page_views = row["page_views"].as<int>(0);
    metrics.unique_visitors = row["unique_visitors"].as<int>(0);
    metrics.dog_views = row["dog_views"].as<int>(0);
    metrics.shelter_views = row["shelter_views"].as<int>(0);
    metrics.search_count = row["search_count"].as<int>(0);

    metrics.favorites_added = row["favorites_added"].as<int>(0);
    metrics.favorites_removed = row["favorites_removed"].as<int>(0);
    metrics.shares = row["shares"].as<int>(0);
    metrics.contact_clicks = row["contact_clicks"].as<int>(0);
    metrics.direction_clicks = row["direction_clicks"].as<int>(0);

    metrics.foster_applications = row["foster_applications"].as<int>(0);
    metrics.foster_approved = row["foster_approved"].as<int>(0);
    metrics.foster_started = row["foster_started"].as<int>(0);
    metrics.foster_ended = row["foster_ended"].as<int>(0);
    metrics.adoption_applications = row["adoption_applications"].as<int>(0);

    metrics.adoptions = row["adoptions"].as<int>(0);
    metrics.rescues = row["rescues"].as<int>(0);
    metrics.transports = row["transports"].as<int>(0);

    metrics.new_users = row["new_users"].as<int>(0);
    metrics.active_users = row["active_users"].as<int>(0);
    metrics.returning_users = row["returning_users"].as<int>(0);

    metrics.social_views = row["social_views"].as<int>(0);
    metrics.social_shares = row["social_shares"].as<int>(0);
    metrics.social_engagement = row["social_engagement"].as<int>(0);
    metrics.social_referrals = row["social_referrals"].as<int>(0);

    metrics.notifications_sent = row["notifications_sent"].as<int>(0);
    metrics.notifications_opened = row["notifications_opened"].as<int>(0);
    metrics.emails_sent = row["emails_sent"].as<int>(0);
    metrics.emails_opened = row["emails_opened"].as<int>(0);

    if (!row["created_at"].is_null()) {
        metrics.created_at = parseTimestamp(row["created_at"].as<std::string>());
    }
    if (!row["updated_at"].is_null()) {
        metrics.updated_at = parseTimestamp(row["updated_at"].as<std::string>());
    }

    return metrics;
}

void AggregatedMetrics::merge(const AggregatedMetrics& other) {
    page_views += other.page_views;
    unique_visitors += other.unique_visitors;
    dog_views += other.dog_views;
    shelter_views += other.shelter_views;
    search_count += other.search_count;

    favorites_added += other.favorites_added;
    favorites_removed += other.favorites_removed;
    shares += other.shares;
    contact_clicks += other.contact_clicks;
    direction_clicks += other.direction_clicks;

    foster_applications += other.foster_applications;
    foster_approved += other.foster_approved;
    foster_started += other.foster_started;
    foster_ended += other.foster_ended;
    adoption_applications += other.adoption_applications;

    adoptions += other.adoptions;
    rescues += other.rescues;
    transports += other.transports;

    new_users += other.new_users;
    active_users += other.active_users;
    returning_users += other.returning_users;

    social_views += other.social_views;
    social_shares += other.social_shares;
    social_engagement += other.social_engagement;
    social_referrals += other.social_referrals;

    notifications_sent += other.notifications_sent;
    notifications_opened += other.notifications_opened;
    emails_sent += other.emails_sent;
    emails_opened += other.emails_opened;
}

Json::Value AggregationResult::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["events_processed"] = events_processed;
    json["metrics_created"] = metrics_created;
    json["metrics_updated"] = metrics_updated;
    if (!error_message.empty()) {
        json["error_message"] = error_message;
    }
    json["started_at"] = formatTimestampISO(started_at);
    json["completed_at"] = formatTimestampISO(completed_at);

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        completed_at - started_at).count();
    json["duration_ms"] = static_cast<Json::Int64>(duration_ms);

    return json;
}

// ============================================================================
// SINGLETON
// ============================================================================

MetricsAggregator& MetricsAggregator::getInstance() {
    static MetricsAggregator instance;
    return instance;
}

MetricsAggregator::MetricsAggregator()
    : last_aggregation_(std::chrono::system_clock::now() - std::chrono::hours(24))
{
}

// ============================================================================
// AGGREGATION METHODS
// ============================================================================

AggregationResult MetricsAggregator::aggregateDaily(const std::string& date) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Parse the date and create time range
    std::tm tm = {};
    std::istringstream ss(date);
    ss >> std::get_time(&tm, "%Y-%m-%d");

    if (ss.fail()) {
        AggregationResult result;
        result.error_message = "Invalid date format: " + date;
        return result;
    }

    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    auto start = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;
    auto end = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    return aggregateEventsForPeriod(start, end, "daily", date);
}

AggregationResult MetricsAggregator::aggregateYesterday() {
    auto yesterday = std::chrono::system_clock::now() - std::chrono::hours(24);
    auto time_t_val = std::chrono::system_clock::to_time_t(yesterday);
    std::tm tm_val;

#ifdef _WIN32
    localtime_s(&tm_val, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_val);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm_val, "%Y-%m-%d");
    return aggregateDaily(ss.str());
}

AggregationResult MetricsAggregator::aggregateWeekly(int year, int week) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Calculate first day of the ISO week
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = 0;
    tm.tm_mday = 1 + (week - 1) * 7;

    // Adjust to Monday
    std::mktime(&tm);
    int days_to_monday = (tm.tm_wday == 0) ? 6 : tm.tm_wday - 1;
    tm.tm_mday -= days_to_monday;

    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    auto start = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    tm.tm_mday += 6;
    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;
    auto end = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    std::ostringstream label;
    label << year << "-W" << std::setfill('0') << std::setw(2) << week;

    return aggregateEventsForPeriod(start, end, "weekly", label.str());
}

AggregationResult MetricsAggregator::aggregateLastWeek() {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now - std::chrono::hours(24 * 7));
    std::tm tm_val;

#ifdef _WIN32
    localtime_s(&tm_val, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_val);
#endif

    // Calculate ISO week number
    std::mktime(&tm_val);
    int day_of_year = tm_val.tm_yday;
    int weekday = tm_val.tm_wday == 0 ? 7 : tm_val.tm_wday;
    int week = (day_of_year - weekday + 10) / 7;

    return aggregateWeekly(tm_val.tm_year + 1900, week);
}

AggregationResult MetricsAggregator::aggregateMonthly(int year, int month) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = 1;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    auto start = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    // Last day of month
    tm.tm_mon = month;
    tm.tm_mday = 0;
    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;
    auto end = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    std::ostringstream label;
    label << year << "-" << std::setfill('0') << std::setw(2) << month;

    return aggregateEventsForPeriod(start, end, "monthly", label.str());
}

AggregationResult MetricsAggregator::aggregateLastMonth() {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val;

#ifdef _WIN32
    localtime_s(&tm_val, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_val);
#endif

    int year = tm_val.tm_year + 1900;
    int month = tm_val.tm_mon; // Previous month (0-indexed)

    if (month == 0) {
        year--;
        month = 12;
    }

    return aggregateMonthly(year, month);
}

AggregationResult MetricsAggregator::aggregatePending(int max_days) {
    AggregationResult total_result;
    total_result.started_at = std::chrono::system_clock::now();
    total_result.success = true;

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;

#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    // Process each day backwards
    for (int i = 1; i <= max_days; i++) {
        std::tm tm_day = tm_now;
        tm_day.tm_mday -= i;
        std::mktime(&tm_day);

        std::ostringstream date_ss;
        date_ss << std::put_time(&tm_day, "%Y-%m-%d");

        auto result = aggregateDaily(date_ss.str());

        total_result.events_processed += result.events_processed;
        total_result.metrics_created += result.metrics_created;
        total_result.metrics_updated += result.metrics_updated;

        if (!result.success) {
            total_result.success = false;
            total_result.error_message = result.error_message;
        }
    }

    total_result.completed_at = std::chrono::system_clock::now();
    return total_result;
}

AggregationResult MetricsAggregator::aggregateEventsForPeriod(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end,
    const std::string& period_type,
    const std::string& period_label)
{
    AggregationResult result;
    result.started_at = std::chrono::system_clock::now();

    try {
        auto conn = ConnectionPool::getInstance().acquire();

        AggregatedMetrics metrics;
        metrics.period = period_label;
        metrics.period_type = period_type;
        metrics.created_at = std::chrono::system_clock::now();
        metrics.updated_at = metrics.created_at;

        std::string start_str = formatTimestampISO(start);
        std::string end_str = formatTimestampISO(end);

        pqxx::work txn(*conn);

        // Count various event types
        auto count_query = R"(
            SELECT
                event_type,
                COUNT(*) as count
            FROM analytics_events
            WHERE timestamp >= $1 AND timestamp <= $2
            GROUP BY event_type
        )";

        auto count_result = txn.exec_params(count_query, start_str, end_str);

        for (const auto& row : count_result) {
            std::string type_str = row["event_type"].as<std::string>();
            int count = row["count"].as<int>();

            auto type_opt = analyticsEventTypeFromString(type_str);
            if (!type_opt) continue;

            result.events_processed += count;

            switch (*type_opt) {
                case AnalyticsEventType::PAGE_VIEW:
                case AnalyticsEventType::HOME_VIEW:
                case AnalyticsEventType::SEARCH_PAGE_VIEW:
                    metrics.page_views += count;
                    break;
                case AnalyticsEventType::DOG_VIEW:
                    metrics.dog_views += count;
                    break;
                case AnalyticsEventType::SHELTER_VIEW:
                    metrics.shelter_views += count;
                    break;
                case AnalyticsEventType::SEARCH:
                    metrics.search_count += count;
                    break;
                case AnalyticsEventType::DOG_FAVORITED:
                    metrics.favorites_added += count;
                    break;
                case AnalyticsEventType::DOG_UNFAVORITED:
                    metrics.favorites_removed += count;
                    break;
                case AnalyticsEventType::DOG_SHARED:
                    metrics.shares += count;
                    break;
                case AnalyticsEventType::DOG_CONTACT_CLICKED:
                    metrics.contact_clicks += count;
                    break;
                case AnalyticsEventType::DOG_DIRECTIONS_CLICKED:
                    metrics.direction_clicks += count;
                    break;
                case AnalyticsEventType::FOSTER_APPLICATION:
                    metrics.foster_applications += count;
                    break;
                case AnalyticsEventType::FOSTER_APPROVED:
                    metrics.foster_approved += count;
                    break;
                case AnalyticsEventType::FOSTER_STARTED:
                    metrics.foster_started += count;
                    break;
                case AnalyticsEventType::FOSTER_ENDED:
                    metrics.foster_ended += count;
                    break;
                case AnalyticsEventType::ADOPTION_APPLICATION:
                    metrics.adoption_applications += count;
                    break;
                case AnalyticsEventType::ADOPTION_COMPLETED:
                    metrics.adoptions += count;
                    break;
                case AnalyticsEventType::DOG_RESCUED:
                    metrics.rescues += count;
                    break;
                case AnalyticsEventType::DOG_TRANSPORTED:
                    metrics.transports += count;
                    break;
                case AnalyticsEventType::USER_REGISTERED:
                    metrics.new_users += count;
                    break;
                case AnalyticsEventType::USER_LOGIN:
                    metrics.active_users += count;
                    break;
                case AnalyticsEventType::TIKTOK_VIEW:
                case AnalyticsEventType::INSTAGRAM_VIEW:
                case AnalyticsEventType::FACEBOOK_VIEW:
                case AnalyticsEventType::TWITTER_VIEW:
                    metrics.social_views += count;
                    break;
                case AnalyticsEventType::TIKTOK_SHARE:
                case AnalyticsEventType::INSTAGRAM_SHARE:
                case AnalyticsEventType::FACEBOOK_SHARE:
                case AnalyticsEventType::TWITTER_SHARE:
                    metrics.social_shares += count;
                    break;
                case AnalyticsEventType::SOCIAL_ENGAGEMENT:
                    metrics.social_engagement += count;
                    break;
                case AnalyticsEventType::SOCIAL_REFERRAL:
                    metrics.social_referrals += count;
                    break;
                case AnalyticsEventType::NOTIFICATION_SENT:
                    metrics.notifications_sent += count;
                    break;
                case AnalyticsEventType::NOTIFICATION_OPENED:
                    metrics.notifications_opened += count;
                    break;
                case AnalyticsEventType::EMAIL_SENT:
                    metrics.emails_sent += count;
                    break;
                case AnalyticsEventType::EMAIL_OPENED:
                    metrics.emails_opened += count;
                    break;
                default:
                    break;
            }
        }

        // Count unique visitors
        auto visitor_query = R"(
            SELECT COUNT(DISTINCT COALESCE(user_id, visitor_id, session_id)) as unique_visitors
            FROM analytics_events
            WHERE timestamp >= $1 AND timestamp <= $2
        )";

        auto visitor_result = txn.exec_params1(visitor_query, start_str, end_str);
        metrics.unique_visitors = visitor_result["unique_visitors"].as<int>(0);

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        // Save the aggregated metrics
        if (upsertMetrics(metrics)) {
            result.metrics_created = 1;
            result.success = true;
        } else {
            result.error_message = "Failed to save metrics";
        }

        last_aggregation_ = std::chrono::system_clock::now();

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to aggregate metrics: " + std::string(e.what()),
            {{"period", period_label}, {"period_type", period_type}}
        );
    }

    result.completed_at = std::chrono::system_clock::now();
    return result;
}

// ============================================================================
// QUERY METHODS
// ============================================================================

std::optional<AggregatedMetrics> MetricsAggregator::getDailyMetrics(const std::string& date) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM daily_metrics WHERE period = $1 AND period_type = 'daily'",
            date
        );

        ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return AggregatedMetrics::fromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get daily metrics: " + std::string(e.what()),
            {{"date", date}}
        );
        return std::nullopt;
    }
}

std::vector<AggregatedMetrics> MetricsAggregator::getDailyMetricsRange(
    const std::string& start_date,
    const std::string& end_date)
{
    std::vector<AggregatedMetrics> metrics;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(SELECT * FROM daily_metrics
               WHERE period_type = 'daily' AND period >= $1 AND period <= $2
               ORDER BY period ASC)",
            start_date, end_date
        );

        ConnectionPool::getInstance().release(conn);

        for (const auto& row : result) {
            metrics.push_back(AggregatedMetrics::fromDbRow(row));
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get daily metrics range: " + std::string(e.what()),
            {{"start_date", start_date}, {"end_date", end_date}}
        );
    }

    return metrics;
}

std::optional<AggregatedMetrics> MetricsAggregator::getWeeklyMetrics(int year, int week) {
    std::ostringstream label;
    label << year << "-W" << std::setfill('0') << std::setw(2) << week;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM daily_metrics WHERE period = $1 AND period_type = 'weekly'",
            label.str()
        );

        ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return AggregatedMetrics::fromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get weekly metrics: " + std::string(e.what()),
            {{"year", std::to_string(year)}, {"week", std::to_string(week)}}
        );
        return std::nullopt;
    }
}

std::optional<AggregatedMetrics> MetricsAggregator::getMonthlyMetrics(int year, int month) {
    std::ostringstream label;
    label << year << "-" << std::setfill('0') << std::setw(2) << month;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM daily_metrics WHERE period = $1 AND period_type = 'monthly'",
            label.str()
        );

        ConnectionPool::getInstance().release(conn);

        if (result.empty()) {
            return std::nullopt;
        }

        return AggregatedMetrics::fromDbRow(result[0]);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get monthly metrics: " + std::string(e.what()),
            {{"year", std::to_string(year)}, {"month", std::to_string(month)}}
        );
        return std::nullopt;
    }
}

AggregatedMetrics MetricsAggregator::getMetricsByDog(const std::string& dog_id, const DateRange& range) {
    AggregatedMetrics metrics;
    metrics.dimension_type = "dog";
    metrics.dimension_id = dog_id;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT
                event_type,
                COUNT(*) as count
            FROM analytics_events
            WHERE entity_id = $1 AND entity_type = 'dog'
              AND timestamp >= $2 AND timestamp <= $3
            GROUP BY event_type
        )";

        auto result = txn.exec_params(query, dog_id, range.getStartISO(), range.getEndISO());

        for (const auto& row : result) {
            std::string type_str = row["event_type"].as<std::string>();
            int count = row["count"].as<int>();

            auto type_opt = analyticsEventTypeFromString(type_str);
            if (!type_opt) continue;

            switch (*type_opt) {
                case AnalyticsEventType::DOG_VIEW:
                    metrics.dog_views = count;
                    break;
                case AnalyticsEventType::DOG_FAVORITED:
                    metrics.favorites_added = count;
                    break;
                case AnalyticsEventType::DOG_SHARED:
                    metrics.shares = count;
                    break;
                case AnalyticsEventType::DOG_CONTACT_CLICKED:
                    metrics.contact_clicks = count;
                    break;
                default:
                    break;
            }
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get dog metrics: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return metrics;
}

AggregatedMetrics MetricsAggregator::getMetricsByShelter(const std::string& shelter_id, const DateRange& range) {
    AggregatedMetrics metrics;
    metrics.dimension_type = "shelter";
    metrics.dimension_id = shelter_id;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT
                event_type,
                COUNT(*) as count
            FROM analytics_events
            WHERE entity_id = $1 AND entity_type = 'shelter'
              AND timestamp >= $2 AND timestamp <= $3
            GROUP BY event_type
        )";

        auto result = txn.exec_params(query, shelter_id, range.getStartISO(), range.getEndISO());

        for (const auto& row : result) {
            std::string type_str = row["event_type"].as<std::string>();
            int count = row["count"].as<int>();

            auto type_opt = analyticsEventTypeFromString(type_str);
            if (!type_opt) continue;

            switch (*type_opt) {
                case AnalyticsEventType::SHELTER_VIEW:
                    metrics.shelter_views = count;
                    break;
                default:
                    break;
            }
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get shelter metrics: " + std::string(e.what()),
            {{"shelter_id", shelter_id}}
        );
    }

    return metrics;
}

AggregatedMetrics MetricsAggregator::getMetricsByState(const std::string& state_code, const DateRange& range) {
    AggregatedMetrics metrics;
    metrics.dimension_type = "state";
    metrics.dimension_id = state_code;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT
                event_type,
                COUNT(*) as count
            FROM analytics_events
            WHERE region_code = $1
              AND timestamp >= $2 AND timestamp <= $3
            GROUP BY event_type
        )";

        auto result = txn.exec_params(query, state_code, range.getStartISO(), range.getEndISO());

        for (const auto& row : result) {
            std::string type_str = row["event_type"].as<std::string>();
            int count = row["count"].as<int>();
            metrics.page_views += count;
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get state metrics: " + std::string(e.what()),
            {{"state_code", state_code}}
        );
    }

    return metrics;
}

std::vector<TrendData> MetricsAggregator::getTrendData(
    const std::string& metric_name,
    const DateRange& range)
{
    std::vector<TrendData> trends;

    auto daily_metrics = getDailyMetricsRange(
        range.getStartISO().substr(0, 10),
        range.getEndISO().substr(0, 10)
    );

    int prev_value = 0;
    for (const auto& m : daily_metrics) {
        TrendData td;
        td.period = m.period;

        // Get the specific metric value
        if (metric_name == "views" || metric_name == "page_views") {
            td.value = m.page_views;
        } else if (metric_name == "dog_views") {
            td.value = m.dog_views;
        } else if (metric_name == "adoptions") {
            td.value = m.adoptions;
        } else if (metric_name == "fosters" || metric_name == "foster_started") {
            td.value = m.foster_started;
        } else if (metric_name == "new_users") {
            td.value = m.new_users;
        } else if (metric_name == "shares") {
            td.value = m.shares;
        } else if (metric_name == "favorites") {
            td.value = m.favorites_added;
        } else {
            td.value = m.page_views;
        }

        // Calculate change percent
        if (prev_value > 0) {
            td.change_percent = ((td.value - prev_value) / static_cast<double>(prev_value)) * 100.0;
        }
        prev_value = td.value;

        trends.push_back(td);
    }

    return trends;
}

// ============================================================================
// COMPARISON METHODS
// ============================================================================

double MetricsAggregator::getWeekOverWeekChange(const std::string& metric_name, const std::string& end_date) {
    // Get metrics for current week and previous week
    auto current_range = DateRange::fromParams(
        "", end_date); // Last 7 days ending at end_date
    auto prev_range = DateRange::custom(
        current_range.start - std::chrono::hours(24 * 7),
        current_range.start
    );

    auto current_metrics = getDailyMetricsRange(
        current_range.getStartISO().substr(0, 10),
        current_range.getEndISO().substr(0, 10)
    );
    auto prev_metrics = getDailyMetricsRange(
        prev_range.getStartISO().substr(0, 10),
        prev_range.getEndISO().substr(0, 10)
    );

    // Sum up the metric for both periods
    int current_total = 0, prev_total = 0;

    auto getValue = [&metric_name](const AggregatedMetrics& m) -> int {
        if (metric_name == "views") return m.page_views;
        if (metric_name == "adoptions") return m.adoptions;
        if (metric_name == "fosters") return m.foster_started;
        return m.page_views;
    };

    for (const auto& m : current_metrics) {
        current_total += getValue(m);
    }
    for (const auto& m : prev_metrics) {
        prev_total += getValue(m);
    }

    if (prev_total == 0) return 0.0;
    return ((current_total - prev_total) / static_cast<double>(prev_total)) * 100.0;
}

double MetricsAggregator::getMonthOverMonthChange(const std::string& metric_name, int year, int month) {
    auto current = getMonthlyMetrics(year, month);

    int prev_year = year;
    int prev_month = month - 1;
    if (prev_month == 0) {
        prev_year--;
        prev_month = 12;
    }
    auto previous = getMonthlyMetrics(prev_year, prev_month);

    if (!current || !previous) return 0.0;

    auto getValue = [&metric_name](const AggregatedMetrics& m) -> int {
        if (metric_name == "views") return m.page_views;
        if (metric_name == "adoptions") return m.adoptions;
        if (metric_name == "fosters") return m.foster_started;
        return m.page_views;
    };

    int current_val = getValue(*current);
    int prev_val = getValue(*previous);

    if (prev_val == 0) return 0.0;
    return ((current_val - prev_val) / static_cast<double>(prev_val)) * 100.0;
}

// ============================================================================
// MAINTENANCE METHODS
// ============================================================================

int MetricsAggregator::cleanupOldMetrics(int retention_days) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * retention_days);
        std::string cutoff_str = formatTimestampISO(cutoff).substr(0, 10);

        auto result = txn.exec_params(
            "DELETE FROM daily_metrics WHERE period_type = 'daily' AND period < $1",
            cutoff_str
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return static_cast<int>(result.affected_rows());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to cleanup old metrics: " + std::string(e.what()),
            {}
        );
        return 0;
    }
}

AggregationResult MetricsAggregator::recalculateMetrics(const std::string& start_date, const std::string& end_date) {
    AggregationResult total_result;
    total_result.started_at = std::chrono::system_clock::now();
    total_result.success = true;

    // Delete existing metrics in range
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        txn.exec_params(
            "DELETE FROM daily_metrics WHERE period_type = 'daily' AND period >= $1 AND period <= $2",
            start_date, end_date
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        total_result.success = false;
        total_result.error_message = "Failed to delete existing metrics: " + std::string(e.what());
        return total_result;
    }

    // Reaggregate
    auto start_tp = parseTimestamp(start_date);
    auto end_tp = parseTimestamp(end_date);

    auto current = start_tp;
    while (current <= end_tp) {
        auto time_t_val = std::chrono::system_clock::to_time_t(current);
        std::tm tm_val;

#ifdef _WIN32
        localtime_s(&tm_val, &time_t_val);
#else
        localtime_r(&time_t_val, &tm_val);
#endif

        std::ostringstream date_ss;
        date_ss << std::put_time(&tm_val, "%Y-%m-%d");

        auto result = aggregateDaily(date_ss.str());
        total_result.events_processed += result.events_processed;
        total_result.metrics_created += result.metrics_created;

        if (!result.success) {
            total_result.success = false;
            total_result.error_message = result.error_message;
        }

        current += std::chrono::hours(24);
    }

    total_result.completed_at = std::chrono::system_clock::now();
    return total_result;
}

int MetricsAggregator::markEventsProcessed(const std::vector<std::string>& event_ids) {
    if (event_ids.empty()) return 0;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::ostringstream id_list;
        id_list << "(";
        for (size_t i = 0; i < event_ids.size(); i++) {
            if (i > 0) id_list << ",";
            id_list << txn.quote(event_ids[i]);
        }
        id_list << ")";

        auto result = txn.exec(
            "UPDATE analytics_events SET is_processed = TRUE WHERE id IN " + id_list.str()
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return static_cast<int>(result.affected_rows());

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to mark events processed: " + std::string(e.what()),
            {}
        );
        return 0;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool MetricsAggregator::saveMetrics(const AggregatedMetrics& metrics) {
    return upsertMetrics(metrics);
}

bool MetricsAggregator::upsertMetrics(const AggregatedMetrics& metrics) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            INSERT INTO daily_metrics (
                period, period_type, dimension_type, dimension_id,
                page_views, unique_visitors, dog_views, shelter_views, search_count,
                favorites_added, favorites_removed, shares, contact_clicks, direction_clicks,
                foster_applications, foster_approved, foster_started, foster_ended, adoption_applications,
                adoptions, rescues, transports,
                new_users, active_users, returning_users,
                social_views, social_shares, social_engagement, social_referrals,
                notifications_sent, notifications_opened, emails_sent, emails_opened,
                created_at, updated_at
            ) VALUES (
                $1, $2, $3, $4,
                $5, $6, $7, $8, $9,
                $10, $11, $12, $13, $14,
                $15, $16, $17, $18, $19,
                $20, $21, $22,
                $23, $24, $25,
                $26, $27, $28, $29,
                $30, $31, $32, $33,
                NOW(), NOW()
            )
            ON CONFLICT (period, period_type, dimension_type, dimension_id)
            DO UPDATE SET
                page_views = EXCLUDED.page_views,
                unique_visitors = EXCLUDED.unique_visitors,
                dog_views = EXCLUDED.dog_views,
                shelter_views = EXCLUDED.shelter_views,
                search_count = EXCLUDED.search_count,
                favorites_added = EXCLUDED.favorites_added,
                favorites_removed = EXCLUDED.favorites_removed,
                shares = EXCLUDED.shares,
                contact_clicks = EXCLUDED.contact_clicks,
                direction_clicks = EXCLUDED.direction_clicks,
                foster_applications = EXCLUDED.foster_applications,
                foster_approved = EXCLUDED.foster_approved,
                foster_started = EXCLUDED.foster_started,
                foster_ended = EXCLUDED.foster_ended,
                adoption_applications = EXCLUDED.adoption_applications,
                adoptions = EXCLUDED.adoptions,
                rescues = EXCLUDED.rescues,
                transports = EXCLUDED.transports,
                new_users = EXCLUDED.new_users,
                active_users = EXCLUDED.active_users,
                returning_users = EXCLUDED.returning_users,
                social_views = EXCLUDED.social_views,
                social_shares = EXCLUDED.social_shares,
                social_engagement = EXCLUDED.social_engagement,
                social_referrals = EXCLUDED.social_referrals,
                notifications_sent = EXCLUDED.notifications_sent,
                notifications_opened = EXCLUDED.notifications_opened,
                emails_sent = EXCLUDED.emails_sent,
                emails_opened = EXCLUDED.emails_opened,
                updated_at = NOW()
        )";

        txn.exec_params(query,
            metrics.period, metrics.period_type, metrics.dimension_type, metrics.dimension_id,
            metrics.page_views, metrics.unique_visitors, metrics.dog_views, metrics.shelter_views, metrics.search_count,
            metrics.favorites_added, metrics.favorites_removed, metrics.shares, metrics.contact_clicks, metrics.direction_clicks,
            metrics.foster_applications, metrics.foster_approved, metrics.foster_started, metrics.foster_ended, metrics.adoption_applications,
            metrics.adoptions, metrics.rescues, metrics.transports,
            metrics.new_users, metrics.active_users, metrics.returning_users,
            metrics.social_views, metrics.social_shares, metrics.social_engagement, metrics.social_referrals,
            metrics.notifications_sent, metrics.notifications_opened, metrics.emails_sent, metrics.emails_opened
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to upsert metrics: " + std::string(e.what()),
            {{"period", metrics.period}}
        );
        return false;
    }
}

} // namespace wtl::analytics
