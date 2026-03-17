/**
 * @file HttpClient.h
 * @brief Reusable HTTP client wrapper with retry and circuit breaker support
 *
 * PURPOSE:
 * Provides a robust HTTP client for making API calls to external services.
 * Includes timeout handling, retry with exponential backoff, circuit breaker
 * integration, and rate limiting support.
 *
 * USAGE:
 * HttpClient client("https://api.example.com");
 * auto response = client.get("/endpoint", {{"param", "value"}});
 * if (response.success) {
 *     auto json = response.json();
 * }
 *
 * DEPENDENCIES:
 * - libcurl (via Drogon's HttpClient or standalone)
 * - jsoncpp for JSON handling
 * - RateLimiter for rate limiting
 * - SelfHealing for circuit breaker
 *
 * MODIFICATION GUIDE:
 * - Add new HTTP methods as needed
 * - Extend headers configuration
 * - Add authentication methods
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
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

// Forward declarations
namespace wtl::clients {
    class RateLimiter;
}

namespace wtl::clients {

/**
 * @struct HttpResponse
 * @brief Response from an HTTP request
 */
struct HttpResponse {
    bool success{false};               ///< Whether request was successful
    int status_code{0};                ///< HTTP status code
    std::string body;                  ///< Response body
    std::unordered_map<std::string, std::string> headers; ///< Response headers
    std::string error_message;         ///< Error message if failed
    std::chrono::milliseconds latency{0};  ///< Request latency
    size_t bytes_received{0};          ///< Total bytes received
    int retry_count{0};                ///< Number of retries made

    /**
     * @brief Parse body as JSON
     * @return Parsed JSON, or null value on parse error
     */
    Json::Value json() const;

    /**
     * @brief Check if response indicates success (2xx status)
     * @return true if status is 2xx
     */
    bool isOk() const { return status_code >= 200 && status_code < 300; }

    /**
     * @brief Check if response is a client error (4xx)
     * @return true if status is 4xx
     */
    bool isClientError() const { return status_code >= 400 && status_code < 500; }

    /**
     * @brief Check if response is a server error (5xx)
     * @return true if status is 5xx
     */
    bool isServerError() const { return status_code >= 500 && status_code < 600; }

    /**
     * @brief Check if error is retryable
     * @return true if should retry
     */
    bool isRetryable() const {
        return status_code == 429 ||  // Rate limited
               status_code == 503 ||  // Service unavailable
               status_code == 504 ||  // Gateway timeout
               (status_code >= 500 && status_code < 600);  // Server errors
    }

    /**
     * @brief Get a header value
     * @param name Header name (case-insensitive)
     * @return Header value or empty string
     */
    std::string getHeader(const std::string& name) const;

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;
};

/**
 * @struct HttpClientConfig
 * @brief Configuration for HTTP client
 */
struct HttpClientConfig {
    std::string base_url;                      ///< Base URL for all requests
    int timeout_seconds{30};                   ///< Request timeout
    int max_retries{3};                        ///< Maximum retry attempts
    int initial_retry_delay_ms{1000};          ///< Initial retry delay
    int max_retry_delay_ms{30000};             ///< Maximum retry delay
    double retry_backoff_multiplier{2.0};      ///< Backoff multiplier
    bool use_circuit_breaker{true};            ///< Enable circuit breaker
    std::string circuit_breaker_name;          ///< Circuit breaker identifier
    std::string rate_limiter_name;             ///< Rate limiter identifier
    std::unordered_map<std::string, std::string> default_headers;  ///< Default headers

    /**
     * @brief Create from JSON
     * @param json Configuration JSON
     * @return HttpClientConfig
     */
    static HttpClientConfig fromJson(const Json::Value& json);

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;
};

/**
 * @class HttpClient
 * @brief Robust HTTP client with retry and circuit breaker support
 *
 * Wraps HTTP operations with:
 * - Automatic retry with exponential backoff
 * - Circuit breaker for failing services
 * - Rate limiting integration
 * - Configurable timeouts
 * - Request/response logging
 */
class HttpClient {
public:
    // Type aliases
    using Headers = std::unordered_map<std::string, std::string>;
    using QueryParams = std::unordered_map<std::string, std::string>;
    using ProgressCallback = std::function<void(size_t downloaded, size_t total)>;

    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Construct with base URL
     * @param base_url Base URL for all requests
     */
    explicit HttpClient(const std::string& base_url);

    /**
     * @brief Construct with configuration
     * @param config Client configuration
     */
    explicit HttpClient(const HttpClientConfig& config);

    /**
     * @brief Default constructor
     */
    HttpClient();

    /**
     * @brief Destructor
     */
    ~HttpClient();

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Configure the client
     * @param config Configuration to apply
     */
    void configure(const HttpClientConfig& config);

    /**
     * @brief Set the base URL
     * @param url Base URL
     */
    void setBaseUrl(const std::string& url);

    /**
     * @brief Get the base URL
     * @return Current base URL
     */
    std::string getBaseUrl() const;

    /**
     * @brief Set request timeout
     * @param seconds Timeout in seconds
     */
    void setTimeout(int seconds);

    /**
     * @brief Set default header
     * @param name Header name
     * @param value Header value
     */
    void setDefaultHeader(const std::string& name, const std::string& value);

    /**
     * @brief Set multiple default headers
     * @param headers Headers to set
     */
    void setDefaultHeaders(const Headers& headers);

    /**
     * @brief Remove a default header
     * @param name Header name
     */
    void removeDefaultHeader(const std::string& name);

    /**
     * @brief Set rate limiter
     * @param limiter_name Name of rate limiter in RateLimiterManager
     */
    void setRateLimiter(const std::string& limiter_name);

    /**
     * @brief Set circuit breaker
     * @param name Circuit breaker name for SelfHealing
     */
    void setCircuitBreaker(const std::string& name);

    /**
     * @brief Enable/disable circuit breaker
     * @param enabled Whether to use circuit breaker
     */
    void setCircuitBreakerEnabled(bool enabled);

    /**
     * @brief Set retry configuration
     * @param max_retries Maximum retry attempts
     * @param initial_delay_ms Initial delay in ms
     * @param max_delay_ms Maximum delay in ms
     * @param backoff_multiplier Backoff multiplier
     */
    void setRetryConfig(int max_retries, int initial_delay_ms = 1000,
                        int max_delay_ms = 30000, double backoff_multiplier = 2.0);

    // =========================================================================
    // HTTP METHODS
    // =========================================================================

    /**
     * @brief Perform GET request
     *
     * @param path URL path (appended to base URL)
     * @param params Query parameters
     * @param headers Additional headers
     * @return HttpResponse
     */
    HttpResponse get(const std::string& path,
                     const QueryParams& params = {},
                     const Headers& headers = {});

    /**
     * @brief Perform POST request with JSON body
     *
     * @param path URL path
     * @param body JSON body
     * @param headers Additional headers
     * @return HttpResponse
     */
    HttpResponse post(const std::string& path,
                      const Json::Value& body,
                      const Headers& headers = {});

    /**
     * @brief Perform POST request with form data
     *
     * @param path URL path
     * @param form_data Form data
     * @param headers Additional headers
     * @return HttpResponse
     */
    HttpResponse postForm(const std::string& path,
                          const QueryParams& form_data,
                          const Headers& headers = {});

    /**
     * @brief Perform PUT request with JSON body
     *
     * @param path URL path
     * @param body JSON body
     * @param headers Additional headers
     * @return HttpResponse
     */
    HttpResponse put(const std::string& path,
                     const Json::Value& body,
                     const Headers& headers = {});

    /**
     * @brief Perform PATCH request with JSON body
     *
     * @param path URL path
     * @param body JSON body
     * @param headers Additional headers
     * @return HttpResponse
     */
    HttpResponse patch(const std::string& path,
                       const Json::Value& body,
                       const Headers& headers = {});

    /**
     * @brief Perform DELETE request
     *
     * @param path URL path
     * @param params Query parameters
     * @param headers Additional headers
     * @return HttpResponse
     */
    HttpResponse del(const std::string& path,
                     const QueryParams& params = {},
                     const Headers& headers = {});

    /**
     * @brief Perform HEAD request
     *
     * @param path URL path
     * @param headers Additional headers
     * @return HttpResponse
     */
    HttpResponse head(const std::string& path,
                      const Headers& headers = {});

    // =========================================================================
    // ADVANCED REQUESTS
    // =========================================================================

    /**
     * @brief Perform request with progress callback
     *
     * @param method HTTP method (GET, POST, etc.)
     * @param path URL path
     * @param body Request body (for POST/PUT/PATCH)
     * @param headers Headers
     * @param progress Progress callback
     * @return HttpResponse
     */
    HttpResponse requestWithProgress(const std::string& method,
                                     const std::string& path,
                                     const std::string& body,
                                     const Headers& headers,
                                     ProgressCallback progress);

    /**
     * @brief Build full URL from path and params
     * @param path URL path
     * @param params Query parameters
     * @return Full URL
     */
    std::string buildUrl(const std::string& path,
                         const QueryParams& params = {}) const;

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get request statistics
     * @return JSON with statistics
     */
    Json::Value getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    /**
     * @brief Get total requests made
     * @return Request count
     */
    size_t getTotalRequests() const;

    /**
     * @brief Get failed request count
     * @return Failed count
     */
    size_t getFailedRequests() const;

    /**
     * @brief Get average latency
     * @return Average latency in ms
     */
    double getAverageLatency() const;

    /**
     * @brief URL encode a string
     * @param str String to encode
     * @return URL encoded string
     */
    static std::string urlEncode(const std::string& str);

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    /**
     * @brief Perform the actual HTTP request
     * @param method HTTP method
     * @param url Full URL
     * @param body Request body
     * @param headers All headers
     * @return HttpResponse
     */
    HttpResponse doRequest(const std::string& method,
                           const std::string& url,
                           const std::string& body,
                           const Headers& headers);

    /**
     * @brief Execute request with retry logic
     * @param method HTTP method
     * @param url Full URL
     * @param body Request body
     * @param headers All headers
     * @return HttpResponse
     */
    HttpResponse executeWithRetry(const std::string& method,
                                  const std::string& url,
                                  const std::string& body,
                                  const Headers& headers);

    /**
     * @brief Merge default headers with request headers
     * @param request_headers Request-specific headers
     * @return Merged headers
     */
    Headers mergeHeaders(const Headers& request_headers) const;

    /**
     * @brief Calculate retry delay with exponential backoff
     * @param attempt Attempt number (0-based)
     * @return Delay in milliseconds
     */
    int calculateRetryDelay(int attempt) const;

    /**
     * @brief Update statistics after request
     * @param response The response
     */
    void updateStats(const HttpResponse& response);

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    std::string base_url_;
    int timeout_seconds_{30};
    Headers default_headers_;

    // Retry configuration
    int max_retries_{3};
    int initial_retry_delay_ms_{1000};
    int max_retry_delay_ms_{30000};
    double retry_backoff_multiplier_{2.0};

    // Circuit breaker and rate limiter
    bool use_circuit_breaker_{true};
    std::string circuit_breaker_name_;
    std::string rate_limiter_name_;

    // Statistics
    mutable std::mutex stats_mutex_;
    size_t total_requests_{0};
    size_t failed_requests_{0};
    size_t total_retries_{0};
    size_t total_latency_ms_{0};
    size_t total_bytes_received_{0};
};

/**
 * @brief URL encode a query parameter map
 * @param params Parameters to encode
 * @return URL encoded query string
 */
std::string encodeQueryParams(const HttpClient::QueryParams& params);

} // namespace wtl::clients
