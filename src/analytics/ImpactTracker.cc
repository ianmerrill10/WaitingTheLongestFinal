/**
 * @file ImpactTracker.cc
 * @brief Implementation of ImpactTracker - tracking lives saved
 * @see ImpactTracker.h for documentation
 */

#include "analytics/ImpactTracker.h"

#include "analytics/AnalyticsEvent.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::analytics {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// ATTRIBUTION
// ============================================================================

Json::Value Attribution::toJson() const {
    Json::Value json;
    json["source"] = source;

    if (campaign_id) json["campaign_id"] = *campaign_id;
    if (content_id) json["content_id"] = *content_id;
    if (notification_id) json["notification_id"] = *notification_id;
    if (referrer) json["referrer"] = *referrer;
    if (utm_source) json["utm_source"] = *utm_source;
    if (utm_medium) json["utm_medium"] = *utm_medium;
    if (utm_campaign) json["utm_campaign"] = *utm_campaign;
    if (user_id) json["user_id"] = *user_id;

    return json;
}

Attribution Attribution::fromJson(const Json::Value& json) {
    Attribution attr;
    attr.source = json.get("source", "direct").asString();

    if (json.isMember("campaign_id") && !json["campaign_id"].isNull()) {
        attr.campaign_id = json["campaign_id"].asString();
    }
    if (json.isMember("content_id") && !json["content_id"].isNull()) {
        attr.content_id = json["content_id"].asString();
    }
    if (json.isMember("notification_id") && !json["notification_id"].isNull()) {
        attr.notification_id = json["notification_id"].asString();
    }
    if (json.isMember("referrer") && !json["referrer"].isNull()) {
        attr.referrer = json["referrer"].asString();
    }
    if (json.isMember("utm_source") && !json["utm_source"].isNull()) {
        attr.utm_source = json["utm_source"].asString();
    }
    if (json.isMember("utm_medium") && !json["utm_medium"].isNull()) {
        attr.utm_medium = json["utm_medium"].asString();
    }
    if (json.isMember("utm_campaign") && !json["utm_campaign"].isNull()) {
        attr.utm_campaign = json["utm_campaign"].asString();
    }
    if (json.isMember("user_id") && !json["user_id"].isNull()) {
        attr.user_id = json["user_id"].asString();
    }

    return attr;
}

// ============================================================================
// IMPACT EVENT
// ============================================================================

Json::Value ImpactEvent::toJson() const {
    Json::Value json;

    json["id"] = id;
    json["impact_type"] = impactTypeToString(impact_type);
    json["dog_id"] = dog_id;
    json["dog_name"] = dog_name;

    if (shelter_id) json["shelter_id"] = *shelter_id;
    if (shelter_name) json["shelter_name"] = *shelter_name;
    if (state_code) json["state_code"] = *state_code;

    json["attribution"] = attribution.toJson();
    json["event_time"] = formatTimestampISO(event_time);

    if (days_in_shelter) json["days_in_shelter"] = *days_in_shelter;
    if (days_on_platform) json["days_on_platform"] = *days_on_platform;
    if (hours_to_euthanasia) json["hours_to_euthanasia"] = *hours_to_euthanasia;

    json["was_urgent"] = was_urgent;
    json["was_critical"] = was_critical;
    json["was_on_euthanasia_list"] = was_on_euthanasia_list;

    if (!additional_data.isNull()) {
        json["additional_data"] = additional_data;
    }

    return json;
}

ImpactEvent ImpactEvent::fromDbRow(const pqxx::row& row) {
    ImpactEvent event;

    event.id = row["id"].as<std::string>();

    auto type_opt = impactTypeFromString(row["impact_type"].as<std::string>());
    event.impact_type = type_opt.value_or(ImpactType::ADOPTION);

    event.dog_id = row["dog_id"].as<std::string>();
    event.dog_name = row["dog_name"].as<std::string>("");

    if (!row["shelter_id"].is_null()) {
        event.shelter_id = row["shelter_id"].as<std::string>();
    }
    if (!row["shelter_name"].is_null()) {
        event.shelter_name = row["shelter_name"].as<std::string>();
    }
    if (!row["state_code"].is_null()) {
        event.state_code = row["state_code"].as<std::string>();
    }

    // Parse attribution from JSON field
    if (!row["attribution"].is_null()) {
        std::string attr_str = row["attribution"].as<std::string>();
        Json::CharReaderBuilder builder;
        std::istringstream stream(attr_str);
        Json::Value attr_json;
        std::string errors;
        if (Json::parseFromStream(builder, stream, &attr_json, &errors)) {
            event.attribution = Attribution::fromJson(attr_json);
        }
    }

    event.event_time = parseTimestamp(row["event_time"].as<std::string>());

    if (!row["days_in_shelter"].is_null()) {
        event.days_in_shelter = row["days_in_shelter"].as<int>();
    }
    if (!row["days_on_platform"].is_null()) {
        event.days_on_platform = row["days_on_platform"].as<int>();
    }
    if (!row["hours_to_euthanasia"].is_null()) {
        event.hours_to_euthanasia = row["hours_to_euthanasia"].as<int>();
    }

    event.was_urgent = row["was_urgent"].as<bool>(false);
    event.was_critical = row["was_critical"].as<bool>(false);
    event.was_on_euthanasia_list = row["was_on_euthanasia_list"].as<bool>(false);

    // Parse additional data
    if (!row["additional_data"].is_null()) {
        std::string data_str = row["additional_data"].as<std::string>();
        Json::CharReaderBuilder builder;
        std::istringstream stream(data_str);
        std::string errors;
        Json::parseFromStream(builder, stream, &event.additional_data, &errors);
    }

    return event;
}

// ============================================================================
// IMPACT METRICS
// ============================================================================

Json::Value ImpactMetrics::toJson() const {
    Json::Value json;

    json["total_lives_saved"] = total_lives_saved;
    json["adoptions"] = adoptions;
    json["foster_placements"] = foster_placements;
    json["foster_to_adoptions"] = foster_to_adoptions;
    json["rescue_pulls"] = rescue_pulls;
    json["transports"] = transports;
    json["saved_from_euthanasia"] = saved_from_euthanasia;
    json["returned_to_owners"] = returned_to_owners;
    json["transfers_to_rescue"] = transfers_to_rescue;

    json["avg_days_in_shelter"] = avg_days_in_shelter;
    json["avg_days_on_platform"] = avg_days_on_platform;
    json["time_improvement_days"] = time_improvement_days;

    // Breakdowns
    Json::Value by_source_json(Json::objectValue);
    for (const auto& [source, count] : by_source) {
        by_source_json[source] = count;
    }
    json["by_source"] = by_source_json;

    Json::Value by_platform_json(Json::objectValue);
    for (const auto& [platform, count] : by_platform) {
        by_platform_json[platform] = count;
    }
    json["by_platform"] = by_platform_json;

    Json::Value by_state_json(Json::objectValue);
    for (const auto& [state, count] : by_state) {
        by_state_json[state] = count;
    }
    json["by_state"] = by_state_json;

    json["urgent_dogs_saved"] = urgent_dogs_saved;
    json["critical_dogs_saved"] = critical_dogs_saved;
    json["euthanasia_list_saves"] = euthanasia_list_saves;

    return json;
}

// ============================================================================
// ATTRIBUTION REPORT
// ============================================================================

Json::Value AttributionReport::toJson() const {
    Json::Value json;

    Json::Value sources_arr(Json::arrayValue);
    for (const auto& src : sources) {
        Json::Value src_json;
        src_json["source"] = src.source;
        src_json["total_saves"] = src.total_saves;
        src_json["adoptions"] = src.adoptions;
        src_json["fosters"] = src.fosters;
        src_json["conversion_rate"] = src.conversion_rate;
        src_json["avg_time_to_save"] = src.avg_time_to_save;
        sources_arr.append(src_json);
    }
    json["sources"] = sources_arr;

    json["top_source"] = top_source;
    json["top_campaign"] = top_campaign;
    json["top_content"] = top_content;
    json["total_attributed"] = total_attributed;
    json["total_unattributed"] = total_unattributed;

    return json;
}

// ============================================================================
// SINGLETON
// ============================================================================

ImpactTracker& ImpactTracker::getInstance() {
    static ImpactTracker instance;
    return instance;
}

ImpactTracker::ImpactTracker()
    : cache_time_(std::chrono::system_clock::now() - std::chrono::hours(1))
{
}

// ============================================================================
// RECORDING IMPACT
// ============================================================================

std::string ImpactTracker::recordAdoption(const std::string& dog_id, const Attribution& attribution) {
    ImpactEvent event;
    event.id = generateUUID();
    event.impact_type = ImpactType::ADOPTION;
    event.dog_id = dog_id;
    event.attribution = attribution;
    event.event_time = std::chrono::system_clock::now();

    populateDogContext(event);
    calculateTimeMetrics(event);

    if (saveImpactEvent(event)) {
        return event.id;
    }
    return "";
}

std::string ImpactTracker::recordFosterPlacement(
    const std::string& dog_id,
    const std::string& foster_id,
    const Attribution& attribution)
{
    ImpactEvent event;
    event.id = generateUUID();
    event.impact_type = ImpactType::FOSTER_PLACEMENT;
    event.dog_id = dog_id;
    event.attribution = attribution;
    event.event_time = std::chrono::system_clock::now();
    event.additional_data["foster_id"] = foster_id;

    populateDogContext(event);
    calculateTimeMetrics(event);

    if (saveImpactEvent(event)) {
        return event.id;
    }
    return "";
}

std::string ImpactTracker::recordFosterToAdoption(
    const std::string& dog_id,
    const std::string& foster_id,
    const Attribution& attribution)
{
    ImpactEvent event;
    event.id = generateUUID();
    event.impact_type = ImpactType::FOSTER_TO_ADOPTION;
    event.dog_id = dog_id;
    event.attribution = attribution;
    event.event_time = std::chrono::system_clock::now();
    event.additional_data["foster_id"] = foster_id;

    populateDogContext(event);
    calculateTimeMetrics(event);

    if (saveImpactEvent(event)) {
        return event.id;
    }
    return "";
}

std::string ImpactTracker::recordRescuePull(
    const std::string& dog_id,
    const std::string& rescue_id,
    const Attribution& attribution)
{
    ImpactEvent event;
    event.id = generateUUID();
    event.impact_type = ImpactType::RESCUE_PULL;
    event.dog_id = dog_id;
    event.attribution = attribution;
    event.event_time = std::chrono::system_clock::now();
    event.additional_data["rescue_id"] = rescue_id;

    populateDogContext(event);
    calculateTimeMetrics(event);

    if (saveImpactEvent(event)) {
        return event.id;
    }
    return "";
}

std::string ImpactTracker::recordTransport(
    const std::string& dog_id,
    const std::string& from_shelter_id,
    const std::string& to_shelter_id,
    const Attribution& attribution)
{
    ImpactEvent event;
    event.id = generateUUID();
    event.impact_type = ImpactType::TRANSPORT;
    event.dog_id = dog_id;
    event.attribution = attribution;
    event.event_time = std::chrono::system_clock::now();
    event.additional_data["from_shelter_id"] = from_shelter_id;
    event.additional_data["to_shelter_id"] = to_shelter_id;

    populateDogContext(event);
    calculateTimeMetrics(event);

    if (saveImpactEvent(event)) {
        return event.id;
    }
    return "";
}

std::string ImpactTracker::recordSavedFromEuthanasia(
    const std::string& dog_id,
    ImpactType outcome,
    const Attribution& attribution)
{
    ImpactEvent event;
    event.id = generateUUID();
    event.impact_type = ImpactType::SAVED_FROM_EUTHANASIA;
    event.dog_id = dog_id;
    event.attribution = attribution;
    event.event_time = std::chrono::system_clock::now();
    event.was_on_euthanasia_list = true;
    event.additional_data["outcome"] = impactTypeToString(outcome);

    populateDogContext(event);
    calculateTimeMetrics(event);

    if (saveImpactEvent(event)) {
        return event.id;
    }
    return "";
}

std::string ImpactTracker::recordImpact(const ImpactEvent& event) {
    ImpactEvent mutable_event = event;
    if (mutable_event.id.empty()) {
        mutable_event.id = generateUUID();
    }
    if (mutable_event.event_time == std::chrono::system_clock::time_point{}) {
        mutable_event.event_time = std::chrono::system_clock::now();
    }

    populateDogContext(mutable_event);
    calculateTimeMetrics(mutable_event);

    if (saveImpactEvent(mutable_event)) {
        return mutable_event.id;
    }
    return "";
}

// ============================================================================
// QUERYING IMPACT
// ============================================================================

ImpactSummary ImpactTracker::getImpactSummary(const DateRange& range) {
    ImpactSummary summary;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT
                COUNT(*) FILTER (WHERE impact_type = 'ADOPTION' OR impact_type = 'FOSTER_TO_ADOPTION') as adoptions,
                COUNT(*) FILTER (WHERE impact_type = 'FOSTER_PLACEMENT') as fosters,
                COUNT(*) FILTER (WHERE impact_type = 'RESCUE_PULL') as rescues,
                COUNT(*) FILTER (WHERE impact_type = 'TRANSPORT') as transports,
                COUNT(*) FILTER (WHERE was_on_euthanasia_list = true) as saved_from_euthanasia,
                COUNT(*) as total_lives_saved,
                AVG(days_in_shelter) FILTER (WHERE impact_type = 'ADOPTION') as avg_time_to_adoption
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
        )";

        auto result = txn.exec_params1(query, range.getStartISO(), range.getEndISO());

        summary.total_adoptions = result["adoptions"].as<int>(0);
        summary.total_fosters = result["fosters"].as<int>(0);
        summary.total_rescues = result["rescues"].as<int>(0);
        summary.total_transports = result["transports"].as<int>(0);
        summary.dogs_saved_from_euthanasia = result["saved_from_euthanasia"].as<int>(0);
        summary.lives_saved_this_month = result["total_lives_saved"].as<int>(0);
        summary.avg_time_to_adoption_days = result["avg_time_to_adoption"].as<double>(0);

        // Get all-time total
        auto total_result = txn.exec1("SELECT COUNT(*) as total FROM impact_tracking");
        summary.lives_saved_total = total_result["total"].as<int>(0);

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get impact summary: " + std::string(e.what()),
            {}
        );
    }

    return summary;
}

ImpactMetrics ImpactTracker::getImpactMetrics(const DateRange& range) {
    ImpactMetrics metrics;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get counts by type
        auto type_query = R"(
            SELECT impact_type, COUNT(*) as count
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
            GROUP BY impact_type
        )";

        auto type_result = txn.exec_params(type_query, range.getStartISO(), range.getEndISO());

        for (const auto& row : type_result) {
            std::string type_str = row["impact_type"].as<std::string>();
            int count = row["count"].as<int>();
            metrics.total_lives_saved += count;

            auto type_opt = impactTypeFromString(type_str);
            if (!type_opt) continue;

            switch (*type_opt) {
                case ImpactType::ADOPTION:
                    metrics.adoptions = count;
                    break;
                case ImpactType::FOSTER_PLACEMENT:
                    metrics.foster_placements = count;
                    break;
                case ImpactType::FOSTER_TO_ADOPTION:
                    metrics.foster_to_adoptions = count;
                    break;
                case ImpactType::RESCUE_PULL:
                    metrics.rescue_pulls = count;
                    break;
                case ImpactType::TRANSPORT:
                    metrics.transports = count;
                    break;
                case ImpactType::SAVED_FROM_EUTHANASIA:
                    metrics.saved_from_euthanasia = count;
                    break;
                case ImpactType::RETURNED_TO_OWNER:
                    metrics.returned_to_owners = count;
                    break;
                case ImpactType::TRANSFER_TO_RESCUE:
                    metrics.transfers_to_rescue = count;
                    break;
            }
        }

        // Get averages
        auto avg_query = R"(
            SELECT
                AVG(days_in_shelter) as avg_shelter,
                AVG(days_on_platform) as avg_platform
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
        )";

        auto avg_result = txn.exec_params1(avg_query, range.getStartISO(), range.getEndISO());
        metrics.avg_days_in_shelter = avg_result["avg_shelter"].as<double>(0);
        metrics.avg_days_on_platform = avg_result["avg_platform"].as<double>(0);

        // Get by state
        auto state_query = R"(
            SELECT state_code, COUNT(*) as count
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2 AND state_code IS NOT NULL
            GROUP BY state_code
        )";

        auto state_result = txn.exec_params(state_query, range.getStartISO(), range.getEndISO());
        for (const auto& row : state_result) {
            metrics.by_state[row["state_code"].as<std::string>()] = row["count"].as<int>();
        }

        // Get by source (from attribution JSON)
        auto source_query = R"(
            SELECT attribution->>'source' as source, COUNT(*) as count
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
              AND attribution->>'source' IS NOT NULL
            GROUP BY attribution->>'source'
        )";

        auto source_result = txn.exec_params(source_query, range.getStartISO(), range.getEndISO());
        for (const auto& row : source_result) {
            if (!row["source"].is_null()) {
                metrics.by_source[row["source"].as<std::string>()] = row["count"].as<int>();
            }
        }

        // Get urgency stats
        auto urgency_query = R"(
            SELECT
                COUNT(*) FILTER (WHERE was_urgent = true) as urgent,
                COUNT(*) FILTER (WHERE was_critical = true) as critical,
                COUNT(*) FILTER (WHERE was_on_euthanasia_list = true) as euth_list
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
        )";

        auto urgency_result = txn.exec_params1(urgency_query, range.getStartISO(), range.getEndISO());
        metrics.urgent_dogs_saved = urgency_result["urgent"].as<int>(0);
        metrics.critical_dogs_saved = urgency_result["critical"].as<int>(0);
        metrics.euthanasia_list_saves = urgency_result["euth_list"].as<int>(0);

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get impact metrics: " + std::string(e.what()),
            {}
        );
    }

    return metrics;
}

std::vector<ImpactEvent> ImpactTracker::getRecentImpactEvents(int limit) {
    std::vector<ImpactEvent> events;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(SELECT * FROM impact_tracking ORDER BY event_time DESC LIMIT $1)",
            limit
        );

        for (const auto& row : result) {
            events.push_back(ImpactEvent::fromDbRow(row));
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get recent impact events: " + std::string(e.what()),
            {}
        );
    }

    return events;
}

std::vector<ImpactEvent> ImpactTracker::getImpactByDog(const std::string& dog_id) {
    std::vector<ImpactEvent> events;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            "SELECT * FROM impact_tracking WHERE dog_id = $1 ORDER BY event_time DESC",
            dog_id
        );

        for (const auto& row : result) {
            events.push_back(ImpactEvent::fromDbRow(row));
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get impact by dog: " + std::string(e.what()),
            {{"dog_id", dog_id}}
        );
    }

    return events;
}

std::vector<ImpactEvent> ImpactTracker::getImpactByShelter(const std::string& shelter_id, const DateRange& range) {
    std::vector<ImpactEvent> events;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(SELECT * FROM impact_tracking
               WHERE shelter_id = $1 AND event_time >= $2 AND event_time <= $3
               ORDER BY event_time DESC)",
            shelter_id, range.getStartISO(), range.getEndISO()
        );

        for (const auto& row : result) {
            events.push_back(ImpactEvent::fromDbRow(row));
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get impact by shelter: " + std::string(e.what()),
            {{"shelter_id", shelter_id}}
        );
    }

    return events;
}

std::vector<ImpactEvent> ImpactTracker::getImpactByState(const std::string& state_code, const DateRange& range) {
    std::vector<ImpactEvent> events;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params(
            R"(SELECT * FROM impact_tracking
               WHERE state_code = $1 AND event_time >= $2 AND event_time <= $3
               ORDER BY event_time DESC)",
            state_code, range.getStartISO(), range.getEndISO()
        );

        for (const auto& row : result) {
            events.push_back(ImpactEvent::fromDbRow(row));
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get impact by state: " + std::string(e.what()),
            {{"state_code", state_code}}
        );
    }

    return events;
}

int ImpactTracker::getTotalLivesSaved() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Return cached value if recent
    auto now = std::chrono::system_clock::now();
    if (now - cache_time_ < std::chrono::minutes(5)) {
        return cached_total_lives_;
    }

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec1("SELECT COUNT(*) as total FROM impact_tracking");
        cached_total_lives_ = result["total"].as<int>(0);
        cache_time_ = now;

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get total lives saved: " + std::string(e.what()),
            {}
        );
    }

    return cached_total_lives_;
}

int ImpactTracker::getLivesSaved(const DateRange& range) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params1(
            R"(SELECT COUNT(*) as total FROM impact_tracking
               WHERE event_time >= $1 AND event_time <= $2)",
            range.getStartISO(), range.getEndISO()
        );

        ConnectionPool::getInstance().release(conn);
        return result["total"].as<int>(0);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get lives saved for range: " + std::string(e.what()),
            {}
        );
        return 0;
    }
}

// ============================================================================
// ATTRIBUTION
// ============================================================================

AttributionReport ImpactTracker::getAttributionReport(const DateRange& range) {
    AttributionReport report;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Get by source
        auto source_query = R"(
            SELECT
                attribution->>'source' as source,
                COUNT(*) as total_saves,
                COUNT(*) FILTER (WHERE impact_type = 'ADOPTION' OR impact_type = 'FOSTER_TO_ADOPTION') as adoptions,
                COUNT(*) FILTER (WHERE impact_type = 'FOSTER_PLACEMENT') as fosters,
                AVG(days_on_platform) as avg_time
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
            GROUP BY attribution->>'source'
            ORDER BY total_saves DESC
        )";

        auto source_result = txn.exec_params(source_query, range.getStartISO(), range.getEndISO());

        for (const auto& row : source_result) {
            AttributionReport::SourcePerformance perf;
            perf.source = row["source"].as<std::string>("unknown");
            perf.total_saves = row["total_saves"].as<int>(0);
            perf.adoptions = row["adoptions"].as<int>(0);
            perf.fosters = row["fosters"].as<int>(0);
            perf.avg_time_to_save = row["avg_time"].as<double>(0);
            report.sources.push_back(perf);

            report.total_attributed += perf.total_saves;
        }

        if (!report.sources.empty()) {
            report.top_source = report.sources[0].source;
        }

        // Get top campaign
        auto campaign_query = R"(
            SELECT attribution->>'campaign_id' as campaign, COUNT(*) as count
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
              AND attribution->>'campaign_id' IS NOT NULL
            GROUP BY attribution->>'campaign_id'
            ORDER BY count DESC
            LIMIT 1
        )";

        auto campaign_result = txn.exec_params(campaign_query, range.getStartISO(), range.getEndISO());
        if (!campaign_result.empty() && !campaign_result[0]["campaign"].is_null()) {
            report.top_campaign = campaign_result[0]["campaign"].as<std::string>();
        }

        // Get top content
        auto content_query = R"(
            SELECT attribution->>'content_id' as content, COUNT(*) as count
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
              AND attribution->>'content_id' IS NOT NULL
            GROUP BY attribution->>'content_id'
            ORDER BY count DESC
            LIMIT 1
        )";

        auto content_result = txn.exec_params(content_query, range.getStartISO(), range.getEndISO());
        if (!content_result.empty() && !content_result[0]["content"].is_null()) {
            report.top_content = content_result[0]["content"].as<std::string>();
        }

        // Count unattributed
        auto unattr_result = txn.exec_params1(
            R"(SELECT COUNT(*) as count FROM impact_tracking
               WHERE event_time >= $1 AND event_time <= $2
                 AND (attribution IS NULL OR attribution->>'source' IS NULL))",
            range.getStartISO(), range.getEndISO()
        );
        report.total_unattributed = unattr_result["count"].as<int>(0);

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get attribution report: " + std::string(e.what()),
            {}
        );
    }

    return report;
}

std::vector<std::pair<std::string, int>> ImpactTracker::getTopContentByAdoptions(const DateRange& range, int limit) {
    std::vector<std::pair<std::string, int>> results;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT attribution->>'content_id' as content, COUNT(*) as count
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
              AND (impact_type = 'ADOPTION' OR impact_type = 'FOSTER_TO_ADOPTION')
              AND attribution->>'content_id' IS NOT NULL
            GROUP BY attribution->>'content_id'
            ORDER BY count DESC
            LIMIT $3
        )";

        auto result = txn.exec_params(query, range.getStartISO(), range.getEndISO(), limit);

        for (const auto& row : result) {
            results.emplace_back(row["content"].as<std::string>(), row["count"].as<int>());
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get top content by adoptions: " + std::string(e.what()),
            {}
        );
    }

    return results;
}

std::vector<std::pair<std::string, int>> ImpactTracker::getTopCampaignsByAdoptions(const DateRange& range, int limit) {
    std::vector<std::pair<std::string, int>> results;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT attribution->>'campaign_id' as campaign, COUNT(*) as count
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
              AND (impact_type = 'ADOPTION' OR impact_type = 'FOSTER_TO_ADOPTION')
              AND attribution->>'campaign_id' IS NOT NULL
            GROUP BY attribution->>'campaign_id'
            ORDER BY count DESC
            LIMIT $3
        )";

        auto result = txn.exec_params(query, range.getStartISO(), range.getEndISO(), limit);

        for (const auto& row : result) {
            results.emplace_back(row["campaign"].as<std::string>(), row["count"].as<int>());
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get top campaigns by adoptions: " + std::string(e.what()),
            {}
        );
    }

    return results;
}

double ImpactTracker::getConversionRate(const std::string& source, const DateRange& range) {
    // This would need integration with view tracking to calculate properly
    // For now, return a placeholder that can be implemented with full event data
    return 0.0;
}

// ============================================================================
// TIME ANALYSIS
// ============================================================================

double ImpactTracker::getAverageTimeToAdoption(const DateRange& range) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params1(
            R"(SELECT AVG(days_in_shelter) as avg_time FROM impact_tracking
               WHERE event_time >= $1 AND event_time <= $2
                 AND (impact_type = 'ADOPTION' OR impact_type = 'FOSTER_TO_ADOPTION')
                 AND days_in_shelter IS NOT NULL)",
            range.getStartISO(), range.getEndISO()
        );

        ConnectionPool::getInstance().release(conn);
        return result["avg_time"].as<double>(0);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get average time to adoption: " + std::string(e.what()),
            {}
        );
        return 0;
    }
}

double ImpactTracker::getTimeImprovement(const DateRange& range) {
    // Compare current average to historical average (e.g., national average of ~60 days)
    double current_avg = getAverageTimeToAdoption(range);
    const double NATIONAL_AVERAGE_DAYS = 60.0;  // Approximation

    return NATIONAL_AVERAGE_DAYS - current_avg;
}

std::map<std::string, int> ImpactTracker::getAdoptionTimeDistribution(const DateRange& range) {
    std::map<std::string, int> distribution;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto query = R"(
            SELECT
                CASE
                    WHEN days_in_shelter <= 7 THEN '0-7 days'
                    WHEN days_in_shelter <= 14 THEN '8-14 days'
                    WHEN days_in_shelter <= 30 THEN '15-30 days'
                    WHEN days_in_shelter <= 60 THEN '31-60 days'
                    WHEN days_in_shelter <= 90 THEN '61-90 days'
                    ELSE '90+ days'
                END as bucket,
                COUNT(*) as count
            FROM impact_tracking
            WHERE event_time >= $1 AND event_time <= $2
              AND (impact_type = 'ADOPTION' OR impact_type = 'FOSTER_TO_ADOPTION')
              AND days_in_shelter IS NOT NULL
            GROUP BY bucket
            ORDER BY bucket
        )";

        auto result = txn.exec_params(query, range.getStartISO(), range.getEndISO());

        for (const auto& row : result) {
            distribution[row["bucket"].as<std::string>()] = row["count"].as<int>();
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to get adoption time distribution: " + std::string(e.what()),
            {}
        );
    }

    return distribution;
}

// ============================================================================
// URGENCY IMPACT
// ============================================================================

int ImpactTracker::getUrgentDogsSaved(const DateRange& range) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params1(
            R"(SELECT COUNT(*) as count FROM impact_tracking
               WHERE event_time >= $1 AND event_time <= $2 AND was_urgent = true)",
            range.getStartISO(), range.getEndISO()
        );

        ConnectionPool::getInstance().release(conn);
        return result["count"].as<int>(0);

    } catch (const std::exception& e) {
        return 0;
    }
}

int ImpactTracker::getCriticalDogsSaved(const DateRange& range) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params1(
            R"(SELECT COUNT(*) as count FROM impact_tracking
               WHERE event_time >= $1 AND event_time <= $2 AND was_critical = true)",
            range.getStartISO(), range.getEndISO()
        );

        ConnectionPool::getInstance().release(conn);
        return result["count"].as<int>(0);

    } catch (const std::exception& e) {
        return 0;
    }
}

int ImpactTracker::getEuthanasiaListSaves(const DateRange& range) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params1(
            R"(SELECT COUNT(*) as count FROM impact_tracking
               WHERE event_time >= $1 AND event_time <= $2 AND was_on_euthanasia_list = true)",
            range.getStartISO(), range.getEndISO()
        );

        ConnectionPool::getInstance().release(conn);
        return result["count"].as<int>(0);

    } catch (const std::exception& e) {
        return 0;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool ImpactTracker::saveImpactEvent(const ImpactEvent& event) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        Json::StreamWriterBuilder writer;
        std::string attribution_json = Json::writeString(writer, event.attribution.toJson());
        std::string additional_json = Json::writeString(writer, event.additional_data);

        auto query = R"(
            INSERT INTO impact_tracking (
                id, impact_type, dog_id, dog_name, shelter_id, shelter_name, state_code,
                attribution, event_time, days_in_shelter, days_on_platform, hours_to_euthanasia,
                was_urgent, was_critical, was_on_euthanasia_list, additional_data, created_at
            ) VALUES (
                $1, $2, $3, $4, $5, $6, $7, $8::jsonb, $9, $10, $11, $12, $13, $14, $15, $16::jsonb, NOW()
            )
        )";

        txn.exec_params(query,
            event.id,
            impactTypeToString(event.impact_type),
            event.dog_id,
            event.dog_name,
            event.shelter_id.value_or(""),
            event.shelter_name.value_or(""),
            event.state_code.value_or(""),
            attribution_json,
            formatTimestampISO(event.event_time),
            event.days_in_shelter.value_or(0),
            event.days_on_platform.value_or(0),
            event.hours_to_euthanasia.value_or(0),
            event.was_urgent,
            event.was_critical,
            event.was_on_euthanasia_list,
            additional_json
        );

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        // Invalidate cache
        cache_time_ = std::chrono::system_clock::time_point{};

        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::DATABASE,
            "Failed to save impact event: " + std::string(e.what()),
            {{"dog_id", event.dog_id}, {"impact_type", impactTypeToString(event.impact_type)}}
        );
        return false;
    }
}

void ImpactTracker::populateDogContext(ImpactEvent& event) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params1(
            R"(SELECT d.name, d.shelter_id, d.urgency_level, d.is_on_euthanasia_list,
                      d.intake_date, d.created_at as listed_at,
                      s.name as shelter_name, s.state_code
               FROM dogs d
               LEFT JOIN shelters s ON d.shelter_id = s.id
               WHERE d.id = $1)",
            event.dog_id
        );

        event.dog_name = result["name"].as<std::string>("");

        if (!result["shelter_id"].is_null()) {
            event.shelter_id = result["shelter_id"].as<std::string>();
        }
        if (!result["shelter_name"].is_null()) {
            event.shelter_name = result["shelter_name"].as<std::string>();
        }
        if (!result["state_code"].is_null()) {
            event.state_code = result["state_code"].as<std::string>();
        }

        std::string urgency = result["urgency_level"].as<std::string>("normal");
        event.was_urgent = (urgency == "high" || urgency == "critical" || urgency == "medium");
        event.was_critical = (urgency == "critical");
        event.was_on_euthanasia_list = result["is_on_euthanasia_list"].as<bool>(false);

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        // Non-critical, continue without context
    }
}

void ImpactTracker::calculateTimeMetrics(ImpactEvent& event) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec_params1(
            R"(SELECT
                  EXTRACT(DAY FROM NOW() - intake_date) as days_in_shelter,
                  EXTRACT(DAY FROM NOW() - created_at) as days_on_platform,
                  EXTRACT(EPOCH FROM (euthanasia_date - NOW())) / 3600 as hours_to_euth
               FROM dogs WHERE id = $1)",
            event.dog_id
        );

        if (!result["days_in_shelter"].is_null()) {
            event.days_in_shelter = static_cast<int>(result["days_in_shelter"].as<double>(0));
        }
        if (!result["days_on_platform"].is_null()) {
            event.days_on_platform = static_cast<int>(result["days_on_platform"].as<double>(0));
        }
        if (!result["hours_to_euth"].is_null()) {
            double hours = result["hours_to_euth"].as<double>(0);
            if (hours > 0) {
                event.hours_to_euthanasia = static_cast<int>(hours);
            }
        }

        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        // Non-critical, continue without time metrics
    }
}

} // namespace wtl::analytics
