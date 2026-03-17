/**
 * @file StateService.cc
 * @brief Implementation of StateService
 * @see StateService.h for documentation
 *
 * DEPENDENCIES (from other agents):
 * - ConnectionPool (Agent 1) - Database connections
 * - ErrorCapture (Agent 1) - Error logging
 * - SelfHealing (Agent 1) - Retry logic
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/StateService.h"

#include <iomanip>
#include <sstream>

// Dependencies from Agent 1
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

namespace wtl::core::services {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

StateService::StateService() {
    // Initialize any required resources
}

StateService::~StateService() {
    // Cleanup resources
}

StateService& StateService::getInstance() {
    static StateService instance;
    return instance;
}

// ============================================================================
// PRIVATE HELPER METHODS (defined before use)
// ============================================================================

// Generic template definition must come first.
template<typename T>
T StateService::executeWithHealing(const std::string& operation_name,
                                    std::function<T()> operation) {
    auto healing_result = ::wtl::core::debug::SelfHealing::getInstance().executeWithHealing<T>(
        operation_name,
        operation,
        [&]() -> T {
            throw std::runtime_error("Operation failed and no fallback available: " + operation_name);
        }
    );
    if (healing_result.value) {
        return std::move(*healing_result.value);
    }
    throw std::runtime_error("Operation '" + operation_name + "' failed");
}

// Explicit template specialization for void must come after the generic template
// but before any code that calls executeWithHealing<void>.
template<>
void StateService::executeWithHealing<void>(const std::string& operation_name,
                                             std::function<void()> operation) {
    auto healing_result = ::wtl::core::debug::SelfHealing::getInstance().executeWithHealing<bool>(
        operation_name,
        [&]() -> bool {
            operation();
            return true;
        },
        [&]() -> bool {
            throw std::runtime_error("Operation failed and no fallback available: " + operation_name);
        }
    );
}

// ============================================================================
// QUERY OPERATIONS
// ============================================================================

std::optional<models::State> StateService::findByCode(const std::string& code) {
    return executeWithHealing<std::optional<models::State>>(
        "state_find_by_code",
        [&]() -> std::optional<models::State> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT * FROM states WHERE code = $1",
                    code
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                if (result.empty()) {
                    return std::nullopt;
                }

                return models::State::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find state by code: " + std::string(e.what()),
                    {{"state_code", code}, {"operation", "findByCode"}}
                );
                throw;
            }
        }
    );
}

std::vector<models::State> StateService::findAll() {
    return executeWithHealing<std::vector<models::State>>(
        "state_find_all",
        [&]() -> std::vector<models::State> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec(
                    "SELECT * FROM states ORDER BY name ASC"
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                std::vector<models::State> states;
                states.reserve(result.size());

                for (const auto& row : result) {
                    states.push_back(models::State::fromDbRow(row));
                }

                return states;

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find all states: " + std::string(e.what()),
                    {}
                );
                throw;
            }
        }
    );
}

std::vector<models::State> StateService::findActive() {
    return executeWithHealing<std::vector<models::State>>(
        "state_find_active",
        [&]() -> std::vector<models::State> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec(
                    "SELECT * FROM states WHERE is_active = TRUE ORDER BY name ASC"
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                std::vector<models::State> states;
                states.reserve(result.size());

                for (const auto& row : result) {
                    states.push_back(models::State::fromDbRow(row));
                }

                return states;

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find active states: " + std::string(e.what()),
                    {}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// SAVE OPERATIONS
// ============================================================================

models::State StateService::save(const models::State& state) {
    return executeWithHealing<models::State>(
        "state_save",
        [&]() -> models::State {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Convert timestamps
                auto now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(now);

                std::ostringstream now_ss;
                now_ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%d %H:%M:%S");

                pqxx::result result = txn.exec_params(
                    R"(
                    INSERT INTO states (
                        code, name, region, is_active,
                        dog_count, shelter_count, created_at, updated_at
                    ) VALUES (
                        $1, $2, $3, $4, $5, $6, $7, $8
                    ) RETURNING *
                    )",
                    state.code,
                    state.name,
                    state.region,
                    state.is_active,
                    0,  // dog_count starts at 0
                    0,  // shelter_count starts at 0
                    now_ss.str(),
                    now_ss.str()
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return models::State::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to save state: " + std::string(e.what()),
                    {{"state_code", state.code}, {"state_name", state.name}}
                );
                throw;
            }
        }
    );
}

models::State StateService::update(const models::State& state) {
    return executeWithHealing<models::State>(
        "state_update",
        [&]() -> models::State {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Current timestamp for updated_at
                auto now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
                std::ostringstream now_ss;
                now_ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%d %H:%M:%S");

                pqxx::result result = txn.exec_params(
                    R"(
                    UPDATE states SET
                        name = $2,
                        region = $3,
                        is_active = $4,
                        updated_at = $5
                    WHERE code = $1
                    RETURNING *
                    )",
                    state.code,
                    state.name,
                    state.region,
                    state.is_active,
                    now_ss.str()
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                if (result.empty()) {
                    throw std::runtime_error("State not found for update: " + state.code);
                }

                return models::State::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to update state: " + std::string(e.what()),
                    {{"state_code", state.code}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// COUNT MANAGEMENT
// ============================================================================

void StateService::updateCounts(const std::string& code) {
    executeWithHealing<void>(
        "state_update_counts",
        [&]() -> void {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Update shelter_count and dog_count in a single query
                txn.exec_params(
                    R"(
                    UPDATE states SET
                        shelter_count = (
                            SELECT COUNT(*) FROM shelters
                            WHERE state_code = $1
                        ),
                        dog_count = (
                            SELECT COUNT(*) FROM dogs d
                            INNER JOIN shelters s ON d.shelter_id = s.id
                            WHERE s.state_code = $1
                        ),
                        updated_at = NOW()
                    WHERE code = $1
                    )",
                    code
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to update counts for state: " + std::string(e.what()),
                    {{"state_code", code}}
                );
                throw;
            }
        }
    );
}

void StateService::updateAllCounts() {
    executeWithHealing<void>(
        "state_update_all_counts",
        [&]() -> void {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Get all state codes first
                pqxx::result states = txn.exec("SELECT code FROM states");

                // Update each state's counts
                for (const auto& row : states) {
                    std::string code = row["code"].as<std::string>();

                    txn.exec_params(
                        R"(
                        UPDATE states SET
                            shelter_count = (
                                SELECT COUNT(*) FROM shelters
                                WHERE state_code = $1
                            ),
                            dog_count = (
                                SELECT COUNT(*) FROM dogs d
                                INNER JOIN shelters s ON d.shelter_id = s.id
                                WHERE s.state_code = $1
                            ),
                            updated_at = NOW()
                        WHERE code = $1
                        )",
                        code
                    );
                }

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to update all state counts: " + std::string(e.what()),
                    {}
                );
                throw;
            }
        }
    );
}

} // namespace wtl::core::services
