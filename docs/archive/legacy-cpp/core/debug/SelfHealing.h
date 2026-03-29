/**
 * @file SelfHealing.h
 * @brief Self-healing system with circuit breaker and retry logic
 *
 * PURPOSE:
 * Provides automatic recovery mechanisms for transient failures.
 * Implements circuit breaker pattern to prevent cascade failures
 * and retry logic with exponential backoff.
 *
 * USAGE:
 * // Execute with automatic retry and fallback
 * auto result = SelfHealing::getInstance().executeWithHealing<Dog>(
 *     "fetch_dog",
 *     [&]() { return fetchDogFromDb(id); },
 *     [&]() { return fetchDogFromCache(id); }
 * );
 *
 * // Check circuit breaker state
 * if (SelfHealing::getInstance().isCircuitOpen("database")) {
 *     // Use cached data instead
 * }
 *
 * DEPENDENCIES:
 * - ErrorCapture (for logging healing attempts)
 * - Standard library (chrono, mutex, etc.)
 *
 * MODIFICATION GUIDE:
 * - Add new healing actions to HealingAction enum
 * - Customize retry delays via configuration
 * - Add new circuit breakers for different services
 *
 * @author Agent 1 - WaitingTheLongest Build System
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
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>

// Third-party includes
#include <json/json.h>

namespace wtl::core::debug {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum HealingAction
 * @brief Types of healing actions that can be performed
 */
enum class HealingAction {
    NONE = 0,           ///< No action taken
    RETRY,              ///< Simple retry
    RETRY_WITH_BACKOFF, ///< Retry with exponential backoff
    FALLBACK,           ///< Use fallback operation
    CIRCUIT_BREAK,      ///< Open circuit breaker
    RESET_CONNECTION,   ///< Reset connection pool
    REFRESH_CACHE,      ///< Refresh cached data
    NOTIFY_ADMIN        ///< Notify administrator
};

/**
 * @enum CircuitState
 * @brief States of a circuit breaker
 */
enum class CircuitState {
    CLOSED = 0,   ///< Normal operation, requests pass through
    OPEN,         ///< Failing, requests blocked
    HALF_OPEN     ///< Testing, limited requests allowed
};

// ============================================================================
// RESULT STRUCTURES
// ============================================================================

/**
 * @struct HealingResult
 * @brief Result of a healing operation
 */
struct HealingResult {
    bool success;                    ///< Whether the operation succeeded
    HealingAction action_taken;      ///< What healing action was used
    int attempts;                    ///< Number of attempts made
    std::chrono::milliseconds total_time;  ///< Total time spent
    std::string error_message;       ///< Error message if failed
    bool used_fallback;              ///< Whether fallback was used

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;
};

/**
 * @struct CircuitBreakerState
 * @brief State information for a circuit breaker
 */
struct CircuitBreakerState {
    std::string name;                ///< Circuit breaker name/identifier
    CircuitState state;              ///< Current state
    int failure_count;               ///< Number of recent failures
    int success_count;               ///< Number of recent successes (half-open)
    std::chrono::system_clock::time_point last_failure;  ///< Time of last failure
    std::chrono::system_clock::time_point state_changed; ///< Time state last changed
    std::chrono::system_clock::time_point next_attempt;  ///< When to try again (if open)

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const;
};

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * @struct RetryConfig
 * @brief Configuration for retry behavior
 */
struct RetryConfig {
    int max_attempts = 3;                       ///< Maximum retry attempts
    std::chrono::milliseconds base_delay{100};  ///< Initial delay between retries
    std::chrono::milliseconds max_delay{5000};  ///< Maximum delay between retries
    double multiplier = 2.0;                    ///< Backoff multiplier
    bool add_jitter = true;                     ///< Add random jitter to delays
};

/**
 * @struct CircuitBreakerConfig
 * @brief Configuration for circuit breaker
 */
struct CircuitBreakerConfig {
    int failure_threshold = 5;                  ///< Failures before opening
    int success_threshold = 2;                  ///< Successes to close from half-open
    std::chrono::seconds timeout{60};           ///< Time before half-open retry
};

// ============================================================================
// SELF HEALING CLASS
// ============================================================================

/**
 * @class SelfHealing
 * @brief Singleton class for self-healing operations
 *
 * Provides circuit breaker pattern implementation and retry logic
 * with exponential backoff for resilient operation execution.
 */
class SelfHealing {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the SelfHealing singleton
     */
    static SelfHealing& getInstance();

    // Delete copy/move constructors
    SelfHealing(const SelfHealing&) = delete;
    SelfHealing& operator=(const SelfHealing&) = delete;
    SelfHealing(SelfHealing&&) = delete;
    SelfHealing& operator=(SelfHealing&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the self-healing system
     *
     * @param retry_config Default retry configuration
     * @param circuit_config Default circuit breaker configuration
     */
    void initialize(const RetryConfig& retry_config = {},
                    const CircuitBreakerConfig& circuit_config = {});

    /**
     * @brief Initialize from Config singleton
     */
    void initializeFromConfig();

    // =========================================================================
    // EXECUTE WITH HEALING
    // =========================================================================

    /**
     * @brief Execute an operation with automatic healing
     *
     * @tparam T Return type of the operation
     * @param operation_name Name for tracking and circuit breaker
     * @param operation The primary operation to execute
     * @param fallback Optional fallback operation
     * @param config Optional custom retry config
     * @return HealingResult with success status and the result value
     *
     * Attempts the operation with retries. If all retries fail and a
     * fallback is provided, the fallback is attempted. Updates circuit
     * breaker state based on results.
     *
     * @example
     * auto result = SelfHealing::getInstance().executeWithHealing<Dog>(
     *     "fetch_dog",
     *     [&]() { return fetchDogFromDb(id); },
     *     [&]() { return fetchDogFromCache(id); }
     * );
     * if (result.success) {
     *     Dog dog = std::get<Dog>(result.value);
     * }
     */
    template<typename T>
    struct HealingResultWithValue {
        HealingResult result;
        std::optional<T> value;
    };

    template<typename T>
    HealingResultWithValue<T> executeWithHealing(
        const std::string& operation_name,
        std::function<T()> operation,
        std::function<T()> fallback = nullptr,
        const RetryConfig* config = nullptr);

    /**
     * @brief Execute operation with healing, returning T directly (throws on failure)
     *
     * Convenience wrapper around executeWithHealing that unwraps
     * the HealingResultWithValue and returns T directly.
     * Throws std::runtime_error if the operation fails.
     */
    template<typename T>
    T executeOrThrow(
        const std::string& operation_name,
        std::function<T()> operation,
        std::function<T()> fallback = nullptr,
        const RetryConfig* config = nullptr);

    /**
     * @brief Execute a void operation with healing
     *
     * @param operation_name Name for tracking
     * @param operation The operation to execute
     * @param fallback Optional fallback
     * @param config Optional custom retry config
     * @return HealingResult
     */
    HealingResult executeWithHealingVoid(
        const std::string& operation_name,
        std::function<void()> operation,
        std::function<void()> fallback = nullptr,
        const RetryConfig* config = nullptr);

    // =========================================================================
    // CIRCUIT BREAKER
    // =========================================================================

    /**
     * @brief Check if a circuit is open (blocking requests)
     *
     * @param circuit_name Name of the circuit
     * @return true if circuit is open, false otherwise
     */
    bool isCircuitOpen(const std::string& circuit_name) const;

    /**
     * @brief Get circuit breaker state
     *
     * @param circuit_name Name of the circuit
     * @return CircuitBreakerState
     */
    CircuitBreakerState getCircuitState(const std::string& circuit_name) const;

    /**
     * @brief Get all circuit breaker states
     *
     * @return Map of circuit names to states
     */
    std::unordered_map<std::string, CircuitBreakerState> getAllCircuitStates() const;

    /**
     * @brief Manually open a circuit
     *
     * @param circuit_name Name of the circuit
     */
    void openCircuit(const std::string& circuit_name);

    /**
     * @brief Manually close a circuit
     *
     * @param circuit_name Name of the circuit
     */
    void closeCircuit(const std::string& circuit_name);

    /**
     * @brief Reset a circuit to initial state
     *
     * @param circuit_name Name of the circuit
     */
    void resetCircuit(const std::string& circuit_name);

    /**
     * @brief Reset all circuits
     */
    void resetAllCircuits();

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get healing statistics
     *
     * @return JSON with statistics
     */
    Json::Value getStats() const;

    /**
     * @brief Get statistics for a specific operation
     *
     * @param operation_name Name of the operation
     * @return JSON with operation statistics
     */
    Json::Value getOperationStats(const std::string& operation_name) const;

    /**
     * @brief Check if self-healing is enabled
     *
     * @return true if initialized and active
     */
    bool isEnabled() const;

    /**
     * @brief Get count of currently active healing operations
     *
     * @return Number of active healings
     */
    int getActiveHealingCount() const;

    /**
     * @brief Get total count of healing operations performed
     *
     * @return Total number of healings
     */
    int getTotalHealingCount() const;

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Set default retry configuration
     *
     * @param config The retry configuration
     */
    void setRetryConfig(const RetryConfig& config);

    /**
     * @brief Set default circuit breaker configuration
     *
     * @param config The circuit breaker configuration
     */
    void setCircuitBreakerConfig(const CircuitBreakerConfig& config);

    /**
     * @brief Set circuit breaker config for specific circuit
     *
     * @param circuit_name Name of the circuit
     * @param config The configuration
     */
    void setCircuitConfig(const std::string& circuit_name,
                          const CircuitBreakerConfig& config);

    // =========================================================================
    // UTILITY
    // =========================================================================

    /**
     * @brief Convert HealingAction to string
     * @param action The action
     * @return String representation
     */
    static std::string actionToString(HealingAction action);

    /**
     * @brief Convert CircuitState to string
     * @param state The state
     * @return String representation
     */
    static std::string stateToString(CircuitState state);

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    SelfHealing() = default;
    ~SelfHealing() = default;

    /**
     * @brief Record a successful operation
     * @param circuit_name Name of the circuit
     */
    void recordSuccess(const std::string& circuit_name);

    /**
     * @brief Record a failed operation
     * @param circuit_name Name of the circuit
     */
    void recordFailure(const std::string& circuit_name);

    /**
     * @brief Check and potentially transition circuit state
     * @param circuit_name Name of the circuit
     * @return true if request should be allowed
     */
    bool shouldAllowRequest(const std::string& circuit_name);

    /**
     * @brief Get or create circuit breaker state
     * @param circuit_name Name of the circuit
     * @return Reference to the circuit state
     */
    CircuitBreakerState& getOrCreateCircuit(const std::string& circuit_name);

    /**
     * @brief Calculate delay with exponential backoff
     * @param attempt Current attempt number (0-based)
     * @param config Retry configuration
     * @return Delay duration
     */
    std::chrono::milliseconds calculateDelay(int attempt,
                                              const RetryConfig& config) const;

    // Circuit breaker states
    std::unordered_map<std::string, CircuitBreakerState> circuits_;

    // Per-circuit configurations
    std::unordered_map<std::string, CircuitBreakerConfig> circuit_configs_;

    // Default configurations
    RetryConfig default_retry_config_;
    CircuitBreakerConfig default_circuit_config_;

    // Statistics
    struct OperationStats {
        std::atomic<uint64_t> total_calls{0};
        std::atomic<uint64_t> successes{0};
        std::atomic<uint64_t> failures{0};
        std::atomic<uint64_t> retries{0};
        std::atomic<uint64_t> fallbacks_used{0};
        std::atomic<uint64_t> circuit_breaks{0};
    };
    std::unordered_map<std::string, OperationStats> operation_stats_;

    // Thread safety
    mutable std::shared_mutex mutex_;
    mutable std::mutex stats_mutex_;

    // Initialized flag
    std::atomic<bool> initialized_{false};
};

// Specialization for void - no optional<void> (invalid in C++17)
// Must be at namespace scope, not inside the class definition.
template<>
struct SelfHealing::HealingResultWithValue<void> {
    HealingResult result;
    bool completed = false;  ///< Whether the void operation completed
};

// ============================================================================
// TEMPLATE IMPLEMENTATION
// ============================================================================

template<typename T>
SelfHealing::HealingResultWithValue<T> SelfHealing::executeWithHealing(
    const std::string& operation_name,
    std::function<T()> operation,
    std::function<T()> fallback,
    const RetryConfig* config) {

    HealingResultWithValue<T> result;
    result.result.success = false;
    result.result.action_taken = HealingAction::NONE;
    result.result.attempts = 0;
    result.result.used_fallback = false;

    auto start_time = std::chrono::steady_clock::now();

    // Get configuration
    const RetryConfig& retry_config = config ? *config : default_retry_config_;

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        operation_stats_[operation_name].total_calls++;
    }

    // Check circuit breaker
    if (!shouldAllowRequest(operation_name)) {
        result.result.action_taken = HealingAction::CIRCUIT_BREAK;
        result.result.error_message = "Circuit breaker is open";

        // Try fallback if available
        if (fallback) {
            try {
                result.value = fallback();
                result.result.success = true;
                result.result.used_fallback = true;
                result.result.action_taken = HealingAction::FALLBACK;

                std::lock_guard<std::mutex> lock(stats_mutex_);
                operation_stats_[operation_name].fallbacks_used++;
            } catch (const std::exception& e) {
                result.result.error_message = e.what();
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        result.result.total_time = std::chrono::duration_cast<
            std::chrono::milliseconds>(end_time - start_time);

        return result;
    }

    // Attempt with retries
    std::string last_error;
    for (int attempt = 0; attempt < retry_config.max_attempts; ++attempt) {
        result.result.attempts++;

        try {
            result.value = operation();
            result.result.success = true;
            result.result.action_taken = attempt > 0 ?
                HealingAction::RETRY_WITH_BACKOFF : HealingAction::NONE;

            // Record success for circuit breaker
            recordSuccess(operation_name);

            std::lock_guard<std::mutex> lock(stats_mutex_);
            operation_stats_[operation_name].successes++;
            if (attempt > 0) {
                operation_stats_[operation_name].retries += attempt;
            }

            break;
        } catch (const std::exception& e) {
            last_error = e.what();

            // Wait before retry (except for last attempt)
            if (attempt < retry_config.max_attempts - 1) {
                auto delay = calculateDelay(attempt, retry_config);
                std::this_thread::sleep_for(delay);
            }
        }
    }

    // If all retries failed
    if (!result.result.success) {
        recordFailure(operation_name);

        std::lock_guard<std::mutex> lock(stats_mutex_);
        operation_stats_[operation_name].failures++;
        operation_stats_[operation_name].retries +=
            (result.result.attempts - 1);

        // Try fallback
        if (fallback) {
            try {
                result.value = fallback();
                result.result.success = true;
                result.result.used_fallback = true;
                result.result.action_taken = HealingAction::FALLBACK;

                operation_stats_[operation_name].fallbacks_used++;
            } catch (const std::exception& e) {
                result.result.error_message = last_error + " (fallback: " +
                    e.what() + ")";
            }
        } else {
            result.result.error_message = last_error;
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    result.result.total_time = std::chrono::duration_cast<
        std::chrono::milliseconds>(end_time - start_time);

    return result;
}

template<typename T>
T SelfHealing::executeOrThrow(
    const std::string& operation_name,
    std::function<T()> operation,
    std::function<T()> fallback,
    const RetryConfig* config) {

    auto healing_result = executeWithHealing<T>(
        operation_name, std::move(operation), std::move(fallback), config);
    if (healing_result.value) {
        return std::move(*healing_result.value);
    }
    throw std::runtime_error(
        healing_result.result.error_message.empty()
            ? "Operation '" + operation_name + "' failed"
            : healing_result.result.error_message);
}

} // namespace wtl::core::debug
