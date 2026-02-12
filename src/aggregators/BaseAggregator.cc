/**
 * @file BaseAggregator.cc
 * @brief Implementation of base aggregator class
 * @see BaseAggregator.h for documentation
 */

#include "aggregators/BaseAggregator.h"

#include "clients/HttpClient.h"
#include "clients/OAuth2Client.h"
#include "clients/RateLimiter.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

#include <thread>

namespace wtl::aggregators {

// ============================================================================
// AGGREGATOR CONFIG
// ============================================================================

AggregatorConfig AggregatorConfig::fromJson(const Json::Value& json) {
    AggregatorConfig config;

    config.name = json.get("name", "").asString();
    config.display_name = json.get("display_name", config.name).asString();
    config.description = json.get("description", "").asString();
    config.api_base_url = json.get("api_base_url", "").asString();
    config.api_key = json.get("api_key", "").asString();
    config.enabled = json.get("enabled", true).asBool();

    // OAuth2 settings
    config.use_oauth2 = json.get("use_oauth2", false).asBool();
    config.oauth2_token_url = json.get("oauth2_token_url", "").asString();
    config.oauth2_client_id = json.get("oauth2_client_id", "").asString();
    config.oauth2_client_secret = json.get("oauth2_client_secret", "").asString();
    config.oauth2_scope = json.get("oauth2_scope", "").asString();

    // Rate limiting
    config.rate_limit_requests = json.get("rate_limit_requests", 100).asInt();
    config.rate_limit_window_seconds = json.get("rate_limit_window_seconds", 60).asInt();
    config.rate_limit_burst = json.get("rate_limit_burst", 10).asInt();

    // Request settings
    config.request_timeout_seconds = json.get("request_timeout_seconds", 30).asInt();
    config.max_retries = json.get("max_retries", 3).asInt();
    config.page_size = json.get("page_size", 100).asInt();

    // Sync settings
    config.sync_interval_minutes = json.get("sync_interval_minutes", 60).asInt();
    config.max_dogs_per_sync = json.get("max_dogs_per_sync", 10000).asInt();
    config.max_shelters_per_sync = json.get("max_shelters_per_sync", 1000).asInt();

    return config;
}

Json::Value AggregatorConfig::toJson() const {
    Json::Value json;

    json["name"] = name;
    json["display_name"] = display_name;
    json["description"] = description;
    json["api_base_url"] = api_base_url;
    // Note: api_key and oauth2 secrets excluded for security
    json["enabled"] = enabled;

    json["use_oauth2"] = use_oauth2;
    json["oauth2_token_url"] = oauth2_token_url;
    // oauth2_client_id/secret excluded

    json["rate_limit_requests"] = rate_limit_requests;
    json["rate_limit_window_seconds"] = rate_limit_window_seconds;
    json["rate_limit_burst"] = rate_limit_burst;

    json["request_timeout_seconds"] = request_timeout_seconds;
    json["max_retries"] = max_retries;
    json["page_size"] = page_size;

    json["sync_interval_minutes"] = sync_interval_minutes;
    json["max_dogs_per_sync"] = max_dogs_per_sync;
    json["max_shelters_per_sync"] = max_shelters_per_sync;

    return json;
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

BaseAggregator::BaseAggregator(const std::string& name) {
    config_.name = name;
    config_.display_name = name;
    http_client_ = std::make_unique<clients::HttpClient>();
}

BaseAggregator::BaseAggregator(const AggregatorConfig& config)
    : config_(config) {
    http_client_ = std::make_unique<clients::HttpClient>(config.api_base_url);
    http_client_->setTimeout(config.request_timeout_seconds);
    http_client_->setRetryConfig(config.max_retries);

    // Setup rate limiter
    rate_limiter_ = std::make_unique<clients::RateLimiter>(
        config.name,
        config.rate_limit_requests,
        config.rate_limit_window_seconds,
        config.rate_limit_burst
    );

    // Setup OAuth2 if needed
    if (config.use_oauth2 && !config.oauth2_token_url.empty()) {
        oauth2_client_ = std::make_unique<clients::OAuth2Client>(
            config.oauth2_token_url,
            config.oauth2_client_id,
            config.oauth2_client_secret
        );
        oauth2_client_->setScope(config.oauth2_scope);
    }

    is_enabled_ = config.enabled;
}

BaseAggregator::~BaseAggregator() {
    // Request cancellation if sync is running
    if (is_syncing_) {
        cancelSync();
    }
}

// ============================================================================
// IAggregator INTERFACE - IDENTIFICATION
// ============================================================================

std::string BaseAggregator::getName() const {
    return config_.name;
}

std::string BaseAggregator::getDisplayName() const {
    return config_.display_name;
}

std::string BaseAggregator::getDescription() const {
    return config_.description;
}

AggregatorInfo BaseAggregator::getInfo() const {
    AggregatorInfo info;
    info.name = config_.name;
    info.display_name = config_.display_name;
    info.description = config_.description;
    info.api_url = config_.api_base_url;
    info.is_enabled = is_enabled_.load();
    info.status = status_.load();
    return info;
}

// ============================================================================
// IAggregator INTERFACE - CONFIGURATION
// ============================================================================

bool BaseAggregator::initialize(const Json::Value& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        config_ = AggregatorConfig::fromJson(config);

        // Setup HTTP client
        http_client_ = std::make_unique<clients::HttpClient>(config_.api_base_url);
        http_client_->setTimeout(config_.request_timeout_seconds);
        http_client_->setRetryConfig(config_.max_retries);

        // Setup rate limiter
        rate_limiter_ = std::make_unique<clients::RateLimiter>(
            config_.name,
            config_.rate_limit_requests,
            config_.rate_limit_window_seconds,
            config_.rate_limit_burst
        );

        // Setup OAuth2 if needed
        if (config_.use_oauth2 && !config_.oauth2_token_url.empty()) {
            oauth2_client_ = std::make_unique<clients::OAuth2Client>(
                config_.oauth2_token_url,
                config_.oauth2_client_id,
                config_.oauth2_client_secret
            );
            oauth2_client_->setScope(config_.oauth2_scope);
        }

        is_enabled_ = config_.enabled;
        is_configured_ = true;
        status_ = AggregatorStatus::HEALTHY;

        return true;
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize aggregator: " + std::string(e.what()),
            {{"aggregator", config_.name}}
        );
        status_ = AggregatorStatus::UNHEALTHY;
        return false;
    }
}

bool BaseAggregator::isConfigured() const {
    return is_configured_.load();
}

void BaseAggregator::setEnabled(bool enabled) {
    is_enabled_ = enabled;
    if (!enabled) {
        status_ = AggregatorStatus::DISABLED;
    } else if (status_ == AggregatorStatus::DISABLED) {
        status_ = AggregatorStatus::UNKNOWN;
    }
}

bool BaseAggregator::isEnabled() const {
    return is_enabled_.load();
}

// ============================================================================
// IAggregator INTERFACE - SYNC OPERATIONS
// ============================================================================

SyncStats BaseAggregator::sync() {
    // Lock to prevent concurrent syncs
    std::lock_guard<std::mutex> sync_lock(sync_mutex_);

    if (!is_enabled_) {
        SyncStats stats;
        stats.source_name = config_.name;
        stats.addError("DISABLED", "Aggregator is disabled");
        return stats;
    }

    if (!is_configured_) {
        SyncStats stats;
        stats.source_name = config_.name;
        stats.addError("NOT_CONFIGURED", "Aggregator is not configured");
        return stats;
    }

    is_syncing_ = true;
    cancel_requested_ = false;

    SyncStats stats;
    stats.source_name = config_.name;
    stats.is_incremental = false;
    stats.markStarted();

    reportProgress("Starting full sync", 0);

    try {
        // If using OAuth2, authenticate first
        if (oauth2_client_ && !oauth2_client_->isAuthenticated()) {
            reportProgress("Authenticating", 5);
            if (!oauth2_client_->authenticate()) {
                stats.addError("AUTH_FAILED", "OAuth2 authentication failed: " +
                               oauth2_client_->getLastError());
                stats.markComplete();
                is_syncing_ = false;
                recordSyncCompletion(stats);
                return stats;
            }
        }

        // Perform the actual sync
        stats = doSync();
        stats.source_name = config_.name;

        if (cancel_requested_) {
            stats.markCancelled();
        } else {
            stats.markComplete();
        }

        recordSyncCompletion(stats);

    } catch (const std::exception& e) {
        stats.addError("EXCEPTION", std::string("Sync failed: ") + e.what());
        stats.markComplete();

        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "Aggregator sync failed: " + std::string(e.what()),
            {{"aggregator", config_.name}}
        );

        recordSyncCompletion(stats);
    }

    is_syncing_ = false;
    return stats;
}

SyncStats BaseAggregator::syncSince(std::chrono::system_clock::time_point since) {
    std::lock_guard<std::mutex> sync_lock(sync_mutex_);

    if (!is_enabled_) {
        SyncStats stats;
        stats.source_name = config_.name;
        stats.addError("DISABLED", "Aggregator is disabled");
        return stats;
    }

    if (!is_configured_) {
        SyncStats stats;
        stats.source_name = config_.name;
        stats.addError("NOT_CONFIGURED", "Aggregator is not configured");
        return stats;
    }

    is_syncing_ = true;
    cancel_requested_ = false;

    SyncStats stats;
    stats.source_name = config_.name;
    stats.is_incremental = true;
    stats.markStarted();

    reportProgress("Starting incremental sync", 0);

    try {
        // Authenticate if needed
        if (oauth2_client_ && !oauth2_client_->isAuthenticated()) {
            reportProgress("Authenticating", 5);
            if (!oauth2_client_->authenticate()) {
                stats.addError("AUTH_FAILED", "OAuth2 authentication failed");
                stats.markComplete();
                is_syncing_ = false;
                recordSyncCompletion(stats);
                return stats;
            }
        }

        stats = doSyncSince(since);
        stats.source_name = config_.name;
        stats.is_incremental = true;

        if (cancel_requested_) {
            stats.markCancelled();
        } else {
            stats.markComplete();
        }

        recordSyncCompletion(stats);

    } catch (const std::exception& e) {
        stats.addError("EXCEPTION", std::string("Sync failed: ") + e.what());
        stats.markComplete();

        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "Aggregator incremental sync failed: " + std::string(e.what()),
            {{"aggregator", config_.name}}
        );

        recordSyncCompletion(stats);
    }

    is_syncing_ = false;
    return stats;
}

void BaseAggregator::cancelSync() {
    cancel_requested_ = true;
    reportProgress("Cancellation requested", -1);
}

bool BaseAggregator::isSyncing() const {
    return is_syncing_.load();
}

// ============================================================================
// IAggregator INTERFACE - STATISTICS
// ============================================================================

size_t BaseAggregator::getDogCount() const {
    return dog_count_.load();
}

size_t BaseAggregator::getShelterCount() const {
    return shelter_count_.load();
}

std::chrono::system_clock::time_point BaseAggregator::getLastSyncTime() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_sync_time_;
}

std::optional<std::chrono::system_clock::time_point> BaseAggregator::getNextSyncTime() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return next_sync_time_;
}

SyncStats BaseAggregator::getLastSyncStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_sync_stats_;
}

// ============================================================================
// IAggregator INTERFACE - HEALTH
// ============================================================================

bool BaseAggregator::isHealthy() const {
    return status_.load() == AggregatorStatus::HEALTHY;
}

AggregatorStatus BaseAggregator::getStatus() const {
    return status_.load();
}

Json::Value BaseAggregator::getHealthDetails() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value health;
    health["name"] = config_.name;
    health["status"] = aggregatorStatusToString(status_.load());
    health["is_enabled"] = is_enabled_.load();
    health["is_configured"] = is_configured_.load();
    health["is_syncing"] = is_syncing_.load();
    health["dog_count"] = static_cast<Json::UInt64>(dog_count_.load());
    health["shelter_count"] = static_cast<Json::UInt64>(shelter_count_.load());
    health["consecutive_failures"] = consecutive_failures_;

    if (last_sync_time_.time_since_epoch().count() > 0) {
        auto time_t_val = std::chrono::system_clock::to_time_t(last_sync_time_);
        char buffer[30];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t_val));
        health["last_sync_time"] = std::string(buffer);
    }

    if (!last_health_details_.empty()) {
        health["details"] = last_health_details_;
    }

    // Rate limiter stats
    if (rate_limiter_) {
        health["rate_limiter"] = rate_limiter_->getStats();
    }

    // OAuth2 status
    if (oauth2_client_) {
        health["oauth2"] = oauth2_client_->getStatus();
    }

    return health;
}

bool BaseAggregator::testConnection() {
    if (!is_configured_) {
        return false;
    }

    try {
        // Authenticate if needed
        if (oauth2_client_ && !oauth2_client_->isAuthenticated()) {
            if (!oauth2_client_->authenticate()) {
                recordHealthCheck(false, "Authentication failed");
                return false;
            }
        }

        bool result = doTestConnection();
        recordHealthCheck(result, result ? "Connection successful" : "Connection failed");
        return result;

    } catch (const std::exception& e) {
        recordHealthCheck(false, std::string("Exception: ") + e.what());
        return false;
    }
}

// ============================================================================
// ADDITIONAL METHODS
// ============================================================================

void BaseAggregator::setProgressCallback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_callback_ = std::move(callback);
}

AggregatorConfig BaseAggregator::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

// ============================================================================
// PROTECTED METHODS
// ============================================================================

clients::HttpClient& BaseAggregator::getHttpClient() {
    return *http_client_;
}

clients::RateLimiter* BaseAggregator::getRateLimiter() {
    return rate_limiter_.get();
}

clients::OAuth2Client* BaseAggregator::getOAuth2Client() {
    return oauth2_client_.get();
}

bool BaseAggregator::shouldCancel() const {
    return cancel_requested_.load();
}

void BaseAggregator::reportProgress(const std::string& status, int percent) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (progress_callback_) {
        progress_callback_(status, percent);
    }
}

bool BaseAggregator::waitForRateLimit() {
    if (!rate_limiter_) {
        return true;
    }

    // Check for cancellation while waiting
    while (!rate_limiter_->tryAcquire()) {
        if (shouldCancel()) {
            return false;
        }

        auto wait_time = rate_limiter_->getWaitTime();
        if (wait_time.count() > 0) {
            // Wait in small increments to allow cancellation
            auto sleep_time = std::min(wait_time, std::chrono::milliseconds(100));
            std::this_thread::sleep_for(sleep_time);
        }
    }

    return true;
}

clients::HttpResponse BaseAggregator::apiGet(
    const std::string& path,
    const std::unordered_map<std::string, std::string>& params) {

    // Wait for rate limit
    if (!waitForRateLimit()) {
        clients::HttpResponse response;
        response.success = false;
        response.error_message = "Cancelled while waiting for rate limit";
        return response;
    }

    // Add auth header if using OAuth2
    clients::HttpClient::Headers headers;
    if (oauth2_client_) {
        std::string auth = oauth2_client_->getAuthorizationHeader();
        if (!auth.empty()) {
            headers["Authorization"] = auth;
        }
    } else if (!config_.api_key.empty()) {
        // Use API key header (varies by API)
        headers["x-api-key"] = config_.api_key;
    }

    return http_client_->get(path, params, headers);
}

clients::HttpResponse BaseAggregator::apiPost(
    const std::string& path,
    const Json::Value& body) {

    if (!waitForRateLimit()) {
        clients::HttpResponse response;
        response.success = false;
        response.error_message = "Cancelled while waiting for rate limit";
        return response;
    }

    clients::HttpClient::Headers headers;
    if (oauth2_client_) {
        std::string auth = oauth2_client_->getAuthorizationHeader();
        if (!auth.empty()) {
            headers["Authorization"] = auth;
        }
    } else if (!config_.api_key.empty()) {
        headers["x-api-key"] = config_.api_key;
    }

    return http_client_->post(path, body, headers);
}

size_t BaseAggregator::fetchAllPages(
    const std::string& path,
    std::unordered_map<std::string, std::string> params,
    std::function<bool(const Json::Value&)> process) {

    pagination_ = PaginationState{};
    pagination_.page_size = config_.page_size;
    size_t total_fetched = 0;

    while (pagination_.has_more && !shouldCancel()) {
        // Set pagination params
        params["page"] = std::to_string(pagination_.current_page);
        params["limit"] = std::to_string(pagination_.page_size);

        auto response = apiGet(path, params);

        if (!response.isOk()) {
            break;
        }

        auto json = response.json();
        if (json.isNull()) {
            break;
        }

        // Process this page
        bool continue_paging = process(json);

        // Update pagination state (subclass should update from response)
        pagination_.items_fetched += pagination_.page_size;  // Approximate
        total_fetched = pagination_.items_fetched;

        if (!continue_paging) {
            break;
        }

        // Check if there are more pages
        if (pagination_.total_pages > 0 &&
            pagination_.current_page >= pagination_.total_pages) {
            pagination_.has_more = false;
        } else if (!pagination_.next_cursor.empty()) {
            // Cursor-based pagination
            params["cursor"] = pagination_.next_cursor;
        } else {
            pagination_.current_page++;
        }
    }

    return total_fetched;
}

PaginationState BaseAggregator::getPaginationState() const {
    return pagination_;
}

void BaseAggregator::setDogCount(size_t count) {
    dog_count_ = count;
}

void BaseAggregator::setShelterCount(size_t count) {
    shelter_count_ = count;
}

void BaseAggregator::recordSyncCompletion(const SyncStats& stats) {
    std::lock_guard<std::mutex> lock(mutex_);

    last_sync_stats_ = stats;
    last_sync_time_ = std::chrono::system_clock::now();

    // Schedule next sync
    next_sync_time_ = last_sync_time_ +
        std::chrono::minutes(config_.sync_interval_minutes);

    // Update health status based on sync result
    if (stats.isSuccessful()) {
        consecutive_failures_ = 0;
        status_ = AggregatorStatus::HEALTHY;
    } else {
        consecutive_failures_++;
        if (consecutive_failures_ >= 3) {
            status_ = AggregatorStatus::UNHEALTHY;
        } else {
            status_ = AggregatorStatus::DEGRADED;
        }
    }
}

void BaseAggregator::recordHealthCheck(bool healthy, const std::string& details) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_health_details_ = details;

    if (healthy) {
        if (status_ != AggregatorStatus::DISABLED) {
            status_ = AggregatorStatus::HEALTHY;
        }
    } else {
        if (status_ != AggregatorStatus::DISABLED) {
            status_ = AggregatorStatus::UNHEALTHY;
        }
    }
}

} // namespace wtl::aggregators
