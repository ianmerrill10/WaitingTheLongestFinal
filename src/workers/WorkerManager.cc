/**
 * @file WorkerManager.cc
 * @brief Implementation of WorkerManager class
 * @see WorkerManager.h for documentation
 */

#include "WorkerManager.h"

// Standard library includes
#include <algorithm>
#include <sstream>

// Project includes
#include "BaseWorker.h"
#include "core/debug/ErrorCapture.h"
#include "core/utils/Config.h"

namespace wtl::workers {

using namespace ::wtl::core::debug;

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

WorkerManager& WorkerManager::getInstance() {
    static WorkerManager instance;
    return instance;
}

WorkerManager::~WorkerManager() {
    if (initialized_.load()) {
        shutdown();
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void WorkerManager::initialize(const WorkerManagerConfig& config) {
    if (initialized_.load()) {
        log("WorkerManager already initialized");
        return;
    }

    config_ = config;
    manager_start_time_ = std::chrono::system_clock::now();

    // Create thread pool
    shutdown_requested_.store(false);
    for (int i = 0; i < config_.thread_pool_size; ++i) {
        thread_pool_.emplace_back(&WorkerManager::threadPoolWorker, this);
    }

    initialized_.store(true);
    log("WorkerManager initialized with " + std::to_string(config_.thread_pool_size) +
        " thread pool workers");
}

void WorkerManager::initializeFromConfig() {
    WorkerManagerConfig config;

    try {
        auto& cfg = wtl::core::utils::Config::getInstance();
        config.thread_pool_size = cfg.getInt("workers.thread_pool_size", 4);
        config.health_check_interval = std::chrono::seconds(
            cfg.getInt("workers.health_check_interval", 60));
        config.auto_restart_on_failure = cfg.getBool("workers.auto_restart_on_failure", true);
        config.max_restart_attempts = cfg.getInt("workers.max_restart_attempts", 3);
        config.restart_delay = std::chrono::seconds(
            cfg.getInt("workers.restart_delay", 10));
    } catch (const std::exception& e) {
        log("Using default config: " + std::string(e.what()));
    }

    initialize(config);
}

bool WorkerManager::isInitialized() const {
    return initialized_.load();
}

void WorkerManager::shutdown() {
    if (!initialized_.load()) {
        return;
    }

    log("WorkerManager shutting down...");

    // Stop all workers first
    stopAll();

    // Signal thread pool to stop
    shutdown_requested_.store(true);
    work_cv_.notify_all();

    // Wait for thread pool
    for (auto& thread : thread_pool_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    thread_pool_.clear();

    // Clear workers
    {
        std::unique_lock<std::shared_mutex> lock(workers_mutex_);
        workers_.clear();
    }

    initialized_.store(false);
    log("WorkerManager shutdown complete");
}

// ============================================================================
// WORKER REGISTRATION
// ============================================================================

bool WorkerManager::registerWorker(const std::string& name,
                                    std::unique_ptr<IWorker> worker,
                                    bool start_immediately) {
    return registerWorker(name, std::move(worker), worker->getInterval(), start_immediately);
}

bool WorkerManager::registerWorker(const std::string& name,
                                    std::unique_ptr<IWorker> worker,
                                    std::chrono::seconds interval,
                                    bool start_immediately) {
    if (!initialized_.load()) {
        logError("Cannot register worker - manager not initialized");
        return false;
    }

    if (!worker) {
        logError("Cannot register null worker");
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(workers_mutex_);

    // Check if already registered
    if (workers_.find(name) != workers_.end()) {
        logError("Worker '" + name + "' is already registered");
        return false;
    }

    // Create registered worker
    auto reg_worker = std::make_unique<RegisteredWorker>();
    reg_worker->worker = std::move(worker);
    reg_worker->info.name = name;
    reg_worker->info.description = reg_worker->worker->getDescription();
    reg_worker->info.priority = reg_worker->worker->getPriority();
    reg_worker->info.interval = interval;
    reg_worker->info.status = WorkerStatus::STOPPED;
    reg_worker->info.enabled = true;
    reg_worker->info.registered_at = std::chrono::system_clock::now();

    workers_[name] = std::move(reg_worker);
    total_workers_registered_++;

    log("Worker '" + name + "' registered successfully");

    if (start_immediately) {
        lock.unlock();
        startWorker(name);
    }

    return true;
}

bool WorkerManager::unregisterWorker(const std::string& name) {
    // First stop the worker
    stopWorker(name, true);

    std::unique_lock<std::shared_mutex> lock(workers_mutex_);

    auto it = workers_.find(name);
    if (it == workers_.end()) {
        logError("Worker '" + name + "' not found");
        return false;
    }

    workers_.erase(it);
    log("Worker '" + name + "' unregistered");

    return true;
}

bool WorkerManager::hasWorker(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    return workers_.find(name) != workers_.end();
}

std::vector<std::string> WorkerManager::getWorkerNames() const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    std::vector<std::string> names;
    names.reserve(workers_.size());

    for (const auto& [name, _] : workers_) {
        names.push_back(name);
    }

    return names;
}

// ============================================================================
// WORKER LIFECYCLE
// ============================================================================

void WorkerManager::startAll() {
    log("Starting all workers...");

    std::vector<std::string> names = getWorkerNames();
    for (const auto& name : names) {
        startWorker(name);
    }

    all_started_.store(true);
    log("All workers started (" + std::to_string(names.size()) + " total)");
}

void WorkerManager::stopAll(std::chrono::seconds timeout) {
    log("Stopping all workers...");

    all_started_.store(false);

    std::vector<std::string> names = getWorkerNames();
    for (const auto& name : names) {
        stopWorker(name, false);  // Don't wait individually
    }

    // Wait for all to stop with timeout
    auto start = std::chrono::steady_clock::now();
    bool all_stopped = false;

    while (!all_stopped) {
        all_stopped = true;

        {
            std::shared_lock<std::shared_mutex> lock(workers_mutex_);
            for (const auto& [name, worker] : workers_) {
                if (worker->running.load()) {
                    all_stopped = false;
                    break;
                }
            }
        }

        if (!all_stopped) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > timeout) {
                logError("Timeout waiting for workers to stop");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    log("All workers stopped");
}

bool WorkerManager::startWorker(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    auto* reg_worker = getRegisteredWorker(name);
    if (!reg_worker) {
        logError("Worker '" + name + "' not found");
        return false;
    }

    if (reg_worker->running.load()) {
        log("Worker '" + name + "' is already running");
        return true;
    }

    if (!reg_worker->info.enabled) {
        logError("Worker '" + name + "' is disabled");
        return false;
    }

    // Check if it's a BaseWorker - use its run loop
    auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
    if (base_worker) {
        base_worker->startRunLoop();
        reg_worker->running.store(true);
        reg_worker->info.status = WorkerStatus::RUNNING;
    } else {
        // For non-BaseWorker, create our own thread
        reg_worker->running.store(true);
        reg_worker->info.status = WorkerStatus::RUNNING;

        IWorker* worker_ptr = reg_worker->worker.get();
        auto interval = reg_worker->info.interval;
        auto* running_flag = &reg_worker->running;
        auto* status_ptr = &reg_worker->info.status;
        auto* stats_ptr = &reg_worker->info.stats;

        reg_worker->thread = std::make_unique<std::thread>([=]() {
            worker_ptr->onStart();

            while (running_flag->load()) {
                auto start_time = std::chrono::steady_clock::now();

                try {
                    if (worker_ptr->shouldRunNow()) {
                        auto result = worker_ptr->execute();

                        // Update stats
                        stats_ptr->total_executions++;
                        stats_ptr->last_run = std::chrono::system_clock::now();

                        if (result.success) {
                            stats_ptr->successful_executions++;
                            stats_ptr->consecutive_failures = 0;
                        } else {
                            stats_ptr->failed_executions++;
                            stats_ptr->consecutive_failures++;
                            stats_ptr->last_error = result.message;
                        }

                        stats_ptr->total_items_processed += result.items_processed;
                        stats_ptr->total_execution_time += result.execution_time;
                    }
                } catch (const std::exception& e) {
                    worker_ptr->onError(e);
                    stats_ptr->failed_executions++;
                    stats_ptr->consecutive_failures++;
                    stats_ptr->last_error = e.what();
                }

                stats_ptr->next_run = std::chrono::system_clock::now() + interval;

                // Wait for next interval
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                if (elapsed < interval) {
                    std::this_thread::sleep_for(interval - elapsed);
                }
            }

            worker_ptr->onStop();
            *status_ptr = WorkerStatus::STOPPED;
        });
    }

    total_workers_started_++;
    log("Worker '" + name + "' started");

    return true;
}

bool WorkerManager::stopWorker(const std::string& name, bool wait) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    auto* reg_worker = getRegisteredWorker(name);
    if (!reg_worker) {
        logError("Worker '" + name + "' not found");
        return false;
    }

    if (!reg_worker->running.load()) {
        return true;  // Already stopped
    }

    reg_worker->info.status = WorkerStatus::STOPPING;

    // Check if it's a BaseWorker
    auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
    if (base_worker) {
        base_worker->stopRunLoop(wait);
    } else if (reg_worker->thread) {
        reg_worker->running.store(false);
        if (wait && reg_worker->thread->joinable()) {
            lock.unlock();
            reg_worker->thread->join();
            lock.lock();
        }
    }

    reg_worker->running.store(false);
    reg_worker->info.status = WorkerStatus::STOPPED;
    total_workers_stopped_++;

    log("Worker '" + name + "' stopped");

    return true;
}

bool WorkerManager::pauseWorker(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    auto* reg_worker = getRegisteredWorker(name);
    if (!reg_worker) {
        return false;
    }

    auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
    if (base_worker) {
        base_worker->pause();
        reg_worker->info.status = WorkerStatus::PAUSED;
        return true;
    }

    logError("Worker '" + name + "' does not support pause");
    return false;
}

bool WorkerManager::resumeWorker(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    auto* reg_worker = getRegisteredWorker(name);
    if (!reg_worker) {
        return false;
    }

    auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
    if (base_worker) {
        base_worker->resume();
        reg_worker->info.status = WorkerStatus::RUNNING;
        return true;
    }

    logError("Worker '" + name + "' does not support resume");
    return false;
}

bool WorkerManager::restartWorker(const std::string& name) {
    if (!stopWorker(name, true)) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    return startWorker(name);
}

bool WorkerManager::runWorkerNow(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    auto* reg_worker = getRegisteredWorker(name);
    if (!reg_worker) {
        return false;
    }

    auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
    if (base_worker) {
        base_worker->requestImmediateExecution();
        log("Immediate execution requested for worker '" + name + "'");
        return true;
    }

    // For non-BaseWorker, execute directly in thread pool
    IWorker* worker_ptr = reg_worker->worker.get();
    enqueueWork([worker_ptr, name, this]() {
        try {
            worker_ptr->execute();
            log("Immediate execution completed for worker '" + name + "'");
        } catch (const std::exception& e) {
            logError("Immediate execution failed for worker '" + name + "': " + e.what());
        }
    });

    return true;
}

// ============================================================================
// WORKER STATUS
// ============================================================================

WorkerStatus WorkerManager::getWorkerStatus(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    const auto* reg_worker = getRegisteredWorker(name);
    if (!reg_worker) {
        return WorkerStatus::STOPPED;
    }

    // For BaseWorker, get actual status
    const auto* base_worker = dynamic_cast<const BaseWorker*>(reg_worker->worker.get());
    if (base_worker) {
        return base_worker->getStatus();
    }

    return reg_worker->info.status;
}

WorkerInfo WorkerManager::getWorkerInfo(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    const auto* reg_worker = getRegisteredWorker(name);
    if (!reg_worker) {
        return WorkerInfo{};
    }

    WorkerInfo info = reg_worker->info;

    // For BaseWorker, get current stats
    const auto* base_worker = dynamic_cast<const BaseWorker*>(reg_worker->worker.get());
    if (base_worker) {
        info.status = base_worker->getStatus();
        info.stats = base_worker->getStats();
        info.enabled = base_worker->isEnabled();
    }

    return info;
}

WorkerStats WorkerManager::getWorkerStats(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    const auto* reg_worker = getRegisteredWorker(name);
    if (!reg_worker) {
        return WorkerStats{};
    }

    const auto* base_worker = dynamic_cast<const BaseWorker*>(reg_worker->worker.get());
    if (base_worker) {
        return base_worker->getStats();
    }

    return reg_worker->info.stats;
}

Json::Value WorkerManager::getAllStatus() const {
    Json::Value status(Json::objectValue);

    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    for (const auto& [name, reg_worker] : workers_) {
        WorkerInfo info = reg_worker->info;

        // Get current status from BaseWorker if applicable
        const auto* base_worker = dynamic_cast<const BaseWorker*>(reg_worker->worker.get());
        if (base_worker) {
            info.status = base_worker->getStatus();
            info.stats = base_worker->getStats();
            info.enabled = base_worker->isEnabled();
        }

        status[name] = info.toJson();
    }

    return status;
}

std::vector<WorkerInfo> WorkerManager::getAllInfo() const {
    std::vector<WorkerInfo> infos;

    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    for (const auto& [name, reg_worker] : workers_) {
        WorkerInfo info = reg_worker->info;

        const auto* base_worker = dynamic_cast<const BaseWorker*>(reg_worker->worker.get());
        if (base_worker) {
            info.status = base_worker->getStatus();
            info.stats = base_worker->getStats();
            info.enabled = base_worker->isEnabled();
        }

        infos.push_back(info);
    }

    return infos;
}

// ============================================================================
// WORKER CONFIGURATION
// ============================================================================

void WorkerManager::enableWorker(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    auto* reg_worker = getRegisteredWorker(name);
    if (reg_worker) {
        reg_worker->info.enabled = true;

        auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
        if (base_worker) {
            base_worker->setEnabled(true);
        }
    }
}

void WorkerManager::disableWorker(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    auto* reg_worker = getRegisteredWorker(name);
    if (reg_worker) {
        reg_worker->info.enabled = false;

        auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
        if (base_worker) {
            base_worker->setEnabled(false);
        }
    }
}

void WorkerManager::setWorkerInterval(const std::string& name, std::chrono::seconds interval) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    auto* reg_worker = getRegisteredWorker(name);
    if (reg_worker) {
        reg_worker->info.interval = interval;

        auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
        if (base_worker) {
            base_worker->setInterval(interval);
        }
    }
}

// ============================================================================
// HEALTH MONITORING
// ============================================================================

bool WorkerManager::isHealthy() const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    for (const auto& [name, reg_worker] : workers_) {
        if (!reg_worker->worker->isHealthy()) {
            return false;
        }
    }

    return true;
}

Json::Value WorkerManager::getHealthStatus() const {
    Json::Value health(Json::objectValue);
    health["manager_healthy"] = isInitialized();

    Json::Value workers(Json::objectValue);

    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    for (const auto& [name, reg_worker] : workers_) {
        workers[name] = reg_worker->worker->getHealthDetails();
    }

    health["workers"] = workers;
    health["total_workers"] = static_cast<int>(workers_.size());
    health["unhealthy_count"] = static_cast<int>(getUnhealthyWorkers().size());

    return health;
}

std::vector<std::string> WorkerManager::getUnhealthyWorkers() const {
    std::vector<std::string> unhealthy;

    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    for (const auto& [name, reg_worker] : workers_) {
        if (!reg_worker->worker->isHealthy()) {
            unhealthy.push_back(name);
        }
    }

    return unhealthy;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value WorkerManager::getManagerStats() const {
    Json::Value stats(Json::objectValue);

    stats["initialized"] = initialized_.load();
    stats["all_started"] = all_started_.load();
    stats["thread_pool_size"] = config_.thread_pool_size;

    stats["total_workers_registered"] = static_cast<Json::UInt64>(total_workers_registered_.load());
    stats["total_workers_started"] = static_cast<Json::UInt64>(total_workers_started_.load());
    stats["total_workers_stopped"] = static_cast<Json::UInt64>(total_workers_stopped_.load());
    stats["total_worker_failures"] = static_cast<Json::UInt64>(total_worker_failures_.load());

    {
        std::shared_lock<std::shared_mutex> lock(workers_mutex_);
        stats["current_worker_count"] = static_cast<int>(workers_.size());

        int running = 0;
        int stopped = 0;
        int paused = 0;
        int error = 0;

        for (const auto& [name, reg_worker] : workers_) {
            const auto* base_worker = dynamic_cast<const BaseWorker*>(reg_worker->worker.get());
            WorkerStatus status = base_worker ? base_worker->getStatus() : reg_worker->info.status;

            switch (status) {
                case WorkerStatus::RUNNING:  running++;  break;
                case WorkerStatus::STOPPED:  stopped++;  break;
                case WorkerStatus::PAUSED:   paused++;   break;
                case WorkerStatus::ERROR:    error++;    break;
                default: break;
            }
        }

        stats["running_count"] = running;
        stats["stopped_count"] = stopped;
        stats["paused_count"] = paused;
        stats["error_count"] = error;
    }

    auto uptime = std::chrono::system_clock::now() - manager_start_time_;
    auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(uptime).count();
    stats["uptime_seconds"] = static_cast<Json::Int64>(uptime_seconds);

    return stats;
}

void WorkerManager::resetAllStats() {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    for (auto& [name, reg_worker] : workers_) {
        auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
        if (base_worker) {
            base_worker->resetStats();
        } else {
            reg_worker->info.stats = WorkerStats{};
        }
    }
}

void WorkerManager::resetWorkerStats(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);

    auto* reg_worker = getRegisteredWorker(name);
    if (reg_worker) {
        auto* base_worker = dynamic_cast<BaseWorker*>(reg_worker->worker.get());
        if (base_worker) {
            base_worker->resetStats();
        } else {
            reg_worker->info.stats = WorkerStats{};
        }
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

WorkerManager::RegisteredWorker* WorkerManager::getRegisteredWorker(const std::string& name) {
    auto it = workers_.find(name);
    return it != workers_.end() ? it->second.get() : nullptr;
}

const WorkerManager::RegisteredWorker* WorkerManager::getRegisteredWorker(const std::string& name) const {
    auto it = workers_.find(name);
    return it != workers_.end() ? it->second.get() : nullptr;
}

void WorkerManager::threadPoolWorker() {
    while (!shutdown_requested_.load()) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(work_mutex_);

            work_cv_.wait(lock, [this]() {
                return shutdown_requested_.load() || !work_queue_.empty();
            });

            if (shutdown_requested_.load() && work_queue_.empty()) {
                return;
            }

            if (!work_queue_.empty()) {
                task = std::move(work_queue_.front());
                work_queue_.pop();
            }
        }

        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                logError("Thread pool task failed: " + std::string(e.what()));
            }
        }
    }
}

void WorkerManager::enqueueWork(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(work_mutex_);
        work_queue_.push(std::move(task));
    }
    work_cv_.notify_one();
}

void WorkerManager::log(const std::string& message) {
    WTL_CAPTURE_INFO(
        ErrorCategory::BUSINESS_LOGIC,
        "[WorkerManager] " + message,
        {{"component", "WorkerManager"}}
    );
}

void WorkerManager::logError(const std::string& message) {
    WTL_CAPTURE_ERROR(
        ErrorCategory::BUSINESS_LOGIC,
        "[WorkerManager] " + message,
        {{"component", "WorkerManager"}}
    );
}

} // namespace wtl::workers
