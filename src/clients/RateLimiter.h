/**
 * @file RateLimiter.h
 * @brief Rate limiting for API calls using token bucket algorithm
 *
 * PURPOSE:
 * Prevents exceeding API rate limits by tracking and controlling
 * the rate of outgoing requests. Uses token bucket algorithm for
 * smooth rate limiting with burst support.
 *
 * USAGE:
 * RateLimiter limiter("rescuegroups", 1000, 60);  // 1000 requests per 60 seconds
 * if (limiter.tryAcquire()) {
 *     // Make API call
 * } else {
 *     // Wait or reject
 * }
 *
 * DEPENDENCIES:
 * - Standard library (chrono, mutex)
 * - jsoncpp for configuration
 *
 * MODIFICATION GUIDE:
 * - Adjust default bucket sizes as needed
 * - Add new rate limiting strategies if required
 * - Monitor and log rate limit hits
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>

// Third-party includes
#include <json/json.h>

namespace wtl::clients {

/**
 * @struct RateLimitConfig
 * @brief Configuration for a rate limiter
 */
struct RateLimitConfig {
    std::string name;                  ///< Identifier for this limiter
    size_t max_requests{100};          ///< Maximum requests allowed in window
    std::chrono::seconds window{60};   ///< Time window for rate limit
    size_t burst_size{10};             ///< Additional burst capacity
    bool allow_wait{true};             ///< Whether to allow waiting vs rejecting

    /**
     * @brief Create from JSON
     * @param json Configuration JSON
     * @return RateLimitConfig
     */
    static RateLimitConfig fromJson(const Json::Value& json) {
        RateLimitConfig config;
        config.name = json.get("name", "default").asString();
        config.max_requests = json.get("max_requests", 100).asUInt();
        config.window = std::chrono::seconds(json.get("window_seconds", 60).asUInt());
        config.burst_size = json.get("burst_size", 10).asUInt();
        config.allow_wait = json.get("allow_wait", true).asBool();
        return config;
    }

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["name"] = name;
        json["max_requests"] = static_cast<Json::UInt>(max_requests);
        json["window_seconds"] = static_cast<Json::Int64>(window.count());
        json["burst_size"] = static_cast<Json::UInt>(burst_size);
        json["allow_wait"] = allow_wait;
        return json;
    }
};

/**
 * @class RateLimiter
 * @brief Token bucket rate limiter for API calls
 *
 * Implements the token bucket algorithm where tokens are added at a
 * steady rate and requests consume tokens. Supports burst capacity
 * and configurable wait behavior.
 */
class RateLimiter {
public:
    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Construct a rate limiter with specified limits
     *
     * @param name Identifier for this limiter
     * @param max_requests Maximum requests allowed in window
     * @param window_seconds Time window in seconds
     * @param burst_size Additional burst capacity (default: 10% of max)
     */
    RateLimiter(const std::string& name,
                size_t max_requests,
                int window_seconds,
                size_t burst_size = 0);

    /**
     * @brief Construct from configuration
     * @param config Rate limit configuration
     */
    explicit RateLimiter(const RateLimitConfig& config);

    /**
     * @brief Default constructor (must call configure before use)
     */
    RateLimiter();

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Configure the rate limiter
     * @param config Configuration to apply
     */
    void configure(const RateLimitConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    RateLimitConfig getConfig() const;

    /**
     * @brief Set whether to allow waiting when rate limited
     * @param allow true to allow waiting, false to reject immediately
     */
    void setAllowWait(bool allow);

    // =========================================================================
    // RATE LIMITING
    // =========================================================================

    /**
     * @brief Try to acquire a token without waiting
     *
     * @return true if token acquired, false if rate limited
     *
     * Non-blocking. Returns immediately with success or failure.
     */
    bool tryAcquire();

    /**
     * @brief Try to acquire multiple tokens without waiting
     *
     * @param count Number of tokens to acquire
     * @return true if all tokens acquired, false otherwise
     */
    bool tryAcquire(size_t count);

    /**
     * @brief Acquire a token, waiting if necessary
     *
     * @return true if acquired, false if waiting is disabled
     *
     * Blocks until a token is available if allow_wait is true.
     * Returns false immediately if allow_wait is false and no tokens.
     */
    bool acquire();

    /**
     * @brief Acquire a token with timeout
     *
     * @param timeout_ms Maximum milliseconds to wait
     * @return true if acquired within timeout, false otherwise
     */
    bool acquireWithTimeout(int timeout_ms);

    /**
     * @brief Get estimated wait time for next token
     *
     * @return Milliseconds until next token available, 0 if available now
     */
    std::chrono::milliseconds getWaitTime() const;

    /**
     * @brief Check if currently rate limited
     * @return true if no tokens available
     */
    bool isLimited() const;

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get number of available tokens
     * @return Current token count
     */
    size_t getAvailableTokens() const;

    /**
     * @brief Get total requests made
     * @return Total request count
     */
    size_t getTotalRequests() const;

    /**
     * @brief Get number of times rate limit was hit
     * @return Rate limit hit count
     */
    size_t getRateLimitHits() const;

    /**
     * @brief Get statistics as JSON
     * @return JSON with statistics
     */
    Json::Value getStats() const;

    /**
     * @brief Reset statistics (not tokens)
     */
    void resetStats();

    // =========================================================================
    // MANAGEMENT
    // =========================================================================

    /**
     * @brief Reset the rate limiter to full capacity
     *
     * Refills all tokens and resets the refill timer.
     * Use when rate limit window has passed.
     */
    void reset();

    /**
     * @brief Get the limiter name
     * @return Name string
     */
    std::string getName() const;

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    /**
     * @brief Refill tokens based on elapsed time
     */
    void refillTokens();

    /**
     * @brief Calculate tokens to add based on elapsed time
     * @param elapsed Time since last refill
     * @return Number of tokens to add
     */
    size_t calculateTokensToAdd(std::chrono::milliseconds elapsed) const;

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    std::string name_;
    size_t max_requests_;
    std::chrono::seconds window_;
    size_t burst_size_;
    bool allow_wait_;

    // Token bucket state
    double tokens_;
    double max_tokens_;
    double refill_rate_;  // Tokens per millisecond
    std::chrono::steady_clock::time_point last_refill_;

    // Statistics
    std::atomic<size_t> total_requests_{0};
    std::atomic<size_t> rate_limit_hits_{0};
    std::atomic<size_t> total_wait_time_ms_{0};

    // Thread safety
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};

/**
 * @class RateLimiterManager
 * @brief Manages multiple rate limiters for different APIs
 *
 * Singleton that holds rate limiters for each external API,
 * allowing centralized configuration and monitoring.
 */
class RateLimiterManager {
public:
    // =========================================================================
    // SINGLETON
    // =========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to the manager
     */
    static RateLimiterManager& getInstance();

    // Delete copy/move
    RateLimiterManager(const RateLimiterManager&) = delete;
    RateLimiterManager& operator=(const RateLimiterManager&) = delete;

    // =========================================================================
    // LIMITER MANAGEMENT
    // =========================================================================

    /**
     * @brief Register a rate limiter
     *
     * @param name Unique name for the limiter
     * @param max_requests Maximum requests in window
     * @param window_seconds Time window in seconds
     * @param burst_size Burst capacity
     */
    void registerLimiter(const std::string& name,
                         size_t max_requests,
                         int window_seconds,
                         size_t burst_size = 0);

    /**
     * @brief Register a limiter from config
     * @param config Rate limit configuration
     */
    void registerLimiter(const RateLimitConfig& config);

    /**
     * @brief Get a rate limiter by name
     *
     * @param name Limiter name
     * @return Pointer to limiter, or nullptr if not found
     */
    RateLimiter* getLimiter(const std::string& name);

    /**
     * @brief Check if a limiter exists
     * @param name Limiter name
     * @return true if exists
     */
    bool hasLimiter(const std::string& name) const;

    /**
     * @brief Remove a rate limiter
     * @param name Limiter name
     */
    void removeLimiter(const std::string& name);

    /**
     * @brief Try to acquire from a named limiter
     *
     * @param name Limiter name
     * @return true if acquired, false if limited or limiter not found
     */
    bool tryAcquire(const std::string& name);

    /**
     * @brief Acquire from a named limiter with wait
     *
     * @param name Limiter name
     * @param timeout_ms Timeout in milliseconds (0 = no timeout)
     * @return true if acquired
     */
    bool acquire(const std::string& name, int timeout_ms = 0);

    // =========================================================================
    // BULK OPERATIONS
    // =========================================================================

    /**
     * @brief Load limiters from JSON configuration
     * @param config JSON array of limiter configs
     */
    void loadFromJson(const Json::Value& config);

    /**
     * @brief Get all limiter statistics
     * @return JSON with all limiter stats
     */
    Json::Value getAllStats() const;

    /**
     * @brief Reset all limiters
     */
    void resetAll();

    /**
     * @brief Get list of all limiter names
     * @return Vector of names
     */
    std::vector<std::string> getLimiterNames() const;

private:
    RateLimiterManager() = default;

    std::unordered_map<std::string, std::unique_ptr<RateLimiter>> limiters_;
    mutable std::mutex mutex_;
};

} // namespace wtl::clients
