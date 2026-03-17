/**
 * @file RateLimiter.cc
 * @brief Implementation of rate limiting for API calls
 * @see RateLimiter.h for documentation
 */

#include "clients/RateLimiter.h"

#include <algorithm>
#include <cmath>

#include "core/debug/ErrorCapture.h"

namespace wtl::clients {

// ============================================================================
// RATE LIMITER CONSTRUCTION
// ============================================================================

RateLimiter::RateLimiter(const std::string& name,
                         size_t max_requests,
                         int window_seconds,
                         size_t burst_size)
    : name_(name)
    , max_requests_(max_requests)
    , window_(std::chrono::seconds(window_seconds))
    , burst_size_(burst_size == 0 ? max_requests / 10 : burst_size)
    , allow_wait_(true) {

    // Calculate max tokens (regular + burst)
    max_tokens_ = static_cast<double>(max_requests_) + static_cast<double>(burst_size_);
    tokens_ = max_tokens_;

    // Calculate refill rate: tokens per millisecond
    // max_requests tokens over window seconds
    double window_ms = static_cast<double>(window_.count()) * 1000.0;
    refill_rate_ = static_cast<double>(max_requests_) / window_ms;

    last_refill_ = std::chrono::steady_clock::now();
}

RateLimiter::RateLimiter(const RateLimitConfig& config)
    : RateLimiter(config.name, config.max_requests,
                  static_cast<int>(config.window.count()),
                  config.burst_size) {
    allow_wait_ = config.allow_wait;
}

RateLimiter::RateLimiter()
    : name_("default")
    , max_requests_(100)
    , window_(std::chrono::seconds(60))
    , burst_size_(10)
    , allow_wait_(true)
    , tokens_(110.0)
    , max_tokens_(110.0)
    , refill_rate_(100.0 / 60000.0)
    , last_refill_(std::chrono::steady_clock::now()) {
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void RateLimiter::configure(const RateLimitConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    name_ = config.name;
    max_requests_ = config.max_requests;
    window_ = config.window;
    burst_size_ = config.burst_size;
    allow_wait_ = config.allow_wait;

    max_tokens_ = static_cast<double>(max_requests_) + static_cast<double>(burst_size_);
    tokens_ = max_tokens_;

    double window_ms = static_cast<double>(window_.count()) * 1000.0;
    refill_rate_ = static_cast<double>(max_requests_) / window_ms;

    last_refill_ = std::chrono::steady_clock::now();
}

RateLimitConfig RateLimiter::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    RateLimitConfig config;
    config.name = name_;
    config.max_requests = max_requests_;
    config.window = window_;
    config.burst_size = burst_size_;
    config.allow_wait = allow_wait_;
    return config;
}

void RateLimiter::setAllowWait(bool allow) {
    std::lock_guard<std::mutex> lock(mutex_);
    allow_wait_ = allow;
}

// ============================================================================
// RATE LIMITING
// ============================================================================

bool RateLimiter::tryAcquire() {
    return tryAcquire(1);
}

bool RateLimiter::tryAcquire(size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);

    refillTokens();

    double requested = static_cast<double>(count);
    if (tokens_ >= requested) {
        tokens_ -= requested;
        total_requests_ += count;
        return true;
    }

    rate_limit_hits_++;
    return false;
}

bool RateLimiter::acquire() {
    total_requests_++;

    std::unique_lock<std::mutex> lock(mutex_);

    while (true) {
        refillTokens();

        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }

        if (!allow_wait_) {
            rate_limit_hits_++;
            return false;
        }

        // Calculate wait time for next token
        double needed = 1.0 - tokens_;
        double wait_ms = needed / refill_rate_;
        auto wait_duration = std::chrono::milliseconds(static_cast<long long>(std::ceil(wait_ms)));

        total_wait_time_ms_ += static_cast<size_t>(wait_duration.count());

        // Wait with timeout to allow for periodic refill checks
        condition_.wait_for(lock, std::min(wait_duration, std::chrono::milliseconds(100)));
    }
}

bool RateLimiter::acquireWithTimeout(int timeout_ms) {
    total_requests_++;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    std::unique_lock<std::mutex> lock(mutex_);

    while (true) {
        refillTokens();

        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }

        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            rate_limit_hits_++;
            return false;
        }

        // Calculate wait time
        double needed = 1.0 - tokens_;
        double wait_ms = needed / refill_rate_;
        auto wait_duration = std::chrono::milliseconds(static_cast<long long>(std::ceil(wait_ms)));

        // Don't wait longer than remaining timeout
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        wait_duration = std::min(wait_duration, remaining);

        if (wait_duration.count() <= 0) {
            rate_limit_hits_++;
            return false;
        }

        total_wait_time_ms_ += static_cast<size_t>(wait_duration.count());
        condition_.wait_for(lock, wait_duration);
    }
}

std::chrono::milliseconds RateLimiter::getWaitTime() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (tokens_ >= 1.0) {
        return std::chrono::milliseconds(0);
    }

    double needed = 1.0 - tokens_;
    double wait_ms = needed / refill_rate_;
    return std::chrono::milliseconds(static_cast<long long>(std::ceil(wait_ms)));
}

bool RateLimiter::isLimited() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Const cast to call non-const refillTokens
    const_cast<RateLimiter*>(this)->refillTokens();
    return tokens_ < 1.0;
}

// ============================================================================
// STATISTICS
// ============================================================================

size_t RateLimiter::getAvailableTokens() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const_cast<RateLimiter*>(this)->refillTokens();
    return static_cast<size_t>(tokens_);
}

size_t RateLimiter::getTotalRequests() const {
    return total_requests_.load();
}

size_t RateLimiter::getRateLimitHits() const {
    return rate_limit_hits_.load();
}

Json::Value RateLimiter::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    stats["name"] = name_;
    stats["max_requests"] = static_cast<Json::UInt>(max_requests_);
    stats["window_seconds"] = static_cast<Json::Int64>(window_.count());
    stats["burst_size"] = static_cast<Json::UInt>(burst_size_);
    stats["allow_wait"] = allow_wait_;

    stats["available_tokens"] = static_cast<Json::UInt>(static_cast<size_t>(tokens_));
    stats["max_tokens"] = static_cast<Json::UInt>(static_cast<size_t>(max_tokens_));
    stats["refill_rate_per_second"] = refill_rate_ * 1000.0;

    stats["total_requests"] = static_cast<Json::UInt64>(total_requests_.load());
    stats["rate_limit_hits"] = static_cast<Json::UInt64>(rate_limit_hits_.load());
    stats["total_wait_time_ms"] = static_cast<Json::UInt64>(total_wait_time_ms_.load());

    double hit_rate = total_requests_ > 0
        ? (static_cast<double>(rate_limit_hits_) / static_cast<double>(total_requests_)) * 100.0
        : 0.0;
    stats["hit_rate_percent"] = hit_rate;

    return stats;
}

void RateLimiter::resetStats() {
    total_requests_ = 0;
    rate_limit_hits_ = 0;
    total_wait_time_ms_ = 0;
}

// ============================================================================
// MANAGEMENT
// ============================================================================

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    tokens_ = max_tokens_;
    last_refill_ = std::chrono::steady_clock::now();
    condition_.notify_all();
}

std::string RateLimiter::getName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return name_;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void RateLimiter::refillTokens() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_refill_);

    if (elapsed.count() > 0) {
        double tokens_to_add = static_cast<double>(elapsed.count()) * refill_rate_;
        tokens_ = std::min(tokens_ + tokens_to_add, max_tokens_);
        last_refill_ = now;
    }
}

size_t RateLimiter::calculateTokensToAdd(std::chrono::milliseconds elapsed) const {
    double tokens_to_add = static_cast<double>(elapsed.count()) * refill_rate_;
    return static_cast<size_t>(tokens_to_add);
}

// ============================================================================
// RATE LIMITER MANAGER
// ============================================================================

RateLimiterManager& RateLimiterManager::getInstance() {
    static RateLimiterManager instance;
    return instance;
}

void RateLimiterManager::registerLimiter(const std::string& name,
                                          size_t max_requests,
                                          int window_seconds,
                                          size_t burst_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    limiters_[name] = std::make_unique<RateLimiter>(name, max_requests, window_seconds, burst_size);
}

void RateLimiterManager::registerLimiter(const RateLimitConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    limiters_[config.name] = std::make_unique<RateLimiter>(config);
}

RateLimiter* RateLimiterManager::getLimiter(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = limiters_.find(name);
    return it != limiters_.end() ? it->second.get() : nullptr;
}

bool RateLimiterManager::hasLimiter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return limiters_.find(name) != limiters_.end();
}

void RateLimiterManager::removeLimiter(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    limiters_.erase(name);
}

bool RateLimiterManager::tryAcquire(const std::string& name) {
    RateLimiter* limiter = getLimiter(name);
    if (!limiter) {
        return true;  // No limiter means no limit
    }
    return limiter->tryAcquire();
}

bool RateLimiterManager::acquire(const std::string& name, int timeout_ms) {
    RateLimiter* limiter = getLimiter(name);
    if (!limiter) {
        return true;  // No limiter means no limit
    }

    if (timeout_ms > 0) {
        return limiter->acquireWithTimeout(timeout_ms);
    }
    return limiter->acquire();
}

void RateLimiterManager::loadFromJson(const Json::Value& config) {
    if (!config.isArray()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& item : config) {
        auto limiter_config = RateLimitConfig::fromJson(item);
        limiters_[limiter_config.name] = std::make_unique<RateLimiter>(limiter_config);
    }
}

Json::Value RateLimiterManager::getAllStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;
    for (const auto& [name, limiter] : limiters_) {
        stats[name] = limiter->getStats();
    }
    return stats;
}

void RateLimiterManager::resetAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, limiter] : limiters_) {
        limiter->reset();
    }
}

std::vector<std::string> RateLimiterManager::getLimiterNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(limiters_.size());
    for (const auto& [name, _] : limiters_) {
        names.push_back(name);
    }
    return names;
}

} // namespace wtl::clients
