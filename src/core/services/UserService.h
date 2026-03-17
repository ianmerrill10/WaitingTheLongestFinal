/**
 * @file UserService.h
 * @brief Service for user-related business logic
 *
 * PURPOSE:
 * Handles all user CRUD operations, authentication helpers, and password
 * management. Works closely with AuthService for login/registration.
 *
 * USAGE:
 * auto& service = UserService::getInstance();
 * auto user = service.findByEmail("user@example.com");
 * bool valid = service.verifyPassword("user@example.com", "password123");
 *
 * DEPENDENCIES:
 * - ConnectionPool (Agent 1) - Database connections
 * - ErrorCapture (Agent 1) - Error logging
 * - SelfHealing (Agent 1) - Retry logic
 * - User model - User data structure
 * - bcrypt (password hashing)
 *
 * MODIFICATION GUIDE:
 * - To add new query methods, follow the pattern in findById()
 * - All database access must use the connection pool
 * - All errors must be captured with WTL_CAPTURE_ERROR macro
 * - Password hashing uses bcrypt with work factor 12
 *
 * @author Agent 3 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

// Project includes
#include "core/models/User.h"

namespace wtl::core::services {

/**
 * @class UserService
 * @brief Singleton service for user operations
 *
 * Thread-safe singleton that manages all user-related business logic.
 * Uses connection pooling for database access and bcrypt for password
 * hashing.
 */
class UserService {
public:
    // ========================================================================
    // SINGLETON ACCESS
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the UserService instance
     */
    static UserService& getInstance();

    // Prevent copying and moving
    UserService(const UserService&) = delete;
    UserService& operator=(const UserService&) = delete;
    UserService(UserService&&) = delete;
    UserService& operator=(UserService&&) = delete;

    // ========================================================================
    // QUERY OPERATIONS
    // ========================================================================

    /**
     * @brief Find a user by their unique ID
     *
     * @param id The UUID of the user to find
     * @return std::optional<User> The user if found, std::nullopt otherwise
     */
    std::optional<models::User> findById(const std::string& id);

    /**
     * @brief Find a user by their email address
     *
     * Email addresses are unique, so this returns at most one user.
     *
     * @param email The email address to search for
     * @return std::optional<User> The user if found, std::nullopt otherwise
     */
    std::optional<models::User> findByEmail(const std::string& email);

    // ========================================================================
    // SAVE OPERATIONS
    // ========================================================================

    /**
     * @brief Save a new user to the database
     *
     * The password_hash field should already be hashed using hashPassword().
     * Do not pass plain text passwords.
     *
     * @param user The user to save
     * @return User The saved user with generated ID and timestamps
     */
    models::User save(const models::User& user);

    /**
     * @brief Update an existing user
     *
     * Note: To update password, use a separate password change flow
     * that properly validates the old password first.
     *
     * @param user The user with updated fields
     * @return User The updated user
     */
    models::User update(const models::User& user);

    /**
     * @brief Delete a user by ID
     *
     * @param id The UUID of the user to delete
     * @return bool True if deleted, false if not found
     */
    bool deleteById(const std::string& id);

    // ========================================================================
    // PASSWORD OPERATIONS
    // ========================================================================

    /**
     * @brief Verify a user's password
     *
     * Compares the provided password against the stored hash using bcrypt.
     *
     * @param email The user's email address
     * @param password The plain text password to verify
     * @return bool True if password is correct, false otherwise
     */
    bool verifyPassword(const std::string& email, const std::string& password);

    /**
     * @brief Hash a password using bcrypt
     *
     * Uses a work factor of 12 for security. This should be used when
     * registering new users or changing passwords.
     *
     * @param password The plain text password to hash
     * @return std::string The bcrypt hash
     */
    std::string hashPassword(const std::string& password);

    // ========================================================================
    // FAVORITES OPERATIONS
    // ========================================================================

    /**
     * @brief Get a user's favorite dog IDs
     *
     * @param user_id The UUID of the user
     * @return Vector of dog UUIDs the user has favorited
     */
    std::vector<std::string> getFavorites(const std::string& user_id);

    /**
     * @brief Add a dog to user's favorites
     *
     * @param user_id The UUID of the user
     * @param dog_id The UUID of the dog to favorite
     * @return true if added, false if already in favorites
     */
    bool addFavorite(const std::string& user_id, const std::string& dog_id);

    /**
     * @brief Remove a dog from user's favorites
     *
     * @param user_id The UUID of the user
     * @param dog_id The UUID of the dog to unfavorite
     * @return true if removed, false if not in favorites
     */
    bool removeFavorite(const std::string& user_id, const std::string& dog_id);

    // ========================================================================
    // LOGIN TRACKING
    // ========================================================================

    /**
     * @brief Update the last login timestamp for a user
     *
     * Should be called after successful authentication.
     *
     * @param user_id The UUID of the user
     */
    void updateLastLogin(const std::string& user_id);

private:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    /**
     * @brief Private constructor for singleton pattern
     */
    UserService();

    /**
     * @brief Destructor
     */
    ~UserService();

    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================

    /**
     * @brief Execute a database query with self-healing
     */
    template<typename T>
    T executeWithHealing(const std::string& operation_name,
                         std::function<T()> operation);

    /**
     * @brief Format a Json::Value to string for database storage
     */
    std::string formatJsonForDb(const Json::Value& json);

    // ========================================================================
    // CONSTANTS
    // ========================================================================

    static const int BCRYPT_WORK_FACTOR = 12;  ///< Bcrypt work factor (2^12 iterations)

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    std::mutex mutex_;  ///< Mutex for thread-safe operations
};

} // namespace wtl::core::services
