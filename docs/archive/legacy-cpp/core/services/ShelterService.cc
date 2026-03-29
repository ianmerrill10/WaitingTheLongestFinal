/**
 * @file ShelterService.cc
 * @brief Implementation of ShelterService
 * @see ShelterService.h for documentation
 *
 * DEPENDENCIES (from other agents):
 * - ConnectionPool (Agent 1) - Database connections
 * - ErrorCapture (Agent 1) - Error logging
 * - SelfHealing (Agent 1) - Retry logic
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/ShelterService.h"

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

ShelterService::ShelterService() {
    // Initialize any required resources
}

ShelterService::~ShelterService() {
    // Cleanup resources
}

ShelterService& ShelterService::getInstance() {
    static ShelterService instance;
    return instance;
}

// ============================================================================
// PRIVATE HELPER METHODS (defined before use)
// ============================================================================

// Generic template definition must come first.
template<typename T>
T ShelterService::executeWithHealing(const std::string& operation_name,
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
void ShelterService::executeWithHealing<void>(const std::string& operation_name,
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
// CRUD OPERATIONS
// ============================================================================

std::optional<models::Shelter> ShelterService::findById(const std::string& id) {
    return executeWithHealing<std::optional<models::Shelter>>(
        "shelter_find_by_id",
        [&]() -> std::optional<models::Shelter> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT * FROM shelters WHERE id = $1",
                    id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                if (result.empty()) {
                    return std::nullopt;
                }

                return models::Shelter::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find shelter by ID: " + std::string(e.what()),
                    {{"shelter_id", id}, {"operation", "findById"}}
                );
                throw;
            }
        }
    );
}

std::vector<models::Shelter> ShelterService::findAll(int limit, int offset) {
    return executeWithHealing<std::vector<models::Shelter>>(
        "shelter_find_all",
        [&]() -> std::vector<models::Shelter> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT * FROM shelters ORDER BY name ASC LIMIT $1 OFFSET $2",
                    limit,
                    offset
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                std::vector<models::Shelter> shelters;
                shelters.reserve(result.size());

                for (const auto& row : result) {
                    shelters.push_back(models::Shelter::fromDbRow(row));
                }

                return shelters;

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find all shelters: " + std::string(e.what()),
                    {{"limit", std::to_string(limit)}, {"offset", std::to_string(offset)}}
                );
                throw;
            }
        }
    );
}

std::vector<models::Shelter> ShelterService::findByStateCode(const std::string& state_code) {
    return executeWithHealing<std::vector<models::Shelter>>(
        "shelter_find_by_state",
        [&]() -> std::vector<models::Shelter> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT * FROM shelters WHERE state_code = $1 ORDER BY name ASC",
                    state_code
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                std::vector<models::Shelter> shelters;
                shelters.reserve(result.size());

                for (const auto& row : result) {
                    shelters.push_back(models::Shelter::fromDbRow(row));
                }

                return shelters;

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find shelters by state: " + std::string(e.what()),
                    {{"state_code", state_code}}
                );
                throw;
            }
        }
    );
}

std::vector<models::Shelter> ShelterService::findKillShelters() {
    return executeWithHealing<std::vector<models::Shelter>>(
        "shelter_find_kill_shelters",
        [&]() -> std::vector<models::Shelter> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Find all active kill shelters, ordered by dog count (highest first)
                // to prioritize shelters with more dogs at risk
                pqxx::result result = txn.exec(
                    R"(
                    SELECT * FROM shelters
                    WHERE is_kill_shelter = TRUE AND is_active = TRUE
                    ORDER BY dog_count DESC, name ASC
                    )"
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                std::vector<models::Shelter> shelters;
                shelters.reserve(result.size());

                for (const auto& row : result) {
                    shelters.push_back(models::Shelter::fromDbRow(row));
                }

                return shelters;

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find kill shelters: " + std::string(e.what()),
                    {}
                );
                throw;
            }
        }
    );
}

models::Shelter ShelterService::save(const models::Shelter& shelter) {
    return executeWithHealing<models::Shelter>(
        "shelter_save",
        [&]() -> models::Shelter {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Generate UUID if not provided
                std::string shelter_id = shelter.id.empty() ?
                    txn.exec("SELECT gen_random_uuid()::text")[0][0].as<std::string>() :
                    shelter.id;

                // Convert timestamps
                auto now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(now);

                std::ostringstream now_ss;
                now_ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%d %H:%M:%S");

                // Convert optional<double> to optional<string> for pqxx parameter binding
                std::optional<std::string> lat_str = shelter.latitude.has_value()
                    ? std::optional<std::string>(std::to_string(*shelter.latitude))
                    : std::nullopt;
                std::optional<std::string> lon_str = shelter.longitude.has_value()
                    ? std::optional<std::string>(std::to_string(*shelter.longitude))
                    : std::nullopt;

                pqxx::result result = txn.exec_params(
                    R"(
                    INSERT INTO shelters (
                        id, name, state_code, city, address, zip_code,
                        latitude, longitude, phone, email, website,
                        shelter_type, is_kill_shelter, avg_hold_days,
                        euthanasia_schedule, accepts_rescue_pulls,
                        dog_count, available_count, is_verified, is_active,
                        created_at, updated_at
                    ) VALUES (
                        $1, $2, $3, $4, $5, $6,
                        CAST($7 AS DOUBLE PRECISION), CAST($8 AS DOUBLE PRECISION),
                        $9, $10, $11,
                        $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22
                    ) RETURNING *
                    )",
                    shelter_id,
                    shelter.name,
                    shelter.state_code,
                    shelter.city,
                    shelter.address,
                    shelter.zip_code,
                    lat_str,
                    lon_str,
                    shelter.phone,
                    shelter.email,
                    shelter.website,
                    shelter.shelter_type,
                    shelter.is_kill_shelter,
                    shelter.avg_hold_days,
                    shelter.euthanasia_schedule,
                    shelter.accepts_rescue_pulls,
                    0,  // dog_count starts at 0
                    0,  // available_count starts at 0
                    shelter.is_verified,
                    shelter.is_active,
                    now_ss.str(),
                    now_ss.str()
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return models::Shelter::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to save shelter: " + std::string(e.what()),
                    {{"shelter_name", shelter.name}, {"state_code", shelter.state_code}}
                );
                throw;
            }
        }
    );
}

models::Shelter ShelterService::update(const models::Shelter& shelter) {
    return executeWithHealing<models::Shelter>(
        "shelter_update",
        [&]() -> models::Shelter {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Current timestamp for updated_at
                auto now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
                std::ostringstream now_ss;
                now_ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%d %H:%M:%S");

                // Convert optional<double> to optional<string> for pqxx parameter binding
                std::optional<std::string> lat_str = shelter.latitude.has_value()
                    ? std::optional<std::string>(std::to_string(*shelter.latitude))
                    : std::nullopt;
                std::optional<std::string> lon_str = shelter.longitude.has_value()
                    ? std::optional<std::string>(std::to_string(*shelter.longitude))
                    : std::nullopt;

                pqxx::result result = txn.exec_params(
                    R"(
                    UPDATE shelters SET
                        name = $2,
                        state_code = $3,
                        city = $4,
                        address = $5,
                        zip_code = $6,
                        latitude = CAST($7 AS DOUBLE PRECISION),
                        longitude = CAST($8 AS DOUBLE PRECISION),
                        phone = $9,
                        email = $10,
                        website = $11,
                        shelter_type = $12,
                        is_kill_shelter = $13,
                        avg_hold_days = $14,
                        euthanasia_schedule = $15,
                        accepts_rescue_pulls = $16,
                        is_verified = $17,
                        is_active = $18,
                        updated_at = $19
                    WHERE id = $1
                    RETURNING *
                    )",
                    shelter.id,
                    shelter.name,
                    shelter.state_code,
                    shelter.city,
                    shelter.address,
                    shelter.zip_code,
                    lat_str,
                    lon_str,
                    shelter.phone,
                    shelter.email,
                    shelter.website,
                    shelter.shelter_type,
                    shelter.is_kill_shelter,
                    shelter.avg_hold_days,
                    shelter.euthanasia_schedule,
                    shelter.accepts_rescue_pulls,
                    shelter.is_verified,
                    shelter.is_active,
                    now_ss.str()
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                if (result.empty()) {
                    throw std::runtime_error("Shelter not found for update: " + shelter.id);
                }

                return models::Shelter::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to update shelter: " + std::string(e.what()),
                    {{"shelter_id", shelter.id}}
                );
                throw;
            }
        }
    );
}

bool ShelterService::deleteById(const std::string& id) {
    return executeWithHealing<bool>(
        "shelter_delete",
        [&]() -> bool {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "DELETE FROM shelters WHERE id = $1 RETURNING id",
                    id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return !result.empty();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to delete shelter: " + std::string(e.what()),
                    {{"shelter_id", id}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// COUNT QUERIES
// ============================================================================

int ShelterService::countAll() {
    return executeWithHealing<int>(
        "shelter_count_all",
        [&]() -> int {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec("SELECT COUNT(*) FROM shelters");

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return result[0][0].as<int>();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to count all shelters: " + std::string(e.what()),
                    {}
                );
                throw;
            }
        }
    );
}

int ShelterService::countByStateCode(const std::string& state_code) {
    return executeWithHealing<int>(
        "shelter_count_by_state",
        [&]() -> int {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT COUNT(*) FROM shelters WHERE state_code = $1",
                    state_code
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return result[0][0].as<int>();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to count shelters by state: " + std::string(e.what()),
                    {{"state_code", state_code}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// DOG COUNT MANAGEMENT
// ============================================================================

void ShelterService::updateDogCount(const std::string& shelter_id) {
    executeWithHealing<void>(
        "shelter_update_dog_count",
        [&]() -> void {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Update dog_count and available_count in a single query
                txn.exec_params(
                    R"(
                    UPDATE shelters SET
                        dog_count = (
                            SELECT COUNT(*) FROM dogs
                            WHERE shelter_id = $1
                        ),
                        available_count = (
                            SELECT COUNT(*) FROM dogs
                            WHERE shelter_id = $1 AND is_available = TRUE
                        ),
                        updated_at = NOW()
                    WHERE id = $1
                    )",
                    shelter_id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to update dog count for shelter: " + std::string(e.what()),
                    {{"shelter_id", shelter_id}}
                );
                throw;
            }
        }
    );
}

} // namespace wtl::core::services
