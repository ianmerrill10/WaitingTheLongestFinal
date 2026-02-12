/**
 * @file ErrorCapture.cc
 * @brief Implementation of the error capture system
 * @see ErrorCapture.h for documentation
 */

#include "core/debug/ErrorCapture.h"
#include "core/utils/Config.h"

// Standard library includes
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace wtl::core::debug {

// ============================================================================
// ERROR CONTEXT IMPLEMENTATION
// ============================================================================

Json::Value ErrorContext::toJson() const {
    Json::Value json;

    json["id"] = id;
    json["category"] = ErrorCapture::categoryToString(category);
    json["severity"] = ErrorCapture::severityToString(severity);
    json["message"] = message;
    json["details"] = details;
    json["file"] = file;
    json["function"] = function;
    json["line"] = line;

    // Format timestamp as ISO 8601
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    std::ostringstream ts_stream;
    ts_stream << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    json["timestamp"] = ts_stream.str();

    // Metadata
    Json::Value meta_json(Json::objectValue);
    for (const auto& [key, value] : metadata) {
        meta_json[key] = value;
    }
    json["metadata"] = meta_json;

    // Optional fields
    if (!stack_trace.empty()) {
        json["stack_trace"] = stack_trace;
    }

    json["occurrence_count"] = occurrence_count;

    if (request_id.has_value()) {
        json["request_id"] = *request_id;
    }

    if (user_id.has_value()) {
        json["user_id"] = *user_id;
    }

    return json;
}

ErrorContext ErrorContext::fromJson(const Json::Value& json) {
    ErrorContext ctx;

    ctx.id = json.get("id", "").asString();
    ctx.category = ErrorCapture::stringToCategory(
        json.get("category", "UNKNOWN").asString());
    ctx.severity = ErrorCapture::stringToSeverity(
        json.get("severity", "ERROR").asString());
    ctx.message = json.get("message", "").asString();
    ctx.details = json.get("details", "").asString();
    ctx.file = json.get("file", "").asString();
    ctx.function = json.get("function", "").asString();
    ctx.line = json.get("line", 0).asInt();

    // Parse timestamp
    std::string ts_str = json.get("timestamp", "").asString();
    if (!ts_str.empty()) {
        std::tm tm = {};
        std::istringstream ss(ts_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        ctx.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    } else {
        ctx.timestamp = std::chrono::system_clock::now();
    }

    // Parse metadata
    const Json::Value& meta = json["metadata"];
    if (meta.isObject()) {
        for (const auto& key : meta.getMemberNames()) {
            ctx.metadata[key] = meta[key].asString();
        }
    }

    ctx.stack_trace = json.get("stack_trace", "").asString();
    ctx.occurrence_count = json.get("occurrence_count", 1).asInt();

    if (json.isMember("request_id")) {
        ctx.request_id = json["request_id"].asString();
    }

    if (json.isMember("user_id")) {
        ctx.user_id = json["user_id"].asString();
    }

    return ctx;
}

// ============================================================================
// ERROR CAPTURE SINGLETON
// ============================================================================

ErrorCapture& ErrorCapture::getInstance() {
    static ErrorCapture instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ErrorCapture::initialize(size_t max_errors, bool capture_stack_traces) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    max_errors_ = max_errors;
    capture_stack_traces_ = capture_stack_traces;

    // Initialize category counts
    category_counts_[ErrorCategory::UNKNOWN].store(0);
    category_counts_[ErrorCategory::DATABASE].store(0);
    category_counts_[ErrorCategory::CONFIGURATION].store(0);
    category_counts_[ErrorCategory::FILE_IO].store(0);
    category_counts_[ErrorCategory::HTTP_REQUEST].store(0);
    category_counts_[ErrorCategory::HTTP_RESPONSE].store(0);
    category_counts_[ErrorCategory::AUTHENTICATION].store(0);
    category_counts_[ErrorCategory::VALIDATION].store(0);
    category_counts_[ErrorCategory::BUSINESS_LOGIC].store(0);
    category_counts_[ErrorCategory::EXTERNAL_API].store(0);
    category_counts_[ErrorCategory::WEBSOCKET].store(0);
    category_counts_[ErrorCategory::MODULE].store(0);

    std::cout << "[ErrorCapture] Initialized with max_errors=" << max_errors_
              << ", capture_stack_traces=" << capture_stack_traces_
              << std::endl;
}

void ErrorCapture::initializeFromConfig() {
    auto& config = wtl::core::utils::Config::getInstance();

    size_t max_errors = static_cast<size_t>(
        config.getInt("debug.max_errors_in_memory", 1000));
    bool capture_stacks = config.getBool("debug.capture_stack_traces", false);

    initialize(max_errors, capture_stacks);
}

// ============================================================================
// ERROR CAPTURE
// ============================================================================

std::string ErrorCapture::capture(
    ErrorCategory category,
    ErrorSeverity severity,
    const std::string& message,
    const std::unordered_map<std::string, std::string>& metadata,
    const std::string& file,
    const std::string& function,
    int line) {

    // Create error context
    ErrorContext ctx;
    ctx.id = generateId();
    ctx.category = category;
    ctx.severity = severity;
    ctx.message = message;
    ctx.metadata = metadata;
    ctx.file = file;
    ctx.function = function;
    ctx.line = line;
    ctx.timestamp = std::chrono::system_clock::now();
    ctx.last_occurrence = ctx.timestamp;
    ctx.occurrence_count = 1;

    // Capture stack trace if enabled
    if (capture_stack_traces_) {
        ctx.stack_trace = captureStackTrace();
    }

    // Store error
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        errors_.push_back(ctx);
        error_index_[ctx.id] = errors_.size() - 1;

        // Update statistics
        ++total_captured_;
        ++category_counts_[category];

        // Prune if needed
        pruneIfNeeded();
    }

    // Log to console based on severity
    if (severity >= ErrorSeverity::WARNING) {
        std::cerr << "[" << severityToString(severity) << "]["
                  << categoryToString(category) << "] " << message;

        if (!file.empty()) {
            std::cerr << " (" << file << ":" << line << ")";
        }

        std::cerr << std::endl;
    }

    // Notify handlers
    notifyHandlers(ctx);

    return ctx.id;
}

std::string ErrorCapture::captureWithRequest(
    ErrorCategory category,
    ErrorSeverity severity,
    const std::string& message,
    const std::string& request_id,
    const std::unordered_map<std::string, std::string>& metadata) {

    auto meta_with_request = metadata;
    auto error_id = capture(category, severity, message, meta_with_request);

    // Update with request ID
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = error_index_.find(error_id);
        if (it != error_index_.end() && it->second < errors_.size()) {
            errors_[it->second].request_id = request_id;
        }
    }

    return error_id;
}

std::string ErrorCapture::captureException(
    const std::exception& e,
    ErrorCategory category,
    const std::unordered_map<std::string, std::string>& metadata) {

    auto meta = metadata;
    meta["exception_type"] = typeid(e).name();

    return capture(category, ErrorSeverity::ERROR, e.what(), meta);
}

// ============================================================================
// QUERYING
// ============================================================================

std::vector<ErrorContext> ErrorCapture::getAll(size_t limit) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<ErrorContext> result;
    size_t count = limit == 0 ? errors_.size() : std::min(limit, errors_.size());
    result.reserve(count);

    // Return newest first
    auto it = errors_.rbegin();
    for (size_t i = 0; i < count && it != errors_.rend(); ++i, ++it) {
        result.push_back(*it);
    }

    return result;
}

std::vector<ErrorContext> ErrorCapture::getByCategory(
    ErrorCategory category, size_t limit) const {

    return query(
        [category](const ErrorContext& err) {
            return err.category == category;
        },
        limit);
}

std::vector<ErrorContext> ErrorCapture::getBySeverity(
    ErrorSeverity min_severity, size_t limit) const {

    return query(
        [min_severity](const ErrorContext& err) {
            return err.severity >= min_severity;
        },
        limit);
}

std::vector<ErrorContext> ErrorCapture::getByTimeRange(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end,
    size_t limit) const {

    return query(
        [start, end](const ErrorContext& err) {
            return err.timestamp >= start && err.timestamp <= end;
        },
        limit);
}

std::vector<ErrorContext> ErrorCapture::query(
    const ErrorPredicate& filter, size_t limit) const {

    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<ErrorContext> result;
    size_t count = 0;

    // Search newest first
    for (auto it = errors_.rbegin(); it != errors_.rend(); ++it) {
        if (filter(*it)) {
            result.push_back(*it);
            ++count;

            if (limit > 0 && count >= limit) {
                break;
            }
        }
    }

    return result;
}

std::optional<ErrorContext> ErrorCapture::getById(const std::string& id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = error_index_.find(id);
    if (it != error_index_.end() && it->second < errors_.size()) {
        return errors_[it->second];
    }

    return std::nullopt;
}

std::vector<ErrorContext> ErrorCapture::getRecent(int minutes, size_t limit) const {
    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::minutes(minutes);

    return getByTimeRange(start, now, limit);
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value ErrorCapture::getStats() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    Json::Value stats;

    stats["total_captured"] = static_cast<Json::UInt64>(total_captured_.load());
    stats["current_count"] = static_cast<Json::UInt64>(errors_.size());
    stats["max_errors"] = static_cast<Json::UInt64>(max_errors_);

    // Counts by category
    Json::Value by_category;
    by_category["unknown"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::UNKNOWN).load());
    by_category["database"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::DATABASE).load());
    by_category["configuration"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::CONFIGURATION).load());
    by_category["file_io"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::FILE_IO).load());
    by_category["http_request"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::HTTP_REQUEST).load());
    by_category["http_response"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::HTTP_RESPONSE).load());
    by_category["authentication"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::AUTHENTICATION).load());
    by_category["validation"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::VALIDATION).load());
    by_category["business_logic"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::BUSINESS_LOGIC).load());
    by_category["external_api"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::EXTERNAL_API).load());
    by_category["websocket"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::WEBSOCKET).load());
    by_category["module"] = static_cast<Json::UInt64>(
        category_counts_.at(ErrorCategory::MODULE).load());

    stats["by_category"] = by_category;

    // Count by severity (from current errors)
    Json::Value by_severity;
    int trace_count = 0, debug_count = 0, info_count = 0;
    int warning_count = 0, error_count = 0, critical_count = 0;

    for (const auto& err : errors_) {
        switch (err.severity) {
            case ErrorSeverity::TRACE: ++trace_count; break;
            case ErrorSeverity::DEBUG: ++debug_count; break;
            case ErrorSeverity::INFO: ++info_count; break;
            case ErrorSeverity::WARNING: ++warning_count; break;
            case ErrorSeverity::ERROR: ++error_count; break;
            case ErrorSeverity::CRITICAL: ++critical_count; break;
        }
    }

    by_severity["trace"] = trace_count;
    by_severity["debug"] = debug_count;
    by_severity["info"] = info_count;
    by_severity["warning"] = warning_count;
    by_severity["error"] = error_count;
    by_severity["critical"] = critical_count;

    stats["by_severity"] = by_severity;

    return stats;
}

size_t ErrorCapture::getCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return errors_.size();
}

size_t ErrorCapture::getCountByCategory(ErrorCategory category) const {
    auto it = category_counts_.find(category);
    if (it != category_counts_.end()) {
        return static_cast<size_t>(it->second.load());
    }
    return 0;
}

// ============================================================================
// EXPORT
// ============================================================================

Json::Value ErrorCapture::exportToJson() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    Json::Value result(Json::arrayValue);
    for (const auto& err : errors_) {
        result.append(err.toJson());
    }

    return result;
}

Json::Value ErrorCapture::exportToJson(const ErrorPredicate& filter) const {
    auto filtered = query(filter, 0);

    Json::Value result(Json::arrayValue);
    for (const auto& err : filtered) {
        result.append(err.toJson());
    }

    return result;
}

// ============================================================================
// MANAGEMENT
// ============================================================================

void ErrorCapture::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    errors_.clear();
    error_index_.clear();

    std::cout << "[ErrorCapture] All errors cleared" << std::endl;
}

size_t ErrorCapture::clearOlderThan(std::chrono::minutes max_age) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto cutoff = std::chrono::system_clock::now() - max_age;
    size_t cleared = 0;

    // Remove from front (oldest first)
    while (!errors_.empty() && errors_.front().timestamp < cutoff) {
        error_index_.erase(errors_.front().id);
        errors_.pop_front();
        ++cleared;
    }

    // Rebuild index
    error_index_.clear();
    for (size_t i = 0; i < errors_.size(); ++i) {
        error_index_[errors_[i].id] = i;
    }

    if (cleared > 0) {
        std::cout << "[ErrorCapture] Cleared " << cleared
                  << " errors older than " << max_age.count() << " minutes"
                  << std::endl;
    }

    return cleared;
}

// ============================================================================
// HANDLERS
// ============================================================================

std::string ErrorCapture::registerHandler(ErrorHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    std::string handler_id = "handler_" + std::to_string(id_counter_++);
    handlers_[handler_id] = std::move(handler);

    return handler_id;
}

void ErrorCapture::unregisterHandler(const std::string& handler_id) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_.erase(handler_id);
}

// ============================================================================
// UTILITY - STRING CONVERSION
// ============================================================================

std::string ErrorCapture::categoryToString(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::UNKNOWN: return "UNKNOWN";
        case ErrorCategory::DATABASE: return "DATABASE";
        case ErrorCategory::CONFIGURATION: return "CONFIGURATION";
        case ErrorCategory::FILE_IO: return "FILE_IO";
        case ErrorCategory::HTTP_REQUEST: return "HTTP_REQUEST";
        case ErrorCategory::HTTP_RESPONSE: return "HTTP_RESPONSE";
        case ErrorCategory::AUTHENTICATION: return "AUTHENTICATION";
        case ErrorCategory::VALIDATION: return "VALIDATION";
        case ErrorCategory::BUSINESS_LOGIC: return "BUSINESS_LOGIC";
        case ErrorCategory::EXTERNAL_API: return "EXTERNAL_API";
        case ErrorCategory::WEBSOCKET: return "WEBSOCKET";
        case ErrorCategory::MODULE: return "MODULE";
        default: return "UNKNOWN";
    }
}

ErrorCategory ErrorCapture::stringToCategory(const std::string& str) {
    static const std::unordered_map<std::string, ErrorCategory> map = {
        {"UNKNOWN", ErrorCategory::UNKNOWN},
        {"DATABASE", ErrorCategory::DATABASE},
        {"CONFIGURATION", ErrorCategory::CONFIGURATION},
        {"FILE_IO", ErrorCategory::FILE_IO},
        {"HTTP_REQUEST", ErrorCategory::HTTP_REQUEST},
        {"HTTP_RESPONSE", ErrorCategory::HTTP_RESPONSE},
        {"AUTHENTICATION", ErrorCategory::AUTHENTICATION},
        {"VALIDATION", ErrorCategory::VALIDATION},
        {"BUSINESS_LOGIC", ErrorCategory::BUSINESS_LOGIC},
        {"EXTERNAL_API", ErrorCategory::EXTERNAL_API},
        {"WEBSOCKET", ErrorCategory::WEBSOCKET},
        {"MODULE", ErrorCategory::MODULE}
    };

    auto it = map.find(str);
    return it != map.end() ? it->second : ErrorCategory::UNKNOWN;
}

std::string ErrorCapture::severityToString(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::TRACE: return "TRACE";
        case ErrorSeverity::DEBUG: return "DEBUG";
        case ErrorSeverity::INFO: return "INFO";
        case ErrorSeverity::WARNING: return "WARNING";
        case ErrorSeverity::ERROR: return "ERROR";
        case ErrorSeverity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

ErrorSeverity ErrorCapture::stringToSeverity(const std::string& str) {
    static const std::unordered_map<std::string, ErrorSeverity> map = {
        {"TRACE", ErrorSeverity::TRACE},
        {"DEBUG", ErrorSeverity::DEBUG},
        {"INFO", ErrorSeverity::INFO},
        {"WARNING", ErrorSeverity::WARNING},
        {"ERROR", ErrorSeverity::ERROR},
        {"CRITICAL", ErrorSeverity::CRITICAL}
    };

    auto it = map.find(str);
    return it != map.end() ? it->second : ErrorSeverity::ERROR;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::string ErrorCapture::generateId() const {
    // Generate a unique ID combining timestamp and counter
    auto now = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    uint64_t counter = id_counter_++;

    std::ostringstream ss;
    ss << "err_" << std::hex << millis << "_" << counter;

    return ss.str();
}

std::string ErrorCapture::captureStackTrace() const {
    // Platform-specific stack trace capture
    // This is a placeholder - real implementation would use:
    // - Windows: CaptureStackBackTrace + SymFromAddr
    // - Linux: backtrace() + backtrace_symbols()
    // - With libbacktrace or similar for better symbol resolution

    return "[Stack trace capture not implemented for this platform]";
}

void ErrorCapture::notifyHandlers(const ErrorContext& error) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    for (const auto& [id, handler] : handlers_) {
        try {
            handler(error);
        } catch (const std::exception& e) {
            // Don't let handler errors break the system
            std::cerr << "[ErrorCapture] Handler " << id << " threw: "
                      << e.what() << std::endl;
        }
    }
}

void ErrorCapture::pruneIfNeeded() {
    // Already holding write lock

    while (errors_.size() > max_errors_) {
        // Remove oldest error (from front)
        error_index_.erase(errors_.front().id);
        errors_.pop_front();
    }

    // Rebuild index after pruning (indices shifted)
    if (errors_.size() < error_index_.size()) {
        error_index_.clear();
        for (size_t i = 0; i < errors_.size(); ++i) {
            error_index_[errors_[i].id] = i;
        }
    }
}

// ============================================================================
// FILTERED QUERIES (using ErrorFilter struct)
// ============================================================================

std::vector<Json::Value> ErrorCapture::getErrors(const ErrorFilter& filter) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<Json::Value> result;
    int skipped = 0;
    int collected = 0;

    // Search newest first
    for (auto it = errors_.rbegin(); it != errors_.rend(); ++it) {
        // Apply filters
        if (!filter.severity.empty()) {
            if (severityToString(it->severity) != filter.severity &&
                // Also match lowercase
                severityToString(it->severity) !=
                    std::string(filter.severity.begin(), filter.severity.end())) {
                // Case-insensitive compare
                std::string err_sev = severityToString(it->severity);
                std::string filt_sev = filter.severity;
                std::transform(err_sev.begin(), err_sev.end(), err_sev.begin(), ::tolower);
                std::transform(filt_sev.begin(), filt_sev.end(), filt_sev.begin(), ::tolower);
                if (err_sev != filt_sev) {
                    continue;
                }
            }
        }

        if (!filter.category.empty()) {
            std::string err_cat = categoryToString(it->category);
            std::string filt_cat = filter.category;
            std::transform(err_cat.begin(), err_cat.end(), err_cat.begin(), ::toupper);
            std::transform(filt_cat.begin(), filt_cat.end(), filt_cat.begin(), ::toupper);
            if (err_cat != filt_cat) {
                continue;
            }
        }

        if (filter.from_time.time_since_epoch().count() > 0 &&
            it->timestamp < filter.from_time) {
            continue;
        }

        if (filter.to_time.time_since_epoch().count() > 0 &&
            it->timestamp > filter.to_time) {
            continue;
        }

        // Apply offset
        if (skipped < filter.offset) {
            ++skipped;
            continue;
        }

        // Apply limit
        if (filter.limit > 0 && collected >= filter.limit) {
            break;
        }

        result.push_back(it->toJson());
        ++collected;
    }

    return result;
}

int ErrorCapture::countErrors(const ErrorFilter& filter) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    int count = 0;

    for (const auto& err : errors_) {
        // Apply filters
        if (!filter.severity.empty()) {
            std::string err_sev = severityToString(err.severity);
            std::string filt_sev = filter.severity;
            std::transform(err_sev.begin(), err_sev.end(), err_sev.begin(), ::tolower);
            std::transform(filt_sev.begin(), filt_sev.end(), filt_sev.begin(), ::tolower);
            if (err_sev != filt_sev) {
                continue;
            }
        }

        if (!filter.category.empty()) {
            std::string err_cat = categoryToString(err.category);
            std::string filt_cat = filter.category;
            std::transform(err_cat.begin(), err_cat.end(), err_cat.begin(), ::toupper);
            std::transform(filt_cat.begin(), filt_cat.end(), filt_cat.begin(), ::toupper);
            if (err_cat != filt_cat) {
                continue;
            }
        }

        if (filter.from_time.time_since_epoch().count() > 0 &&
            err.timestamp < filter.from_time) {
            continue;
        }

        if (filter.to_time.time_since_epoch().count() > 0 &&
            err.timestamp > filter.to_time) {
            continue;
        }

        ++count;
    }

    return count;
}

} // namespace wtl::core::debug
