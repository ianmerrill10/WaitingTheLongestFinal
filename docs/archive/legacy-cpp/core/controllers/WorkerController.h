/**
 * @file WorkerController.h
 * @brief Admin API endpoints for worker and scheduler management
 *
 * PURPOSE:
 * Provides RESTful API endpoints for managing background workers,
 * scheduled jobs, and monitoring system health.
 *
 * ENDPOINTS:
 * GET  /api/admin/workers              - List all workers and status
 * GET  /api/admin/workers/:name        - Get specific worker details
 * POST /api/admin/workers/:name/start  - Start a worker
 * POST /api/admin/workers/:name/stop   - Stop a worker
 * POST /api/admin/workers/:name/pause  - Pause a worker
 * POST /api/admin/workers/:name/resume - Resume a worker
 * POST /api/admin/workers/:name/run    - Trigger immediate execution
 *
 * GET  /api/admin/scheduler/jobs       - List scheduled jobs
 * POST /api/admin/scheduler/jobs       - Create a new job
 * GET  /api/admin/scheduler/jobs/:id   - Get job details
 * PUT  /api/admin/scheduler/jobs/:id   - Update a job
 * DELETE /api/admin/scheduler/jobs/:id - Delete a job
 * POST /api/admin/scheduler/jobs/:id/run - Run job immediately
 *
 * GET  /api/admin/health               - System health status
 * GET  /api/admin/health/history       - Health check history
 *
 * @author Agent 18 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <string>

// Third-party includes
#include <drogon/HttpController.h>
#include <json/json.h>

namespace wtl::core::controllers {

/**
 * @class WorkerController
 * @brief Controller for worker and scheduler admin API
 */
class WorkerController : public drogon::HttpController<WorkerController> {
public:
    METHOD_LIST_BEGIN
    // Worker endpoints
    ADD_METHOD_TO(WorkerController::listWorkers, "/api/admin/workers", drogon::Get);
    ADD_METHOD_TO(WorkerController::getWorker, "/api/admin/workers/{name}", drogon::Get);
    ADD_METHOD_TO(WorkerController::startWorker, "/api/admin/workers/{name}/start", drogon::Post);
    ADD_METHOD_TO(WorkerController::stopWorker, "/api/admin/workers/{name}/stop", drogon::Post);
    ADD_METHOD_TO(WorkerController::pauseWorker, "/api/admin/workers/{name}/pause", drogon::Post);
    ADD_METHOD_TO(WorkerController::resumeWorker, "/api/admin/workers/{name}/resume", drogon::Post);
    ADD_METHOD_TO(WorkerController::runWorkerNow, "/api/admin/workers/{name}/run", drogon::Post);

    // Scheduler endpoints
    ADD_METHOD_TO(WorkerController::listJobs, "/api/admin/scheduler/jobs", drogon::Get);
    ADD_METHOD_TO(WorkerController::createJob, "/api/admin/scheduler/jobs", drogon::Post);
    ADD_METHOD_TO(WorkerController::getJob, "/api/admin/scheduler/jobs/{id}", drogon::Get);
    ADD_METHOD_TO(WorkerController::updateJob, "/api/admin/scheduler/jobs/{id}", drogon::Put);
    ADD_METHOD_TO(WorkerController::deleteJob, "/api/admin/scheduler/jobs/{id}", drogon::Delete);
    ADD_METHOD_TO(WorkerController::runJobNow, "/api/admin/scheduler/jobs/{id}/run", drogon::Post);

    // Health endpoints
    ADD_METHOD_TO(WorkerController::getHealth, "/api/admin/health", drogon::Get);
    ADD_METHOD_TO(WorkerController::getHealthHistory, "/api/admin/health/history", drogon::Get);

    // Job queue endpoints
    ADD_METHOD_TO(WorkerController::getQueueStats, "/api/admin/jobs/queue", drogon::Get);
    ADD_METHOD_TO(WorkerController::getDeadLetterQueue, "/api/admin/jobs/dead-letter", drogon::Get);
    ADD_METHOD_TO(WorkerController::retryDeadLetterJob, "/api/admin/jobs/dead-letter/{id}/retry", drogon::Post);
    METHOD_LIST_END

    // Worker management
    void listWorkers(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getWorker(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& name);

    void startWorker(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& name);

    void stopWorker(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& name);

    void pauseWorker(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& name);

    void resumeWorker(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& name);

    void runWorkerNow(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& name);

    // Scheduler management
    void listJobs(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void createJob(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getJob(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
               const std::string& id);

    void updateJob(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& id);

    void deleteJob(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& id);

    void runJobNow(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& id);

    // Health management
    void getHealth(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getHealthHistory(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    // Job queue management
    void getQueueStats(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getDeadLetterQueue(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void retryDeadLetterJob(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                           const std::string& id);

private:
    bool requireAdminAuth(const drogon::HttpRequestPtr& req);
};

} // namespace wtl::core::controllers
