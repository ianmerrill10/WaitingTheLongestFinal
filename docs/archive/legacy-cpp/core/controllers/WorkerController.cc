/**
 * @file WorkerController.cc
 * @brief Implementation of WorkerController
 * @see WorkerController.h for documentation
 */

#include "core/controllers/WorkerController.h"

// Standard library includes
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

// Project includes
#include "core/utils/ApiResponse.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "workers/WorkerManager.h"
#include "workers/scheduler/Scheduler.h"
#include "workers/jobs/JobQueue.h"
#include "workers/HealthCheckWorker.h"

namespace wtl::core::controllers {

using namespace ::wtl::core::utils;
using namespace ::wtl::core::debug;
using namespace ::wtl::core::db;
using namespace ::wtl::workers;
using namespace ::wtl::workers::scheduler;
using namespace ::wtl::workers::jobs;

// ============================================================================
// WORKER ENDPOINTS
// ============================================================================

void WorkerController::listWorkers(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& manager = WorkerManager::getInstance();
        auto worker_infos = manager.getAllInfo();

        Json::Value workers_json(Json::arrayValue);

        for (const auto& info : worker_infos) {
            workers_json.append(info.toJson());
        }

        Json::Value data;
        data["workers"] = workers_json;
        data["total"] = static_cast<int>(workers_json.size());

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to list workers: " + std::string(e.what()),
            {{"endpoint", "/api/admin/workers"}, {"method", "GET"}}

        );
        callback(ApiResponse::serverError("Failed to list workers"));
    }
}

void WorkerController::getWorker(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& name
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& manager = WorkerManager::getInstance();

        if (!manager.hasWorker(name)) {
            callback(ApiResponse::notFound("Worker"));
            return;
        }

        auto info = manager.getWorkerInfo(name);
        callback(ApiResponse::success(info.toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get worker: " + std::string(e.what()),
            {{"endpoint", "/api/admin/workers/:name"}, {"method", "GET"}, {"name", name}}

        );
        callback(ApiResponse::serverError("Failed to get worker"));
    }
}

void WorkerController::startWorker(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& name
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& manager = WorkerManager::getInstance();

        if (!manager.hasWorker(name)) {
            callback(ApiResponse::notFound("Worker"));
            return;
        }

        if (!manager.startWorker(name)) {
            callback(ApiResponse::badRequest("Failed to start worker"));
            return;
        }

        Json::Value data;
        data["success"] = true;
        data["message"] = "Worker " + name + " started";
        data["status"] = workerStatusToString(manager.getWorkerStatus(name));

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to start worker: " + std::string(e.what()),
            {{"endpoint", "/api/admin/workers/:name/start"}, {"method", "POST"}, {"name", name}}

        );
        callback(ApiResponse::serverError("Failed to start worker"));
    }
}

void WorkerController::stopWorker(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& name
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& manager = WorkerManager::getInstance();

        if (!manager.hasWorker(name)) {
            callback(ApiResponse::notFound("Worker"));
            return;
        }

        if (!manager.stopWorker(name)) {
            callback(ApiResponse::badRequest("Failed to stop worker"));
            return;
        }

        Json::Value data;
        data["success"] = true;
        data["message"] = "Worker " + name + " stopped";
        data["status"] = workerStatusToString(manager.getWorkerStatus(name));

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to stop worker: " + std::string(e.what()),
            {{"endpoint", "/api/admin/workers/:name/stop"}, {"method", "POST"}, {"name", name}}

        );
        callback(ApiResponse::serverError("Failed to stop worker"));
    }
}

void WorkerController::pauseWorker(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& name
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& manager = WorkerManager::getInstance();

        if (!manager.hasWorker(name)) {
            callback(ApiResponse::notFound("Worker"));
            return;
        }

        if (!manager.pauseWorker(name)) {
            callback(ApiResponse::badRequest("Failed to pause worker"));
            return;
        }

        Json::Value data;
        data["success"] = true;
        data["message"] = "Worker " + name + " paused";
        data["status"] = workerStatusToString(manager.getWorkerStatus(name));

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to pause worker: " + std::string(e.what()),
            {{"endpoint", "/api/admin/workers/:name/pause"}, {"method", "POST"}, {"name", name}}

        );
        callback(ApiResponse::serverError("Failed to pause worker"));
    }
}

void WorkerController::resumeWorker(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& name
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& manager = WorkerManager::getInstance();

        if (!manager.hasWorker(name)) {
            callback(ApiResponse::notFound("Worker"));
            return;
        }

        if (!manager.resumeWorker(name)) {
            callback(ApiResponse::badRequest("Failed to resume worker"));
            return;
        }

        Json::Value data;
        data["success"] = true;
        data["message"] = "Worker " + name + " resumed";
        data["status"] = workerStatusToString(manager.getWorkerStatus(name));

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to resume worker: " + std::string(e.what()),
            {{"endpoint", "/api/admin/workers/:name/resume"}, {"method", "POST"}, {"name", name}}

        );
        callback(ApiResponse::serverError("Failed to resume worker"));
    }
}

void WorkerController::runWorkerNow(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& name
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& manager = WorkerManager::getInstance();

        if (!manager.hasWorker(name)) {
            callback(ApiResponse::notFound("Worker"));
            return;
        }

        // Request immediate execution via WorkerManager
        if (!manager.runWorkerNow(name)) {
            callback(ApiResponse::badRequest("Failed to trigger immediate execution"));
            return;
        }

        Json::Value data;
        data["success"] = true;
        data["message"] = "Immediate execution requested for " + name;

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to trigger worker: " + std::string(e.what()),
            {{"endpoint", "/api/admin/workers/:name/run"}, {"method", "POST"}, {"name", name}}

        );
        callback(ApiResponse::serverError("Failed to trigger worker"));
    }
}

// ============================================================================
// SCHEDULER ENDPOINTS
// ============================================================================

void WorkerController::listJobs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& scheduler = Scheduler::getInstance();
        auto jobs = scheduler.getScheduledJobs(false);

        Json::Value jobs_json(Json::arrayValue);

        for (const auto& job : jobs) {
            jobs_json.append(job.toJson());
        }

        Json::Value data;
        data["jobs"] = jobs_json;
        data["total"] = static_cast<int>(jobs_json.size());

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to list jobs: " + std::string(e.what()),
            {{"endpoint", "/api/admin/scheduler/jobs"}, {"method", "GET"}}

        );
        callback(ApiResponse::serverError("Failed to list jobs"));
    }
}

void WorkerController::createJob(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto body = req->getJsonObject();
        if (!body) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        std::string name = (*body)["name"].asString();
        std::string handler = (*body)["handler"].asString();

        if (name.empty() || handler.empty()) {
            callback(ApiResponse::badRequest("Name and handler are required"));
            return;
        }

        // Build the ScheduledJob from request body
        ScheduledJob job;
        job.name = name;
        job.job_type = handler;
        job.data = (*body).isMember("payload") ? (*body)["payload"] : Json::Value();

        auto& scheduler = Scheduler::getInstance();
        std::string job_id;

        if (body->isMember("cron_expression")) {
            std::string cron = (*body)["cron_expression"].asString();
            job_id = scheduler.scheduleRecurring(job, cron);
            if (job_id.empty()) {
                callback(ApiResponse::badRequest("Invalid cron expression"));
                return;
            }
        } else if (body->isMember("run_at")) {
            // Parse ISO timestamp
            std::string run_at_str = (*body)["run_at"].asString();
            std::tm tm = {};
            std::istringstream ss(run_at_str);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            auto run_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            job_id = scheduler.schedule(job, run_at);
        } else if (body->isMember("delay_seconds")) {
            int delay = (*body)["delay_seconds"].asInt();
            job_id = scheduler.scheduleDelayed(job, std::chrono::seconds(delay));
        } else {
            callback(ApiResponse::badRequest("Either cron_expression, run_at, or delay_seconds is required"));
            return;
        }

        Json::Value data;
        data["success"] = true;
        data["message"] = "Job scheduled: " + name;
        data["job_id"] = job_id;

        callback(ApiResponse::created(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to create job: " + std::string(e.what()),
            {{"endpoint", "/api/admin/scheduler/jobs"}, {"method", "POST"}}

        );
        callback(ApiResponse::serverError("Failed to create job"));
    }
}

void WorkerController::getJob(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& scheduler = Scheduler::getInstance();
        auto job = scheduler.getJob(id);

        if (!job) {
            callback(ApiResponse::notFound("Job"));
            return;
        }

        callback(ApiResponse::success(job->toJson()));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get job: " + std::string(e.what()),
            {{"endpoint", "/api/admin/scheduler/jobs/:id"}, {"method", "GET"}, {"id", id}}

        );
        callback(ApiResponse::serverError("Failed to get job"));
    }
}

void WorkerController::updateJob(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto body = req->getJsonObject();
        if (!body) {
            callback(ApiResponse::badRequest("Invalid JSON body"));
            return;
        }

        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Build update query dynamically
        std::vector<std::string> updates;

        if (body->isMember("enabled")) {
            updates.push_back("enabled = " + std::string((*body)["enabled"].asBool() ? "true" : "false"));
        }

        if (body->isMember("cron_expression")) {
            std::string cron = txn.esc((*body)["cron_expression"].asString());
            updates.push_back("cron_expression = '" + cron + "'");
        }

        if (body->isMember("payload")) {
            std::string payload_str = txn.esc((*body)["payload"].toStyledString());
            updates.push_back("payload = '" + payload_str + "'::jsonb");
        }

        if (updates.empty()) {
            callback(ApiResponse::badRequest("No fields to update"));
            return;
        }

        updates.push_back("updated_at = NOW()");

        // Build comma-separated update SET clause
        std::string set_clause;
        for (size_t i = 0; i < updates.size(); ++i) {
            if (i > 0) set_clause += ", ";
            set_clause += updates[i];
        }

        std::string update_sql = "UPDATE scheduled_jobs SET " + set_clause +
            " WHERE id = '" + txn.esc(id) + "' RETURNING *";

        auto result = txn.exec(update_sql);

        if (result.empty()) {
            txn.abort();
            ConnectionPool::getInstance().release(conn);
            callback(ApiResponse::notFound("Job"));
            return;
        }

        txn.commit();
        ConnectionPool::getInstance().release(conn);

        ScheduledJob job = ScheduledJob::fromDbRow(result[0]);

        Json::Value data;
        data["success"] = true;
        data["job"] = job.toJson();

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to update job: " + std::string(e.what()),
            {{"endpoint", "/api/admin/scheduler/jobs/:id"}, {"method", "PUT"}, {"id", id}}

        );
        callback(ApiResponse::serverError("Failed to update job"));
    }
}

void WorkerController::deleteJob(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& scheduler = Scheduler::getInstance();

        if (!scheduler.cancel(id)) {
            callback(ApiResponse::notFound("Job"));
            return;
        }

        Json::Value data;
        data["success"] = true;
        data["message"] = "Job deleted";

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to delete job: " + std::string(e.what()),
            {{"endpoint", "/api/admin/scheduler/jobs/:id"}, {"method", "DELETE"}, {"id", id}}

        );
        callback(ApiResponse::serverError("Failed to delete job"));
    }
}

void WorkerController::runJobNow(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& scheduler = Scheduler::getInstance();
        auto job_opt = scheduler.getJob(id);

        if (!job_opt) {
            callback(ApiResponse::notFound("Job"));
            return;
        }

        // Execute the job immediately via the scheduler
        auto exec_result = scheduler.executeJobNow(id);

        Json::Value data;
        data["success"] = exec_result.success;
        data["message"] = exec_result.success
            ? "Job executed successfully"
            : "Job execution failed: " + exec_result.message;
        data["result"] = exec_result.toJson();

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to run job: " + std::string(e.what()),
            {{"endpoint", "/api/admin/scheduler/jobs/:id/run"}, {"method", "POST"}, {"id", id}}

        );
        callback(ApiResponse::serverError("Failed to run job"));
    }
}

// ============================================================================
// HEALTH ENDPOINTS
// ============================================================================

void WorkerController::getHealth(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    // Health endpoint is public for monitoring
    try {
        auto& manager = WorkerManager::getInstance();

        // Check if the health check worker is registered
        if (!manager.hasWorker("health_check_worker")) {
            // Health worker not available, return basic manager health
            Json::Value data = manager.getHealthStatus();
            callback(ApiResponse::success(data));
            return;
        }

        // Get health status from the worker manager
        Json::Value data = manager.getHealthStatus();
        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get health: " + std::string(e.what()),
            {{"endpoint", "/api/admin/health"}, {"method", "GET"}}

        );
        callback(ApiResponse::serverError("Failed to get health status"));
    }
}

void WorkerController::getHealthHistory(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        int limit = 100;
        auto limit_param = req->getParameter("limit");
        if (!limit_param.empty()) {
            limit = std::stoi(limit_param);
            limit = std::min(limit, 1000);
        }

        auto result = txn.exec_params(R"(
            SELECT overall_status, healthy_count, degraded_count, unhealthy_count,
                   check_time, details
            FROM health_metrics
            ORDER BY check_time DESC
            LIMIT $1
        )", limit);

        txn.abort();
        ConnectionPool::getInstance().release(conn);

        Json::Value history(Json::arrayValue);

        for (const auto& row : result) {
            Json::Value entry;
            entry["overall_status"] = row["overall_status"].as<std::string>();
            entry["healthy_count"] = row["healthy_count"].as<int>();
            entry["degraded_count"] = row["degraded_count"].as<int>();
            entry["unhealthy_count"] = row["unhealthy_count"].as<int>();
            entry["check_time"] = row["check_time"].as<std::string>();

            std::string details_str = row["details"].as<std::string>("");
            if (!details_str.empty()) {
                Json::CharReaderBuilder reader_builder;
                std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
                Json::Value details;
                std::string errs;
                if (reader->parse(details_str.c_str(),
                                  details_str.c_str() + details_str.size(),
                                  &details, &errs)) {
                    entry["details"] = details;
                }
            }

            history.append(entry);
        }

        Json::Value data;
        data["history"] = history;
        data["total"] = static_cast<int>(history.size());

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get health history: " + std::string(e.what()),
            {{"endpoint", "/api/admin/health/history"}, {"method", "GET"}}

        );
        callback(ApiResponse::serverError("Failed to get health history"));
    }
}

// ============================================================================
// JOB QUEUE ENDPOINTS
// ============================================================================

void WorkerController::getQueueStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& queue = JobQueue::getInstance();
        auto queue_stats = queue.getStats();

        Json::Value data = queue_stats.toJson();

        // Get processing stats from database
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(R"(
            SELECT
                COUNT(*) FILTER (WHERE status = 'completed') as completed,
                COUNT(*) FILTER (WHERE status = 'failed') as failed,
                COUNT(*) FILTER (WHERE status = 'processing') as processing,
                AVG(EXTRACT(EPOCH FROM (completed_at - started_at))) FILTER (WHERE status = 'completed') as avg_duration
            FROM job_history
            WHERE created_at > NOW() - INTERVAL '24 hours'
        )");

        txn.abort();
        ConnectionPool::getInstance().release(conn);

        if (!result.empty()) {
            Json::Value last_24h;
            last_24h["completed"] = result[0]["completed"].as<int>(0);
            last_24h["failed"] = result[0]["failed"].as<int>(0);
            last_24h["processing"] = result[0]["processing"].as<int>(0);
            last_24h["avg_duration_seconds"] = result[0]["avg_duration"].is_null()
                ? 0.0 : result[0]["avg_duration"].as<double>();
            data["last_24h"] = last_24h;
        }

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get queue stats: " + std::string(e.what()),
            {{"endpoint", "/api/admin/jobs/queue"}, {"method", "GET"}}

        );
        callback(ApiResponse::serverError("Failed to get queue stats"));
    }
}

void WorkerController::getDeadLetterQueue(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& queue = JobQueue::getInstance();
        auto dead_jobs = queue.getDeadLetterJobs();

        Json::Value jobs_json(Json::arrayValue);

        for (const auto& job : dead_jobs) {
            jobs_json.append(job.toJson());
        }

        Json::Value data;
        data["jobs"] = jobs_json;
        data["total"] = static_cast<int>(dead_jobs.size());

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to get dead letter queue: " + std::string(e.what()),
            {{"endpoint", "/api/admin/jobs/dead-letter"}, {"method", "GET"}}

        );
        callback(ApiResponse::serverError("Failed to get dead letter queue"));
    }
}

void WorkerController::retryDeadLetterJob(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id
) {
    if (!requireAdminAuth(req)) {
        callback(ApiResponse::unauthorized());
        return;
    }

    try {
        auto& queue = JobQueue::getInstance();

        if (!queue.retry(id)) {
            callback(ApiResponse::notFound("Job"));
            return;
        }

        Json::Value data;
        data["success"] = true;
        data["message"] = "Job re-queued for processing";

        callback(ApiResponse::success(data));

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ErrorCategory::HTTP_REQUEST,
            "Failed to retry dead letter job: " + std::string(e.what()),
            {{"endpoint", "/api/admin/jobs/dead-letter/:id/retry"}, {"method", "POST"}, {"id", id}}

        );
        callback(ApiResponse::serverError("Failed to retry dead letter job"));
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool WorkerController::requireAdminAuth(const drogon::HttpRequestPtr& req) {
    // Check for API key in header
    auto api_key = req->getHeader("X-Admin-API-Key");
    if (!api_key.empty()) {
        // In production, validate against stored API keys
        return true;
    }

    // Check for admin session
    auto session = req->session();
    if (session) {
        try {
            if (session->get<bool>("is_admin")) {
                return true;
            }
        } catch (...) {
            // Session key not found or wrong type
        }
    }

    return false;
}

} // namespace wtl::core::controllers
