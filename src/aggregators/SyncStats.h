/**
 * @file SyncStats.h
 * @brief Synchronization statistics for aggregator operations
 *
 * PURPOSE:
 * Tracks the results of sync operations including counts of dogs and
 * shelters added, updated, or removed, along with errors and timing.
 *
 * USAGE:
 * SyncStats stats;
 * stats.dogs_added++;
 * stats.markComplete();
 * Json::Value json = stats.toJson();
 *
 * DEPENDENCIES:
 * - jsoncpp for JSON serialization
 * - chrono for timing
 *
 * MODIFICATION GUIDE:
 * - Add new tracking fields as needed
 * - Update toJson() when adding fields
 * - Keep reset() in sync with all fields
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::aggregators {

/**
 * @struct SyncError
 * @brief Represents an error that occurred during sync
 */
struct SyncError {
    std::string code;                  ///< Error code
    std::string message;               ///< Error message
    std::string external_id;           ///< External ID that caused error (if applicable)
    std::string entity_type;           ///< "dog" or "shelter"
    std::chrono::system_clock::time_point timestamp;  ///< When error occurred

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["code"] = code;
        json["message"] = message;
        if (!external_id.empty()) {
            json["external_id"] = external_id;
        }
        if (!entity_type.empty()) {
            json["entity_type"] = entity_type;
        }

        auto time_t_val = std::chrono::system_clock::to_time_t(timestamp);
        char buffer[30];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t_val));
        json["timestamp"] = std::string(buffer);

        return json;
    }
};

/**
 * @struct SyncWarning
 * @brief Represents a warning during sync (non-fatal)
 */
struct SyncWarning {
    std::string code;                  ///< Warning code
    std::string message;               ///< Warning message
    std::string external_id;           ///< Related external ID
    std::string details;               ///< Additional details

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["code"] = code;
        json["message"] = message;
        if (!external_id.empty()) {
            json["external_id"] = external_id;
        }
        if (!details.empty()) {
            json["details"] = details;
        }
        return json;
    }
};

/**
 * @struct SyncStats
 * @brief Statistics from a sync operation
 *
 * Thread-safe tracking of sync operation results including counts,
 * timing, errors, and warnings.
 */
struct SyncStats {
    // =========================================================================
    // DOG STATISTICS
    // =========================================================================

    size_t dogs_fetched{0};            ///< Total dogs fetched from source
    size_t dogs_added{0};              ///< New dogs added to database
    size_t dogs_updated{0};            ///< Existing dogs updated
    size_t dogs_removed{0};            ///< Dogs removed (no longer at source)
    size_t dogs_unchanged{0};          ///< Dogs with no changes
    size_t dogs_skipped{0};            ///< Dogs skipped (duplicates, invalid, etc.)

    // =========================================================================
    // SHELTER STATISTICS
    // =========================================================================

    size_t shelters_fetched{0};        ///< Total shelters fetched from source
    size_t shelters_added{0};          ///< New shelters added
    size_t shelters_updated{0};        ///< Existing shelters updated
    size_t shelters_removed{0};        ///< Shelters removed
    size_t shelters_unchanged{0};      ///< Shelters with no changes

    // =========================================================================
    // ERROR AND WARNING TRACKING
    // =========================================================================

    size_t error_count{0};             ///< Total errors encountered
    size_t warning_count{0};           ///< Total warnings encountered
    std::vector<SyncError> errors;     ///< Detailed error list (limited)
    std::vector<SyncWarning> warnings; ///< Detailed warning list (limited)

    // =========================================================================
    // API STATISTICS
    // =========================================================================

    size_t api_requests_made{0};       ///< Number of API requests made
    size_t api_requests_failed{0};     ///< Number of failed API requests
    size_t api_rate_limit_hits{0};     ///< Times rate limit was hit
    size_t pages_fetched{0};           ///< Number of pages fetched (pagination)
    size_t bytes_received{0};          ///< Total bytes received from API

    // =========================================================================
    // TIMING
    // =========================================================================

    std::chrono::system_clock::time_point start_time;  ///< When sync started
    std::chrono::system_clock::time_point end_time;    ///< When sync completed
    std::chrono::milliseconds duration{0};             ///< Total sync duration

    // =========================================================================
    // STATUS
    // =========================================================================

    bool is_complete{false};           ///< Whether sync completed
    bool is_cancelled{false};          ///< Whether sync was cancelled
    bool is_incremental{false};        ///< Whether this was an incremental sync
    std::string source_name;           ///< Name of the aggregator
    std::string sync_id;               ///< Unique ID for this sync operation

    // =========================================================================
    // CONSTRUCTORS (move-only due to mutex)
    // =========================================================================

    SyncStats() = default;
    SyncStats(const SyncStats& other)
        : dogs_fetched(other.dogs_fetched), dogs_added(other.dogs_added),
          dogs_updated(other.dogs_updated), dogs_removed(other.dogs_removed),
          dogs_unchanged(other.dogs_unchanged), dogs_skipped(other.dogs_skipped),
          shelters_fetched(other.shelters_fetched), shelters_added(other.shelters_added),
          shelters_updated(other.shelters_updated), shelters_removed(other.shelters_removed),
          shelters_unchanged(other.shelters_unchanged),
          error_count(other.error_count), warning_count(other.warning_count),
          errors(other.errors), warnings(other.warnings),
          api_requests_made(other.api_requests_made),
          api_requests_failed(other.api_requests_failed),
          api_rate_limit_hits(other.api_rate_limit_hits),
          pages_fetched(other.pages_fetched), bytes_received(other.bytes_received),
          start_time(other.start_time), end_time(other.end_time),
          duration(other.duration),
          is_complete(other.is_complete), is_cancelled(other.is_cancelled),
          is_incremental(other.is_incremental),
          source_name(other.source_name), sync_id(other.sync_id) {}
    SyncStats& operator=(const SyncStats& other) {
        if (this != &other) {
            dogs_fetched = other.dogs_fetched;
            dogs_added = other.dogs_added;
            dogs_updated = other.dogs_updated;
            dogs_removed = other.dogs_removed;
            dogs_unchanged = other.dogs_unchanged;
            dogs_skipped = other.dogs_skipped;
            shelters_fetched = other.shelters_fetched;
            shelters_added = other.shelters_added;
            shelters_updated = other.shelters_updated;
            shelters_removed = other.shelters_removed;
            shelters_unchanged = other.shelters_unchanged;
            error_count = other.error_count;
            warning_count = other.warning_count;
            errors = other.errors;
            warnings = other.warnings;
            api_requests_made = other.api_requests_made;
            api_requests_failed = other.api_requests_failed;
            api_rate_limit_hits = other.api_rate_limit_hits;
            pages_fetched = other.pages_fetched;
            bytes_received = other.bytes_received;
            start_time = other.start_time;
            end_time = other.end_time;
            duration = other.duration;
            is_complete = other.is_complete;
            is_cancelled = other.is_cancelled;
            is_incremental = other.is_incremental;
            source_name = other.source_name;
            sync_id = other.sync_id;
        }
        return *this;
    }
    SyncStats(SyncStats&& other) noexcept
        : dogs_fetched(other.dogs_fetched), dogs_added(other.dogs_added),
          dogs_updated(other.dogs_updated), dogs_removed(other.dogs_removed),
          dogs_unchanged(other.dogs_unchanged), dogs_skipped(other.dogs_skipped),
          shelters_fetched(other.shelters_fetched), shelters_added(other.shelters_added),
          shelters_updated(other.shelters_updated), shelters_removed(other.shelters_removed),
          shelters_unchanged(other.shelters_unchanged),
          error_count(other.error_count), warning_count(other.warning_count),
          errors(std::move(other.errors)), warnings(std::move(other.warnings)),
          api_requests_made(other.api_requests_made),
          api_requests_failed(other.api_requests_failed),
          api_rate_limit_hits(other.api_rate_limit_hits),
          pages_fetched(other.pages_fetched), bytes_received(other.bytes_received),
          start_time(other.start_time), end_time(other.end_time),
          duration(other.duration),
          is_complete(other.is_complete), is_cancelled(other.is_cancelled),
          is_incremental(other.is_incremental),
          source_name(std::move(other.source_name)),
          sync_id(std::move(other.sync_id)) {}
    SyncStats& operator=(SyncStats&& other) noexcept {
        if (this != &other) {
            dogs_fetched = other.dogs_fetched;
            dogs_added = other.dogs_added;
            dogs_updated = other.dogs_updated;
            dogs_removed = other.dogs_removed;
            dogs_unchanged = other.dogs_unchanged;
            dogs_skipped = other.dogs_skipped;
            shelters_fetched = other.shelters_fetched;
            shelters_added = other.shelters_added;
            shelters_updated = other.shelters_updated;
            shelters_removed = other.shelters_removed;
            shelters_unchanged = other.shelters_unchanged;
            error_count = other.error_count;
            warning_count = other.warning_count;
            errors = std::move(other.errors);
            warnings = std::move(other.warnings);
            api_requests_made = other.api_requests_made;
            api_requests_failed = other.api_requests_failed;
            api_rate_limit_hits = other.api_rate_limit_hits;
            pages_fetched = other.pages_fetched;
            bytes_received = other.bytes_received;
            start_time = other.start_time;
            end_time = other.end_time;
            duration = other.duration;
            is_complete = other.is_complete;
            is_cancelled = other.is_cancelled;
            is_incremental = other.is_incremental;
            source_name = std::move(other.source_name);
            sync_id = std::move(other.sync_id);
        }
        return *this;
    }

    // =========================================================================
    // THREAD SAFETY
    // =========================================================================
private:
    mutable std::mutex mutex_;
    static const size_t MAX_ERRORS = 100;
    static const size_t MAX_WARNINGS = 100;

public:
    // =========================================================================
    // METHODS
    // =========================================================================

    /**
     * @brief Mark sync as started, recording start time
     */
    void markStarted() {
        std::lock_guard<std::mutex> lock(mutex_);
        start_time = std::chrono::system_clock::now();
        is_complete = false;
        is_cancelled = false;
    }

    /**
     * @brief Mark sync as complete, recording end time and duration
     */
    void markComplete() {
        std::lock_guard<std::mutex> lock(mutex_);
        end_time = std::chrono::system_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        is_complete = true;
    }

    /**
     * @brief Mark sync as cancelled
     */
    void markCancelled() {
        std::lock_guard<std::mutex> lock(mutex_);
        end_time = std::chrono::system_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        is_cancelled = true;
        is_complete = false;
    }

    /**
     * @brief Add an error
     * @param error The error to add
     */
    void addError(const SyncError& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_count++;
        if (errors.size() < MAX_ERRORS) {
            errors.push_back(error);
        }
    }

    /**
     * @brief Add an error with details
     * @param code Error code
     * @param message Error message
     * @param external_id Related external ID
     * @param entity_type Entity type ("dog" or "shelter")
     */
    void addError(const std::string& code, const std::string& message,
                  const std::string& external_id = "",
                  const std::string& entity_type = "") {
        SyncError error;
        error.code = code;
        error.message = message;
        error.external_id = external_id;
        error.entity_type = entity_type;
        error.timestamp = std::chrono::system_clock::now();
        addError(error);
    }

    /**
     * @brief Add a warning
     * @param warning The warning to add
     */
    void addWarning(const SyncWarning& warning) {
        std::lock_guard<std::mutex> lock(mutex_);
        warning_count++;
        if (warnings.size() < MAX_WARNINGS) {
            warnings.push_back(warning);
        }
    }

    /**
     * @brief Add a warning with details
     * @param code Warning code
     * @param message Warning message
     * @param external_id Related external ID
     * @param details Additional details
     */
    void addWarning(const std::string& code, const std::string& message,
                    const std::string& external_id = "",
                    const std::string& details = "") {
        SyncWarning warning;
        warning.code = code;
        warning.message = message;
        warning.external_id = external_id;
        warning.details = details;
        addWarning(warning);
    }

    /**
     * @brief Reset all statistics
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        dogs_fetched = 0;
        dogs_added = 0;
        dogs_updated = 0;
        dogs_removed = 0;
        dogs_unchanged = 0;
        dogs_skipped = 0;

        shelters_fetched = 0;
        shelters_added = 0;
        shelters_updated = 0;
        shelters_removed = 0;
        shelters_unchanged = 0;

        error_count = 0;
        warning_count = 0;
        errors.clear();
        warnings.clear();

        api_requests_made = 0;
        api_requests_failed = 0;
        api_rate_limit_hits = 0;
        pages_fetched = 0;
        bytes_received = 0;

        start_time = std::chrono::system_clock::time_point{};
        end_time = std::chrono::system_clock::time_point{};
        duration = std::chrono::milliseconds{0};

        is_complete = false;
        is_cancelled = false;
        is_incremental = false;
        sync_id.clear();
    }

    /**
     * @brief Check if sync was successful (complete with no errors)
     * @return true if successful
     */
    bool isSuccessful() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_complete && !is_cancelled && error_count == 0;
    }

    /**
     * @brief Get total dogs processed
     * @return Sum of added, updated, unchanged, skipped
     */
    size_t getTotalDogsProcessed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dogs_added + dogs_updated + dogs_unchanged + dogs_skipped;
    }

    /**
     * @brief Get total shelters processed
     * @return Sum of added, updated, unchanged
     */
    size_t getTotalSheltersProcessed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return shelters_added + shelters_updated + shelters_unchanged;
    }

    /**
     * @brief Get duration in seconds
     * @return Duration in seconds
     */
    double getDurationSeconds() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return duration.count() / 1000.0;
    }

    /**
     * @brief Convert to JSON for API responses
     * @return JSON representation
     */
    Json::Value toJson() const {
        std::lock_guard<std::mutex> lock(mutex_);

        Json::Value json;

        // Identification
        json["sync_id"] = sync_id;
        json["source_name"] = source_name;
        json["is_incremental"] = is_incremental;

        // Status
        json["is_complete"] = is_complete;
        json["is_cancelled"] = is_cancelled;
        json["is_successful"] = is_complete && !is_cancelled && error_count == 0;

        // Dog stats
        Json::Value dogs;
        dogs["fetched"] = static_cast<Json::UInt64>(dogs_fetched);
        dogs["added"] = static_cast<Json::UInt64>(dogs_added);
        dogs["updated"] = static_cast<Json::UInt64>(dogs_updated);
        dogs["removed"] = static_cast<Json::UInt64>(dogs_removed);
        dogs["unchanged"] = static_cast<Json::UInt64>(dogs_unchanged);
        dogs["skipped"] = static_cast<Json::UInt64>(dogs_skipped);
        json["dogs"] = dogs;

        // Shelter stats
        Json::Value shelters;
        shelters["fetched"] = static_cast<Json::UInt64>(shelters_fetched);
        shelters["added"] = static_cast<Json::UInt64>(shelters_added);
        shelters["updated"] = static_cast<Json::UInt64>(shelters_updated);
        shelters["removed"] = static_cast<Json::UInt64>(shelters_removed);
        shelters["unchanged"] = static_cast<Json::UInt64>(shelters_unchanged);
        json["shelters"] = shelters;

        // API stats
        Json::Value api;
        api["requests_made"] = static_cast<Json::UInt64>(api_requests_made);
        api["requests_failed"] = static_cast<Json::UInt64>(api_requests_failed);
        api["rate_limit_hits"] = static_cast<Json::UInt64>(api_rate_limit_hits);
        api["pages_fetched"] = static_cast<Json::UInt64>(pages_fetched);
        api["bytes_received"] = static_cast<Json::UInt64>(bytes_received);
        json["api"] = api;

        // Error/warning counts
        json["error_count"] = static_cast<Json::UInt64>(error_count);
        json["warning_count"] = static_cast<Json::UInt64>(warning_count);

        // Detailed errors (limited)
        Json::Value error_array(Json::arrayValue);
        for (const auto& error : errors) {
            error_array.append(error.toJson());
        }
        json["errors"] = error_array;

        // Detailed warnings (limited)
        Json::Value warning_array(Json::arrayValue);
        for (const auto& warning : warnings) {
            warning_array.append(warning.toJson());
        }
        json["warnings"] = warning_array;

        // Timing
        Json::Value timing;
        if (start_time.time_since_epoch().count() > 0) {
            auto start_t = std::chrono::system_clock::to_time_t(start_time);
            char buffer[30];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&start_t));
            timing["start_time"] = std::string(buffer);
        }
        if (end_time.time_since_epoch().count() > 0) {
            auto end_t = std::chrono::system_clock::to_time_t(end_time);
            char buffer[30];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&end_t));
            timing["end_time"] = std::string(buffer);
        }
        timing["duration_ms"] = static_cast<Json::Int64>(duration.count());
        timing["duration_seconds"] = duration.count() / 1000.0;
        json["timing"] = timing;

        return json;
    }

    /**
     * @brief Create SyncStats from JSON
     * @param json The JSON to parse
     * @return SyncStats object
     */
    static SyncStats fromJson(const Json::Value& json) {
        SyncStats stats;

        stats.sync_id = json.get("sync_id", "").asString();
        stats.source_name = json.get("source_name", "").asString();
        stats.is_incremental = json.get("is_incremental", false).asBool();
        stats.is_complete = json.get("is_complete", false).asBool();
        stats.is_cancelled = json.get("is_cancelled", false).asBool();

        if (json.isMember("dogs")) {
            const auto& dogs = json["dogs"];
            stats.dogs_fetched = dogs.get("fetched", 0).asUInt64();
            stats.dogs_added = dogs.get("added", 0).asUInt64();
            stats.dogs_updated = dogs.get("updated", 0).asUInt64();
            stats.dogs_removed = dogs.get("removed", 0).asUInt64();
            stats.dogs_unchanged = dogs.get("unchanged", 0).asUInt64();
            stats.dogs_skipped = dogs.get("skipped", 0).asUInt64();
        }

        if (json.isMember("shelters")) {
            const auto& shelters = json["shelters"];
            stats.shelters_fetched = shelters.get("fetched", 0).asUInt64();
            stats.shelters_added = shelters.get("added", 0).asUInt64();
            stats.shelters_updated = shelters.get("updated", 0).asUInt64();
            stats.shelters_removed = shelters.get("removed", 0).asUInt64();
            stats.shelters_unchanged = shelters.get("unchanged", 0).asUInt64();
        }

        if (json.isMember("api")) {
            const auto& api = json["api"];
            stats.api_requests_made = api.get("requests_made", 0).asUInt64();
            stats.api_requests_failed = api.get("requests_failed", 0).asUInt64();
            stats.api_rate_limit_hits = api.get("rate_limit_hits", 0).asUInt64();
            stats.pages_fetched = api.get("pages_fetched", 0).asUInt64();
            stats.bytes_received = api.get("bytes_received", 0).asUInt64();
        }

        stats.error_count = json.get("error_count", 0).asUInt64();
        stats.warning_count = json.get("warning_count", 0).asUInt64();

        return stats;
    }

    /**
     * @brief Get a summary string for logging
     * @return Summary string
     */
    std::string getSummary() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string status_str = is_complete ? "complete" : (is_cancelled ? "cancelled" : "incomplete");
        return source_name + " sync " + status_str +
               ": dogs(+" + std::to_string(dogs_added) +
               "/~" + std::to_string(dogs_updated) +
               "/-" + std::to_string(dogs_removed) +
               "), shelters(+" + std::to_string(shelters_added) +
               "/~" + std::to_string(shelters_updated) +
               "), errors: " + std::to_string(error_count) +
               ", duration: " + std::to_string(duration.count()) + "ms";
    }
};

} // namespace wtl::aggregators
