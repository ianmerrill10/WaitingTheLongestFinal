/**
 * @file ErrorCapture.h
 * @brief Error capture and logging system for WaitingTheLongest application
 *
 * PURPOSE:
 * Provides centralized error capture, categorization, and storage.
 * Supports error querying, export, and integration with self-healing
 * and AI analysis systems.
 *
 * USAGE:
 * // Using macros (preferred):
 * WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, "Connection failed",
 *                   {{"host", "localhost"}, {"port", "5432"}});
 *
 * // Using class directly:
 * ErrorCapture::getInstance().capture(
 *     ErrorCategory::DATABASE,
 *     ErrorSeverity::ERROR,
 *     "Connection failed",
 *     {{"host", "localhost"}});
 *
 * DEPENDENCIES:
 * - jsoncpp (JSON handling)
 * - Standard library (chrono, mutex, etc.)
 *
 * MODIFICATION GUIDE:
 * - Add new error categories to ErrorCategory enum
 * - Add new severity levels to ErrorSeverity enum
 * - Extend ErrorContext with additional fields as needed
 *
 * @author Agent 1 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::core::debug {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum ErrorCategory
 * @brief Categories for classifying errors
 *
 * Used to organize errors by subsystem for filtering and analysis.
 */
enum class ErrorCategory {
    UNKNOWN = 0,          ///< Uncategorized errors
    DATABASE,             ///< PostgreSQL and database-related errors
    CONFIGURATION,        ///< Configuration loading/parsing errors
    FILE_IO,              ///< File system operations
    HTTP_REQUEST,         ///< Incoming HTTP request errors
    HTTP_RESPONSE,        ///< Outgoing HTTP response errors
    AUTHENTICATION,       ///< Login, token, and auth errors
    VALIDATION,           ///< Input validation failures
    BUSINESS_LOGIC,       ///< Application logic errors
    EXTERNAL_API,         ///< Third-party API errors (RescueGroups, etc.)
    WEBSOCKET,            ///< WebSocket connection/message errors
    MODULE,               ///< Module loading and execution errors
    INTERNAL,             ///< Internal application errors
    CONTENT               ///< Content generation and publishing errors
};

/**
 * @enum ErrorSeverity
 * @brief Severity levels for errors
 *
 * Determines urgency and handling of errors.
 */
enum class ErrorSeverity {
    TRACE = 0,    ///< Detailed debugging information
    DEBUG,        ///< Debug-level messages
    INFO,         ///< Informational messages
    WARNING,      ///< Warning - something unexpected but recoverable
    ERROR,        ///< Error - operation failed but system continues
    CRITICAL      ///< Critical - system stability affected
};

// ============================================================================
// ERROR CONTEXT STRUCTURE
// ============================================================================

/**
 * @struct ErrorContext
 * @brief Complete context for a captured error
 *
 * Contains all information about an error including when it occurred,
 * where in the code, what category, and any additional metadata.
 */
struct ErrorContext {
    // Unique identifier for this error
    std::string id;

    // Error classification
    ErrorCategory category;
    ErrorSeverity severity;

    // Error message
    std::string message;

    // Optional detailed description
    std::string details;

    // Source location (file, function, line)
    std::string file;
    std::string function;
    int line;

    // Timestamp
    std::chrono::system_clock::time_point timestamp;

    // Additional key-value metadata
    std::unordered_map<std::string, std::string> metadata;

    // Optional stack trace
    std::string stack_trace;

    // Error occurrence count (for deduplication)
    int occurrence_count;

    // Last occurrence time (for deduplication)
    std::chrono::system_clock::time_point last_occurrence;

    // Request ID if applicable
    std::optional<std::string> request_id;

    // User ID if applicable
    std::optional<std::string> user_id;

    // Convert to JSON for export/logging
    Json::Value toJson() const;

    // Create from JSON (for import)
    static ErrorContext fromJson(const Json::Value& json);
};

// ============================================================================
// ERROR QUERY PARAMETERS
// ============================================================================

/**
 * @struct ErrorFilter
 * @brief Query parameters for filtering errors
 *
 * Used by controllers and admin interfaces to query errors
 * with filtering, pagination, and time range support.
 */
struct ErrorFilter {
    std::string severity;   ///< Filter by severity level (empty = all)
    std::string category;   ///< Filter by category (empty = all)
    std::chrono::system_clock::time_point from_time;  ///< Start time
    std::chrono::system_clock::time_point to_time;    ///< End time
    int limit = 100;        ///< Maximum results to return
    int offset = 0;         ///< Offset for pagination
};

// ============================================================================
// ERROR CAPTURE CLASS
// ============================================================================

/**
 * @class ErrorCapture
 * @brief Singleton class for capturing and managing errors
 *
 * Thread-safe error capture system that stores errors in memory,
 * supports querying, and provides export capabilities.
 */
class ErrorCapture {
public:
    // Type aliases
    using ErrorHandler = std::function<void(const ErrorContext&)>;
    using ErrorPredicate = std::function<bool(const ErrorContext&)>;

    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the ErrorCapture singleton
     */
    static ErrorCapture& getInstance();

    // Delete copy/move constructors
    ErrorCapture(const ErrorCapture&) = delete;
    ErrorCapture& operator=(const ErrorCapture&) = delete;
    ErrorCapture(ErrorCapture&&) = delete;
    ErrorCapture& operator=(ErrorCapture&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the error capture system
     *
     * @param max_errors Maximum errors to keep in memory (default 1000)
     * @param capture_stack_traces Whether to capture stack traces (slow)
     */
    void initialize(size_t max_errors = 1000,
                    bool capture_stack_traces = false);

    /**
     * @brief Initialize from Config
     */
    void initializeFromConfig();

    // =========================================================================
    // ERROR CAPTURE
    // =========================================================================

    /**
     * @brief Capture an error
     *
     * @param category Error category
     * @param severity Error severity
     * @param message Error message
     * @param metadata Additional key-value data
     * @param file Source file (auto-filled by macro)
     * @param function Function name (auto-filled by macro)
     * @param line Line number (auto-filled by macro)
     * @return The error ID
     *
     * @example
     * captureError(ErrorCategory::DATABASE, ErrorSeverity::ERROR,
     *              "Connection failed", {{"host", "localhost"}},
     *              __FILE__, __func__, __LINE__);
     */
    std::string capture(
        ErrorCategory category,
        ErrorSeverity severity,
        const std::string& message,
        const std::unordered_map<std::string, std::string>& metadata = {},
        const std::string& file = "",
        const std::string& function = "",
        int line = 0);

    /**
     * @brief Capture an error with request context
     *
     * @param category Error category
     * @param severity Error severity
     * @param message Error message
     * @param request_id Request ID for correlation
     * @param metadata Additional key-value data
     * @return The error ID
     */
    std::string captureWithRequest(
        ErrorCategory category,
        ErrorSeverity severity,
        const std::string& message,
        const std::string& request_id,
        const std::unordered_map<std::string, std::string>& metadata = {});

    /**
     * @brief Capture an exception
     *
     * @param e The exception to capture
     * @param category Error category
     * @param metadata Additional metadata
     * @return The error ID
     */
    std::string captureException(
        const std::exception& e,
        ErrorCategory category,
        const std::unordered_map<std::string, std::string>& metadata = {});

    // =========================================================================
    // QUERYING
    // =========================================================================

    /**
     * @brief Get all errors
     *
     * @param limit Maximum number to return (0 = all)
     * @return Vector of error contexts
     */
    std::vector<ErrorContext> getAll(size_t limit = 0) const;

    /**
     * @brief Get errors by category
     *
     * @param category The category to filter by
     * @param limit Maximum number to return
     * @return Vector of matching errors
     */
    std::vector<ErrorContext> getByCategory(ErrorCategory category,
                                             size_t limit = 0) const;

    /**
     * @brief Get errors by severity
     *
     * @param min_severity Minimum severity level
     * @param limit Maximum number to return
     * @return Vector of matching errors
     */
    std::vector<ErrorContext> getBySeverity(ErrorSeverity min_severity,
                                             size_t limit = 0) const;

    /**
     * @brief Get errors in time range
     *
     * @param start Start time
     * @param end End time
     * @param limit Maximum number to return
     * @return Vector of matching errors
     */
    std::vector<ErrorContext> getByTimeRange(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end,
        size_t limit = 0) const;

    /**
     * @brief Query errors with custom filter
     *
     * @param filter Filter function
     * @param limit Maximum number to return
     * @return Vector of matching errors
     */
    std::vector<ErrorContext> query(const ErrorPredicate& filter,
                                     size_t limit = 0) const;

    /**
     * @brief Get a specific error by ID
     *
     * @param id The error ID
     * @return The error context if found
     */
    std::optional<ErrorContext> getById(const std::string& id) const;

    /**
     * @brief Get recent errors (last N minutes)
     *
     * @param minutes Number of minutes to look back
     * @param limit Maximum number to return
     * @return Vector of recent errors
     */
    std::vector<ErrorContext> getRecent(int minutes = 60,
                                         size_t limit = 100) const;

    // =========================================================================
    // FILTERED QUERIES (using ErrorFilter struct)
    // =========================================================================

    /**
     * @brief Get errors matching filter parameters
     *
     * @param filter ErrorFilter with query parameters
     * @return Vector of matching error contexts as JSON
     */
    std::vector<Json::Value> getErrors(const ErrorFilter& filter) const;

    /**
     * @brief Count errors matching filter parameters
     *
     * @param filter ErrorFilter with query parameters
     * @return Number of matching errors
     */
    int countErrors(const ErrorFilter& filter) const;

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get error statistics
     *
     * @return JSON with error counts by category and severity
     */
    Json::Value getStats() const;

    /**
     * @brief Get count of errors
     *
     * @return Total number of errors captured
     */
    size_t getCount() const;

    /**
     * @brief Get count by category
     *
     * @param category The category to count
     * @return Number of errors in that category
     */
    size_t getCountByCategory(ErrorCategory category) const;

    // =========================================================================
    // EXPORT
    // =========================================================================

    /**
     * @brief Export all errors as JSON
     *
     * @return JSON array of all errors
     */
    Json::Value exportToJson() const;

    /**
     * @brief Export errors matching filter as JSON
     *
     * @param filter Filter function
     * @return JSON array of matching errors
     */
    Json::Value exportToJson(const ErrorPredicate& filter) const;

    // =========================================================================
    // MANAGEMENT
    // =========================================================================

    /**
     * @brief Clear all captured errors
     */
    void clear();

    /**
     * @brief Clear errors older than specified time
     *
     * @param max_age Maximum age of errors to keep
     * @return Number of errors cleared
     */
    size_t clearOlderThan(std::chrono::minutes max_age);

    // =========================================================================
    // HANDLERS
    // =========================================================================

    /**
     * @brief Register an error handler
     *
     * @param handler Function to call when error is captured
     * @return Handler ID for unregistering
     *
     * Handlers are called synchronously when an error is captured.
     * Use for real-time alerting or logging.
     */
    std::string registerHandler(ErrorHandler handler);

    /**
     * @brief Unregister an error handler
     *
     * @param handler_id The handler ID returned from registerHandler
     */
    void unregisterHandler(const std::string& handler_id);

    // =========================================================================
    // UTILITY
    // =========================================================================

    /**
     * @brief Convert ErrorCategory to string
     * @param category The category
     * @return String representation
     */
    static std::string categoryToString(ErrorCategory category);

    /**
     * @brief Convert string to ErrorCategory
     * @param str The string
     * @return The category
     */
    static ErrorCategory stringToCategory(const std::string& str);

    /**
     * @brief Convert ErrorSeverity to string
     * @param severity The severity
     * @return String representation
     */
    static std::string severityToString(ErrorSeverity severity);

    /**
     * @brief Convert string to ErrorSeverity
     * @param str The string
     * @return The severity
     */
    static ErrorSeverity stringToSeverity(const std::string& str);

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    ErrorCapture() = default;
    ~ErrorCapture() = default;

    /**
     * @brief Generate a unique error ID
     * @return Unique ID string
     */
    std::string generateId() const;

    /**
     * @brief Capture current stack trace
     * @return Stack trace as string
     */
    std::string captureStackTrace() const;

    /**
     * @brief Call all registered handlers
     * @param error The error to handle
     */
    void notifyHandlers(const ErrorContext& error);

    /**
     * @brief Prune old errors if over limit
     */
    void pruneIfNeeded();

    // Error storage (newest at back)
    std::deque<ErrorContext> errors_;

    // Index by ID for fast lookup
    std::unordered_map<std::string, size_t> error_index_;

    // Registered handlers
    std::unordered_map<std::string, ErrorHandler> handlers_;

    // Configuration
    size_t max_errors_{1000};
    bool capture_stack_traces_{false};

    // Statistics
    std::atomic<uint64_t> total_captured_{0};
    std::unordered_map<ErrorCategory, std::atomic<uint64_t>> category_counts_;

    // Thread safety
    mutable std::shared_mutex mutex_;
    mutable std::mutex handlers_mutex_;

    // Counter for generating IDs
    mutable std::atomic<uint64_t> id_counter_{0};
};

// ============================================================================
// MACROS FOR CONVENIENT ERROR CAPTURE
// ============================================================================

/**
 * @def WTL_CAPTURE_ERROR
 * @brief Macro for capturing errors with automatic source location
 *
 * @param category ErrorCategory value
 * @param message Error message string
 * @param metadata Metadata map (use {} for empty)
 *
 * @example
 * WTL_CAPTURE_ERROR(ErrorCategory::DATABASE, "Query failed",
 *                   {{"query", "SELECT * FROM dogs"}});
 */
#define WTL_CAPTURE_ERROR(category, message, ...) \
    ::wtl::core::debug::ErrorCapture::getInstance().capture( \
        category, \
        ::wtl::core::debug::ErrorSeverity::ERROR, \
        message, \
        std::unordered_map<std::string, std::string>{__VA_ARGS__}, \
        __FILE__, \
        __func__, \
        __LINE__)

/**
 * @def WTL_CAPTURE_CRITICAL
 * @brief Macro for capturing critical errors
 *
 * @param category ErrorCategory value
 * @param message Error message string
 * @param metadata Metadata map
 */
#define WTL_CAPTURE_CRITICAL(category, message, ...) \
    ::wtl::core::debug::ErrorCapture::getInstance().capture( \
        category, \
        ::wtl::core::debug::ErrorSeverity::CRITICAL, \
        message, \
        std::unordered_map<std::string, std::string>{__VA_ARGS__}, \
        __FILE__, \
        __func__, \
        __LINE__)

/**
 * @def WTL_CAPTURE_WARNING
 * @brief Macro for capturing warnings
 *
 * @param category ErrorCategory value
 * @param message Warning message string
 * @param metadata Metadata map
 */
#define WTL_CAPTURE_WARNING(category, message, ...) \
    ::wtl::core::debug::ErrorCapture::getInstance().capture( \
        category, \
        ::wtl::core::debug::ErrorSeverity::WARNING, \
        message, \
        std::unordered_map<std::string, std::string>{__VA_ARGS__}, \
        __FILE__, \
        __func__, \
        __LINE__)

/**
 * @def WTL_CAPTURE_INFO
 * @brief Macro for capturing informational messages
 *
 * @param category ErrorCategory value
 * @param message Info message string
 * @param metadata Metadata map
 */
#define WTL_CAPTURE_INFO(category, message, ...) \
    ::wtl::core::debug::ErrorCapture::getInstance().capture( \
        category, \
        ::wtl::core::debug::ErrorSeverity::INFO, \
        message, \
        std::unordered_map<std::string, std::string>{__VA_ARGS__}, \
        __FILE__, \
        __func__, \
        __LINE__)

/**
 * @def WTL_CAPTURE_EXCEPTION
 * @brief Macro for capturing exceptions
 *
 * @param exception The exception object
 * @param category ErrorCategory value
 * @param metadata Additional metadata
 */
#define WTL_CAPTURE_EXCEPTION(exception, category, metadata) \
    ::wtl::core::debug::ErrorCapture::getInstance().captureException( \
        exception, \
        category, \
        metadata)

} // namespace wtl::core::debug

// Make ErrorCategory available in sibling namespaces that commonly use WTL_CAPTURE_ERROR
// This avoids requiring fully-qualified names when calling the error macros
namespace wtl::core::controllers {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::core::services {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::core {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::modules {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::aggregators {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::content {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::content::media {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::content::tiktok {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::content::social {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::content::blog {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::content::templates {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::analytics {
    using ::wtl::core::debug::ErrorCategory;
}
namespace wtl::notifications {
    using ::wtl::core::debug::ErrorCategory;
}
