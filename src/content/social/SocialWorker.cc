/**
 * @file SocialWorker.cc
 * @brief Implementation of background worker for social media operations
 *
 * @author Agent 13 (Phase 2, Agent 3) - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "content/social/SocialWorker.h"
#include "content/social/SocialMediaEngine.h"
#include "content/social/SocialAnalytics.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

#include <drogon/drogon.h>
#include <pqxx/pqxx>
#include <random>
#include <sstream>
#include <iomanip>

namespace wtl::content::social {

using namespace ::wtl::core::db;
using namespace ::wtl::core::debug;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string workerTaskTypeToString(WorkerTaskType type) {
    switch (type) {
        case WorkerTaskType::SCHEDULED_POST: return "scheduled_post";
        case WorkerTaskType::ANALYTICS_SYNC: return "analytics_sync";
        case WorkerTaskType::URGENT_DOG_CHECK: return "urgent_dog_check";
        case WorkerTaskType::TIKTOK_CROSSPOST: return "tiktok_crosspost";
        case WorkerTaskType::TOKEN_REFRESH: return "token_refresh";
        case WorkerTaskType::MILESTONE_CHECK: return "milestone_check";
        case WorkerTaskType::CLEANUP: return "cleanup";
        default: return "unknown";
    }
}

WorkerTaskType workerTaskTypeFromString(const std::string& str) {
    if (str == "scheduled_post") return WorkerTaskType::SCHEDULED_POST;
    if (str == "analytics_sync") return WorkerTaskType::ANALYTICS_SYNC;
    if (str == "urgent_dog_check") return WorkerTaskType::URGENT_DOG_CHECK;
    if (str == "tiktok_crosspost") return WorkerTaskType::TIKTOK_CROSSPOST;
    if (str == "token_refresh") return WorkerTaskType::TOKEN_REFRESH;
    if (str == "milestone_check") return WorkerTaskType::MILESTONE_CHECK;
    if (str == "cleanup") return WorkerTaskType::CLEANUP;
    return WorkerTaskType::SCHEDULED_POST;
}

// ============================================================================
// WORKER TASK IMPLEMENTATION
// ============================================================================

Json::Value WorkerTask::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["type"] = workerTaskTypeToString(type);
    json["scheduled_at"] = std::chrono::duration_cast<std::chrono::seconds>(
        scheduled_at.time_since_epoch()).count();
    json["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(
        created_at.time_since_epoch()).count();
    json["priority"] = priority;
    json["payload"] = payload;
    json["retry_count"] = retry_count;
    json["max_retries"] = max_retries;
    json["last_error"] = last_error;
    return json;
}

WorkerTask WorkerTask::fromJson(const Json::Value& json) {
    WorkerTask task;
    task.id = json.get("id", "").asString();
    task.type = workerTaskTypeFromString(json.get("type", "").asString());
    task.scheduled_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(json.get("scheduled_at", 0).asInt64()));
    task.created_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(json.get("created_at", 0).asInt64()));
    task.priority = json.get("priority", 0).asInt();
    task.payload = json.get("payload", Json::Value());
    task.retry_count = json.get("retry_count", 0).asInt();
    task.max_retries = json.get("max_retries", 3).asInt();
    task.last_error = json.get("last_error", "").asString();
    return task;
}

// ============================================================================
// WORKER CONFIG IMPLEMENTATION
// ============================================================================

SocialWorkerConfig SocialWorkerConfig::fromJson(const Json::Value& json) {
    SocialWorkerConfig config;

    if (json.isMember("scheduled_post_check_interval")) {
        config.scheduled_post_check_interval = std::chrono::seconds(
            json["scheduled_post_check_interval"].asInt());
    }
    if (json.isMember("analytics_sync_interval")) {
        config.analytics_sync_interval = std::chrono::seconds(
            json["analytics_sync_interval"].asInt());
    }
    if (json.isMember("urgent_dog_check_interval")) {
        config.urgent_dog_check_interval = std::chrono::seconds(
            json["urgent_dog_check_interval"].asInt());
    }
    if (json.isMember("tiktok_crosspost_interval")) {
        config.tiktok_crosspost_interval = std::chrono::seconds(
            json["tiktok_crosspost_interval"].asInt());
    }
    if (json.isMember("token_refresh_interval")) {
        config.token_refresh_interval = std::chrono::seconds(
            json["token_refresh_interval"].asInt());
    }
    if (json.isMember("milestone_check_interval")) {
        config.milestone_check_interval = std::chrono::seconds(
            json["milestone_check_interval"].asInt());
    }
    if (json.isMember("cleanup_interval")) {
        config.cleanup_interval = std::chrono::seconds(
            json["cleanup_interval"].asInt());
    }
    if (json.isMember("worker_threads")) {
        config.worker_threads = json["worker_threads"].asInt();
    }
    if (json.isMember("max_queue_size")) {
        config.max_queue_size = json["max_queue_size"].asInt();
    }
    if (json.isMember("default_max_retries")) {
        config.default_max_retries = json["default_max_retries"].asInt();
    }
    if (json.isMember("retry_delay")) {
        config.retry_delay = std::chrono::seconds(json["retry_delay"].asInt());
    }
    if (json.isMember("analytics_lookback_hours")) {
        config.analytics_lookback_hours = json["analytics_lookback_hours"].asInt();
    }
    if (json.isMember("urgent_dog_min_wait_days")) {
        config.urgent_dog_min_wait_days = json["urgent_dog_min_wait_days"].asInt();
    }
    if (json.isMember("urgent_dog_post_cooldown_days")) {
        config.urgent_dog_post_cooldown_days = json["urgent_dog_post_cooldown_days"].asInt();
    }
    if (json.isMember("auto_crosspost_tiktok")) {
        config.auto_crosspost_tiktok = json["auto_crosspost_tiktok"].asBool();
    }

    return config;
}

Json::Value SocialWorkerConfig::toJson() const {
    Json::Value json;
    json["scheduled_post_check_interval"] = static_cast<int>(
        scheduled_post_check_interval.count());
    json["analytics_sync_interval"] = static_cast<int>(
        analytics_sync_interval.count());
    json["urgent_dog_check_interval"] = static_cast<int>(
        urgent_dog_check_interval.count());
    json["tiktok_crosspost_interval"] = static_cast<int>(
        tiktok_crosspost_interval.count());
    json["token_refresh_interval"] = static_cast<int>(
        token_refresh_interval.count());
    json["milestone_check_interval"] = static_cast<int>(
        milestone_check_interval.count());
    json["cleanup_interval"] = static_cast<int>(cleanup_interval.count());
    json["worker_threads"] = worker_threads;
    json["max_queue_size"] = max_queue_size;
    json["default_max_retries"] = default_max_retries;
    json["retry_delay"] = static_cast<int>(retry_delay.count());
    json["analytics_lookback_hours"] = analytics_lookback_hours;
    json["urgent_dog_min_wait_days"] = urgent_dog_min_wait_days;
    json["urgent_dog_post_cooldown_days"] = urgent_dog_post_cooldown_days;
    json["auto_crosspost_tiktok"] = auto_crosspost_tiktok;
    return json;
}

// ============================================================================
// WORKER STATS IMPLEMENTATION
// ============================================================================

Json::Value WorkerStats::toJson() const {
    Json::Value json;
    json["tasks_processed"] = static_cast<Json::UInt64>(tasks_processed.load());
    json["tasks_succeeded"] = static_cast<Json::UInt64>(tasks_succeeded.load());
    json["tasks_failed"] = static_cast<Json::UInt64>(tasks_failed.load());
    json["tasks_retried"] = static_cast<Json::UInt64>(tasks_retried.load());
    json["posts_published"] = static_cast<Json::UInt64>(posts_published.load());
    json["analytics_synced"] = static_cast<Json::UInt64>(analytics_synced.load());
    json["tokens_refreshed"] = static_cast<Json::UInt64>(tokens_refreshed.load());
    json["urgent_posts_generated"] = static_cast<Json::UInt64>(urgent_posts_generated.load());
    json["tiktok_crossposts"] = static_cast<Json::UInt64>(tiktok_crossposts.load());
    json["started_at"] = std::chrono::duration_cast<std::chrono::seconds>(
        started_at.time_since_epoch()).count();
    json["last_task_at"] = std::chrono::duration_cast<std::chrono::seconds>(
        last_task_at.time_since_epoch()).count();
    return json;
}

void WorkerStats::reset() {
    tasks_processed = 0;
    tasks_succeeded = 0;
    tasks_failed = 0;
    tasks_retried = 0;
    posts_published = 0;
    analytics_synced = 0;
    tokens_refreshed = 0;
    urgent_posts_generated = 0;
    tiktok_crossposts = 0;
    started_at = std::chrono::system_clock::now();
    last_task_at = std::chrono::system_clock::time_point{};
}

// ============================================================================
// SOCIAL WORKER IMPLEMENTATION
// ============================================================================

SocialWorker& SocialWorker::getInstance() {
    static SocialWorker instance;
    return instance;
}

SocialWorker::~SocialWorker() {
    if (running_) {
        stop(false);
    }
}

void SocialWorker::initialize(const SocialWorkerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return;
    }

    config_ = config;
    stats_.reset();
    initialized_ = true;

    LOG_INFO << "SocialWorker: Initialized with " +
        std::to_string(config_.worker_threads) + " worker threads";
}

void SocialWorker::initializeFromConfig() {
    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        auto result = txn.exec(
            "SELECT config_value FROM system_config WHERE config_key = 'social_worker'");

        if (!result.empty() && !result[0][0].is_null()) {
            Json::Value json;
            Json::Reader reader;
            if (reader.parse(result[0][0].as<std::string>(), json)) {
                initialize(SocialWorkerConfig::fromJson(json));
                return;
            }
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Failed to load config from database",
            {{"error", e.what()}});
    }

    // Use defaults if database config not available
    initialize(SocialWorkerConfig{});
}

void SocialWorker::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Cannot start - not initialized", {});
        return;
    }

    if (running_) {
        return;
    }

    running_ = true;
    stopping_ = false;
    stats_.started_at = std::chrono::system_clock::now();

    // Start scheduler thread
    scheduler_thread_ = std::thread(&SocialWorker::schedulerLoop, this);

    // Start worker threads
    worker_threads_.clear();
    for (int i = 0; i < config_.worker_threads; ++i) {
        worker_threads_.emplace_back(&SocialWorker::workerLoop, this);
    }

    // Schedule recurring tasks
    scheduleRecurringTasks();

    LOG_INFO << "SocialWorker: Started with " +
        std::to_string(config_.worker_threads) + " workers";
}

void SocialWorker::stop(bool wait_for_tasks) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!running_) {
            return;
        }

        stopping_ = true;

        if (!wait_for_tasks) {
            running_ = false;
        }
    }

    // Wake up all waiting threads
    queue_cv_.notify_all();

    // Wait for queue to drain if requested
    if (wait_for_tasks) {
        while (getPendingTaskCount() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        queue_cv_.notify_all();
    }

    // Join threads
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }

    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();

    LOG_INFO << "SocialWorker: Stopped";
}

bool SocialWorker::isRunning() const {
    return running_;
}

bool SocialWorker::isInitialized() const {
    return initialized_;
}

// ============================================================================
// TASK MANAGEMENT
// ============================================================================

bool SocialWorker::queueTask(const WorkerTask& task) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (task_queue_.size() >= static_cast<size_t>(config_.max_queue_size)) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Task queue full",
            {{"queue_size", std::to_string(task_queue_.size())}});
        return false;
    }

    task_queue_.push(task);
    task_map_[task.id] = task;

    queue_cv_.notify_one();
    return true;
}

std::string SocialWorker::queueScheduledPost(
    const std::string& post_id,
    std::chrono::system_clock::time_point scheduled_time) {

    WorkerTask task;
    task.id = generateTaskId();
    task.type = WorkerTaskType::SCHEDULED_POST;
    task.scheduled_at = scheduled_time;
    task.created_at = std::chrono::system_clock::now();
    task.priority = 10;  // High priority for scheduled posts
    task.payload["post_id"] = post_id;
    task.retry_count = 0;
    task.max_retries = config_.default_max_retries;

    if (queueTask(task)) {
        return task.id;
    }
    return "";
}

std::string SocialWorker::queueAnalyticsSync(const std::string& post_id) {
    WorkerTask task;
    task.id = generateTaskId();
    task.type = WorkerTaskType::ANALYTICS_SYNC;
    task.scheduled_at = std::chrono::system_clock::now();
    task.created_at = std::chrono::system_clock::now();
    task.priority = 5;  // Medium priority
    task.payload["post_id"] = post_id;
    task.payload["hours_back"] = config_.analytics_lookback_hours;
    task.retry_count = 0;
    task.max_retries = config_.default_max_retries;

    if (queueTask(task)) {
        return task.id;
    }
    return "";
}

std::string SocialWorker::queueTikTokCrosspost(
    const std::string& tiktok_url,
    const std::string& dog_id,
    const std::vector<Platform>& platforms) {

    WorkerTask task;
    task.id = generateTaskId();
    task.type = WorkerTaskType::TIKTOK_CROSSPOST;
    task.scheduled_at = std::chrono::system_clock::now();
    task.created_at = std::chrono::system_clock::now();
    task.priority = 8;  // High priority
    task.payload["tiktok_url"] = tiktok_url;
    task.payload["dog_id"] = dog_id;

    Json::Value platformsJson(Json::arrayValue);
    for (const auto& p : platforms) {
        platformsJson.append(platformToString(p));
    }
    task.payload["platforms"] = platformsJson;

    task.retry_count = 0;
    task.max_retries = config_.default_max_retries;

    if (queueTask(task)) {
        return task.id;
    }
    return "";
}

std::string SocialWorker::queueTokenRefresh(const std::string& connection_id) {
    WorkerTask task;
    task.id = generateTaskId();
    task.type = WorkerTaskType::TOKEN_REFRESH;
    task.scheduled_at = std::chrono::system_clock::now();
    task.created_at = std::chrono::system_clock::now();
    task.priority = 9;  // High priority - tokens are critical
    task.payload["connection_id"] = connection_id;
    task.retry_count = 0;
    task.max_retries = config_.default_max_retries;

    if (queueTask(task)) {
        return task.id;
    }
    return "";
}

bool SocialWorker::cancelTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = task_map_.find(task_id);
    if (it == task_map_.end()) {
        return false;
    }

    task_map_.erase(it);
    // Note: We can't easily remove from priority_queue, so task will be skipped when dequeued
    return true;
}

std::optional<WorkerTask> SocialWorker::getTask(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = task_map_.find(task_id);
    if (it != task_map_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<WorkerTask> SocialWorker::getPendingTasks() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    std::vector<WorkerTask> tasks;
    tasks.reserve(task_map_.size());

    for (const auto& [id, task] : task_map_) {
        tasks.push_back(task);
    }

    return tasks;
}

size_t SocialWorker::getPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return task_queue_.size();
}

// ============================================================================
// MANUAL TRIGGERS
// ============================================================================

int SocialWorker::triggerScheduledPostCheck() {
    auto& engine = SocialMediaEngine::getInstance();
    return engine.processScheduledPosts();
}

int SocialWorker::triggerAnalyticsSync(int hours_back) {
    auto& engine = SocialMediaEngine::getInstance();
    return engine.syncAnalytics(hours_back);
}

int SocialWorker::triggerUrgentDogCheck() {
    int count = 0;
    auto urgentDogs = findUrgentDogs();
    auto& engine = SocialMediaEngine::getInstance();

    for (const auto& dog_id : urgentDogs) {
        auto post = engine.generateUrgentDogPost(dog_id);
        if (post) {
            auto result = engine.crossPost(*post);
            if (result.overall_success) {
                count++;
                stats_.urgent_posts_generated++;
            }
        }
    }

    return count;
}

int SocialWorker::triggerTikTokCrosspostCheck() {
    int count = 0;
    auto videos = findNewTikTokVideos();
    auto& engine = SocialMediaEngine::getInstance();

    for (const auto& [tiktok_url, dog_id] : videos) {
        auto result = engine.crossPostTikTok(tiktok_url, dog_id);
        if (result.overall_success) {
            count++;
            stats_.tiktok_crossposts++;
        }
    }

    return count;
}

int SocialWorker::triggerTokenRefresh() {
    int count = 0;
    auto connections = findExpiringConnections();
    auto& engine = SocialMediaEngine::getInstance();

    for (const auto& connection_id : connections) {
        if (engine.refreshToken(connection_id)) {
            count++;
            stats_.tokens_refreshed++;
        }
    }

    return count;
}

int SocialWorker::triggerMilestoneCheck() {
    int count = 0;
    auto milestones = findMilestoneDogs();
    auto& engine = SocialMediaEngine::getInstance();

    for (const auto& [dog_id, milestone_type] : milestones) {
        auto post = engine.generateMilestonePost(dog_id, milestone_type);
        if (post) {
            auto result = engine.crossPost(*post);
            if (result.overall_success) {
                count++;
            }
        }
    }

    return count;
}

// ============================================================================
// STATISTICS
// ============================================================================

WorkerStats SocialWorker::getStats() const {
    return stats_;
}

void SocialWorker::resetStats() {
    stats_.reset();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

SocialWorkerConfig SocialWorker::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void SocialWorker::updateConfig(const SocialWorkerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void SocialWorker::setTaskCompletionCallback(
    std::function<void(const WorkerTaskResult&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    completion_callback_ = std::move(callback);
}

void SocialWorker::setTaskFailureCallback(
    std::function<void(const WorkerTask&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    failure_callback_ = std::move(callback);
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void SocialWorker::schedulerLoop() {
    while (running_) {
        // Check for recurring tasks that need scheduling
        // This is handled by scheduleRecurringTasks() on start

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void SocialWorker::workerLoop() {
    while (running_ || !task_queue_.empty()) {
        WorkerTask task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // Wait for a task or stop signal
            queue_cv_.wait(lock, [this] {
                return !running_ || !task_queue_.empty();
            });

            if (!running_ && task_queue_.empty()) {
                break;
            }

            if (task_queue_.empty()) {
                continue;
            }

            // Check if top task is ready
            task = task_queue_.top();

            // Skip cancelled tasks
            if (task_map_.find(task.id) == task_map_.end()) {
                task_queue_.pop();
                continue;
            }

            // Check if task is due
            auto now = std::chrono::system_clock::now();
            if (task.scheduled_at > now) {
                // Not ready yet, wait
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            task_queue_.pop();
            task_map_.erase(task.id);
        }

        // Execute the task
        auto result = executeTask(task);

        stats_.tasks_processed++;
        stats_.last_task_at = std::chrono::system_clock::now();

        if (result.success) {
            stats_.tasks_succeeded++;

            if (completion_callback_) {
                try {
                    completion_callback_(result);
                } catch (...) {
                    // Ignore callback errors
                }
            }
        } else {
            stats_.tasks_failed++;

            // Retry if applicable
            if (task.retry_count < task.max_retries) {
                retryTask(task, result.error_message);
            } else if (failure_callback_) {
                try {
                    failure_callback_(task, result.error_message);
                } catch (...) {
                    // Ignore callback errors
                }
            }
        }
    }
}

WorkerTaskResult SocialWorker::executeTask(const WorkerTask& task) {
    auto start = std::chrono::steady_clock::now();
    WorkerTaskResult result;
    result.task_id = task.id;

    try {
        switch (task.type) {
            case WorkerTaskType::SCHEDULED_POST:
                result = executeScheduledPost(task);
                break;
            case WorkerTaskType::ANALYTICS_SYNC:
                result = executeAnalyticsSync(task);
                break;
            case WorkerTaskType::URGENT_DOG_CHECK:
                result = executeUrgentDogCheck(task);
                break;
            case WorkerTaskType::TIKTOK_CROSSPOST:
                result = executeTikTokCrosspost(task);
                break;
            case WorkerTaskType::TOKEN_REFRESH:
                result = executeTokenRefresh(task);
                break;
            case WorkerTaskType::MILESTONE_CHECK:
                result = executeMilestoneCheck(task);
                break;
            case WorkerTaskType::CLEANUP:
                result = executeCleanup(task);
                break;
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Task execution failed",
            {{"task_id", task.id}, {"type", workerTaskTypeToString(task.type)},
             {"error", e.what()}});
    }

    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return result;
}

WorkerTaskResult SocialWorker::executeScheduledPost(const WorkerTask& task) {
    WorkerTaskResult result;
    result.task_id = task.id;

    std::string post_id = task.payload.get("post_id", "").asString();
    if (post_id.empty()) {
        result.success = false;
        result.error_message = "Missing post_id";
        return result;
    }

    auto& engine = SocialMediaEngine::getInstance();
    auto post = engine.getPost(post_id);

    if (!post) {
        result.success = false;
        result.error_message = "Post not found: " + post_id;
        return result;
    }

    auto crossResult = engine.crossPost(*post);

    result.success = crossResult.overall_success;
    result.result_data = crossResult.toJson();

    if (!result.success) {
        result.error_message = "Cross-post failed for some platforms";
    } else {
        stats_.posts_published++;
    }

    return result;
}

WorkerTaskResult SocialWorker::executeAnalyticsSync(const WorkerTask& task) {
    WorkerTaskResult result;
    result.task_id = task.id;

    std::string post_id = task.payload.get("post_id", "").asString();
    int hours_back = task.payload.get("hours_back", 24).asInt();

    auto& engine = SocialMediaEngine::getInstance();

    if (!post_id.empty()) {
        result.success = engine.syncPostAnalytics(post_id);
        if (result.success) {
            stats_.analytics_synced++;
        }
    } else {
        int count = engine.syncAnalytics(hours_back);
        result.success = true;
        result.result_data["posts_synced"] = count;
        stats_.analytics_synced += count;
    }

    return result;
}

WorkerTaskResult SocialWorker::executeUrgentDogCheck(const WorkerTask& task) {
    WorkerTaskResult result;
    result.task_id = task.id;

    int count = triggerUrgentDogCheck();
    result.success = true;
    result.result_data["posts_generated"] = count;

    return result;
}

WorkerTaskResult SocialWorker::executeTikTokCrosspost(const WorkerTask& task) {
    WorkerTaskResult result;
    result.task_id = task.id;

    std::string tiktok_url = task.payload.get("tiktok_url", "").asString();
    std::string dog_id = task.payload.get("dog_id", "").asString();

    if (tiktok_url.empty()) {
        result.success = false;
        result.error_message = "Missing tiktok_url";
        return result;
    }

    std::vector<Platform> platforms;
    const auto& platformsJson = task.payload["platforms"];
    if (platformsJson.isArray()) {
        for (const auto& p : platformsJson) {
            platforms.push_back(stringToPlatform(p.asString()));
        }
    }

    auto& engine = SocialMediaEngine::getInstance();
    auto crossResult = engine.crossPostTikTok(tiktok_url, dog_id, platforms);

    result.success = crossResult.overall_success;
    result.result_data = crossResult.toJson();

    if (result.success) {
        stats_.tiktok_crossposts++;
    }

    return result;
}

WorkerTaskResult SocialWorker::executeTokenRefresh(const WorkerTask& task) {
    WorkerTaskResult result;
    result.task_id = task.id;

    std::string connection_id = task.payload.get("connection_id", "").asString();
    if (connection_id.empty()) {
        result.success = false;
        result.error_message = "Missing connection_id";
        return result;
    }

    auto& engine = SocialMediaEngine::getInstance();
    result.success = engine.refreshToken(connection_id);

    if (result.success) {
        stats_.tokens_refreshed++;
    } else {
        result.error_message = "Token refresh failed";
    }

    return result;
}

WorkerTaskResult SocialWorker::executeMilestoneCheck(const WorkerTask& task) {
    WorkerTaskResult result;
    result.task_id = task.id;

    int count = triggerMilestoneCheck();
    result.success = true;
    result.result_data["posts_generated"] = count;

    return result;
}

WorkerTaskResult SocialWorker::executeCleanup(const WorkerTask& task) {
    WorkerTaskResult result;
    result.task_id = task.id;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Clean up old analytics cache (older than 30 days)
        txn.exec(
            "DELETE FROM social_analytics_cache "
            "WHERE cached_at < NOW() - INTERVAL '30 days'");

        // Clean up old worker task logs (older than 7 days)
        txn.exec(
            "DELETE FROM social_worker_tasks "
            "WHERE completed_at < NOW() - INTERVAL '7 days'");

        // Clean up deleted posts (older than 90 days)
        txn.exec(
            "DELETE FROM social_posts "
            "WHERE deleted_at IS NOT NULL "
            "AND deleted_at < NOW() - INTERVAL '90 days'");

        txn.commit();
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Cleanup failed", {{"error", e.what()}});
    }

    return result;
}

void SocialWorker::scheduleRecurringTasks() {
    auto now = std::chrono::system_clock::now();

    // Schedule first occurrence of each recurring task

    // Scheduled post check (every minute)
    {
        WorkerTask task;
        task.id = generateTaskId();
        task.type = WorkerTaskType::SCHEDULED_POST;
        task.scheduled_at = now + config_.scheduled_post_check_interval;
        task.created_at = now;
        task.priority = 10;
        task.max_retries = 1;
        queueTask(task);
    }

    // Analytics sync (every hour)
    {
        WorkerTask task;
        task.id = generateTaskId();
        task.type = WorkerTaskType::ANALYTICS_SYNC;
        task.scheduled_at = now + config_.analytics_sync_interval;
        task.created_at = now;
        task.priority = 5;
        task.payload["hours_back"] = config_.analytics_lookback_hours;
        task.max_retries = 2;
        queueTask(task);
    }

    // Urgent dog check (every 30 minutes)
    {
        WorkerTask task;
        task.id = generateTaskId();
        task.type = WorkerTaskType::URGENT_DOG_CHECK;
        task.scheduled_at = now + config_.urgent_dog_check_interval;
        task.created_at = now;
        task.priority = 7;
        task.max_retries = 1;
        queueTask(task);
    }

    // TikTok crosspost check (every 5 minutes)
    if (config_.auto_crosspost_tiktok) {
        WorkerTask task;
        task.id = generateTaskId();
        task.type = WorkerTaskType::TIKTOK_CROSSPOST;
        task.scheduled_at = now + config_.tiktok_crosspost_interval;
        task.created_at = now;
        task.priority = 6;
        task.max_retries = 2;
        queueTask(task);
    }

    // Token refresh (every hour)
    {
        WorkerTask task;
        task.id = generateTaskId();
        task.type = WorkerTaskType::TOKEN_REFRESH;
        task.scheduled_at = now + config_.token_refresh_interval;
        task.created_at = now;
        task.priority = 9;
        task.max_retries = 3;
        queueTask(task);
    }

    // Milestone check (daily)
    {
        WorkerTask task;
        task.id = generateTaskId();
        task.type = WorkerTaskType::MILESTONE_CHECK;
        task.scheduled_at = now + config_.milestone_check_interval;
        task.created_at = now;
        task.priority = 4;
        task.max_retries = 1;
        queueTask(task);
    }

    // Cleanup (daily)
    {
        WorkerTask task;
        task.id = generateTaskId();
        task.type = WorkerTaskType::CLEANUP;
        task.scheduled_at = now + config_.cleanup_interval;
        task.created_at = now;
        task.priority = 1;
        task.max_retries = 1;
        queueTask(task);
    }
}

std::string SocialWorker::generateTaskId() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

    std::stringstream ss;
    ss << "task_";
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << dis(gen);
    ss << std::setw(8) << dis(gen);

    return ss.str();
}

std::vector<std::string> SocialWorker::findUrgentDogs() const {
    std::vector<std::string> urgent_dogs;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find dogs waiting longer than threshold without recent social post
        auto result = txn.exec_params(
            "SELECT d.id FROM dogs d "
            "WHERE d.status = 'available' "
            "AND d.intake_date < NOW() - INTERVAL '1 day' * $1 "
            "AND NOT EXISTS ("
            "    SELECT 1 FROM social_posts sp "
            "    WHERE sp.dog_id = d.id "
            "    AND sp.post_type = 'urgent_appeal' "
            "    AND sp.created_at > NOW() - INTERVAL '1 day' * $2"
            ") "
            "ORDER BY d.intake_date ASC "
            "LIMIT 10",
            config_.urgent_dog_min_wait_days,
            config_.urgent_dog_post_cooldown_days);

        for (const auto& row : result) {
            urgent_dogs.push_back(row[0].as<std::string>());
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Failed to find urgent dogs",
            {{"error", e.what()}});
    }

    return urgent_dogs;
}

std::vector<std::pair<std::string, std::string>> SocialWorker::findMilestoneDogs() const {
    std::vector<std::pair<std::string, std::string>> milestones;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find 1-year milestones (365 days)
        auto result1 = txn.exec(
            "SELECT d.id FROM dogs d "
            "WHERE d.status = 'available' "
            "AND d.intake_date::date = (CURRENT_DATE - INTERVAL '1 year')::date "
            "AND NOT EXISTS ("
            "    SELECT 1 FROM social_posts sp "
            "    WHERE sp.dog_id = d.id "
            "    AND sp.post_type = 'waiting_milestone' "
            "    AND sp.created_at > NOW() - INTERVAL '1 day'"
            ")");

        for (const auto& row : result1) {
            milestones.emplace_back(row[0].as<std::string>(), "1_year");
        }

        // Find 2-year milestones
        auto result2 = txn.exec(
            "SELECT d.id FROM dogs d "
            "WHERE d.status = 'available' "
            "AND d.intake_date::date = (CURRENT_DATE - INTERVAL '2 years')::date "
            "AND NOT EXISTS ("
            "    SELECT 1 FROM social_posts sp "
            "    WHERE sp.dog_id = d.id "
            "    AND sp.post_type = 'waiting_milestone' "
            "    AND sp.created_at > NOW() - INTERVAL '1 day'"
            ")");

        for (const auto& row : result2) {
            milestones.emplace_back(row[0].as<std::string>(), "2_years");
        }

        // Find 500-day milestones
        auto result500 = txn.exec(
            "SELECT d.id FROM dogs d "
            "WHERE d.status = 'available' "
            "AND d.intake_date::date = (CURRENT_DATE - INTERVAL '500 days')::date "
            "AND NOT EXISTS ("
            "    SELECT 1 FROM social_posts sp "
            "    WHERE sp.dog_id = d.id "
            "    AND sp.post_type = 'waiting_milestone' "
            "    AND sp.created_at > NOW() - INTERVAL '1 day'"
            ")");

        for (const auto& row : result500) {
            milestones.emplace_back(row[0].as<std::string>(), "500_days");
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Failed to find milestone dogs",
            {{"error", e.what()}});
    }

    return milestones;
}

std::vector<std::pair<std::string, std::string>> SocialWorker::findNewTikTokVideos() const {
    std::vector<std::pair<std::string, std::string>> videos;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find TikTok posts not yet cross-posted
        auto result = txn.exec(
            "SELECT sp.platform_post_id, sp.dog_id "
            "FROM social_posts sp "
            "WHERE sp.platform = 'tiktok' "
            "AND sp.status = 'posted' "
            "AND sp.created_at > NOW() - INTERVAL '7 days' "
            "AND NOT EXISTS ("
            "    SELECT 1 FROM social_post_crossposts spc "
            "    WHERE spc.source_post_id = sp.id"
            ") "
            "ORDER BY sp.created_at DESC "
            "LIMIT 10");

        for (const auto& row : result) {
            std::string tiktok_url = "https://www.tiktok.com/@user/video/" +
                row[0].as<std::string>();
            videos.emplace_back(tiktok_url, row[1].as<std::string>());
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Failed to find TikTok videos",
            {{"error", e.what()}});
    }

    return videos;
}

std::vector<std::string> SocialWorker::findExpiringConnections() const {
    std::vector<std::string> connections;

    try {
        auto conn = ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Find connections expiring in the next 24 hours
        auto result = txn.exec(
            "SELECT id FROM social_platform_connections "
            "WHERE token_expires_at IS NOT NULL "
            "AND token_expires_at < NOW() + INTERVAL '24 hours' "
            "AND token_expires_at > NOW() "
            "AND refresh_token IS NOT NULL "
            "AND refresh_token != ''");

        for (const auto& row : result) {
            connections.push_back(row[0].as<std::string>());
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONTENT, "Failed to find expiring connections",
            {{"error", e.what()}});
    }

    return connections;
}

void SocialWorker::retryTask(WorkerTask task, const std::string& error) {
    task.retry_count++;
    task.last_error = error;
    task.scheduled_at = std::chrono::system_clock::now() + config_.retry_delay;
    task.id = generateTaskId();  // New ID for retry

    stats_.tasks_retried++;

    if (queueTask(task)) {
        LOG_INFO << "SocialWorker: Task queued for retry: " +
            workerTaskTypeToString(task.type) + " (attempt " +
            std::to_string(task.retry_count) + "/" +
            std::to_string(task.max_retries) + ")";
    }
}

} // namespace wtl::content::social
