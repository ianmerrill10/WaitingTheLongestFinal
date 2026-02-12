/**
 * @file PasswordUtils.cc
 * @brief Implementation of password hashing utilities
 * @see PasswordUtils.h for documentation
 */

#include "core/auth/PasswordUtils.h"

// Standard library includes
#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

// System bcrypt via crypt()
#include <crypt.h>

// OpenSSL for SHA-256 hashing and random bytes
#include <openssl/sha.h>
#include <openssl/rand.h>

namespace wtl::core::auth {

// ============================================================================
// PUBLIC METHODS
// ============================================================================

std::string PasswordUtils::hashPassword(const std::string& password) {
    // Security: Never log the password

    // Validate password length to prevent DoS
    if (password.length() > MAX_PASSWORD_LENGTH) {
        throw std::runtime_error("Password exceeds maximum length");
    }

    if (password.empty()) {
        throw std::runtime_error("Password cannot be empty");
    }

    try {
        // Generate bcrypt salt using OpenSSL random bytes
        // Format: $2b$CC$ followed by 22 base64 chars
        unsigned char salt_bytes[16];
        if (RAND_bytes(salt_bytes, sizeof(salt_bytes)) != 1) {
            throw std::runtime_error("Failed to generate random salt");
        }

        // Encode salt in bcrypt's custom base64
        static const char b64[] =
            "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::string salt_str = "$2b$";
        if (BCRYPT_COST < 10) salt_str += "0";
        salt_str += std::to_string(BCRYPT_COST) + "$";
        for (int i = 0; i < 16; ++i) {
            salt_str += b64[salt_bytes[i] % 64];
        }
        // Pad to 22 chars
        while (salt_str.length() < 7 + 22) salt_str += b64[0];

        struct crypt_data data;
        memset(&data, 0, sizeof(data));
        char* result = crypt_r(password.c_str(), salt_str.c_str(), &data);
        if (!result) {
            throw std::runtime_error("crypt_r failed");
        }
        return std::string(result);
    } catch (const std::exception& e) {
        // Don't include password in error message
        throw std::runtime_error("Failed to hash password: " + std::string(e.what()));
    }
}

bool PasswordUtils::verifyPassword(const std::string& password, const std::string& hash) {
    // Security: Never log the password or hash

    // Basic validation
    if (password.empty() || hash.empty()) {
        return false;
    }

    // Validate password length to prevent DoS
    if (password.length() > MAX_PASSWORD_LENGTH) {
        return false;
    }

    // Validate hash format (should start with $2a$, $2b$, or $2y$)
    if (hash.length() < 60 || hash[0] != '$' || hash[1] != '2') {
        return false;
    }

    try {
        // Use crypt_r with the stored hash as salt (bcrypt extracts salt from hash)
        struct crypt_data data;
        memset(&data, 0, sizeof(data));
        char* result = crypt_r(password.c_str(), hash.c_str(), &data);
        if (!result) {
            return false;
        }
        // Constant-time comparison to prevent timing attacks
        std::string computed(result);
        if (computed.length() != hash.length()) return false;
        unsigned char diff = 0;
        for (size_t i = 0; i < computed.length(); ++i) {
            diff |= static_cast<unsigned char>(computed[i]) ^ static_cast<unsigned char>(hash[i]);
        }
        return diff == 0;
    } catch (const std::exception&) {
        // Any error during verification means the password is invalid
        return false;
    }
}

std::string PasswordUtils::generateSalt() {
    auto random_bytes = generateRandomBytes(SALT_LENGTH);
    return base64UrlEncode(random_bytes);
}

std::string PasswordUtils::generateSecureToken(int length) {
    if (length <= 0 || length > 256) {
        throw std::invalid_argument("Token length must be between 1 and 256");
    }

    auto random_bytes = generateRandomBytes(length);
    return base64UrlEncode(random_bytes);
}

std::string PasswordUtils::validatePassword(const std::string& password) {
    // Check minimum length
    if (password.length() < MIN_PASSWORD_LENGTH) {
        return "Password must be at least " + std::to_string(MIN_PASSWORD_LENGTH) + " characters";
    }

    // Check maximum length
    if (password.length() > MAX_PASSWORD_LENGTH) {
        return "Password must be at most " + std::to_string(MAX_PASSWORD_LENGTH) + " characters";
    }

    // Check for at least one uppercase letter
    bool has_upper = std::any_of(password.begin(), password.end(), ::isupper);
    if (!has_upper) {
        return "Password must contain at least one uppercase letter";
    }

    // Check for at least one lowercase letter
    bool has_lower = std::any_of(password.begin(), password.end(), ::islower);
    if (!has_lower) {
        return "Password must contain at least one lowercase letter";
    }

    // Check for at least one digit
    bool has_digit = std::any_of(password.begin(), password.end(), ::isdigit);
    if (!has_digit) {
        return "Password must contain at least one digit";
    }

    // Check for at least one special character
    auto is_special = [](char c) {
        return !std::isalnum(static_cast<unsigned char>(c)) && !std::isspace(static_cast<unsigned char>(c));
    };
    bool has_special = std::any_of(password.begin(), password.end(), is_special);
    if (!has_special) {
        return "Password must contain at least one special character";
    }

    // Check for common weak passwords
    static const std::vector<std::string> weak_passwords = {
        "password", "Password1!", "Qwerty123!", "Admin123!",
        "Welcome1!", "Passw0rd!", "12345678", "87654321"
    };

    for (const auto& weak : weak_passwords) {
        // Case-insensitive comparison of the base word
        std::string lower_password = password;
        std::transform(lower_password.begin(), lower_password.end(),
                      lower_password.begin(), ::tolower);
        std::string lower_weak = weak;
        std::transform(lower_weak.begin(), lower_weak.end(),
                      lower_weak.begin(), ::tolower);

        if (lower_password.find(lower_weak.substr(0, 8)) != std::string::npos) {
            return "Password is too common or easily guessable";
        }
    }

    // Password meets all requirements
    return "";
}

std::string PasswordUtils::hashToken(const std::string& token) {
    if (token.empty()) {
        throw std::invalid_argument("Token cannot be empty");
    }

    // Use SHA-256 for token hashing (fast, deterministic)
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(token.c_str()),
           token.length(), hash);

    std::vector<unsigned char> hash_vec(hash, hash + SHA256_DIGEST_LENGTH);
    return hexEncode(hash_vec);
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::vector<unsigned char> PasswordUtils::generateRandomBytes(int length) {
    std::vector<unsigned char> buffer(length);

    // Use OpenSSL's cryptographically secure random number generator
    if (RAND_bytes(buffer.data(), length) != 1) {
        // Fallback to std::random_device if OpenSSL fails
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<unsigned int> dist(0, 255);

        for (int i = 0; i < length; ++i) {
            buffer[i] = static_cast<unsigned char>(dist(gen));
        }
    }

    return buffer;
}

std::string PasswordUtils::base64UrlEncode(const std::vector<unsigned char>& data) {
    static const char encoding_table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        unsigned int octet_a = i < data.size() ? data[i] : 0;
        unsigned int octet_b = i + 1 < data.size() ? data[i + 1] : 0;
        unsigned int octet_c = i + 2 < data.size() ? data[i + 2] : 0;

        unsigned int triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        result.push_back(encoding_table[(triple >> 18) & 0x3F]);
        result.push_back(encoding_table[(triple >> 12) & 0x3F]);

        if (i + 1 < data.size()) {
            result.push_back(encoding_table[(triple >> 6) & 0x3F]);
        }

        if (i + 2 < data.size()) {
            result.push_back(encoding_table[triple & 0x3F]);
        }
    }

    // No padding for URL-safe base64
    return result;
}

std::string PasswordUtils::hexEncode(const std::vector<unsigned char>& data) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');

    for (unsigned char byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }

    return ss.str();
}

} // namespace wtl::core::auth
