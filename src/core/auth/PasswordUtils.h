/**
 * @file PasswordUtils.h
 * @brief Password hashing and verification utilities using bcrypt
 *
 * PURPOSE:
 * Provides secure password hashing and verification functions using
 * the bcrypt algorithm. All password operations in the system should
 * use these utilities to ensure consistent security.
 *
 * USAGE:
 * // Hash a password
 * std::string hash = PasswordUtils::hashPassword("user_password");
 *
 * // Verify a password
 * bool valid = PasswordUtils::verifyPassword("user_password", hash);
 *
 * DEPENDENCIES:
 * - bcrypt library for password hashing
 * - ErrorCapture (Agent 1) - Error logging
 *
 * MODIFICATION GUIDE:
 * - Do not reduce the cost factor below 12
 * - Always use generateSalt() for new hashes
 * - Never log passwords or hashes
 *
 * @author Agent 6 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes (alphabetical)
#include <random>
#include <string>

namespace wtl::core::auth {

/**
 * @class PasswordUtils
 * @brief Static utility class for password operations
 *
 * Provides bcrypt-based password hashing with configurable cost factor.
 * All methods are static and thread-safe.
 */
class PasswordUtils {
public:
    // ========================================================================
    // CONSTANTS
    // ========================================================================

    /**
     * @brief Bcrypt cost factor (2^12 iterations)
     *
     * Cost factor of 12 provides good security while keeping
     * hash time under 500ms on modern hardware. Research indicates
     * this is the minimum recommended value for production systems.
     */
    static constexpr int BCRYPT_COST = 12;

    /**
     * @brief Length of generated salts in bytes
     */
    static constexpr int SALT_LENGTH = 16;

    /**
     * @brief Minimum password length requirement
     */
    static constexpr int MIN_PASSWORD_LENGTH = 8;

    /**
     * @brief Maximum password length (to prevent DoS via long passwords)
     */
    static constexpr int MAX_PASSWORD_LENGTH = 72;

    // ========================================================================
    // PUBLIC METHODS
    // ========================================================================

    /**
     * @brief Hash a password using bcrypt
     *
     * Generates a secure hash of the provided password using bcrypt
     * with the configured cost factor. The salt is automatically
     * generated and embedded in the hash.
     *
     * @param password The plaintext password to hash
     * @return Bcrypt hash string containing algorithm, cost, salt, and hash
     * @throws std::runtime_error if hashing fails
     *
     * @note Never log the password parameter
     * @note Hash format: $2a$12$[22-char salt][31-char hash]
     *
     * @example
     * std::string hash = PasswordUtils::hashPassword("my_password");
     * // Returns something like: $2a$12$R9h/cIPz0gi.URNNX3kh2OPST...
     */
    static std::string hashPassword(const std::string& password);

    /**
     * @brief Verify a password against a bcrypt hash
     *
     * Compares the provided plaintext password against the stored
     * hash to determine if they match. Uses constant-time comparison
     * to prevent timing attacks.
     *
     * @param password The plaintext password to verify
     * @param hash The bcrypt hash to compare against
     * @return true if password matches hash, false otherwise
     *
     * @note Never log the password parameter
     * @note Returns false for any error condition (invalid hash format, etc.)
     *
     * @example
     * if (PasswordUtils::verifyPassword("my_password", stored_hash)) {
     *     // Password is correct
     * }
     */
    static bool verifyPassword(const std::string& password, const std::string& hash);

    /**
     * @brief Generate a cryptographically secure random salt
     *
     * Creates a random salt suitable for use with bcrypt or
     * other hashing algorithms. Uses the system's secure
     * random number generator.
     *
     * @return Base64-encoded salt string
     *
     * @example
     * std::string salt = PasswordUtils::generateSalt();
     */
    static std::string generateSalt();

    /**
     * @brief Generate a secure random token
     *
     * Creates a cryptographically secure random token suitable
     * for password reset tokens, email verification tokens, etc.
     *
     * @param length Number of bytes of randomness (default: 32)
     * @return URL-safe base64-encoded token string
     *
     * @example
     * std::string reset_token = PasswordUtils::generateSecureToken();
     */
    static std::string generateSecureToken(int length = 32);

    /**
     * @brief Validate password meets security requirements
     *
     * Checks if the provided password meets minimum security
     * requirements (length, complexity, etc.).
     *
     * @param password The password to validate
     * @return Empty string if valid, error message if invalid
     *
     * @example
     * std::string error = PasswordUtils::validatePassword("weak");
     * if (!error.empty()) {
     *     // Password is too weak
     * }
     */
    static std::string validatePassword(const std::string& password);

    /**
     * @brief Hash a token for secure storage
     *
     * Creates a SHA-256 hash of a token for storage in the database.
     * Used for refresh tokens and other tokens that need to be
     * stored but verified later.
     *
     * @param token The token to hash
     * @return Hex-encoded SHA-256 hash
     *
     * @example
     * std::string token_hash = PasswordUtils::hashToken(refresh_token);
     */
    static std::string hashToken(const std::string& token);

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /**
     * @brief Generate cryptographically secure random bytes
     * @param length Number of random bytes to generate
     * @return Vector of random bytes
     */
    static std::vector<unsigned char> generateRandomBytes(int length);

    /**
     * @brief Encode bytes to URL-safe base64
     * @param data Raw bytes to encode
     * @return URL-safe base64 string
     */
    static std::string base64UrlEncode(const std::vector<unsigned char>& data);

    /**
     * @brief Encode bytes to hexadecimal string
     * @param data Raw bytes to encode
     * @return Hexadecimal string
     */
    static std::string hexEncode(const std::vector<unsigned char>& data);

    // Prevent instantiation - all methods are static
    PasswordUtils() = delete;
    ~PasswordUtils() = delete;
};

} // namespace wtl::core::auth
