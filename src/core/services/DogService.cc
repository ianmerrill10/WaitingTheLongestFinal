/**
 * @file DogService.cc
 * @brief Implementation of DogService
 * @see DogService.h for documentation
 *
 * DEPENDENCIES (from other agents):
 * - ConnectionPool (Agent 1) - Database connections
 * - ErrorCapture (Agent 1) - Error logging
 * - SelfHealing (Agent 1) - Retry logic
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/DogService.h"

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

DogService::DogService() {
    // Initialize any required resources
}

DogService::~DogService() {
    // Cleanup resources
}

DogService& DogService::getInstance() {
    // Meyer's singleton - thread-safe in C++11 and later
    static DogService instance;
    return instance;
}

// ============================================================================
// CRUD OPERATIONS
// ============================================================================

std::optional<models::Dog> DogService::findById(const std::string& id) {
    return executeWithHealing<std::optional<models::Dog>>(
        "dog_find_by_id",
        [&]() -> std::optional<models::Dog> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT * FROM dogs WHERE id = $1",
                    id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                if (result.empty()) {
                    return std::nullopt;
                }

                return models::Dog::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find dog by ID: " + std::string(e.what()),
                    {{"dog_id", id}, {"operation", "findById"}}
                );
                throw;
            }
        }
    );
}

std::vector<models::Dog> DogService::findAll(int limit, int offset) {
    return executeWithHealing<std::vector<models::Dog>>(
        "dog_find_all",
        [&]() -> std::vector<models::Dog> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT * FROM dogs ORDER BY intake_date ASC LIMIT $1 OFFSET $2",
                    limit,
                    offset
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                std::vector<models::Dog> dogs;
                dogs.reserve(result.size());

                for (const auto& row : result) {
                    dogs.push_back(models::Dog::fromDbRow(row));
                }

                return dogs;

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find all dogs: " + std::string(e.what()),
                    {{"limit", std::to_string(limit)}, {"offset", std::to_string(offset)}}
                );
                throw;
            }
        }
    );
}

std::vector<models::Dog> DogService::findByShelterId(const std::string& shelter_id) {
    return executeWithHealing<std::vector<models::Dog>>(
        "dog_find_by_shelter",
        [&]() -> std::vector<models::Dog> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT * FROM dogs WHERE shelter_id = $1 ORDER BY intake_date ASC",
                    shelter_id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                std::vector<models::Dog> dogs;
                dogs.reserve(result.size());

                for (const auto& row : result) {
                    dogs.push_back(models::Dog::fromDbRow(row));
                }

                return dogs;

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find dogs by shelter: " + std::string(e.what()),
                    {{"shelter_id", shelter_id}}
                );
                throw;
            }
        }
    );
}

std::vector<models::Dog> DogService::findByStateCode(const std::string& state_code) {
    return executeWithHealing<std::vector<models::Dog>>(
        "dog_find_by_state",
        [&]() -> std::vector<models::Dog> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Join with shelters to get dogs by state
                pqxx::result result = txn.exec_params(
                    R"(
                    SELECT d.* FROM dogs d
                    INNER JOIN shelters s ON d.shelter_id = s.id
                    WHERE s.state_code = $1
                    ORDER BY d.intake_date ASC
                    )",
                    state_code
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                std::vector<models::Dog> dogs;
                dogs.reserve(result.size());

                for (const auto& row : result) {
                    dogs.push_back(models::Dog::fromDbRow(row));
                }

                return dogs;

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find dogs by state: " + std::string(e.what()),
                    {{"state_code", state_code}}
                );
                throw;
            }
        }
    );
}

models::Dog DogService::save(const models::Dog& dog) {
    return executeWithHealing<models::Dog>(
        "dog_save",
        [&]() -> models::Dog {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Generate UUID if not provided
                std::string dog_id = dog.id.empty() ?
                    txn.exec("SELECT gen_random_uuid()::text")[0][0].as<std::string>() :
                    dog.id;

                // Format arrays for PostgreSQL
                std::string photo_urls_arr = formatPostgresArray(dog.photo_urls);
                std::string tags_arr = formatPostgresArray(dog.tags);

                // Convert timestamps
                auto now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
                std::time_t intake_time = std::chrono::system_clock::to_time_t(dog.intake_date);

                std::ostringstream now_ss, intake_ss;
                now_ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%d %H:%M:%S");
                intake_ss << std::put_time(std::gmtime(&intake_time), "%Y-%m-%d %H:%M:%S");

                pqxx::result result = txn.exec_params(
                    R"(
                    INSERT INTO dogs (
                        id, name, shelter_id, breed_primary, breed_secondary,
                        size, age_category, age_months, gender, color_primary,
                        color_secondary, weight_lbs, status, is_available,
                        intake_date, available_date, euthanasia_date,
                        urgency_level, is_on_euthanasia_list,
                        photo_urls, video_url, description, tags,
                        external_id, source, created_at, updated_at
                    ) VALUES (
                        $1, $2, $3, $4, $5, $6, $7, $8, $9, $10,
                        $11, $12, $13, $14, $15, $16, $17, $18, $19,
                        $20, $21, $22, $23, $24, $25, $26, $27
                    ) RETURNING *
                    )",
                    dog_id,
                    dog.name,
                    dog.shelter_id,
                    dog.breed_primary,
                    dog.breed_secondary,
                    dog.size,
                    dog.age_category,
                    dog.age_months,
                    dog.gender,
                    dog.color_primary,
                    dog.color_secondary,
                    dog.weight_lbs,
                    dog.status,
                    dog.is_available,
                    intake_ss.str(),
                    std::optional<std::string>(std::nullopt),  // available_date
                    std::optional<std::string>(std::nullopt),  // euthanasia_date
                    dog.urgency_level,
                    dog.is_on_euthanasia_list,
                    photo_urls_arr,
                    dog.video_url,
                    dog.description,
                    tags_arr,
                    dog.external_id,
                    dog.source,
                    now_ss.str(),
                    now_ss.str()
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return models::Dog::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to save dog: " + std::string(e.what()),
                    {{"dog_name", dog.name}, {"shelter_id", dog.shelter_id}}
                );
                throw;
            }
        }
    );
}

models::Dog DogService::update(const models::Dog& dog) {
    return executeWithHealing<models::Dog>(
        "dog_update",
        [&]() -> models::Dog {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Format arrays
                std::string photo_urls_arr = formatPostgresArray(dog.photo_urls);
                std::string tags_arr = formatPostgresArray(dog.tags);

                // Current timestamp for updated_at
                auto now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
                std::ostringstream now_ss;
                now_ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%d %H:%M:%S");

                pqxx::result result = txn.exec_params(
                    R"(
                    UPDATE dogs SET
                        name = $2,
                        shelter_id = $3,
                        breed_primary = $4,
                        breed_secondary = $5,
                        size = $6,
                        age_category = $7,
                        age_months = $8,
                        gender = $9,
                        color_primary = $10,
                        color_secondary = $11,
                        weight_lbs = $12,
                        status = $13,
                        is_available = $14,
                        urgency_level = $15,
                        is_on_euthanasia_list = $16,
                        photo_urls = $17,
                        video_url = $18,
                        description = $19,
                        tags = $20,
                        external_id = $21,
                        source = $22,
                        updated_at = $23
                    WHERE id = $1
                    RETURNING *
                    )",
                    dog.id,
                    dog.name,
                    dog.shelter_id,
                    dog.breed_primary,
                    dog.breed_secondary,
                    dog.size,
                    dog.age_category,
                    dog.age_months,
                    dog.gender,
                    dog.color_primary,
                    dog.color_secondary,
                    dog.weight_lbs,
                    dog.status,
                    dog.is_available,
                    dog.urgency_level,
                    dog.is_on_euthanasia_list,
                    photo_urls_arr,
                    dog.video_url,
                    dog.description,
                    tags_arr,
                    dog.external_id,
                    dog.source,
                    now_ss.str()
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                if (result.empty()) {
                    throw std::runtime_error("Dog not found for update: " + dog.id);
                }

                return models::Dog::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to update dog: " + std::string(e.what()),
                    {{"dog_id", dog.id}}
                );
                throw;
            }
        }
    );
}

bool DogService::deleteById(const std::string& id) {
    return executeWithHealing<bool>(
        "dog_delete",
        [&]() -> bool {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "DELETE FROM dogs WHERE id = $1 RETURNING id",
                    id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return !result.empty();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to delete dog: " + std::string(e.what()),
                    {{"dog_id", id}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// COUNT QUERIES
// ============================================================================

int DogService::countAll() {
    return executeWithHealing<int>(
        "dog_count_all",
        [&]() -> int {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec("SELECT COUNT(*) FROM dogs");

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return result[0][0].as<int>();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to count all dogs: " + std::string(e.what()),
                    {}
                );
                throw;
            }
        }
    );
}

int DogService::countByShelterId(const std::string& shelter_id) {
    return executeWithHealing<int>(
        "dog_count_by_shelter",
        [&]() -> int {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT COUNT(*) FROM dogs WHERE shelter_id = $1",
                    shelter_id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return result[0][0].as<int>();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to count dogs by shelter: " + std::string(e.what()),
                    {{"shelter_id", shelter_id}}
                );
                throw;
            }
        }
    );
}

int DogService::countByStateCode(const std::string& state_code) {
    return executeWithHealing<int>(
        "dog_count_by_state",
        [&]() -> int {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    R"(
                    SELECT COUNT(*) FROM dogs d
                    INNER JOIN shelters s ON d.shelter_id = s.id
                    WHERE s.state_code = $1
                    )",
                    state_code
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return result[0][0].as<int>();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to count dogs by state: " + std::string(e.what()),
                    {{"state_code", state_code}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// WAIT TIME CALCULATIONS
// ============================================================================

WaitTimeComponents DogService::calculateWaitTime(const models::Dog& dog) {
    return ::wtl::core::utils::WaitTimeCalculator::calculate(dog.intake_date);
}

std::optional<CountdownComponents> DogService::calculateCountdown(const models::Dog& dog) {
    // Only calculate countdown if dog has euthanasia date
    if (!dog.euthanasia_date) {
        return std::nullopt;
    }

    return ::wtl::core::utils::WaitTimeCalculator::calculateCountdown(*dog.euthanasia_date);
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

template<typename T>
T DogService::executeWithHealing(const std::string& operation_name,
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

std::string DogService::formatPostgresArray(const std::vector<std::string>& vec) {
    if (vec.empty()) {
        return "{}";
    }

    std::ostringstream ss;
    ss << "{";

    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) {
            ss << ",";
        }

        const std::string& val = vec[i];
        bool needs_quotes = val.find(',') != std::string::npos ||
                           val.find('"') != std::string::npos ||
                           val.find('{') != std::string::npos ||
                           val.find('}') != std::string::npos ||
                           val.find(' ') != std::string::npos;

        if (needs_quotes) {
            ss << "\"";
            for (char c : val) {
                if (c == '"') {
                    ss << "\\\"";
                } else {
                    ss << c;
                }
            }
            ss << "\"";
        } else {
            ss << val;
        }
    }

    ss << "}";
    return ss.str();
}

} // namespace wtl::core::services
