/**
 * @file AdminService.cc
 * @brief Implementation of AdminService
 * @see AdminService.h for documentation
 */

#include "admin/AdminService.h"

// Standard library includes
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

// Project includes
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/debug/HealthCheck.h"
#include "core/services/DogService.h"
#include "core/services/ShelterService.h"
#include "core/services/UserService.h"
#include "core/services/UrgencyWorker.h"

namespace wtl::admin {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;
using namespace ::wtl::core::services;

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

AdminService& AdminService::getInstance() {
    static AdminService instance;
    return instance;
}

AdminService::AdminService() = default;
AdminService::~AdminService() = default;

// ============================================================================
// DASHBOARD STATISTICS
// ============================================================================

Json::Value AdminService::getDashboardStats() {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Total counts
        stats["counts"]["total_dogs"] = getTableCount("dogs");
        stats["counts"]["available_dogs"] = getTableCount("dogs", "status = 'available'");
        stats["counts"]["total_shelters"] = getTableCount("shelters");
        stats["counts"]["verified_shelters"] = getTableCount("shelters", "is_verified = true");
        stats["counts"]["kill_shelters"] = getTableCount("shelters", "is_kill_shelter = true");
        stats["counts"]["total_users"] = getTableCount("users");
        stats["counts"]["foster_homes"] = getTableCount("foster_homes");
        stats["counts"]["active_placements"] = getTableCount("foster_placements", "status = 'active'");

        // Active users
        auto active_users = getActiveUsers("24h");
        stats["active_users"] = active_users;

        // Urgency overview
        auto urgency = getUrgencyOverview();
        stats["urgency"] = urgency;

        // Recent activity
        Json::Value activity(Json::arrayValue);

        // Get recent dogs added
        auto result = txn.exec(
            "SELECT id, name, 'dog_added' as action, created_at "
            "FROM dogs ORDER BY created_at DESC LIMIT 5"
        );

        for (const auto& row : result) {
            Json::Value item;
            item["type"] = "dog_added";
            item["entity_id"] = row["id"].c_str();
            item["entity_name"] = row["name"].c_str();
            item["timestamp"] = row["created_at"].c_str();
            activity.append(item);
        }

        // Get recent adoptions
        result = txn.exec(
            "SELECT id, name, 'dog_adopted' as action, updated_at "
            "FROM dogs WHERE status = 'adopted' "
            "ORDER BY updated_at DESC LIMIT 5"
        );

        for (const auto& row : result) {
            Json::Value item;
            item["type"] = "dog_adopted";
            item["entity_id"] = row["id"].c_str();
            item["entity_name"] = row["name"].c_str();
            item["timestamp"] = row["updated_at"].c_str();
            activity.append(item);
        }

        stats["recent_activity"] = activity;

        // System health summary
        auto health = getSystemHealth();
        stats["system_health"]["status"] = health["status"];
        stats["system_health"]["database"] = health["database"]["status"];
        stats["system_health"]["workers"] = health["workers"]["healthy_count"];

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get dashboard stats: " + std::string(e.what()),
                         {});

        // Return minimal stats on error
        stats["error"] = "Failed to retrieve complete statistics";
        stats["counts"]["total_dogs"] = 0;
        stats["counts"]["total_shelters"] = 0;
        stats["counts"]["total_users"] = 0;
    }

    return stats;
}

Json::Value AdminService::getSystemHealth() {
    Json::Value health;

    try {
        auto& health_check = HealthCheck::getInstance();
        auto full_health = health_check.runAllChecks();

        // Overall status
        bool all_healthy = true;
        for (const auto& check : full_health.checks) {
            if (!check.isHealthy()) {
                all_healthy = false;
                break;
            }
        }

        health["status"] = all_healthy ? "healthy" : "degraded";
        health["timestamp"] = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        // Database health
        auto& pool = ConnectionPool::getInstance();
        Json::Value db_health;
        db_health["status"] = pool.testConnectivity() ? "healthy" : "unhealthy";
        db_health["pool_stats"] = pool.getStats();
        health["database"] = db_health;

        // Worker health
        Json::Value workers_health;
        auto workers = getWorkerStatus();
        int healthy_count = 0;
        int total_count = workers.size();

        for (const auto& worker : workers) {
            if (worker["status"].asString() == "running") {
                healthy_count++;
            }
        }

        workers_health["healthy_count"] = healthy_count;
        workers_health["total_count"] = total_count;
        workers_health["workers"] = workers;
        health["workers"] = workers_health;

        // Error rates
        auto& error_capture = ErrorCapture::getInstance();
        auto error_stats = error_capture.getStats();
        health["errors"] = error_stats;

        // Circuit breakers
        auto& self_healing = SelfHealing::getInstance();
        Json::Value cb_states;
        for (const auto& [name, state] : self_healing.getAllCircuitStates()) {
            cb_states[name] = state.toJson();
        }
        health["circuit_breakers"] = cb_states;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get system health: " + std::string(e.what()),
                         {});

        health["status"] = "error";
        health["error"] = e.what();
    }

    return health;
}

Json::Value AdminService::getActiveUsers(const std::string& timeframe) {
    Json::Value result;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string time_condition = getTimeframeTimestamp(timeframe);

        // Active users count
        auto query_result = txn.exec_params(
            "SELECT COUNT(DISTINCT user_id) as count FROM sessions "
            "WHERE last_used_at > $1",
            time_condition
        );

        result["active_users"] = query_result[0]["count"].as<int>();

        // New registrations
        query_result = txn.exec_params(
            "SELECT COUNT(*) as count FROM users WHERE created_at > $1",
            time_condition
        );

        result["new_registrations"] = query_result[0]["count"].as<int>();

        // Users by role
        query_result = txn.exec(
            "SELECT role, COUNT(*) as count FROM users "
            "WHERE is_active = true GROUP BY role"
        );

        Json::Value by_role;
        for (const auto& row : query_result) {
            by_role[row["role"].c_str()] = row["count"].as<int>();
        }
        result["by_role"] = by_role;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get active users: " + std::string(e.what()),
                         {{"timeframe", timeframe}});

        result["active_users"] = 0;
        result["new_registrations"] = 0;
    }

    return result;
}

Json::Value AdminService::getContentStats() {
    Json::Value stats;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Content counts by type and status
        stats["tiktok_posts"]["total"] = 0;
        stats["tiktok_posts"]["published"] = 0;
        stats["tiktok_posts"]["pending"] = 0;

        stats["blog_posts"]["total"] = 0;
        stats["blog_posts"]["published"] = 0;
        stats["blog_posts"]["draft"] = 0;

        stats["social_posts"]["total"] = 0;
        stats["social_posts"]["scheduled"] = 0;
        stats["social_posts"]["posted"] = 0;

        // Pending moderation queue count
        stats["pending_moderation"] = 0;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get content stats: " + std::string(e.what()),
                         {});
    }

    return stats;
}

Json::Value AdminService::getSocialStats() {
    Json::Value stats;

    try {
        // Social media engagement metrics
        stats["total_posts"] = 0;
        stats["total_engagement"] = 0;
        stats["avg_engagement_rate"] = 0.0;

        // Platform breakdown
        Json::Value platforms;
        platforms["tiktok"]["posts"] = 0;
        platforms["tiktok"]["views"] = 0;
        platforms["tiktok"]["likes"] = 0;

        platforms["instagram"]["posts"] = 0;
        platforms["instagram"]["reach"] = 0;
        platforms["instagram"]["engagement"] = 0;

        platforms["facebook"]["posts"] = 0;
        platforms["facebook"]["reach"] = 0;
        platforms["facebook"]["engagement"] = 0;

        stats["platforms"] = platforms;

        // Top performing posts
        Json::Value top_posts(Json::arrayValue);
        stats["top_posts"] = top_posts;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get social stats: " + std::string(e.what()),
                         {});
    }

    return stats;
}

Json::Value AdminService::getUrgencyOverview() {
    Json::Value overview;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Count by urgency level
        auto result = txn.exec(
            "SELECT urgency_level, COUNT(*) as count "
            "FROM dogs WHERE status = 'available' "
            "GROUP BY urgency_level"
        );

        overview["critical"] = 0;
        overview["high"] = 0;
        overview["medium"] = 0;
        overview["normal"] = 0;

        for (const auto& row : result) {
            std::string level = row["urgency_level"].c_str();
            int count = row["count"].as<int>();

            if (level == "CRITICAL" || level == "critical") {
                overview["critical"] = count;
            } else if (level == "HIGH" || level == "high") {
                overview["high"] = count;
            } else if (level == "MEDIUM" || level == "medium") {
                overview["medium"] = count;
            } else {
                overview["normal"] = overview["normal"].asInt() + count;
            }
        }

        // Calculate total urgent
        overview["total_urgent"] = overview["critical"].asInt() + overview["high"].asInt();

        // Dogs with upcoming euthanasia dates
        result = txn.exec(
            "SELECT COUNT(*) as count FROM dogs "
            "WHERE euthanasia_date IS NOT NULL "
            "AND euthanasia_date > NOW() "
            "AND status = 'available'"
        );

        overview["with_euthanasia_date"] = result[0]["count"].as<int>();

        // Get trend data (compare to 7 days ago)
        result = txn.exec(
            "SELECT COUNT(*) as count FROM dogs "
            "WHERE urgency_level IN ('CRITICAL', 'HIGH', 'critical', 'high') "
            "AND status = 'available' "
            "AND updated_at < NOW() - INTERVAL '7 days'"
        );

        int previous_urgent = result[0]["count"].as<int>();
        int current_urgent = overview["total_urgent"].asInt();

        overview["trend_percentage"] = calculatePercentageChange(current_urgent, previous_urgent);
        overview["trend_direction"] = current_urgent > previous_urgent ? "up" :
                                      (current_urgent < previous_urgent ? "down" : "stable");

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get urgency overview: " + std::string(e.what()),
                         {});

        overview["critical"] = 0;
        overview["high"] = 0;
        overview["medium"] = 0;
        overview["normal"] = 0;
        overview["total_urgent"] = 0;
    }

    return overview;
}

// ============================================================================
// USER MANAGEMENT
// ============================================================================

Json::Value AdminService::getUsers(int page, int per_page,
                                   const std::string& role,
                                   const std::string& search) {
    Json::Value result;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Build query
        std::string base_query = "SELECT * FROM users";
        std::string count_query = "SELECT COUNT(*) FROM users";
        std::string where_clause = " WHERE 1=1";

        std::vector<std::string> params;
        int param_index = 1;

        if (!role.empty()) {
            where_clause += " AND role = $" + std::to_string(param_index++);
            params.push_back(role);
        }

        if (!search.empty()) {
            where_clause += " AND (display_name ILIKE $" + std::to_string(param_index) +
                           " OR email ILIKE $" + std::to_string(param_index) + ")";
            params.push_back("%" + search + "%");
            param_index++;
        }

        // Get total count
        auto count_result = txn.exec(count_query + where_clause);
        result["total"] = count_result[0][0].as<int>();

        // Get users with pagination
        int offset = (page - 1) * per_page;
        std::string full_query = base_query + where_clause +
                                " ORDER BY created_at DESC" +
                                " LIMIT " + std::to_string(per_page) +
                                " OFFSET " + std::to_string(offset);

        auto users_result = txn.exec(full_query);

        Json::Value users_array(Json::arrayValue);
        for (const auto& row : users_result) {
            Json::Value user;
            user["id"] = row["id"].c_str();
            user["email"] = row["email"].c_str();
            user["display_name"] = row["display_name"].c_str();
            user["role"] = row["role"].c_str();
            user["is_active"] = row["is_active"].as<bool>();
            user["email_verified"] = row["email_verified"].as<bool>();
            user["created_at"] = row["created_at"].c_str();
            user["last_login_at"] = row["last_login_at"].is_null() ? "" : row["last_login_at"].c_str();
            users_array.append(user);
        }

        result["users"] = users_array;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get users: " + std::string(e.what()),
                         {{"page", std::to_string(page)}, {"role", role}});

        result["total"] = 0;
        result["users"] = Json::Value(Json::arrayValue);
    }

    return result;
}

// ============================================================================
// ANALYTICS
// ============================================================================

Json::Value AdminService::getAnalytics(const std::string& period) {
    Json::Value analytics;

    try {
        // Determine date range based on period
        std::string interval;
        if (period == "day") interval = "1 day";
        else if (period == "week") interval = "7 days";
        else if (period == "month") interval = "30 days";
        else if (period == "year") interval = "365 days";
        else interval = "7 days";

        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Dog statistics
        Json::Value dogs;
        auto result = txn.exec_params(
            "SELECT COUNT(*) as count FROM dogs WHERE created_at > NOW() - INTERVAL '" + interval + "'"
        );
        dogs["new_dogs"] = result[0]["count"].as<int>();

        result = txn.exec_params(
            "SELECT COUNT(*) as count FROM dogs WHERE status = 'adopted' AND updated_at > NOW() - INTERVAL '" + interval + "'"
        );
        dogs["adoptions"] = result[0]["count"].as<int>();

        result = txn.exec_params(
            "SELECT COUNT(*) as count FROM dogs WHERE status = 'fostered' AND updated_at > NOW() - INTERVAL '" + interval + "'"
        );
        dogs["fosters"] = result[0]["count"].as<int>();

        analytics["dogs"] = dogs;

        // User statistics
        Json::Value users;
        result = txn.exec_params(
            "SELECT COUNT(*) as count FROM users WHERE created_at > NOW() - INTERVAL '" + interval + "'"
        );
        users["new_users"] = result[0]["count"].as<int>();

        result = txn.exec_params(
            "SELECT COUNT(*) as count FROM foster_homes WHERE created_at > NOW() - INTERVAL '" + interval + "'"
        );
        users["new_fosters"] = result[0]["count"].as<int>();

        analytics["users"] = users;

        // Shelter statistics
        Json::Value shelters;
        result = txn.exec_params(
            "SELECT COUNT(*) as count FROM shelters WHERE created_at > NOW() - INTERVAL '" + interval + "'"
        );
        shelters["new_shelters"] = result[0]["count"].as<int>();

        analytics["shelters"] = shelters;

        // Period info
        analytics["period"] = period;
        analytics["interval"] = interval;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get analytics: " + std::string(e.what()),
                         {{"period", period}});

        analytics["error"] = "Failed to retrieve analytics";
    }

    return analytics;
}

Json::Value AdminService::getTrafficStats() {
    Json::Value stats;

    // Traffic stats would typically come from an analytics service
    // For now, return placeholder structure
    stats["page_views"]["today"] = 0;
    stats["page_views"]["week"] = 0;
    stats["page_views"]["month"] = 0;

    stats["unique_visitors"]["today"] = 0;
    stats["unique_visitors"]["week"] = 0;
    stats["unique_visitors"]["month"] = 0;

    stats["api_calls"]["today"] = 0;
    stats["api_calls"]["week"] = 0;
    stats["api_calls"]["month"] = 0;

    // Popular pages
    Json::Value popular(Json::arrayValue);
    Json::Value page1;
    page1["path"] = "/dogs";
    page1["views"] = 0;
    popular.append(page1);
    stats["popular_pages"] = popular;

    return stats;
}

Json::Value AdminService::getAdoptionFunnel() {
    Json::Value funnel;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Funnel stages (simplified - would need analytics tracking in production)
        funnel["stages"]["dog_views"] = 0;
        funnel["stages"]["added_to_favorites"] = getTableCount("favorites");
        funnel["stages"]["applications_submitted"] = getTableCount("foster_placements", "status != 'withdrawn'");
        funnel["stages"]["adoptions_completed"] = getTableCount("dogs", "status = 'adopted'");

        // Conversion rates
        int favorites = funnel["stages"]["added_to_favorites"].asInt();
        int applications = funnel["stages"]["applications_submitted"].asInt();
        int adoptions = funnel["stages"]["adoptions_completed"].asInt();

        funnel["conversion_rates"]["favorites_to_application"] =
            favorites > 0 ? (double)applications / favorites * 100 : 0;
        funnel["conversion_rates"]["application_to_adoption"] =
            applications > 0 ? (double)adoptions / applications * 100 : 0;

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get adoption funnel: " + std::string(e.what()),
                         {});
    }

    return funnel;
}

Json::Value AdminService::getImpactMetrics() {
    Json::Value impact;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Lives saved (dogs rescued from kill shelters)
        auto result = txn.exec(
            "SELECT COUNT(*) as count FROM dogs d "
            "JOIN shelters s ON d.shelter_id = s.id "
            "WHERE s.is_kill_shelter = true "
            "AND d.status IN ('adopted', 'fostered')"
        );
        impact["lives_saved"] = result[0]["count"].as<int>();

        // Total adoptions
        result = txn.exec(
            "SELECT COUNT(*) as count FROM dogs WHERE status = 'adopted'"
        );
        impact["total_adoptions"] = result[0]["count"].as<int>();

        // Total fosters
        result = txn.exec(
            "SELECT COUNT(*) as count FROM foster_placements WHERE status = 'completed'"
        );
        impact["total_foster_placements"] = result[0]["count"].as<int>();

        // Average wait time (in days)
        result = txn.exec(
            "SELECT AVG(EXTRACT(DAY FROM (updated_at - intake_date))) as avg_days "
            "FROM dogs WHERE status = 'adopted' AND intake_date IS NOT NULL"
        );
        impact["avg_wait_days"] = result[0]["avg_days"].is_null() ? 0.0 : result[0]["avg_days"].as<double>();

        // Critical dogs saved (had euthanasia date but were adopted/fostered)
        result = txn.exec(
            "SELECT COUNT(*) as count FROM dogs "
            "WHERE euthanasia_date IS NOT NULL "
            "AND status IN ('adopted', 'fostered')"
        );
        impact["critical_dogs_saved"] = result[0]["count"].as<int>();

        // States covered
        result = txn.exec(
            "SELECT COUNT(DISTINCT state_code) as count FROM shelters WHERE is_active = true"
        );
        impact["states_covered"] = result[0]["count"].as<int>();

        // Partner shelters
        impact["partner_shelters"] = getTableCount("shelters", "is_verified = true");

        txn.commit();
        ConnectionPool::getInstance().release(conn);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get impact metrics: " + std::string(e.what()),
                         {});

        impact["lives_saved"] = 0;
        impact["total_adoptions"] = 0;
    }

    return impact;
}

// ============================================================================
// CONTENT MANAGEMENT
// ============================================================================

Json::Value AdminService::getContentQueue() {
    Json::Value queue(Json::arrayValue);

    // Content moderation queue would be populated from a content table
    // Returning empty array as placeholder

    return queue;
}

bool AdminService::approveContent(const std::string& content_id) {
    try {
        // Would update content status in database
        // Returning true as placeholder
        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to approve content: " + std::string(e.what()),
                         {{"content_id", content_id}});
        return false;
    }
}

bool AdminService::rejectContent(const std::string& content_id, const std::string& reason) {
    try {
        // Would update content status in database
        // Returning true as placeholder
        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to reject content: " + std::string(e.what()),
                         {{"content_id", content_id}, {"reason", reason}});
        return false;
    }
}

// ============================================================================
// WORKER MANAGEMENT
// ============================================================================

Json::Value AdminService::getWorkerStatus() {
    Json::Value workers(Json::arrayValue);

    try {
        // Urgency worker
        Json::Value urgency_worker;
        urgency_worker["name"] = "urgency_worker";
        urgency_worker["description"] = "Recalculates dog urgency levels";
        urgency_worker["interval"] = "15 minutes";

        auto& uw = UrgencyWorker::getInstance();
        urgency_worker["status"] = uw.isRunning() ? "running" : "stopped";
        auto last_result = uw.getLastRunResult();
        urgency_worker["last_run"] = last_result ? last_result->toJson()["run_time"].asString() : "";
        urgency_worker["next_run"] = std::to_string(uw.getTimeUntilNextRun().count()) + "s";
        workers.append(urgency_worker);

        // Wait time worker
        Json::Value wait_time_worker;
        wait_time_worker["name"] = "wait_time_worker";
        wait_time_worker["description"] = "Updates wait time displays";
        wait_time_worker["interval"] = "1 second";
        wait_time_worker["status"] = "running"; // Assumed always running
        wait_time_worker["last_run"] = "";
        wait_time_worker["next_run"] = "";
        workers.append(wait_time_worker);

        // Data sync worker
        Json::Value sync_worker;
        sync_worker["name"] = "data_sync_worker";
        sync_worker["description"] = "Syncs data from external APIs";
        sync_worker["interval"] = "1 hour";
        sync_worker["status"] = "running";
        sync_worker["last_run"] = "";
        sync_worker["next_run"] = "";
        workers.append(sync_worker);

        // Cleanup worker
        Json::Value cleanup_worker;
        cleanup_worker["name"] = "cleanup_worker";
        cleanup_worker["description"] = "Cleans up old logs and sessions";
        cleanup_worker["interval"] = "24 hours";
        cleanup_worker["status"] = "running";
        cleanup_worker["last_run"] = "";
        cleanup_worker["next_run"] = "";
        workers.append(cleanup_worker);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to get worker status: " + std::string(e.what()),
                         {});
    }

    return workers;
}

bool AdminService::restartWorker(const std::string& worker_name) {
    try {
        if (worker_name == "urgency_worker") {
            auto& worker = UrgencyWorker::getInstance();
            worker.stop();
            worker.start();
            return true;
        }

        // Other workers would be handled similarly
        return false;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Failed to restart worker: " + std::string(e.what()),
                         {{"worker", worker_name}});
        return false;
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

int AdminService::getTableCount(const std::string& table_name,
                                const std::string& where_clause) {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        std::string query = "SELECT COUNT(*) FROM " + table_name;
        if (!where_clause.empty()) {
            query += " WHERE " + where_clause;
        }

        auto result = txn.exec(query);
        int count = result[0][0].as<int>();

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        return count;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::DATABASE,
                         "Failed to get table count: " + std::string(e.what()),
                         {{"table", table_name}});
        return 0;
    }
}

double AdminService::calculatePercentageChange(int current, int previous) {
    if (previous == 0) {
        return current > 0 ? 100.0 : 0.0;
    }
    return ((double)(current - previous) / previous) * 100.0;
}

std::string AdminService::getTimeframeTimestamp(const std::string& timeframe) {
    auto now = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point target;

    if (timeframe == "24h") {
        target = now - std::chrono::hours(24);
    } else if (timeframe == "7d") {
        target = now - std::chrono::hours(24 * 7);
    } else if (timeframe == "30d") {
        target = now - std::chrono::hours(24 * 30);
    } else {
        target = now - std::chrono::hours(24);
    }

    auto time_t_target = std::chrono::system_clock::to_time_t(target);
    std::tm tm = *std::gmtime(&time_t_target);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace wtl::admin
