/**
 * @file TikTokWorker.cc
 * @brief Implementation of TikTok background worker
 * @see TikTokWorker.h for documentation
 */

#include "TikTokWorker.h"

// Standard library includes
#include <ctime>
#include <iomanip>
#include <sstream>

// Project includes
#include "TikTokEngine.h"
#include "TikTokClient.h"
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

namespace wtl::content::tiktok {

// ============================================================================
// STATISTICS IMPLEMENTATIONS
// ============================================================================

Json::Value WorkerStatistics::toJson() const {
    Json::Value json;

    json["total_cycles"] = static_cast<Json::UInt64>(total_cycles);
    json["scheduled_posts_processed"] = static_cast<Json::UInt64>(scheduled_posts_processed);
    json["analytics_updates"] = static_cast<Json::UInt64>(analytics_updates);
    json["auto_posts_generated"] = static_cast<Json::UInt64>(auto_posts_generated);
    json["failed_retries"] = static_cast<Json::UInt64>(failed_retries);
    json["expired_posts_cleaned"] = static_cast<Json::UInt64>(expired_posts_cleaned);
    json["errors"] = static_cast<Json::UInt64>(errors);

    auto last_run_time = std::chrono::system_clock::to_time_t(last_run);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&last_run_time), "%Y-%m-%dT%H:%M:%SZ");
    json["last_run"] = oss.str();

    json["last_cycle_duration_ms"] = static_cast<Json::Int64>(last_cycle_duration.count());

    return json;
}

Json::Value WorkerRunResult::toJson() const {
    Json::Value json;

    json["success"] = success;
    json["posts_processed"] = posts_processed;
    json["analytics_updated"] = analytics_updated;
    json["posts_generated"] = posts_generated;
    json["retries_attempted"] = retries_attempted;
    json["posts_expired"] = posts_expired;
    json["error_message"] = error_message;
    json["duration_ms"] = static_cast<Json::Int64>(duration.count());

    return json;
}

// ============================================================================
// TIKTOKWORKER SINGLETON
// ============================================================================

TikTokWorker& TikTokWorker::getInstance() {
    static TikTokWorker instance;
    return instance;
}

TikTokWorker::~TikTokWorker() {
    stop();
}

// ============================================================================
// TIKTOKWORKER LIFECYCLE
// ============================================================================

void TikTokWorker::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
        return;  // Already running
    }

    stop_requested_ = false;
    running_ = true;

    // Initialize timing
    last_analytics_update_ = std::chrono::system_clock::now();
    last_auto_generate_ = std::chrono::system_clock::now();

    worker_thread_ = std::make_unique<std::thread>(&TikTokWorker::workerLoop, this);

    LOG_INFO << "TikTokWorker started with interval " << interval_.count() << " seconds";
}

void TikTokWorker::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!running_) {
            return;
        }

        stop_requested_ = true;
    }

    cv_.notify_all();

    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }

    running_ = false;
    worker_thread_.reset();

    LOG_INFO << "TikTokWorker stopped";
}

bool TikTokWorker::isRunning() const {
    return running_;
}

void TikTokWorker::triggerRun() {
    cv_.notify_one();
}

// ============================================================================
// TIKTOKWORKER CONFIGURATION
// ============================================================================

void TikTokWorker::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();

        interval_ = std::chrono::seconds(
            config.getInt("tiktok.worker.interval_seconds", 300));

        analytics_interval_ = std::chrono::seconds(
            config.getInt("tiktok.worker.analytics_interval_seconds", 1800));

        auto_generate_interval_ = std::chrono::seconds(
            config.getInt("tiktok.worker.auto_generate_interval_seconds", 3600));

        process_scheduled_enabled_ = config.getBool(
            "tiktok.worker.process_scheduled", true);

        analytics_update_enabled_ = config.getBool(
            "tiktok.worker.update_analytics", true);

        auto_generate_enabled_ = config.getBool(
            "tiktok.worker.auto_generate", true);

        retry_processing_enabled_ = config.getBool(
            "tiktok.worker.retry_failed", true);

        max_retries_ = config.getInt("tiktok.worker.max_retries", 3);
        max_auto_posts_per_cycle_ = config.getInt(
            "tiktok.worker.max_auto_posts_per_cycle", 5);
        max_analytics_updates_per_cycle_ = config.getInt(
            "tiktok.worker.max_analytics_updates_per_cycle", 50);

        LOG_INFO << "TikTokWorker configured: interval=" << interval_.count()
                 << "s, analytics_interval=" << analytics_interval_.count()
                 << "s, auto_generate_interval=" << auto_generate_interval_.count() << "s";

    } catch (const std::exception& e) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to load TikTokWorker config, using defaults: " + std::string(e.what()),
            {});
    }
}

void TikTokWorker::setInterval(std::chrono::seconds interval) {
    std::lock_guard<std::mutex> lock(mutex_);
    interval_ = interval;
}

std::chrono::seconds TikTokWorker::getInterval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return interval_;
}

void TikTokWorker::setProcessScheduledEnabled(bool enabled) {
    process_scheduled_enabled_ = enabled;
}

void TikTokWorker::setAnalyticsUpdateEnabled(bool enabled) {
    analytics_update_enabled_ = enabled;
}

void TikTokWorker::setAutoGenerateEnabled(bool enabled) {
    auto_generate_enabled_ = enabled;
}

void TikTokWorker::setRetryProcessingEnabled(bool enabled) {
    retry_processing_enabled_ = enabled;
}

void TikTokWorker::setAnalyticsInterval(std::chrono::seconds interval) {
    analytics_interval_ = interval;
}

void TikTokWorker::setAutoGenerateInterval(std::chrono::seconds interval) {
    auto_generate_interval_ = interval;
}

// ============================================================================
// TIKTOKWORKER MAIN LOOP
// ============================================================================

void TikTokWorker::workerLoop() {
    LOG_INFO << "TikTokWorker loop started";

    while (!stop_requested_) {
        try {
            auto result = runCycle();

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_cycles++;
                stats_.last_run = std::chrono::system_clock::now();
                stats_.last_cycle_duration = result.duration;

                if (!result.success) {
                    stats_.errors++;
                }
            }

            // Log cycle summary
            LOG_DEBUG << "TikTokWorker cycle complete: "
                     << result.posts_processed << " posts processed, "
                     << result.analytics_updated << " analytics updated, "
                     << result.posts_generated << " posts generated, "
                     << result.duration.count() << "ms";

        } catch (const std::exception& e) {
            WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
                (std::unordered_map<std::string, std::string>{{"component", "TikTokWorker"}}));

            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.errors++;
        }

        // Wait for next interval or trigger
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, interval_, [this] {
                return stop_requested_.load();
            });
        }
    }

    LOG_INFO << "TikTokWorker loop exited";
}

WorkerRunResult TikTokWorker::runCycle() {
    WorkerRunResult result;
    auto start = std::chrono::steady_clock::now();

    try {
        // 1. Process scheduled posts
        if (process_scheduled_enabled_) {
            result.posts_processed = processScheduledPosts();
        }

        // 2. Update analytics (if interval elapsed)
        if (analytics_update_enabled_ && shouldUpdateAnalytics()) {
            result.analytics_updated = updateAnalytics();
            last_analytics_update_ = std::chrono::system_clock::now();
        }

        // 3. Auto-generate posts (if interval elapsed)
        if (auto_generate_enabled_ && shouldAutoGenerate()) {
            result.posts_generated = generateAutoPosts();
            last_auto_generate_ = std::chrono::system_clock::now();
        }

        // 4. Retry failed posts
        if (retry_processing_enabled_) {
            result.retries_attempted = retryFailedPosts();
        }

        // 5. Clean up expired posts
        result.posts_expired = cleanupExpiredPosts();

        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "TikTokWorker::runCycle"}}));
    }

    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return result;
}

// ============================================================================
// TIKTOKWORKER OPERATIONS
// ============================================================================

int TikTokWorker::processScheduledPosts() {
    try {
        auto& engine = TikTokEngine::getInstance();
        int processed = engine.processScheduledPosts();

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.scheduled_posts_processed += processed;
        }

        return processed;

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "processScheduledPosts"}}));
        return 0;
    }
}

int TikTokWorker::updateAnalytics() {
    try {
        auto& engine = TikTokEngine::getInstance();
        int updated = engine.updateAllAnalytics(max_analytics_updates_per_cycle_);

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.analytics_updates += updated;
        }

        return updated;

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "updateAnalytics"}}));
        return 0;
    }
}

int TikTokWorker::generateAutoPosts() {
    int generated = 0;

    try {
        auto& engine = TikTokEngine::getInstance();

        // Generate urgent dog posts (highest priority)
        int urgent_count = max_auto_posts_per_cycle_ / 2;
        generated += engine.generateUrgentDogPosts(urgent_count);

        // Generate longest waiting posts
        int remaining = max_auto_posts_per_cycle_ - generated;
        if (remaining > 0) {
            generated += engine.generateLongestWaitingPosts(remaining);
        }

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.auto_posts_generated += generated;
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "generateAutoPosts"}}));
    }

    return generated;
}

int TikTokWorker::retryFailedPosts() {
    int retried = 0;

    try {
        auto& engine = TikTokEngine::getInstance();
        auto failed_posts = engine.getPostsByStatus(TikTokPostStatus::FAILED, 10);

        for (const auto& post : failed_posts) {
            // Only retry if under max retry count
            if (post.retry_count < max_retries_) {
                auto result = engine.retryFailedPost(post.id);
                retried++;

                if (result.success) {
                    LOG_INFO << "Successfully retried post " << post.id;
                }
            }
        }

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.failed_retries += retried;
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            (std::unordered_map<std::string, std::string>{{"operation", "retryFailedPosts"}}));
    }

    return retried;
}

int TikTokWorker::cleanupExpiredPosts() {
    int cleaned = 0;

    try {
        auto conn = wtl::core::db::ConnectionPool::getInstance().acquire();
        pqxx::work txn(*conn);

        // Mark scheduled posts that are past their time as expired
        auto result = txn.exec(
            "UPDATE tiktok_posts "
            "SET status = 'expired', updated_at = NOW() "
            "WHERE status = 'scheduled' "
            "AND scheduled_at < NOW() - INTERVAL '1 hour' "
            "RETURNING id"
        );

        cleaned = result.size();

        txn.commit();
        wtl::core::db::ConnectionPool::getInstance().release(conn);

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.expired_posts_cleaned += cleaned;
        }

        if (cleaned > 0) {
            LOG_INFO << "Cleaned up " << cleaned << " expired posts";
        }

    } catch (const std::exception& e) {
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::DATABASE,
            (std::unordered_map<std::string, std::string>{{"operation", "cleanupExpiredPosts"}}));
    }

    return cleaned;
}

// ============================================================================
// TIKTOKWORKER STATISTICS
// ============================================================================

WorkerStatistics TikTokWorker::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void TikTokWorker::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = WorkerStatistics();
}

Json::Value TikTokWorker::getStatus() const {
    Json::Value status;

    status["running"] = running_.load();
    status["interval_seconds"] = static_cast<int>(interval_.count());
    status["analytics_interval_seconds"] = static_cast<int>(analytics_interval_.count());
    status["auto_generate_interval_seconds"] = static_cast<int>(auto_generate_interval_.count());

    status["process_scheduled_enabled"] = process_scheduled_enabled_;
    status["analytics_update_enabled"] = analytics_update_enabled_;
    status["auto_generate_enabled"] = auto_generate_enabled_;
    status["retry_processing_enabled"] = retry_processing_enabled_;

    status["statistics"] = getStatistics().toJson();

    // Time until next analytics update
    auto now = std::chrono::system_clock::now();
    auto analytics_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_analytics_update_);
    auto analytics_remaining = analytics_interval_ - analytics_elapsed;
    status["next_analytics_update_seconds"] = static_cast<int>(
        std::max(std::chrono::seconds(0), analytics_remaining).count());

    // Time until next auto-generate
    auto generate_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_auto_generate_);
    auto generate_remaining = auto_generate_interval_ - generate_elapsed;
    status["next_auto_generate_seconds"] = static_cast<int>(
        std::max(std::chrono::seconds(0), generate_remaining).count());

    return status;
}

// ============================================================================
// TIKTOKWORKER PRIVATE METHODS
// ============================================================================

bool TikTokWorker::shouldUpdateAnalytics() const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_analytics_update_);
    return elapsed >= analytics_interval_;
}

bool TikTokWorker::shouldAutoGenerate() const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_auto_generate_);
    return elapsed >= auto_generate_interval_;
}

} // namespace wtl::content::tiktok
