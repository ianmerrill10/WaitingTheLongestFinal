/**
 * @file HttpClient.cc
 * @brief Implementation of HTTP client with retry and circuit breaker
 * @see HttpClient.h for documentation
 */

#include "clients/HttpClient.h"
#include "clients/RateLimiter.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

// Drogon HTTP client
#include <drogon/HttpClient.h>
#include <drogon/drogon.h>

#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::clients {

// ============================================================================
// HTTP RESPONSE METHODS
// ============================================================================

Json::Value HttpResponse::json() const {
    Json::Value result;
    Json::Reader reader;

    if (!reader.parse(body, result)) {
        return Json::Value();
    }
    return result;
}

std::string HttpResponse::getHeader(const std::string& name) const {
    // Case-insensitive header lookup
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    for (const auto& [key, value] : headers) {
        std::string lower_key = key;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
        if (lower_key == lower_name) {
            return value;
        }
    }
    return "";
}

Json::Value HttpResponse::toJson() const {
    Json::Value json;
    json["success"] = success;
    json["status_code"] = status_code;
    json["error_message"] = error_message;
    json["latency_ms"] = static_cast<Json::Int64>(latency.count());
    json["bytes_received"] = static_cast<Json::UInt64>(bytes_received);
    json["retry_count"] = retry_count;

    Json::Value header_json;
    for (const auto& [key, value] : headers) {
        header_json[key] = value;
    }
    json["headers"] = header_json;

    // Only include body if not too large
    if (body.length() < 10000) {
        json["body"] = body;
    } else {
        json["body_truncated"] = true;
        json["body_length"] = static_cast<Json::UInt64>(body.length());
    }

    return json;
}

// ============================================================================
// HTTP CLIENT CONFIG
// ============================================================================

HttpClientConfig HttpClientConfig::fromJson(const Json::Value& json) {
    HttpClientConfig config;
    config.base_url = json.get("base_url", "").asString();
    config.timeout_seconds = json.get("timeout_seconds", 30).asInt();
    config.max_retries = json.get("max_retries", 3).asInt();
    config.initial_retry_delay_ms = json.get("initial_retry_delay_ms", 1000).asInt();
    config.max_retry_delay_ms = json.get("max_retry_delay_ms", 30000).asInt();
    config.retry_backoff_multiplier = json.get("retry_backoff_multiplier", 2.0).asDouble();
    config.use_circuit_breaker = json.get("use_circuit_breaker", true).asBool();
    config.circuit_breaker_name = json.get("circuit_breaker_name", "").asString();
    config.rate_limiter_name = json.get("rate_limiter_name", "").asString();

    if (json.isMember("default_headers")) {
        for (const auto& key : json["default_headers"].getMemberNames()) {
            config.default_headers[key] = json["default_headers"][key].asString();
        }
    }

    return config;
}

Json::Value HttpClientConfig::toJson() const {
    Json::Value json;
    json["base_url"] = base_url;
    json["timeout_seconds"] = timeout_seconds;
    json["max_retries"] = max_retries;
    json["initial_retry_delay_ms"] = initial_retry_delay_ms;
    json["max_retry_delay_ms"] = max_retry_delay_ms;
    json["retry_backoff_multiplier"] = retry_backoff_multiplier;
    json["use_circuit_breaker"] = use_circuit_breaker;
    json["circuit_breaker_name"] = circuit_breaker_name;
    json["rate_limiter_name"] = rate_limiter_name;

    Json::Value headers_json;
    for (const auto& [key, value] : default_headers) {
        headers_json[key] = value;
    }
    json["default_headers"] = headers_json;

    return json;
}

// ============================================================================
// HTTP CLIENT CONSTRUCTION
// ============================================================================

HttpClient::HttpClient(const std::string& base_url)
    : base_url_(base_url) {
    // Set default headers
    default_headers_["User-Agent"] = "WaitingTheLongest/1.0";
    default_headers_["Accept"] = "application/json";
}

HttpClient::HttpClient(const HttpClientConfig& config) {
    configure(config);
}

HttpClient::HttpClient() {
    default_headers_["User-Agent"] = "WaitingTheLongest/1.0";
    default_headers_["Accept"] = "application/json";
}

HttpClient::~HttpClient() = default;

// ============================================================================
// CONFIGURATION
// ============================================================================

void HttpClient::configure(const HttpClientConfig& config) {
    base_url_ = config.base_url;
    timeout_seconds_ = config.timeout_seconds;
    max_retries_ = config.max_retries;
    initial_retry_delay_ms_ = config.initial_retry_delay_ms;
    max_retry_delay_ms_ = config.max_retry_delay_ms;
    retry_backoff_multiplier_ = config.retry_backoff_multiplier;
    use_circuit_breaker_ = config.use_circuit_breaker;
    circuit_breaker_name_ = config.circuit_breaker_name;
    rate_limiter_name_ = config.rate_limiter_name;
    default_headers_ = config.default_headers;

    if (default_headers_.find("User-Agent") == default_headers_.end()) {
        default_headers_["User-Agent"] = "WaitingTheLongest/1.0";
    }
    if (default_headers_.find("Accept") == default_headers_.end()) {
        default_headers_["Accept"] = "application/json";
    }
}

void HttpClient::setBaseUrl(const std::string& url) {
    base_url_ = url;
}

std::string HttpClient::getBaseUrl() const {
    return base_url_;
}

void HttpClient::setTimeout(int seconds) {
    timeout_seconds_ = seconds;
}

void HttpClient::setDefaultHeader(const std::string& name, const std::string& value) {
    default_headers_[name] = value;
}

void HttpClient::setDefaultHeaders(const Headers& headers) {
    for (const auto& [name, value] : headers) {
        default_headers_[name] = value;
    }
}

void HttpClient::removeDefaultHeader(const std::string& name) {
    default_headers_.erase(name);
}

void HttpClient::setRateLimiter(const std::string& limiter_name) {
    rate_limiter_name_ = limiter_name;
}

void HttpClient::setCircuitBreaker(const std::string& name) {
    circuit_breaker_name_ = name;
}

void HttpClient::setCircuitBreakerEnabled(bool enabled) {
    use_circuit_breaker_ = enabled;
}

void HttpClient::setRetryConfig(int max_retries, int initial_delay_ms,
                                int max_delay_ms, double backoff_multiplier) {
    max_retries_ = max_retries;
    initial_retry_delay_ms_ = initial_delay_ms;
    max_retry_delay_ms_ = max_delay_ms;
    retry_backoff_multiplier_ = backoff_multiplier;
}

// ============================================================================
// HTTP METHODS
// ============================================================================

HttpResponse HttpClient::get(const std::string& path,
                              const QueryParams& params,
                              const Headers& headers) {
    std::string url = buildUrl(path, params);
    Headers merged_headers = mergeHeaders(headers);
    return executeWithRetry("GET", url, "", merged_headers);
}

HttpResponse HttpClient::post(const std::string& path,
                               const Json::Value& body,
                               const Headers& headers) {
    std::string url = buildUrl(path);
    Headers merged_headers = mergeHeaders(headers);
    merged_headers["Content-Type"] = "application/json";

    Json::StreamWriterBuilder writer;
    std::string body_str = Json::writeString(writer, body);

    return executeWithRetry("POST", url, body_str, merged_headers);
}

HttpResponse HttpClient::postForm(const std::string& path,
                                   const QueryParams& form_data,
                                   const Headers& headers) {
    std::string url = buildUrl(path);
    Headers merged_headers = mergeHeaders(headers);
    merged_headers["Content-Type"] = "application/x-www-form-urlencoded";

    std::string body_str = encodeQueryParams(form_data);
    return executeWithRetry("POST", url, body_str, merged_headers);
}

HttpResponse HttpClient::put(const std::string& path,
                              const Json::Value& body,
                              const Headers& headers) {
    std::string url = buildUrl(path);
    Headers merged_headers = mergeHeaders(headers);
    merged_headers["Content-Type"] = "application/json";

    Json::StreamWriterBuilder writer;
    std::string body_str = Json::writeString(writer, body);

    return executeWithRetry("PUT", url, body_str, merged_headers);
}

HttpResponse HttpClient::patch(const std::string& path,
                                const Json::Value& body,
                                const Headers& headers) {
    std::string url = buildUrl(path);
    Headers merged_headers = mergeHeaders(headers);
    merged_headers["Content-Type"] = "application/json";

    Json::StreamWriterBuilder writer;
    std::string body_str = Json::writeString(writer, body);

    return executeWithRetry("PATCH", url, body_str, merged_headers);
}

HttpResponse HttpClient::del(const std::string& path,
                              const QueryParams& params,
                              const Headers& headers) {
    std::string url = buildUrl(path, params);
    Headers merged_headers = mergeHeaders(headers);
    return executeWithRetry("DELETE", url, "", merged_headers);
}

HttpResponse HttpClient::head(const std::string& path,
                               const Headers& headers) {
    std::string url = buildUrl(path);
    Headers merged_headers = mergeHeaders(headers);
    return executeWithRetry("HEAD", url, "", merged_headers);
}

HttpResponse HttpClient::requestWithProgress(const std::string& method,
                                              const std::string& path,
                                              const std::string& body,
                                              const Headers& headers,
                                              ProgressCallback progress) {
    // Progress callback not fully implemented - would require custom curl handling
    std::string url = buildUrl(path);
    Headers merged_headers = mergeHeaders(headers);
    return executeWithRetry(method, url, body, merged_headers);
}

std::string HttpClient::buildUrl(const std::string& path,
                                  const QueryParams& params) const {
    std::string url = base_url_;

    // Ensure proper path joining
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    if (!path.empty() && path.front() != '/') {
        url += '/';
    }
    url += path;

    // Add query parameters
    if (!params.empty()) {
        url += '?' + encodeQueryParams(params);
    }

    return url;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value HttpClient::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Json::Value stats;
    stats["base_url"] = base_url_;
    stats["total_requests"] = static_cast<Json::UInt64>(total_requests_);
    stats["failed_requests"] = static_cast<Json::UInt64>(failed_requests_);
    stats["total_retries"] = static_cast<Json::UInt64>(total_retries_);
    stats["total_bytes_received"] = static_cast<Json::UInt64>(total_bytes_received_);

    if (total_requests_ > 0) {
        stats["average_latency_ms"] = static_cast<double>(total_latency_ms_) /
                                       static_cast<double>(total_requests_);
        stats["success_rate"] = (1.0 - static_cast<double>(failed_requests_) /
                                        static_cast<double>(total_requests_)) * 100.0;
    } else {
        stats["average_latency_ms"] = 0.0;
        stats["success_rate"] = 0.0;
    }

    return stats;
}

void HttpClient::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_requests_ = 0;
    failed_requests_ = 0;
    total_retries_ = 0;
    total_latency_ms_ = 0;
    total_bytes_received_ = 0;
}

size_t HttpClient::getTotalRequests() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return total_requests_;
}

size_t HttpClient::getFailedRequests() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return failed_requests_;
}

double HttpClient::getAverageLatency() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (total_requests_ == 0) return 0.0;
    return static_cast<double>(total_latency_ms_) / static_cast<double>(total_requests_);
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

HttpResponse HttpClient::doRequest(const std::string& method,
                                    const std::string& url,
                                    const std::string& body,
                                    const Headers& headers) {
    HttpResponse response;
    auto start_time = std::chrono::steady_clock::now();

    try {
        // Create Drogon HTTP client
        auto client = drogon::HttpClient::newHttpClient(url);
        client->setUserAgent(headers.count("User-Agent") ? headers.at("User-Agent") : "WaitingTheLongest/1.0");

        // Create request
        auto req = drogon::HttpRequest::newHttpRequest();

        // Set method
        if (method == "GET") {
            req->setMethod(drogon::HttpMethod::Get);
        } else if (method == "POST") {
            req->setMethod(drogon::HttpMethod::Post);
        } else if (method == "PUT") {
            req->setMethod(drogon::HttpMethod::Put);
        } else if (method == "DELETE") {
            req->setMethod(drogon::HttpMethod::Delete);
        } else if (method == "PATCH") {
            req->setMethod(drogon::HttpMethod::Patch);
        } else if (method == "HEAD") {
            req->setMethod(drogon::HttpMethod::Head);
        }

        // Set headers
        for (const auto& [name, value] : headers) {
            req->addHeader(name, value);
        }

        // Set body
        if (!body.empty()) {
            req->setBody(body);
        }

        // Blocking request using a promise/future pattern
        std::promise<std::pair<drogon::ReqResult, drogon::HttpResponsePtr>> promise;
        auto future = promise.get_future();

        client->sendRequest(req,
            [&promise](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
                promise.set_value({result, resp});
            },
            timeout_seconds_);

        // Wait for response with timeout
        auto status = future.wait_for(std::chrono::seconds(timeout_seconds_ + 5));
        if (status == std::future_status::timeout) {
            response.success = false;
            response.error_message = "Request timed out";
            auto end_time = std::chrono::steady_clock::now();
            response.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            return response;
        }

        auto [result, resp] = future.get();

        if (result == drogon::ReqResult::Ok && resp) {
            response.success = true;
            response.status_code = static_cast<int>(resp->getStatusCode());
            response.body = std::string(resp->getBody());
            response.bytes_received = response.body.size();

            // Copy response headers
            for (const auto& [name, value] : resp->getHeaders()) {
                response.headers[name] = value;
            }
        } else {
            response.success = false;
            switch (result) {
                case drogon::ReqResult::BadResponse:
                    response.error_message = "Bad response from server";
                    break;
                case drogon::ReqResult::NetworkFailure:
                    response.error_message = "Network failure";
                    break;
                case drogon::ReqResult::BadServerAddress:
                    response.error_message = "Bad server address";
                    break;
                case drogon::ReqResult::Timeout:
                    response.error_message = "Request timed out";
                    break;
                default:
                    response.error_message = "Unknown error";
            }
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = std::string("Exception: ") + e.what();

        WTL_CAPTURE_ERROR(
            wtl::core::debug::ErrorCategory::EXTERNAL_API,
            "HTTP request failed: " + std::string(e.what()),
            {{"url", url}, {"method", method}}
        );
    }

    auto end_time = std::chrono::steady_clock::now();
    response.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return response;
}

HttpResponse HttpClient::executeWithRetry(const std::string& method,
                                           const std::string& url,
                                           const std::string& body,
                                           const Headers& headers) {
    // Check rate limiter first
    if (!rate_limiter_name_.empty()) {
        auto& limiter_manager = RateLimiterManager::getInstance();
        if (!limiter_manager.acquire(rate_limiter_name_, 30000)) {  // 30s timeout
            HttpResponse response;
            response.success = false;
            response.status_code = 429;
            response.error_message = "Rate limit exceeded";
            return response;
        }
    }

    HttpResponse response;
    int retry_count = 0;

    while (retry_count <= max_retries_) {
        response = doRequest(method, url, body, headers);
        response.retry_count = retry_count;

        // Success or non-retryable error
        if (response.isOk() || !response.isRetryable()) {
            updateStats(response);
            return response;
        }

        // Check if we should retry
        if (retry_count >= max_retries_) {
            break;
        }

        // Calculate delay with jitter
        int delay = calculateRetryDelay(retry_count);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));

        retry_count++;
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            total_retries_++;
        }
    }

    // All retries exhausted
    updateStats(response);
    return response;
}

HttpClient::Headers HttpClient::mergeHeaders(const Headers& request_headers) const {
    Headers merged = default_headers_;
    for (const auto& [name, value] : request_headers) {
        merged[name] = value;
    }
    return merged;
}

std::string HttpClient::urlEncode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        if (isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2)
                    << static_cast<int>(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

int HttpClient::calculateRetryDelay(int attempt) const {
    // Exponential backoff with jitter
    double delay = static_cast<double>(initial_retry_delay_ms_) *
                   std::pow(retry_backoff_multiplier_, attempt);

    // Cap at max delay
    delay = std::min(delay, static_cast<double>(max_retry_delay_ms_));

    // Add jitter (up to 25% of delay)
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.75, 1.25);
    delay *= dis(gen);

    return static_cast<int>(delay);
}

void HttpClient::updateStats(const HttpResponse& response) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    total_requests_++;
    total_latency_ms_ += response.latency.count();
    total_bytes_received_ += response.bytes_received;

    if (!response.isOk()) {
        failed_requests_++;
    }
}

// ============================================================================
// FREE FUNCTIONS
// ============================================================================

std::string encodeQueryParams(const HttpClient::QueryParams& params) {
    std::ostringstream oss;
    bool first = true;

    for (const auto& [key, value] : params) {
        if (!first) {
            oss << '&';
        }
        first = false;
        oss << HttpClient::urlEncode(key) << '=' << HttpClient::urlEncode(value);
    }

    return oss.str();
}

} // namespace wtl::clients
