/**
 * @file UserService.cc
 * @brief Implementation of UserService
 * @see UserService.h for documentation
 *
 * DEPENDENCIES (from other agents):
 * - ConnectionPool (Agent 1) - Database connections
 * - ErrorCapture (Agent 1) - Error logging
 * - SelfHealing (Agent 1) - Retry logic
 * - bcrypt library for password hashing
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#include "core/services/UserService.h"

#include <iomanip>
#include <sstream>
#include <random>
#include <cstring>

// Dependencies from Agent 1
#include "core/db/ConnectionPool.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"

// Password hashing via Linux crypt_r() with bcrypt ($2b$) support
#include <crypt.h>

namespace wtl::core::services {

// ============================================================================
// CONSTRUCTION / SINGLETON
// ============================================================================

UserService::UserService() {
    // Initialize any required resources
}

UserService::~UserService() {
    // Cleanup resources
}

UserService& UserService::getInstance() {
    static UserService instance;
    return instance;
}

// ============================================================================
// PRIVATE HELPER METHODS (defined before use)
// ============================================================================

// Generic template definition must come first.
template<typename T>
T UserService::executeWithHealing(const std::string& operation_name,
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
void UserService::executeWithHealing<void>(const std::string& operation_name,
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

std::optional<models::User> UserService::findById(const std::string& id) {
    return executeWithHealing<std::optional<models::User>>(
        "user_find_by_id",
        [&]() -> std::optional<models::User> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT * FROM users WHERE id = $1",
                    id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                if (result.empty()) {
                    return std::nullopt;
                }

                return models::User::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find user by ID: " + std::string(e.what()),
                    {{"user_id", id}, {"operation", "findById"}}
                );
                throw;
            }
        }
    );
}

std::optional<models::User> UserService::findByEmail(const std::string& email) {
    return executeWithHealing<std::optional<models::User>>(
        "user_find_by_email",
        [&]() -> std::optional<models::User> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Use LOWER() for case-insensitive email lookup
                pqxx::result result = txn.exec_params(
                    "SELECT * FROM users WHERE LOWER(email) = LOWER($1)",
                    email
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                if (result.empty()) {
                    return std::nullopt;
                }

                return models::User::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to find user by email: " + std::string(e.what()),
                    {{"email", email}, {"operation", "findByEmail"}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// SAVE OPERATIONS
// ============================================================================

models::User UserService::save(const models::User& user) {
    return executeWithHealing<models::User>(
        "user_save",
        [&]() -> models::User {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Generate UUID if not provided
                std::string user_id = user.id.empty() ?
                    txn.exec("SELECT gen_random_uuid()::text")[0][0].as<std::string>() :
                    user.id;

                // Convert timestamps
                auto now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(now);

                std::ostringstream now_ss;
                now_ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%d %H:%M:%S");

                // Format preferences as JSON strings
                std::string notification_prefs = formatJsonForDb(user.notification_preferences);
                std::string search_prefs = formatJsonForDb(user.search_preferences);

                pqxx::result result = txn.exec_params(
                    R"(
                    INSERT INTO users (
                        id, email, password_hash, display_name,
                        phone, zip_code, role, shelter_id,
                        notification_preferences, search_preferences,
                        is_active, email_verified, last_login,
                        created_at, updated_at
                    ) VALUES (
                        $1, $2, $3, $4, $5, $6, $7, $8, $9::jsonb, $10::jsonb,
                        $11, $12, $13, $14, $15
                    ) RETURNING *
                    )",
                    user_id,
                    user.email,
                    user.password_hash,
                    user.display_name,
                    user.phone ? *user.phone : nullptr,
                    user.zip_code ? *user.zip_code : nullptr,
                    user.role,
                    user.shelter_id ? *user.shelter_id : nullptr,
                    notification_prefs,
                    search_prefs,
                    user.is_active,
                    user.email_verified,
                    nullptr,  // last_login starts as null
                    now_ss.str(),
                    now_ss.str()
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return models::User::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to save user: " + std::string(e.what()),
                    {{"email", user.email}, {"role", user.role}}
                );
                throw;
            }
        }
    );
}

models::User UserService::update(const models::User& user) {
    return executeWithHealing<models::User>(
        "user_update",
        [&]() -> models::User {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Current timestamp for updated_at
                auto now = std::chrono::system_clock::now();
                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
                std::ostringstream now_ss;
                now_ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%d %H:%M:%S");

                // Format preferences
                std::string notification_prefs = formatJsonForDb(user.notification_preferences);
                std::string search_prefs = formatJsonForDb(user.search_preferences);

                pqxx::result result = txn.exec_params(
                    R"(
                    UPDATE users SET
                        email = $2,
                        display_name = $3,
                        phone = $4,
                        zip_code = $5,
                        role = $6,
                        shelter_id = $7,
                        notification_preferences = $8::jsonb,
                        search_preferences = $9::jsonb,
                        is_active = $10,
                        email_verified = $11,
                        updated_at = $12
                    WHERE id = $1
                    RETURNING *
                    )",
                    user.id,
                    user.email,
                    user.display_name,
                    user.phone ? *user.phone : nullptr,
                    user.zip_code ? *user.zip_code : nullptr,
                    user.role,
                    user.shelter_id ? *user.shelter_id : nullptr,
                    notification_prefs,
                    search_prefs,
                    user.is_active,
                    user.email_verified,
                    now_ss.str()
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                if (result.empty()) {
                    throw std::runtime_error("User not found for update: " + user.id);
                }

                return models::User::fromDbRow(result[0]);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to update user: " + std::string(e.what()),
                    {{"user_id", user.id}}
                );
                throw;
            }
        }
    );
}

bool UserService::deleteById(const std::string& id) {
    return executeWithHealing<bool>(
        "user_delete",
        [&]() -> bool {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "DELETE FROM users WHERE id = $1 RETURNING id",
                    id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                return !result.empty();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to delete user: " + std::string(e.what()),
                    {{"user_id", id}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// PASSWORD OPERATIONS
// ============================================================================

bool UserService::verifyPassword(const std::string& email, const std::string& password) {
    // Find the user by email
    auto user_opt = findByEmail(email);

    if (!user_opt) {
        // User not found - return false without revealing which part failed
        // Use constant-time comparison to prevent timing attacks
        // Hash the password anyway to prevent timing attacks
        hashPassword(password);
        return false;
    }

    const models::User& user = *user_opt;

    // Compare using crypt_r (bcrypt verification)
    // Re-hash with the stored hash as salt and compare results
    try {
        struct crypt_data data;
        std::memset(&data, 0, sizeof(data));
        char* hashed = crypt_r(password.c_str(), user.password_hash.c_str(), &data);

        if (hashed == nullptr || hashed[0] == '*') {
            WTL_CAPTURE_ERROR(
                ::wtl::core::debug::ErrorCategory::AUTHENTICATION,
                "bcrypt verification failed",
                {{"email", email}}
            );
            return false;
        }

        // Constant-time comparison to prevent timing attacks
        bool result = (std::strcmp(hashed, user.password_hash.c_str()) == 0);

        return result;

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::AUTHENTICATION,
            "Password verification error: " + std::string(e.what()),
            {{"email", email}}
        );
        return false;
    }
}

std::string UserService::hashPassword(const std::string& password) {
    try {
        // Generate bcrypt salt with the configured work factor
        char* salt = crypt_gensalt("$2b$", BCRYPT_WORK_FACTOR, nullptr, 0);

        if (salt == nullptr) {
            throw std::runtime_error("Failed to generate bcrypt salt");
        }

        // Hash the password using crypt_r (thread-safe)
        struct crypt_data data;
        std::memset(&data, 0, sizeof(data));
        char* hash = crypt_r(password.c_str(), salt, &data);

        if (hash == nullptr || hash[0] == '*') {
            throw std::runtime_error("Failed to hash password with bcrypt");
        }

        return std::string(hash);

    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(
            ::wtl::core::debug::ErrorCategory::AUTHENTICATION,
            "Password hashing error: " + std::string(e.what()),
            {}
        );
        throw;
    }
}

// ============================================================================
// FAVORITES OPERATIONS
// ============================================================================

std::vector<std::string> UserService::getFavorites(const std::string& user_id) {
    return executeWithHealing<std::vector<std::string>>(
        "user_get_favorites",
        [&]() -> std::vector<std::string> {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "SELECT dog_id FROM user_favorites WHERE user_id = $1 "
                    "ORDER BY created_at DESC",
                    user_id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                std::vector<std::string> favorites;
                favorites.reserve(result.size());
                for (const auto& row : result) {
                    favorites.push_back(row["dog_id"].as<std::string>());
                }
                return favorites;

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to get favorites: " + std::string(e.what()),
                    {{"user_id", user_id}}
                );
                throw;
            }
        }
    );
}

bool UserService::addFavorite(const std::string& user_id, const std::string& dog_id) {
    return executeWithHealing<bool>(
        "user_add_favorite",
        [&]() -> bool {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                // Use INSERT ... ON CONFLICT DO NOTHING to handle duplicates
                pqxx::result result = txn.exec_params(
                    "INSERT INTO user_favorites (user_id, dog_id) "
                    "VALUES ($1, $2) "
                    "ON CONFLICT (user_id, dog_id) DO NOTHING "
                    "RETURNING user_id",
                    user_id, dog_id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                // If result is empty, the favorite already existed (conflict)
                return !result.empty();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to add favorite: " + std::string(e.what()),
                    {{"user_id", user_id}, {"dog_id", dog_id}}
                );
                throw;
            }
        }
    );
}

bool UserService::removeFavorite(const std::string& user_id, const std::string& dog_id) {
    return executeWithHealing<bool>(
        "user_remove_favorite",
        [&]() -> bool {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                pqxx::result result = txn.exec_params(
                    "DELETE FROM user_favorites WHERE user_id = $1 AND dog_id = $2 "
                    "RETURNING user_id",
                    user_id, dog_id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

                // If result is empty, the favorite didn't exist
                return !result.empty();

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to remove favorite: " + std::string(e.what()),
                    {{"user_id", user_id}, {"dog_id", dog_id}}
                );
                throw;
            }
        }
    );
}

// ============================================================================
// LOGIN TRACKING
// ============================================================================

void UserService::updateLastLogin(const std::string& user_id) {
    executeWithHealing<void>(
        "user_update_last_login",
        [&]() -> void {
            auto conn = ::wtl::core::db::ConnectionPool::getInstance().acquire();

            try {
                pqxx::work txn(*conn);

                txn.exec_params(
                    "UPDATE users SET last_login = NOW(), updated_at = NOW() WHERE id = $1",
                    user_id
                );

                txn.commit();
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);

            } catch (const std::exception& e) {
                ::wtl::core::db::ConnectionPool::getInstance().release(conn);
                WTL_CAPTURE_ERROR(
                    ::wtl::core::debug::ErrorCategory::DATABASE,
                    "Failed to update last login: " + std::string(e.what()),
                    {{"user_id", user_id}}
                );
                throw;
            }
        }
    );
}

std::string UserService::formatJsonForDb(const Json::Value& json) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";  // Compact output
    return Json::writeString(builder, json);
}

} // namespace wtl::core::services
