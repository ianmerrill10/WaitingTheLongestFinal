/**
 * @file DashboardStats.cc
 * @brief Implementation of dashboard statistics structures
 * @see DashboardStats.h for documentation
 */

#include "analytics/DashboardStats.h"
#include "analytics/AnalyticsEvent.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace wtl::analytics {

// ============================================================================
// DATE RANGE
// ============================================================================

DateRange DateRange::lastDays(int days) {
    DateRange range;
    range.end = std::chrono::system_clock::now();
    range.start = range.end - std::chrono::hours(24 * days);
    return range;
}

DateRange DateRange::lastWeek() {
    return lastDays(7);
}

DateRange DateRange::lastMonth() {
    return lastDays(30);
}

DateRange DateRange::lastYear() {
    return lastDays(365);
}

DateRange DateRange::today() {
    DateRange range;
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val;

#ifdef _WIN32
    localtime_s(&tm_val, &now_time);
#else
    localtime_r(&now_time, &tm_val);
#endif

    // Set to start of day
    tm_val.tm_hour = 0;
    tm_val.tm_min = 0;
    tm_val.tm_sec = 0;

    range.start = std::chrono::system_clock::from_time_t(std::mktime(&tm_val));
    range.end = now;
    return range;
}

DateRange DateRange::yesterday() {
    DateRange range;
    auto now = std::chrono::system_clock::now();
    auto yesterday = now - std::chrono::hours(24);
    auto yesterday_time = std::chrono::system_clock::to_time_t(yesterday);
    std::tm tm_val;

#ifdef _WIN32
    localtime_s(&tm_val, &yesterday_time);
#else
    localtime_r(&yesterday_time, &tm_val);
#endif

    // Set to start of yesterday
    tm_val.tm_hour = 0;
    tm_val.tm_min = 0;
    tm_val.tm_sec = 0;

    range.start = std::chrono::system_clock::from_time_t(std::mktime(&tm_val));

    // Set to end of yesterday
    tm_val.tm_hour = 23;
    tm_val.tm_min = 59;
    tm_val.tm_sec = 59;

    range.end = std::chrono::system_clock::from_time_t(std::mktime(&tm_val));
    return range;
}

DateRange DateRange::custom(const std::chrono::system_clock::time_point& start,
                            const std::chrono::system_clock::time_point& end) {
    DateRange range;
    range.start = start;
    range.end = end;
    return range;
}

DateRange DateRange::fromParams(const std::string& start_date, const std::string& end_date) {
    DateRange range;

    if (start_date.empty() && end_date.empty()) {
        return lastWeek();
    }

    if (!start_date.empty()) {
        range.start = parseTimestamp(start_date);
    } else {
        range.start = std::chrono::system_clock::now() - std::chrono::hours(24 * 7);
    }

    if (!end_date.empty()) {
        range.end = parseTimestamp(end_date);
    } else {
        range.end = std::chrono::system_clock::now();
    }

    return range;
}

std::string DateRange::getStartISO() const {
    return formatTimestampISO(start);
}

std::string DateRange::getEndISO() const {
    return formatTimestampISO(end);
}

int DateRange::getDurationDays() const {
    auto duration = std::chrono::duration_cast<std::chrono::hours>(end - start);
    return static_cast<int>(duration.count() / 24);
}

Json::Value DateRange::toJson() const {
    Json::Value json;
    json["start"] = getStartISO();
    json["end"] = getEndISO();
    json["duration_days"] = getDurationDays();
    return json;
}

// ============================================================================
// REAL TIME STATS
// ============================================================================

Json::Value RealTimeStats::toJson() const {
    Json::Value json;
    json["active_dogs"] = active_dogs;
    json["urgent_dogs"] = urgent_dogs;
    json["critical_dogs"] = critical_dogs;
    json["active_users_today"] = active_users_today;
    json["active_sessions"] = active_sessions;
    json["pending_applications"] = pending_applications;
    json["dogs_fostered_today"] = dogs_fostered_today;
    json["dogs_adopted_today"] = dogs_adopted_today;
    return json;
}

RealTimeStats RealTimeStats::fromDbRow(const pqxx::row& row) {
    RealTimeStats stats;
    stats.active_dogs = row["active_dogs"].as<int>(0);
    stats.urgent_dogs = row["urgent_dogs"].as<int>(0);
    stats.critical_dogs = row["critical_dogs"].as<int>(0);
    stats.active_users_today = row["active_users_today"].as<int>(0);
    stats.active_sessions = row["active_sessions"].as<int>(0);
    stats.pending_applications = row["pending_applications"].as<int>(0);
    stats.dogs_fostered_today = row["dogs_fostered_today"].as<int>(0);
    stats.dogs_adopted_today = row["dogs_adopted_today"].as<int>(0);
    return stats;
}

// ============================================================================
// ENGAGEMENT STATS
// ============================================================================

Json::Value EngagementStats::toJson() const {
    Json::Value json;
    json["total_page_views"] = total_page_views;
    json["unique_visitors"] = unique_visitors;
    json["dog_views"] = dog_views;
    json["searches"] = searches;
    json["favorites_added"] = favorites_added;
    json["shares"] = shares;
    json["avg_session_duration"] = avg_session_duration;
    json["bounce_rate"] = bounce_rate;
    json["pages_per_session"] = pages_per_session;
    return json;
}

EngagementStats EngagementStats::fromDbRow(const pqxx::row& row) {
    EngagementStats stats;
    stats.total_page_views = row["total_page_views"].as<int>(0);
    stats.unique_visitors = row["unique_visitors"].as<int>(0);
    stats.dog_views = row["dog_views"].as<int>(0);
    stats.searches = row["searches"].as<int>(0);
    stats.favorites_added = row["favorites_added"].as<int>(0);
    stats.shares = row["shares"].as<int>(0);
    stats.avg_session_duration = row["avg_session_duration"].as<double>(0);
    stats.bounce_rate = row["bounce_rate"].as<double>(0);
    stats.pages_per_session = row["pages_per_session"].as<int>(0);
    return stats;
}

// ============================================================================
// CONVERSION STATS
// ============================================================================

Json::Value ConversionStats::toJson() const {
    Json::Value json;
    json["foster_applications"] = foster_applications;
    json["foster_approved"] = foster_approved;
    json["foster_started"] = foster_started;
    json["adoption_applications"] = adoption_applications;
    json["adoptions_completed"] = adoptions_completed;
    json["dogs_rescued"] = dogs_rescued;
    json["view_to_application"] = view_to_application;
    json["application_to_adoption"] = application_to_adoption;
    json["foster_to_adoption"] = foster_to_adoption;
    return json;
}

ConversionStats ConversionStats::fromDbRow(const pqxx::row& row) {
    ConversionStats stats;
    stats.foster_applications = row["foster_applications"].as<int>(0);
    stats.foster_approved = row["foster_approved"].as<int>(0);
    stats.foster_started = row["foster_started"].as<int>(0);
    stats.adoption_applications = row["adoption_applications"].as<int>(0);
    stats.adoptions_completed = row["adoptions_completed"].as<int>(0);
    stats.dogs_rescued = row["dogs_rescued"].as<int>(0);
    stats.view_to_application = row["view_to_application"].as<double>(0);
    stats.application_to_adoption = row["application_to_adoption"].as<double>(0);
    stats.foster_to_adoption = row["foster_to_adoption"].as<double>(0);
    return stats;
}

// ============================================================================
// TREND DATA
// ============================================================================

Json::Value TrendData::toJson() const {
    Json::Value json;
    json["period"] = period;
    json["value"] = value;
    json["change_percent"] = change_percent;
    return json;
}

Json::Value TrendStats::toJson() const {
    Json::Value json;

    Json::Value dog_views_arr(Json::arrayValue);
    for (const auto& d : dog_views) {
        dog_views_arr.append(d.toJson());
    }
    json["dog_views"] = dog_views_arr;

    Json::Value adoptions_arr(Json::arrayValue);
    for (const auto& d : adoptions) {
        adoptions_arr.append(d.toJson());
    }
    json["adoptions"] = adoptions_arr;

    Json::Value fosters_arr(Json::arrayValue);
    for (const auto& d : fosters) {
        fosters_arr.append(d.toJson());
    }
    json["fosters"] = fosters_arr;

    Json::Value new_users_arr(Json::arrayValue);
    for (const auto& d : new_users) {
        new_users_arr.append(d.toJson());
    }
    json["new_users"] = new_users_arr;

    Json::Value urgent_dogs_arr(Json::arrayValue);
    for (const auto& d : urgent_dogs) {
        urgent_dogs_arr.append(d.toJson());
    }
    json["urgent_dogs"] = urgent_dogs_arr;

    // Week-over-week
    json["views_wow_change"] = views_wow_change;
    json["adoptions_wow_change"] = adoptions_wow_change;
    json["fosters_wow_change"] = fosters_wow_change;

    // Month-over-month
    json["views_mom_change"] = views_mom_change;
    json["adoptions_mom_change"] = adoptions_mom_change;
    json["fosters_mom_change"] = fosters_mom_change;

    return json;
}

// ============================================================================
// TOP PERFORMERS
// ============================================================================

Json::Value TopPerformer::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["name"] = name;
    json["type"] = type;
    json["metric_value"] = metric_value;
    json["metric_name"] = metric_name;
    if (!thumbnail_url.empty()) {
        json["thumbnail_url"] = thumbnail_url;
    }
    return json;
}

Json::Value TopPerformersStats::toJson() const {
    Json::Value json;

    auto arrayFromVector = [](const std::vector<TopPerformer>& vec) {
        Json::Value arr(Json::arrayValue);
        for (const auto& item : vec) {
            arr.append(item.toJson());
        }
        return arr;
    };

    json["most_viewed_dogs"] = arrayFromVector(most_viewed_dogs);
    json["most_shared_dogs"] = arrayFromVector(most_shared_dogs);
    json["most_favorited_dogs"] = arrayFromVector(most_favorited_dogs);
    json["top_shelters_by_adoptions"] = arrayFromVector(top_shelters_by_adoptions);
    json["top_content_by_engagement"] = arrayFromVector(top_content_by_engagement);
    json["fastest_adopted_dogs"] = arrayFromVector(fastest_adopted_dogs);

    return json;
}

// ============================================================================
// GEOGRAPHIC STATS
// ============================================================================

Json::Value GeographicStats::toJson() const {
    Json::Value json;

    Json::Value dogs_json(Json::objectValue);
    for (const auto& [state, count] : dogs_by_state) {
        dogs_json[state] = count;
    }
    json["dogs_by_state"] = dogs_json;

    Json::Value adoptions_json(Json::objectValue);
    for (const auto& [state, count] : adoptions_by_state) {
        adoptions_json[state] = count;
    }
    json["adoptions_by_state"] = adoptions_json;

    Json::Value users_json(Json::objectValue);
    for (const auto& [state, count] : users_by_state) {
        users_json[state] = count;
    }
    json["users_by_state"] = users_json;

    Json::Value urgent_json(Json::objectValue);
    for (const auto& [state, count] : urgent_dogs_by_state) {
        urgent_json[state] = count;
    }
    json["urgent_dogs_by_state"] = urgent_json;

    return json;
}

// ============================================================================
// SOCIAL MEDIA SUMMARY
// ============================================================================

Json::Value SocialMediaSummary::toJson() const {
    Json::Value json;
    json["total_social_views"] = total_social_views;
    json["total_social_shares"] = total_social_shares;
    json["total_social_engagement"] = total_social_engagement;
    json["social_referrals"] = social_referrals;
    json["social_attributed_adoptions"] = social_attributed_adoptions;
    json["top_platform"] = top_platform;
    json["best_performing_content_id"] = best_performing_content_id;
    return json;
}

// ============================================================================
// IMPACT SUMMARY
// ============================================================================

Json::Value ImpactSummary::toJson() const {
    Json::Value json;
    json["dogs_saved_from_euthanasia"] = dogs_saved_from_euthanasia;
    json["total_adoptions"] = total_adoptions;
    json["total_fosters"] = total_fosters;
    json["total_rescues"] = total_rescues;
    json["total_transports"] = total_transports;
    json["avg_time_to_adoption_days"] = avg_time_to_adoption_days;
    json["avg_time_improvement_days"] = avg_time_improvement_days;
    json["lives_saved_this_month"] = lives_saved_this_month;
    json["lives_saved_total"] = lives_saved_total;
    return json;
}

// ============================================================================
// DASHBOARD STATS
// ============================================================================

Json::Value DashboardStats::toJson() const {
    Json::Value json;

    json["date_range"] = date_range.toJson();
    json["generated_at"] = formatTimestampISO(generated_at);

    json["real_time"] = real_time.toJson();
    json["engagement"] = engagement.toJson();
    json["conversions"] = conversions.toJson();
    json["trends"] = trends.toJson();
    json["top_performers"] = top_performers.toJson();
    json["geographic"] = geographic.toJson();
    json["social"] = social.toJson();
    json["impact"] = impact.toJson();

    return json;
}

Json::Value DashboardStats::toSummaryJson() const {
    Json::Value json;

    json["date_range"] = date_range.toJson();
    json["generated_at"] = formatTimestampISO(generated_at);

    // Key metrics only
    json["active_dogs"] = real_time.active_dogs;
    json["urgent_dogs"] = real_time.urgent_dogs;
    json["critical_dogs"] = real_time.critical_dogs;
    json["total_views"] = engagement.total_page_views;
    json["unique_visitors"] = engagement.unique_visitors;
    json["adoptions"] = conversions.adoptions_completed;
    json["fosters"] = conversions.foster_started;
    json["lives_saved"] = impact.lives_saved_this_month;

    // Key trends
    json["views_wow_change"] = trends.views_wow_change;
    json["adoptions_wow_change"] = trends.adoptions_wow_change;

    return json;
}

// ============================================================================
// DOG STATS
// ============================================================================

Json::Value DogStats::toJson() const {
    Json::Value json;

    json["dog_id"] = dog_id;
    json["dog_name"] = dog_name;

    // View metrics
    json["total_views"] = total_views;
    json["unique_viewers"] = unique_viewers;
    json["photo_views"] = photo_views;
    json["video_plays"] = video_plays;

    // Engagement
    json["favorites_count"] = favorites_count;
    json["shares_count"] = shares_count;
    json["contact_clicks"] = contact_clicks;
    json["direction_clicks"] = direction_clicks;

    // Social
    json["social_views"] = social_views;
    json["social_shares"] = social_shares;
    json["social_engagement"] = social_engagement;

    // Conversion
    json["application_starts"] = application_starts;
    json["applications_submitted"] = applications_submitted;
    json["is_adopted"] = is_adopted;
    json["is_fostered"] = is_fostered;

    // Time
    json["days_listed"] = days_listed;
    json["first_view_date"] = first_view_date;
    json["last_view_date"] = last_view_date;

    // Trends
    Json::Value views_arr(Json::arrayValue);
    for (const auto& d : daily_views) {
        views_arr.append(d.toJson());
    }
    json["daily_views"] = views_arr;

    return json;
}

DogStats DogStats::fromDbRow(const pqxx::row& row) {
    DogStats stats;

    stats.dog_id = row["dog_id"].as<std::string>("");
    stats.dog_name = row["dog_name"].as<std::string>("");
    stats.total_views = row["total_views"].as<int>(0);
    stats.unique_viewers = row["unique_viewers"].as<int>(0);
    stats.photo_views = row["photo_views"].as<int>(0);
    stats.video_plays = row["video_plays"].as<int>(0);
    stats.favorites_count = row["favorites_count"].as<int>(0);
    stats.shares_count = row["shares_count"].as<int>(0);
    stats.contact_clicks = row["contact_clicks"].as<int>(0);
    stats.direction_clicks = row["direction_clicks"].as<int>(0);
    stats.social_views = row["social_views"].as<int>(0);
    stats.social_shares = row["social_shares"].as<int>(0);
    stats.social_engagement = row["social_engagement"].as<int>(0);
    stats.application_starts = row["application_starts"].as<int>(0);
    stats.applications_submitted = row["applications_submitted"].as<int>(0);
    stats.is_adopted = row["is_adopted"].as<bool>(false);
    stats.is_fostered = row["is_fostered"].as<bool>(false);
    stats.days_listed = row["days_listed"].as<int>(0);
    stats.first_view_date = row["first_view_date"].as<std::string>("");
    stats.last_view_date = row["last_view_date"].as<std::string>("");

    return stats;
}

// ============================================================================
// SHELTER STATS
// ============================================================================

Json::Value ShelterStats::toJson() const {
    Json::Value json;

    json["shelter_id"] = shelter_id;
    json["shelter_name"] = shelter_name;

    // Dog metrics
    json["active_dogs"] = active_dogs;
    json["urgent_dogs"] = urgent_dogs;
    json["total_dogs_listed"] = total_dogs_listed;

    // Outcomes
    json["adoptions"] = adoptions;
    json["fosters"] = fosters;
    json["rescues"] = rescues;
    json["returns"] = returns;

    // Performance
    json["avg_time_to_adoption"] = avg_time_to_adoption;
    json["adoption_rate"] = adoption_rate;
    json["foster_rate"] = foster_rate;

    // Engagement
    json["page_views"] = page_views;
    json["dog_views"] = dog_views;
    json["applications"] = applications;

    // Trends
    Json::Value adoptions_arr(Json::arrayValue);
    for (const auto& d : weekly_adoptions) {
        adoptions_arr.append(d.toJson());
    }
    json["weekly_adoptions"] = adoptions_arr;

    Json::Value intakes_arr(Json::arrayValue);
    for (const auto& d : weekly_intakes) {
        intakes_arr.append(d.toJson());
    }
    json["weekly_intakes"] = intakes_arr;

    return json;
}

ShelterStats ShelterStats::fromDbRow(const pqxx::row& row) {
    ShelterStats stats;

    stats.shelter_id = row["shelter_id"].as<std::string>("");
    stats.shelter_name = row["shelter_name"].as<std::string>("");
    stats.active_dogs = row["active_dogs"].as<int>(0);
    stats.urgent_dogs = row["urgent_dogs"].as<int>(0);
    stats.total_dogs_listed = row["total_dogs_listed"].as<int>(0);
    stats.adoptions = row["adoptions"].as<int>(0);
    stats.fosters = row["fosters"].as<int>(0);
    stats.rescues = row["rescues"].as<int>(0);
    stats.returns = row["returns"].as<int>(0);
    stats.avg_time_to_adoption = row["avg_time_to_adoption"].as<double>(0);
    stats.adoption_rate = row["adoption_rate"].as<double>(0);
    stats.foster_rate = row["foster_rate"].as<double>(0);
    stats.page_views = row["page_views"].as<int>(0);
    stats.dog_views = row["dog_views"].as<int>(0);
    stats.applications = row["applications"].as<int>(0);

    return stats;
}

} // namespace wtl::analytics
