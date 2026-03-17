/**
 * @file AggregatorManager.cc
 * @brief Implementation of aggregator manager
 * @see AggregatorManager.h for documentation
 */

#include "aggregators/AggregatorManager.h"

#include <fstream>

#include "aggregators/RescueGroupsAggregator.h"
#include "aggregators/ShelterDirectAggregator.h"
#include "core/debug/ErrorCapture.h"

namespace wtl::aggregators {

// ============================================================================
// SINGLETON
// ============================================================================

AggregatorManager& AggregatorManager::getInstance() {
    static AggregatorManager instance;
    return instance;
}

AggregatorManager::~AggregatorManager() {
    shutdown();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool AggregatorManager::initialize(const Json::Value& config) {
    if (is_initialized_) {
        return true;
    }

    try {
        // Register aggregators from config
        if (config.isMember("aggregators") && config["aggregators"].isArray()) {
            for (const auto& agg_config : config["aggregators"]) {
                auto aggregator = createAggregator(agg_config);
                if (aggregator) {
                    std::string name = agg_config.get("name", "").asString();
                    if (!name.empty()) {
                        registerAggregator(name, std::move(aggregator));
                    }
                }
            }
        }

        // Start auto-sync if configured
        if (config.isMember("auto_sync")) {
            bool enabled = config["auto_sync"].get("enabled", false).asBool();
            int interval = config["auto_sync"].get("interval_minutes", 60).asInt();

            if (enabled) {
                startAutoSync(interval);
            }
        }

        is_initialized_ = true;
        return true;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Failed to initialize AggregatorManager: " + std::string(e.what()),
            {}
        );
        return false;
    }
}

bool AggregatorManager::initializeFromFile(const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::FILE_IO,
                "Failed to open aggregator config file: " + config_path,
                {}
            );
            return false;
        }

        Json::Value config;
        Json::Reader reader;
        if (!reader.parse(file, config)) {
            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::CONFIGURATION,
                "Failed to parse aggregator config file",
                {{"path", config_path}}
            );
            return false;
        }

        return initialize(config);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::CONFIGURATION,
            "Exception loading aggregator config: " + std::string(e.what()),
            {{"path", config_path}}
        );
        return false;
    }
}

void AggregatorManager::shutdown() {
    stopAutoSync();
    cancelAllSyncs();

    std::lock_guard<std::mutex> lock(aggregators_mutex_);
    aggregators_.clear();

    is_initialized_ = false;
}

// ============================================================================
// AGGREGATOR MANAGEMENT
// ============================================================================

void AggregatorManager::registerAggregator(const std::string& name, AggregatorPtr aggregator) {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);

    if (aggregators_.find(name) != aggregators_.end()) {
        WTL_CAPTURE_WARNING(
            wtl::core::debug::ErrorCategory::BUSINESS_LOGIC,
            "Replacing existing aggregator: " + name,
            {}
        );
    }

    aggregators_[name] = std::move(aggregator);
}

void AggregatorManager::unregisterAggregator(const std::string& name) {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);

    auto it = aggregators_.find(name);
    if (it != aggregators_.end()) {
        // Cancel sync if running
        if (it->second->isSyncing()) {
            it->second->cancelSync();
        }
        aggregators_.erase(it);
    }
}

IAggregator* AggregatorManager::getAggregator(const std::string& name) {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);

    auto it = aggregators_.find(name);
    return it != aggregators_.end() ? it->second.get() : nullptr;
}

bool AggregatorManager::hasAggregator(const std::string& name) const {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);
    return aggregators_.find(name) != aggregators_.end();
}

std::vector<std::string> AggregatorManager::getAggregatorNames() const {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);

    std::vector<std::string> names;
    names.reserve(aggregators_.size());
    for (const auto& [name, _] : aggregators_) {
        names.push_back(name);
    }
    return names;
}

std::vector<AggregatorInfo> AggregatorManager::getAllAggregatorInfo() const {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);

    std::vector<AggregatorInfo> info;
    info.reserve(aggregators_.size());
    for (const auto& [name, agg] : aggregators_) {
        info.push_back(agg->getInfo());
    }
    return info;
}

// ============================================================================
// SYNC OPERATIONS
// ============================================================================

SyncAllResult AggregatorManager::syncAll() {
    SyncAllResult result;
    auto overall_start = std::chrono::system_clock::now();

    std::vector<std::pair<std::string, AggregatorPtr*>> to_sync;

    {
        std::lock_guard<std::mutex> lock(aggregators_mutex_);
        for (auto& [name, agg] : aggregators_) {
            if (agg->isEnabled()) {
                to_sync.emplace_back(name, &agg);
            }
        }
    }

    result.total_aggregators = to_sync.size();

    for (auto& [name, agg_ptr] : to_sync) {
        if (shutdown_requested_) break;

        auto& agg = *agg_ptr;
        AggregatorSyncResult agg_result;
        agg_result.name = name;
        agg_result.started_at = std::chrono::system_clock::now();

        reportProgress(name, 0);

        try {
            auto stats = agg->sync();
            agg_result.stats = stats;
            agg_result.success = stats.isSuccessful();

            result.total_dogs_added += stats.dogs_added;
            result.total_dogs_updated += stats.dogs_updated;
            result.total_shelters_added += stats.shelters_added;
            result.total_shelters_updated += stats.shelters_updated;

            if (agg_result.success) {
                result.successful++;
            } else {
                result.failed++;
                result.overall_success = false;
            }

        } catch (const std::exception& e) {
            agg_result.success = false;
            agg_result.stats.addError("EXCEPTION", e.what());
            result.failed++;
            result.overall_success = false;
        }

        agg_result.completed_at = std::chrono::system_clock::now();
        result.results.push_back(agg_result);

        reportCompletion(agg_result);

        // Store in history
        {
            std::lock_guard<std::mutex> lock(history_mutex_);
            sync_history_.push_back(agg_result);
            while (sync_history_.size() > MAX_HISTORY_SIZE) {
                sync_history_.erase(sync_history_.begin());
            }
        }
    }

    auto overall_end = std::chrono::system_clock::now();
    result.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        overall_end - overall_start);

    return result;
}

void AggregatorManager::syncAllAsync(std::function<void(SyncAllResult)> callback) {
    std::thread([this, callback]() {
        auto result = syncAll();
        if (callback) {
            callback(result);
        }
    }).detach();
}

SyncStats AggregatorManager::syncOne(const std::string& name) {
    IAggregator* agg = getAggregator(name);
    if (!agg) {
        SyncStats stats;
        stats.source_name = name;
        stats.addError("NOT_FOUND", "Aggregator not found: " + name);
        return stats;
    }

    if (!agg->isEnabled()) {
        SyncStats stats;
        stats.source_name = name;
        stats.addError("DISABLED", "Aggregator is disabled: " + name);
        return stats;
    }

    reportProgress(name, 0);
    auto stats = agg->sync();

    AggregatorSyncResult result;
    result.name = name;
    result.stats = stats;
    result.success = stats.isSuccessful();
    result.started_at = std::chrono::system_clock::now() -
        std::chrono::milliseconds(stats.duration.count());
    result.completed_at = std::chrono::system_clock::now();

    reportCompletion(result);

    // Store in history
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        sync_history_.push_back(result);
        while (sync_history_.size() > MAX_HISTORY_SIZE) {
            sync_history_.erase(sync_history_.begin());
        }
    }

    return stats;
}

void AggregatorManager::syncOneAsync(const std::string& name,
                                       std::function<void(SyncStats)> callback) {
    std::thread([this, name, callback]() {
        auto stats = syncOne(name);
        if (callback) {
            callback(stats);
        }
    }).detach();
}

SyncAllResult AggregatorManager::syncAllIncremental() {
    SyncAllResult result;
    auto overall_start = std::chrono::system_clock::now();

    std::vector<std::pair<std::string, AggregatorPtr*>> to_sync;

    {
        std::lock_guard<std::mutex> lock(aggregators_mutex_);
        for (auto& [name, agg] : aggregators_) {
            if (agg->isEnabled()) {
                to_sync.emplace_back(name, &agg);
            }
        }
    }

    result.total_aggregators = to_sync.size();

    for (auto& [name, agg_ptr] : to_sync) {
        if (shutdown_requested_) break;

        auto& agg = *agg_ptr;
        auto since = agg->getLastSyncTime();

        AggregatorSyncResult agg_result;
        agg_result.name = name;
        agg_result.started_at = std::chrono::system_clock::now();

        try {
            auto stats = agg->syncSince(since);
            agg_result.stats = stats;
            agg_result.success = stats.isSuccessful();

            result.total_dogs_added += stats.dogs_added;
            result.total_dogs_updated += stats.dogs_updated;
            result.total_shelters_added += stats.shelters_added;
            result.total_shelters_updated += stats.shelters_updated;

            if (agg_result.success) {
                result.successful++;
            } else {
                result.failed++;
                result.overall_success = false;
            }

        } catch (const std::exception& e) {
            agg_result.success = false;
            agg_result.stats.addError("EXCEPTION", e.what());
            result.failed++;
            result.overall_success = false;
        }

        agg_result.completed_at = std::chrono::system_clock::now();
        result.results.push_back(agg_result);
    }

    auto overall_end = std::chrono::system_clock::now();
    result.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        overall_end - overall_start);

    return result;
}

void AggregatorManager::cancelAllSyncs() {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);
    for (auto& [name, agg] : aggregators_) {
        if (agg->isSyncing()) {
            agg->cancelSync();
        }
    }
}

void AggregatorManager::cancelSync(const std::string& name) {
    IAggregator* agg = getAggregator(name);
    if (agg && agg->isSyncing()) {
        agg->cancelSync();
    }
}

bool AggregatorManager::isSyncing() const {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);
    for (const auto& [name, agg] : aggregators_) {
        if (agg->isSyncing()) {
            return true;
        }
    }
    return false;
}

bool AggregatorManager::isSyncing(const std::string& name) const {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);
    auto it = aggregators_.find(name);
    return it != aggregators_.end() && it->second->isSyncing();
}

// ============================================================================
// STATUS AND STATISTICS
// ============================================================================

Json::Value AggregatorManager::getStatus() const {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);

    Json::Value status;
    status["total_aggregators"] = static_cast<Json::UInt64>(aggregators_.size());
    status["is_syncing"] = isSyncing();
    status["auto_sync_enabled"] = auto_sync_enabled_.load();

    Json::Value aggregators_json(Json::arrayValue);
    for (const auto& [name, agg] : aggregators_) {
        Json::Value agg_status;
        agg_status["name"] = name;
        agg_status["display_name"] = agg->getDisplayName();
        agg_status["enabled"] = agg->isEnabled();
        agg_status["healthy"] = agg->isHealthy();
        agg_status["status"] = aggregatorStatusToString(agg->getStatus());
        agg_status["is_syncing"] = agg->isSyncing();
        agg_status["dog_count"] = static_cast<Json::UInt64>(agg->getDogCount());
        agg_status["shelter_count"] = static_cast<Json::UInt64>(agg->getShelterCount());

        auto last_sync = agg->getLastSyncTime();
        if (last_sync.time_since_epoch().count() > 0) {
            auto time_t_val = std::chrono::system_clock::to_time_t(last_sync);
            char buffer[30];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t_val));
            agg_status["last_sync_time"] = std::string(buffer);
        }

        aggregators_json.append(agg_status);
    }
    status["aggregators"] = aggregators_json;

    return status;
}

Json::Value AggregatorManager::getStatus(const std::string& name) const {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);

    auto it = aggregators_.find(name);
    if (it == aggregators_.end()) {
        Json::Value error;
        error["error"] = "Aggregator not found: " + name;
        return error;
    }

    return it->second->getHealthDetails();
}

std::chrono::system_clock::time_point AggregatorManager::getLastSync(const std::string& name) const {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);

    auto it = aggregators_.find(name);
    if (it != aggregators_.end()) {
        return it->second->getLastSyncTime();
    }
    return std::chrono::system_clock::time_point{};
}

SyncStats AggregatorManager::getLastSyncStats(const std::string& name) const {
    std::lock_guard<std::mutex> lock(aggregators_mutex_);

    auto it = aggregators_.find(name);
    if (it != aggregators_.end()) {
        return it->second->getLastSyncStats();
    }

    SyncStats empty;
    empty.source_name = name;
    return empty;
}

Json::Value AggregatorManager::getCombinedStats() const {
    Json::Value stats;

    size_t total_dogs = 0;
    size_t total_shelters = 0;

    {
        std::lock_guard<std::mutex> lock(aggregators_mutex_);
        for (const auto& [name, agg] : aggregators_) {
            total_dogs += agg->getDogCount();
            total_shelters += agg->getShelterCount();
        }
    }

    stats["total_dogs"] = static_cast<Json::UInt64>(total_dogs);
    stats["total_shelters"] = static_cast<Json::UInt64>(total_shelters);

    // Recent sync stats
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        size_t recent_syncs = std::min(sync_history_.size(), static_cast<size_t>(10));
        size_t successful = 0;
        size_t failed = 0;

        for (size_t i = sync_history_.size() > recent_syncs ? sync_history_.size() - recent_syncs : 0;
             i < sync_history_.size(); i++) {
            if (sync_history_[i].success) successful++;
            else failed++;
        }

        stats["recent_syncs_successful"] = static_cast<Json::UInt64>(successful);
        stats["recent_syncs_failed"] = static_cast<Json::UInt64>(failed);
    }

    return stats;
}

Json::Value AggregatorManager::getRecentErrors(size_t limit) const {
    Json::Value errors(Json::arrayValue);

    std::lock_guard<std::mutex> lock(history_mutex_);

    size_t count = 0;
    for (auto it = sync_history_.rbegin(); it != sync_history_.rend() && count < limit; ++it) {
        for (const auto& error : it->stats.errors) {
            errors.append(error.toJson());
            count++;
            if (count >= limit) break;
        }
    }

    return errors;
}

// ============================================================================
// SCHEDULING
// ============================================================================

void AggregatorManager::startAutoSync(int interval_minutes) {
    if (auto_sync_enabled_) {
        return;  // Already running
    }

    auto_sync_interval_minutes_ = interval_minutes;
    auto_sync_enabled_ = true;
    shutdown_requested_ = false;

    next_scheduled_sync_ = std::chrono::system_clock::now() +
        std::chrono::minutes(interval_minutes);

    auto_sync_thread_ = std::thread(&AggregatorManager::autoSyncLoop, this);
}

void AggregatorManager::stopAutoSync() {
    if (!auto_sync_enabled_) {
        return;
    }

    shutdown_requested_ = true;
    auto_sync_enabled_ = false;

    {
        std::lock_guard<std::mutex> lock(auto_sync_mutex_);
        auto_sync_cv_.notify_all();
    }

    if (auto_sync_thread_.joinable()) {
        auto_sync_thread_.join();
    }
}

bool AggregatorManager::isAutoSyncEnabled() const {
    return auto_sync_enabled_.load();
}

std::chrono::system_clock::time_point AggregatorManager::getNextScheduledSync() const {
    return next_scheduled_sync_;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void AggregatorManager::setProgressCallback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    progress_callback_ = std::move(callback);
}

void AggregatorManager::setCompletionCallback(CompletionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    completion_callback_ = std::move(callback);
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

AggregatorPtr AggregatorManager::createAggregator(const Json::Value& config) {
    std::string type = config.get("type", "").asString();
    auto agg_config = AggregatorConfig::fromJson(config);

    if (type == "rescuegroups") {
        auto agg = std::make_shared<RescueGroupsAggregator>(agg_config);
        agg->initialize(config);
        return agg;
    } else if (type == "shelter_direct") {
        auto agg = std::make_shared<ShelterDirectAggregator>(agg_config);
        agg->initialize(config);

        // Load feeds
        if (config.isMember("feeds")) {
            static_cast<ShelterDirectAggregator*>(agg.get())->loadFeeds(config["feeds"]);
        }

        return agg;
    }

    WTL_CAPTURE_WARNING(
        wtl::core::debug::ErrorCategory::CONFIGURATION,
        "Unknown aggregator type: " + type,
        {{"type", type}}
    );

    return nullptr;
}

void AggregatorManager::autoSyncLoop() {
    while (!shutdown_requested_) {
        std::unique_lock<std::mutex> lock(auto_sync_mutex_);

        auto now = std::chrono::system_clock::now();
        if (now >= next_scheduled_sync_) {
            // Time to sync
            lock.unlock();
            syncAllIncremental();
            lock.lock();

            next_scheduled_sync_ = std::chrono::system_clock::now() +
                std::chrono::minutes(auto_sync_interval_minutes_);
        }

        // Wait until next sync or shutdown
        auto_sync_cv_.wait_for(lock, std::chrono::minutes(1),
            [this]() { return shutdown_requested_.load(); });
    }
}

void AggregatorManager::reportProgress(const std::string& name, int percent) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (progress_callback_) {
        progress_callback_(name, percent);
    }
}

void AggregatorManager::reportCompletion(const AggregatorSyncResult& result) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (completion_callback_) {
        completion_callback_(result);
    }
}

} // namespace wtl::aggregators
