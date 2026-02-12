/**
 * @file AnalyticsController.cc
 * @brief Implementation of AnalyticsController
 * @see AnalyticsController.h for documentation
 */

#include "controllers/AnalyticsController.h"

#include "analytics/AnalyticsService.h"
#include "analytics/AnalyticsWorker.h"
#include "analytics/ImpactTracker.h"
#include "analytics/MetricsAggregator.h"
#include "analytics/ReportGenerator.h"
#include "analytics/SocialAnalytics.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::controllers {

using namespace ::wtl::analytics;
using namespace ::wtl::core::debug;
using namespace drogon;

// Helper to create JSON response
static HttpResponsePtr jsonResponse(const Json::Value& data, HttpStatusCode status = k200OK) {
    auto resp = HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(status);
    return resp;
}

// Helper to create success response
static HttpResponsePtr successResponse(const Json::Value& data) {
    Json::Value response;
    response["success"] = true;
    response["data"] = data;
    return jsonResponse(response);
}

// Helper to create error response
static HttpResponsePtr errorResponse(const std::string& message, HttpStatusCode status = k400BadRequest) {
    Json::Value response;
    response["success"] = false;
    response["error"]["message"] = message;
    return jsonResponse(response, status);
}

// Helper to parse date range from request
static DateRange parseDateRange(const HttpRequestPtr& req) {
    auto start_date = req->getParameter("start_date");
    auto end_date = req->getParameter("end_date");
    auto range_param = req->getParameter("range");

    if (!range_param.empty()) {
        if (range_param == "today") return DateRange::today();
        if (range_param == "yesterday") return DateRange::yesterday();
        if (range_param == "week") return DateRange::lastWeek();
        if (range_param == "month") return DateRange::lastMonth();
        if (range_param == "year") return DateRange::lastYear();
    }

    return DateRange::fromParams(start_date, end_date);
}

// ============================================================================
// EVENT TRACKING HANDLERS
// ============================================================================

void AnalyticsController::trackEvent(const HttpRequestPtr& req,
                                      std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(errorResponse("Invalid JSON body"));
            return;
        }

        auto& service = AnalyticsService::getInstance();
        auto event = AnalyticsEvent::fromJson(*json);

        // Get session ID from cookie or header if not in body
        if (event.session_id.empty()) {
            auto session_cookie = req->getCookie("wtl_session");
            if (!session_cookie.empty()) {
                event.session_id = session_cookie;
            } else {
                auto session_header = req->getHeader("X-Session-ID");
                if (!session_header.empty()) {
                    event.session_id = session_header;
                }
            }
        }

        // Set source info
        event.source = "web";
        auto referrer = req->getHeader("Referer");
        if (!referrer.empty()) {
            event.referrer = referrer;
        }

        auto user_agent = req->getHeader("User-Agent");
        if (!user_agent.empty()) {
            // Parse user agent (simplified)
            if (user_agent.find("Mobile") != std::string::npos) {
                event.device_type = "mobile";
            } else if (user_agent.find("Tablet") != std::string::npos) {
                event.device_type = "tablet";
            } else {
                event.device_type = "desktop";
            }
        }

        std::string event_id = service.trackEvent(event);

        Json::Value response;
        response["event_id"] = event_id;
        callback(successResponse(response));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::VALIDATION,
            "Failed to track event: " + std::string(e.what()),
            {}
        );
        callback(errorResponse("Failed to track event: " + std::string(e.what())));
    }
}

void AnalyticsController::trackPageView(const HttpRequestPtr& req,
                                         std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto json = req->getJsonObject();
        if (!json) {
            callback(errorResponse("Invalid JSON body"));
            return;
        }

        auto& service = AnalyticsService::getInstance();

        std::string page = json->get("page", "").asString();
        std::string session_id = json->get("session_id", "").asString();

        std::optional<std::string> user_id;
        if (json->isMember("user_id") && !(*json)["user_id"].isNull()) {
            user_id = (*json)["user_id"].asString();
        }

        std::optional<std::string> referrer;
        auto ref_header = req->getHeader("Referer");
        if (!ref_header.empty()) {
            referrer = ref_header;
        } else if (json->isMember("referrer") && !(*json)["referrer"].isNull()) {
            referrer = (*json)["referrer"].asString();
        }

        std::string event_id = service.trackPageView(page, user_id, session_id, referrer);

        Json::Value response;
        response["event_id"] = event_id;
        callback(successResponse(response));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to track page view: " + std::string(e.what())));
    }
}

void AnalyticsController::trackBatch(const HttpRequestPtr& req,
                                      std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("events") || !(*json)["events"].isArray()) {
            callback(errorResponse("Invalid JSON body - expected events array"));
            return;
        }

        auto& service = AnalyticsService::getInstance();
        auto& events = (*json)["events"];

        Json::Value event_ids(Json::arrayValue);
        int tracked = 0;
        int failed = 0;

        for (const auto& event_json : events) {
            try {
                auto event = AnalyticsEvent::fromJson(event_json);
                std::string event_id = service.trackEvent(event);
                event_ids.append(event_id);
                tracked++;
            } catch (...) {
                failed++;
            }
        }

        Json::Value response;
        response["tracked"] = tracked;
        response["failed"] = failed;
        response["event_ids"] = event_ids;
        callback(successResponse(response));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to track batch: " + std::string(e.what())));
    }
}

// ============================================================================
// DASHBOARD HANDLERS
// ============================================================================

void AnalyticsController::getDashboard(const HttpRequestPtr& req,
                                        std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto range = parseDateRange(req);
        auto& service = AnalyticsService::getInstance();
        auto stats = service.getDashboardStats(range);
        callback(successResponse(stats.toJson()));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get dashboard: " + std::string(e.what())));
    }
}

void AnalyticsController::getRealTimeStats(const HttpRequestPtr& req,
                                            std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto& service = AnalyticsService::getInstance();
        auto stats = service.getRealTimeStats();
        callback(successResponse(stats.toJson()));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get real-time stats: " + std::string(e.what())));
    }
}

void AnalyticsController::getTrends(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto range = parseDateRange(req);
        auto& service = AnalyticsService::getInstance();
        auto trends = service.getTrendStats(range);
        callback(successResponse(trends.toJson()));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get trends: " + std::string(e.what())));
    }
}

// ============================================================================
// ENTITY STATISTICS HANDLERS
// ============================================================================

void AnalyticsController::getDogStats(const HttpRequestPtr& req,
                                       std::function<void(const HttpResponsePtr&)>&& callback,
                                       const std::string& id)
{
    try {
        auto range = parseDateRange(req);
        auto& service = AnalyticsService::getInstance();
        auto stats = service.getDogStats(id, range);
        callback(successResponse(stats.toJson()));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get dog stats: " + std::string(e.what())));
    }
}

void AnalyticsController::getShelterStats(const HttpRequestPtr& req,
                                           std::function<void(const HttpResponsePtr&)>&& callback,
                                           const std::string& id)
{
    try {
        auto range = parseDateRange(req);
        auto& service = AnalyticsService::getInstance();
        auto stats = service.getShelterStats(id, range);
        callback(successResponse(stats.toJson()));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get shelter stats: " + std::string(e.what())));
    }
}

// ============================================================================
// SOCIAL ANALYTICS HANDLERS
// ============================================================================

void AnalyticsController::getSocialStats(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto range = parseDateRange(req);
        auto& service = AnalyticsService::getInstance();
        auto stats = service.getSocialStats("all", range);
        callback(successResponse(stats));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get social stats: " + std::string(e.what())));
    }
}

void AnalyticsController::getSocialPlatformStats(const HttpRequestPtr& req,
                                                  std::function<void(const HttpResponsePtr&)>&& callback,
                                                  const std::string& platform)
{
    try {
        auto range = parseDateRange(req);
        auto& service = AnalyticsService::getInstance();
        auto stats = service.getSocialStats(platform, range);
        callback(successResponse(stats));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get platform stats: " + std::string(e.what())));
    }
}

// ============================================================================
// IMPACT HANDLERS
// ============================================================================

void AnalyticsController::getImpactStats(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto range = parseDateRange(req);
        auto& service = AnalyticsService::getInstance();
        auto impact = service.getImpactStats(range);
        callback(successResponse(impact.toJson()));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get impact stats: " + std::string(e.what())));
    }
}

void AnalyticsController::getRecentImpact(const HttpRequestPtr& req,
                                           std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        int limit = std::stoi(req->getParameter("limit").empty() ? "20" : req->getParameter("limit"));
        auto& impact = ImpactTracker::getInstance();
        auto events = impact.getRecentImpactEvents(limit);

        Json::Value arr(Json::arrayValue);
        for (const auto& event : events) {
            arr.append(event.toJson());
        }

        callback(successResponse(arr));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get recent impact: " + std::string(e.what())));
    }
}

void AnalyticsController::getLivesSaved(const HttpRequestPtr& req,
                                         std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto& impact = ImpactTracker::getInstance();

        Json::Value response;
        response["total_all_time"] = impact.getTotalLivesSaved();

        auto this_month = impact.getLivesSaved(DateRange::lastDays(30));
        response["this_month"] = this_month;

        auto this_week = impact.getLivesSaved(DateRange::lastWeek());
        response["this_week"] = this_week;

        auto today = impact.getLivesSaved(DateRange::today());
        response["today"] = today;

        callback(successResponse(response));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get lives saved: " + std::string(e.what())));
    }
}

// ============================================================================
// REPORT HANDLERS
// ============================================================================

void AnalyticsController::generateReport(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback,
                                          const std::string& type)
{
    try {
        auto range = parseDateRange(req);
        auto format = req->getParameter("format");
        if (format.empty()) {
            format = "json";
        }

        auto& generator = ReportGenerator::getInstance();
        std::string report = generator.generateReport(type, range, format);

        if (format == "csv") {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(report);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            resp->addHeader("Content-Disposition", "attachment; filename=\"report.csv\"");
            callback(resp);
        } else if (format == "text") {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(report);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        } else {
            // JSON
            Json::CharReaderBuilder builder;
            Json::Value json;
            std::istringstream stream(report);
            std::string errors;
            if (Json::parseFromStream(builder, stream, &json, &errors)) {
                callback(successResponse(json));
            } else {
                callback(errorResponse("Failed to parse report JSON"));
            }
        }

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to generate report: " + std::string(e.what())));
    }
}

void AnalyticsController::getReportTypes(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto& generator = ReportGenerator::getInstance();
        auto types = generator.getAvailableReportTypes();

        Json::Value arr(Json::arrayValue);
        for (const auto& type : types) {
            Json::Value report_meta = generator.getReportMetadata(type);
            arr.append(report_meta);
        }

        callback(successResponse(arr));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get report types: " + std::string(e.what())));
    }
}

// ============================================================================
// TOP PERFORMERS HANDLERS
// ============================================================================

void AnalyticsController::getTopDogs(const HttpRequestPtr& req,
                                      std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto range = parseDateRange(req);
        auto metric = req->getParameter("metric");
        if (metric.empty()) metric = "views";

        int limit = std::stoi(req->getParameter("limit").empty() ? "10" : req->getParameter("limit"));

        auto& service = AnalyticsService::getInstance();
        std::vector<TopPerformer> performers;

        if (metric == "views") {
            performers = service.getMostViewedDogs(range, limit);
        } else if (metric == "shares") {
            performers = service.getMostSharedDogs(range, limit);
        } else {
            performers = service.getMostViewedDogs(range, limit);
        }

        Json::Value arr(Json::arrayValue);
        for (const auto& p : performers) {
            arr.append(p.toJson());
        }

        callback(successResponse(arr));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get top dogs: " + std::string(e.what())));
    }
}

void AnalyticsController::getTopShelters(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto range = parseDateRange(req);
        int limit = std::stoi(req->getParameter("limit").empty() ? "10" : req->getParameter("limit"));

        auto& service = AnalyticsService::getInstance();
        auto performers = service.getTopSheltersByAdoptions(range, limit);

        Json::Value arr(Json::arrayValue);
        for (const auto& p : performers) {
            arr.append(p.toJson());
        }

        callback(successResponse(arr));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get top shelters: " + std::string(e.what())));
    }
}

void AnalyticsController::getTopContent(const HttpRequestPtr& req,
                                         std::function<void(const HttpResponsePtr&)>&& callback)
{
    try {
        auto range = parseDateRange(req);
        auto metric = req->getParameter("metric");
        if (metric.empty()) metric = "views";

        int limit = std::stoi(req->getParameter("limit").empty() ? "10" : req->getParameter("limit"));

        auto& social = SocialAnalytics::getInstance();
        std::vector<ContentPerformanceRanking> rankings;

        if (metric == "views") {
            rankings = social.getTopByViews(range, limit);
        } else if (metric == "engagement") {
            rankings = social.getTopByEngagement(range, limit);
        } else if (metric == "adoptions") {
            rankings = social.getTopByAdoptions(range, limit);
        } else {
            rankings = social.getTopByViews(range, limit);
        }

        Json::Value arr(Json::arrayValue);
        for (const auto& r : rankings) {
            arr.append(r.toJson());
        }

        callback(successResponse(arr));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get top content: " + std::string(e.what())));
    }
}

// ============================================================================
// ADMIN HANDLERS
// ============================================================================

void AnalyticsController::getWorkerStatus(const HttpRequestPtr& req,
                                           std::function<void(const HttpResponsePtr&)>&& callback)
{
    // Note: In production, add REQUIRE_ADMIN check here
    try {
        auto& worker = AnalyticsWorker::getInstance();
        auto status = worker.getStatusJson();

        Json::Value response;
        response["status"] = status;
        response["config"] = worker.getConfig().toJson();

        callback(successResponse(response));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get worker status: " + std::string(e.what())));
    }
}

void AnalyticsController::triggerAggregation(const HttpRequestPtr& req,
                                              std::function<void(const HttpResponsePtr&)>&& callback)
{
    // Note: In production, add REQUIRE_ADMIN check here
    try {
        auto& worker = AnalyticsWorker::getInstance();
        int aggregated = worker.triggerAggregation();

        Json::Value response;
        response["events_aggregated"] = aggregated;
        callback(successResponse(response));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to trigger aggregation: " + std::string(e.what())));
    }
}

void AnalyticsController::triggerCleanup(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback)
{
    // Note: In production, add REQUIRE_ADMIN check here
    try {
        auto& worker = AnalyticsWorker::getInstance();
        int cleaned = worker.triggerCleanup();

        Json::Value response;
        response["events_cleaned"] = cleaned;
        callback(successResponse(response));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to trigger cleanup: " + std::string(e.what())));
    }
}

void AnalyticsController::getEventsByEntity(const HttpRequestPtr& req,
                                             std::function<void(const HttpResponsePtr&)>&& callback,
                                             const std::string& entity_type,
                                             const std::string& entity_id)
{
    // Note: In production, add REQUIRE_ADMIN check here
    try {
        auto range = parseDateRange(req);
        auto& service = AnalyticsService::getInstance();
        auto events = service.getEventsByEntity(entity_type, entity_id, range);

        Json::Value arr(Json::arrayValue);
        for (const auto& event : events) {
            arr.append(event.toJson());
        }

        callback(successResponse(arr));

    } catch (const std::exception& e) {
        callback(errorResponse("Failed to get events by entity: " + std::string(e.what())));
    }
}

} // namespace wtl::controllers
