/**
 * @file SelfHealing.cc
 * @brief Implementation of the self-healing system
 * @see SelfHealing.h for documentation
 */

#include "core/debug/SelfHealing.h"
#include "core/debug/ErrorCapture.h"
#include "core/utils/Config.h"

// Standard library includes
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace wtl::core::debug {

// ============================================================================
// RESULT STRUCTURES IMPLEMENTATION
// ============================================================================

Json::Value HealingResult::toJson() const {
    Json::Value json;

    json["success"] = success;
    json["action_taken"] = SelfHealing::actionToString(action_taken);
    json["attempts"] = attempts;
    json["total_time_ms"] = static_cast<Json::Int64>(total_time.count());
    json["error_message"] = error_message;
    json["used_fallback"] = used_fallback;

    return json;
}

Json::Value CircuitBreakerState::toJson() const {
    Json::Value json;

    json["name"] = name;
    json["state"] = SelfHealing::stateToString(state);
    json["failure_count"] = failure_count;
    json["success_count"] = success_count;

    // Format timestamps as ISO 8601
    auto format_time = [](std::chrono::system_clock::time_point tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_buf;
#ifdef _WIN32
        gmtime_s(&tm_buf, &time_t);
#else
        gmtime_r(&time_t, &tm_buf);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    };

    json["last_failure"] = format_time(last_failure);
    json["state_changed"] = format_time(state_changed);
    json["next_attempt"] = format_time(next_attempt);

    return json;
}

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

SelfHealing& SelfHealing::getInstance() {
    static SelfHealing instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void SelfHealing::initialize(const RetryConfig& retry_config,
                              const CircuitBreakerConfig& circuit_config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    default_retry_config_ = retry_config;
    default_circuit_config_ = circuit_config;
    initialized_.store(true);

    std::cout << "[SelfHealing] Initialized with max_attempts="
              << retry_config.max_attempts
              << ", circuit_threshold=" << circuit_config.failure_threshold
              << std::endl;
}

void SelfHealing::initializeFromConfig() {
    auto& config = wtl::core::utils::Config::getInstance();

    RetryConfig retry_config;
    retry_config.max_attempts = config.getInt(
        "self_healing.retry_max_attempts", 3);
    retry_config.base_delay = std::chrono::milliseconds(
        config.getInt("self_healing.retry_base_delay_ms", 100));
    retry_config.max_delay = std::chrono::milliseconds(
        config.getInt("self_healing.retry_max_delay_ms", 5000));
    retry_config.multiplier = config.getDouble(
        "self_healing.retry_multiplier", 2.0);
    retry_config.add_jitter = true;

    CircuitBreakerConfig circuit_config;
    circuit_config.failure_threshold = config.getInt(
        "self_healing.circuit_breaker_threshold", 5);
    circuit_config.timeout = std::chrono::seconds(
        config.getInt("self_healing.circuit_breaker_timeout_seconds", 60));
    circuit_config.success_threshold = 2;

    initialize(retry_config, circuit_config);
}

// ============================================================================
// EXECUTE WITH HEALING (VOID VERSION)
// ============================================================================

HealingResult SelfHealing::executeWithHealingVoid(
    const std::string& operation_name,
    std::function<void()> operation,
    std::function<void()> fallback,
    const RetryConfig* config) {

    // Wrap void functions to return a dummy value
    auto wrapped_operation = [&operation]() -> int {
        operation();
        return 0;
    };

    std::function<int()> wrapped_fallback = nullptr;
    if (fallback) {
        wrapped_fallback = [&fallback]() -> int {
            fallback();
            return 0;
        };
    }

    auto result = executeWithHealing<int>(operation_name, wrapped_operation,
                                           wrapped_fallback, config);
    return result.result;
}

// ============================================================================
// CIRCUIT BREAKER
// ============================================================================

bool SelfHealing::isCircuitOpen(const std::string& circuit_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = circuits_.find(circuit_name);
    if (it == circuits_.end()) {
        return false;
    }

    return it->second.state == CircuitState::OPEN;
}

CircuitBreakerState SelfHealing::getCircuitState(
    const std::string& circuit_name) const {

    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = circuits_.find(circuit_name);
    if (it == circuits_.end()) {
        CircuitBreakerState default_state;
        default_state.name = circuit_name;
        default_state.state = CircuitState::CLOSED;
        default_state.failure_count = 0;
        default_state.success_count = 0;
        return default_state;
    }

    return it->second;
}

std::unordered_map<std::string, CircuitBreakerState>
SelfHealing::getAllCircuitStates() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return circuits_;
}

void SelfHealing::openCircuit(const std::string& circuit_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto& circuit = getOrCreateCircuit(circuit_name);
    circuit.state = CircuitState::OPEN;
    circuit.state_changed = std::chrono::system_clock::now();

    // Get timeout from config or use default
    auto it = circuit_configs_.find(circuit_name);
    auto timeout = it != circuit_configs_.end() ?
        it->second.timeout : default_circuit_config_.timeout;

    circuit.next_attempt = circuit.state_changed + timeout;

    std::cout << "[SelfHealing] Circuit '" << circuit_name
              << "' manually opened" << std::endl;
}

void SelfHealing::closeCircuit(const std::string& circuit_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto& circuit = getOrCreateCircuit(circuit_name);
    circuit.state = CircuitState::CLOSED;
    circuit.state_changed = std::chrono::system_clock::now();
    circuit.failure_count = 0;
    circuit.success_count = 0;

    std::cout << "[SelfHealing] Circuit '" << circuit_name
              << "' manually closed" << std::endl;
}

void SelfHealing::resetCircuit(const std::string& circuit_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = circuits_.find(circuit_name);
    if (it != circuits_.end()) {
        it->second.state = CircuitState::CLOSED;
        it->second.failure_count = 0;
        it->second.success_count = 0;
        it->second.state_changed = std::chrono::system_clock::now();
    }
}

void SelfHealing::resetAllCircuits() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (auto& [name, circuit] : circuits_) {
        circuit.state = CircuitState::CLOSED;
        circuit.failure_count = 0;
        circuit.success_count = 0;
        circuit.state_changed = std::chrono::system_clock::now();
    }

    std::cout << "[SelfHealing] All circuits reset" << std::endl;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value SelfHealing::getStats() const {
    Json::Value stats;

    // Circuit breaker stats
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        Json::Value circuits(Json::objectValue);
        for (const auto& [name, circuit] : circuits_) {
            circuits[name] = circuit.toJson();
        }
        stats["circuits"] = circuits;
    }

    // Operation stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);

        Json::Value operations(Json::objectValue);
        for (const auto& [name, op_stats] : operation_stats_) {
            Json::Value op;
            op["total_calls"] = static_cast<Json::UInt64>(
                op_stats.total_calls.load());
            op["successes"] = static_cast<Json::UInt64>(
                op_stats.successes.load());
            op["failures"] = static_cast<Json::UInt64>(
                op_stats.failures.load());
            op["retries"] = static_cast<Json::UInt64>(
                op_stats.retries.load());
            op["fallbacks_used"] = static_cast<Json::UInt64>(
                op_stats.fallbacks_used.load());
            op["circuit_breaks"] = static_cast<Json::UInt64>(
                op_stats.circuit_breaks.load());

            // Calculate success rate
            uint64_t total = op_stats.total_calls.load();
            if (total > 0) {
                op["success_rate"] = static_cast<double>(
                    op_stats.successes.load()) / static_cast<double>(total);
            } else {
                op["success_rate"] = 0.0;
            }

            operations[name] = op;
        }
        stats["operations"] = operations;
    }

    return stats;
}

Json::Value SelfHealing::getOperationStats(const std::string& operation_name) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto it = operation_stats_.find(operation_name);
    if (it == operation_stats_.end()) {
        return Json::Value::null;
    }

    const auto& op_stats = it->second;

    Json::Value op;
    op["total_calls"] = static_cast<Json::UInt64>(op_stats.total_calls.load());
    op["successes"] = static_cast<Json::UInt64>(op_stats.successes.load());
    op["failures"] = static_cast<Json::UInt64>(op_stats.failures.load());
    op["retries"] = static_cast<Json::UInt64>(op_stats.retries.load());
    op["fallbacks_used"] = static_cast<Json::UInt64>(
        op_stats.fallbacks_used.load());
    op["circuit_breaks"] = static_cast<Json::UInt64>(
        op_stats.circuit_breaks.load());

    return op;
}

bool SelfHealing::isEnabled() const {
    return initialized_.load();
}

int SelfHealing::getActiveHealingCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    int active = 0;
    for (const auto& [name, circuit] : circuits_) {
        if (circuit.state == CircuitState::OPEN ||
            circuit.state == CircuitState::HALF_OPEN) {
            ++active;
        }
    }
    return active;
}

int SelfHealing::getTotalHealingCount() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    int total = 0;
    for (const auto& [name, op_stats] : operation_stats_) {
        total += static_cast<int>(op_stats.retries.load() +
                                  op_stats.fallbacks_used.load() +
                                  op_stats.circuit_breaks.load());
    }
    return total;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void SelfHealing::setRetryConfig(const RetryConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    default_retry_config_ = config;
}

void SelfHealing::setCircuitBreakerConfig(const CircuitBreakerConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    default_circuit_config_ = config;
}

void SelfHealing::setCircuitConfig(const std::string& circuit_name,
                                    const CircuitBreakerConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    circuit_configs_[circuit_name] = config;
}

// ============================================================================
// UTILITY
// ============================================================================

std::string SelfHealing::actionToString(HealingAction action) {
    switch (action) {
        case HealingAction::NONE: return "NONE";
        case HealingAction::RETRY: return "RETRY";
        case HealingAction::RETRY_WITH_BACKOFF: return "RETRY_WITH_BACKOFF";
        case HealingAction::FALLBACK: return "FALLBACK";
        case HealingAction::CIRCUIT_BREAK: return "CIRCUIT_BREAK";
        case HealingAction::RESET_CONNECTION: return "RESET_CONNECTION";
        case HealingAction::REFRESH_CACHE: return "REFRESH_CACHE";
        case HealingAction::NOTIFY_ADMIN: return "NOTIFY_ADMIN";
        default: return "UNKNOWN";
    }
}

std::string SelfHealing::stateToString(CircuitState state) {
    switch (state) {
        case CircuitState::CLOSED: return "CLOSED";
        case CircuitState::OPEN: return "OPEN";
        case CircuitState::HALF_OPEN: return "HALF_OPEN";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void SelfHealing::recordSuccess(const std::string& circuit_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto& circuit = getOrCreateCircuit(circuit_name);

    if (circuit.state == CircuitState::HALF_OPEN) {
        circuit.success_count++;

        // Check if we should close the circuit
        auto it = circuit_configs_.find(circuit_name);
        int threshold = it != circuit_configs_.end() ?
            it->second.success_threshold : default_circuit_config_.success_threshold;

        if (circuit.success_count >= threshold) {
            circuit.state = CircuitState::CLOSED;
            circuit.state_changed = std::chrono::system_clock::now();
            circuit.failure_count = 0;
            circuit.success_count = 0;

            std::cout << "[SelfHealing] Circuit '" << circuit_name
                      << "' closed after " << threshold << " successes"
                      << std::endl;
        }
    } else if (circuit.state == CircuitState::CLOSED) {
        // Reset failure count on success
        circuit.failure_count = 0;
    }
}

void SelfHealing::recordFailure(const std::string& circuit_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto& circuit = getOrCreateCircuit(circuit_name);
    circuit.failure_count++;
    circuit.last_failure = std::chrono::system_clock::now();

    // Get configuration
    auto it = circuit_configs_.find(circuit_name);
    const auto& config = it != circuit_configs_.end() ?
        it->second : default_circuit_config_;

    if (circuit.state == CircuitState::HALF_OPEN) {
        // Any failure in half-open reopens the circuit
        circuit.state = CircuitState::OPEN;
        circuit.state_changed = std::chrono::system_clock::now();
        circuit.next_attempt = circuit.state_changed + config.timeout;
        circuit.success_count = 0;

        std::cout << "[SelfHealing] Circuit '" << circuit_name
                  << "' re-opened after failure in half-open state"
                  << std::endl;

        WTL_CAPTURE_WARNING(ErrorCategory::BUSINESS_LOGIC,
            "Circuit breaker re-opened",
            {{"circuit", circuit_name}});
    }
    else if (circuit.state == CircuitState::CLOSED &&
             circuit.failure_count >= config.failure_threshold) {
        // Too many failures, open the circuit
        circuit.state = CircuitState::OPEN;
        circuit.state_changed = std::chrono::system_clock::now();
        circuit.next_attempt = circuit.state_changed + config.timeout;

        std::cout << "[SelfHealing] Circuit '" << circuit_name
                  << "' opened after " << circuit.failure_count << " failures"
                  << std::endl;

        WTL_CAPTURE_WARNING(ErrorCategory::BUSINESS_LOGIC,
            "Circuit breaker opened",
            {{"circuit", circuit_name},
             {"failure_count", std::to_string(circuit.failure_count)}});

        // Update stats
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        operation_stats_[circuit_name].circuit_breaks++;
    }
}

bool SelfHealing::shouldAllowRequest(const std::string& circuit_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto& circuit = getOrCreateCircuit(circuit_name);
    auto now = std::chrono::system_clock::now();

    switch (circuit.state) {
        case CircuitState::CLOSED:
            return true;

        case CircuitState::OPEN:
            // Check if timeout has passed
            if (now >= circuit.next_attempt) {
                // Transition to half-open
                circuit.state = CircuitState::HALF_OPEN;
                circuit.state_changed = now;
                circuit.success_count = 0;

                std::cout << "[SelfHealing] Circuit '" << circuit_name
                          << "' transitioned to half-open" << std::endl;

                return true;  // Allow one test request
            }
            return false;

        case CircuitState::HALF_OPEN:
            // In half-open state, allow limited requests
            // This simple implementation allows all requests in half-open
            // A more sophisticated version could limit concurrent requests
            return true;

        default:
            return true;
    }
}

CircuitBreakerState& SelfHealing::getOrCreateCircuit(
    const std::string& circuit_name) {
    // Caller must hold write lock

    auto it = circuits_.find(circuit_name);
    if (it == circuits_.end()) {
        CircuitBreakerState state;
        state.name = circuit_name;
        state.state = CircuitState::CLOSED;
        state.failure_count = 0;
        state.success_count = 0;
        state.state_changed = std::chrono::system_clock::now();

        circuits_[circuit_name] = state;
        return circuits_[circuit_name];
    }

    return it->second;
}

std::chrono::milliseconds SelfHealing::calculateDelay(
    int attempt, const RetryConfig& config) const {

    // Exponential backoff: base_delay * multiplier^attempt
    double delay_ms = static_cast<double>(config.base_delay.count()) *
                      std::pow(config.multiplier, attempt);

    // Cap at max delay
    delay_ms = std::min(delay_ms,
                        static_cast<double>(config.max_delay.count()));

    // Add jitter if enabled (up to 25% random variation)
    if (config.add_jitter) {
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<> dis(0.75, 1.25);
        delay_ms *= dis(gen);
    }

    return std::chrono::milliseconds(static_cast<int64_t>(delay_ms));
}

} // namespace wtl::core::debug
