/**
 * @file BaseAggregator.h
 * @brief Base implementation for data source aggregators
 *
 * PURPOSE:
 * Provides common functionality for all aggregators including HTTP client
 * setup, rate limiting, error handling, pagination support, and progress
 * tracking. Concrete aggregators extend this class.
 *
 * USAGE:
 * class MyAggregator : public BaseAggregator {
 *     SyncStats doSync() override { ... }
 *     SyncStats doSyncSince(time_point since) override { ... }
 * };
 *
 * DEPENDENCIES:
 * - IAggregator interface
 * - HttpClient for API requests
 * - RateLimiter for rate limiting
 * - ConnectionPool for database access
 * - DataMapper for data transformation
 *
 * MODIFICATION GUIDE:
 * - Override doSync() and doSyncSince() in subclasses
 * - Add common functionality here that applies to all aggregators
 * - Keep aggregator-specific logic in subclasses
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

// Project includes
#include "aggregators/IAggregator.h"
#include "aggregators/SyncStats.h"

// Forward declarations
namespace wtl::clients {
    class HttpClient;
    class RateLimiter;
    class OAuth2Client;
    struct HttpResponse;
}

namespace wtl::aggregators {

/**
 * @struct AggregatorConfig
 * @brief Configuration for an aggregator
 */
struct AggregatorConfig {
    std::string name;                  ///< Unique aggregator name
    std::string display_name;          ///< Human-readable name
    std::string description;           ///< Description
    std::string api_base_url;          ///< Base URL for API
    std::string api_key;               ///< API key (if using simple auth)
    bool enabled{true};                ///< Whether aggregator is enabled

    // OAuth2 settings (optional)
    bool use_oauth2{false};            ///< Use OAuth2 authentication
    std::string oauth2_token_url;      ///< OAuth2 token URL
    std::string oauth2_client_id;      ///< OAuth2 client ID
    std::string oauth2_client_secret;  ///< OAuth2 client secret
    std::string oauth2_scope;          ///< OAuth2 scope

    // Rate limiting
    int rate_limit_requests{100};      ///< Requests per window
    int rate_limit_window_seconds{60}; ///< Rate limit window
    int rate_limit_burst{10};          ///< Burst size

    // Request settings
    int request_timeout_seconds{30};   ///< Request timeout
    int max_retries{3};                ///< Max retry attempts
    int page_size{100};                ///< Items per page

    // Sync settings
    int sync_interval_minutes{60};     ///< Minutes between syncs
    int max_dogs_per_sync{10000};      ///< Maximum dogs to sync
    int max_shelters_per_sync{1000};   ///< Maximum shelters to sync

    /**
     * @brief Create from JSON
     * @param json Configuration JSON
     * @return AggregatorConfig
     */
    static AggregatorConfig fromJson(const Json::Value& json);

    /**
     * @brief Convert to JSON (excludes secrets)
     * @return JSON representation
     */
    Json::Value toJson() const;
};

/**
 * @struct PaginationState
 * @brief Tracks pagination state during sync
 */
struct PaginationState {
    int current_page{1};
    int total_pages{0};
    int page_size{100};
    int total_items{0};
    int items_fetched{0};
    bool has_more{true};
    std::string next_cursor;           ///< For cursor-based pagination

    /**
     * @brief Get progress percentage
     * @return Progress as 0-100
     */
    int getProgressPercent() const {
        if (total_items == 0) return 0;
        return static_cast<int>((items_fetched * 100) / total_items);
    }
};

/**
 * @class BaseAggregator
 * @brief Base class for data source aggregators
 *
 * Provides common functionality for all aggregators. Subclasses must
 * implement doSync() and doSyncSince() to perform the actual sync.
 */
class BaseAggregator : public IAggregator {
public:
    // Type for progress callback
    using ProgressCallback = std::function<void(const std::string& status, int percent)>;

    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Construct with name
     * @param name Unique aggregator name
     */
    explicit BaseAggregator(const std::string& name);

    /**
     * @brief Construct with configuration
     * @param config Aggregator configuration
     */
    explicit BaseAggregator(const AggregatorConfig& config);

    /**
     * @brief Virtual destructor
     */
    virtual ~BaseAggregator();

    // =========================================================================
    // IAggregator INTERFACE IMPLEMENTATION
    // =========================================================================

    std::string getName() const override;
    std::string getDisplayName() const override;
    std::string getDescription() const override;
    AggregatorInfo getInfo() const override;

    bool initialize(const Json::Value& config) override;
    bool isConfigured() const override;
    void setEnabled(bool enabled) override;
    bool isEnabled() const override;

    SyncStats sync() override;
    SyncStats syncSince(std::chrono::system_clock::time_point since) override;
    void cancelSync() override;
    bool isSyncing() const override;

    size_t getDogCount() const override;
    size_t getShelterCount() const override;
    std::chrono::system_clock::time_point getLastSyncTime() const override;
    std::optional<std::chrono::system_clock::time_point> getNextSyncTime() const override;
    SyncStats getLastSyncStats() const override;

    bool isHealthy() const override;
    AggregatorStatus getStatus() const override;
    Json::Value getHealthDetails() const override;
    bool testConnection() override;

    // =========================================================================
    // ADDITIONAL METHODS
    // =========================================================================

    /**
     * @brief Set progress callback
     * @param callback Callback for progress updates
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    AggregatorConfig getConfig() const;

protected:
    // =========================================================================
    // ABSTRACT METHODS (Subclasses must implement)
    // =========================================================================

    /**
     * @brief Perform full sync
     * @return Sync statistics
     *
     * Subclasses implement this to fetch all data from the source.
     */
    virtual SyncStats doSync() = 0;

    /**
     * @brief Perform incremental sync
     * @param since Sync records modified after this time
     * @return Sync statistics
     *
     * Subclasses implement this for incremental updates.
     */
    virtual SyncStats doSyncSince(std::chrono::system_clock::time_point since) = 0;

    /**
     * @brief Perform connection test
     * @return true if connection successful
     *
     * Subclasses implement this to test API connectivity.
     */
    virtual bool doTestConnection() = 0;

    // =========================================================================
    // PROTECTED HELPER METHODS
    // =========================================================================

    /**
     * @brief Get HTTP client for API requests
     * @return Reference to HTTP client
     */
    clients::HttpClient& getHttpClient();

    /**
     * @brief Get rate limiter
     * @return Pointer to rate limiter
     */
    clients::RateLimiter* getRateLimiter();

    /**
     * @brief Get OAuth2 client (if using OAuth2)
     * @return Pointer to OAuth2 client, or nullptr
     */
    clients::OAuth2Client* getOAuth2Client();

    /**
     * @brief Check if sync should be cancelled
     * @return true if cancellation requested
     */
    bool shouldCancel() const;

    /**
     * @brief Report progress
     * @param status Status message
     * @param percent Progress percentage (0-100)
     */
    void reportProgress(const std::string& status, int percent);

    /**
     * @brief Wait for rate limit
     * @return true if can proceed, false if cancelled
     */
    bool waitForRateLimit();

    /**
     * @brief Make API request with rate limiting
     * @param path API path
     * @param params Query parameters
     * @return HTTP response
     */
    clients::HttpResponse apiGet(const std::string& path,
                                  const std::unordered_map<std::string, std::string>& params = {});

    /**
     * @brief Make API POST request
     * @param path API path
     * @param body Request body
     * @return HTTP response
     */
    clients::HttpResponse apiPost(const std::string& path,
                                   const Json::Value& body);

    /**
     * @brief Fetch all pages of data
     * @param path API path
     * @param params Initial query parameters
     * @param process Callback to process each page
     * @return Total items fetched
     *
     * Handles pagination automatically, calling process() for each page.
     */
    size_t fetchAllPages(const std::string& path,
                         std::unordered_map<std::string, std::string> params,
                         std::function<bool(const Json::Value&)> process);

    /**
     * @brief Get current pagination state
     * @return Pagination state
     */
    PaginationState getPaginationState() const;

    /**
     * @brief Update dog count
     * @param count New count
     */
    void setDogCount(size_t count);

    /**
     * @brief Update shelter count
     * @param count New count
     */
    void setShelterCount(size_t count);

    /**
     * @brief Record sync completion
     * @param stats Sync statistics
     */
    void recordSyncCompletion(const SyncStats& stats);

    /**
     * @brief Record health check result
     * @param healthy Whether check passed
     * @param details Details of check
     */
    void recordHealthCheck(bool healthy, const std::string& details);

    // =========================================================================
    // PROTECTED MEMBERS
    // =========================================================================

    AggregatorConfig config_;
    SyncStats last_sync_stats_;
    PaginationState pagination_;

    // Counts
    std::atomic<size_t> dog_count_{0};
    std::atomic<size_t> shelter_count_{0};

    // Timing
    std::chrono::system_clock::time_point last_sync_time_;
    std::optional<std::chrono::system_clock::time_point> next_sync_time_;

    // Status
    std::atomic<AggregatorStatus> status_{AggregatorStatus::UNKNOWN};
    std::string last_health_details_;
    int consecutive_failures_{0};

private:
    // HTTP client and rate limiter
    std::unique_ptr<clients::HttpClient> http_client_;
    std::unique_ptr<clients::RateLimiter> rate_limiter_;
    std::unique_ptr<clients::OAuth2Client> oauth2_client_;

    // State
    std::atomic<bool> is_syncing_{false};
    std::atomic<bool> cancel_requested_{false};
    std::atomic<bool> is_configured_{false};
    std::atomic<bool> is_enabled_{true};

    // Progress callback
    ProgressCallback progress_callback_;

    // Thread safety
    mutable std::mutex mutex_;
    mutable std::mutex sync_mutex_;
};

} // namespace wtl::aggregators
