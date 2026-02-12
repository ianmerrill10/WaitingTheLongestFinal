/**
 * @file JwtUtils.cc
 * @brief Implementation of JWT token utilities
 * @see JwtUtils.h for documentation
 */

#include "core/auth/JwtUtils.h"

// Standard library includes
#include <chrono>
#include <mutex>
#include <stdexcept>

// Third-party includes
#include <jwt-cpp/jwt.h>

namespace wtl::core::auth {

// ============================================================================
// STATIC MEMBER INITIALIZATION
// ============================================================================

std::string JwtUtils::jwt_secret_;
bool JwtUtils::secret_configured_ = false;

// Thread-safety for secret access
static std::mutex secret_mutex;

// ============================================================================
// PUBLIC METHODS
// ============================================================================

std::string JwtUtils::createToken(const std::string& user_id,
                                   const std::string& role,
                                   int expiry_hours) {
    const auto& secret = getSecret();

    // Validate inputs
    if (user_id.empty()) {
        throw std::invalid_argument("User ID cannot be empty");
    }
    if (role.empty()) {
        throw std::invalid_argument("Role cannot be empty");
    }
    if (expiry_hours <= 0) {
        throw std::invalid_argument("Expiry hours must be positive");
    }

    auto now = std::chrono::system_clock::now();
    auto expires_at = now + std::chrono::hours(expiry_hours);

    try {
        // Create and sign the JWT token using HS256 algorithm
        auto token = jwt::create()
            .set_issuer(ISSUER)
            .set_type("JWT")
            .set_subject(user_id)                    // sub claim = user_id
            .set_payload_claim("role", jwt::claim(role))
            .set_payload_claim("typ", jwt::claim(std::string(TOKEN_TYPE_ACCESS)))
            .set_issued_at(now)                      // iat claim
            .set_expires_at(expires_at)              // exp claim
            .sign(jwt::algorithm::hs256{secret});

        return token;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create JWT token: " + std::string(e.what()));
    }
}

std::string JwtUtils::createRefreshToken(const std::string& user_id, int expiry_days) {
    const auto& secret = getSecret();

    // Validate inputs
    if (user_id.empty()) {
        throw std::invalid_argument("User ID cannot be empty");
    }
    if (expiry_days <= 0) {
        throw std::invalid_argument("Expiry days must be positive");
    }

    auto now = std::chrono::system_clock::now();
    auto expires_at = now + std::chrono::hours(24 * expiry_days);

    try {
        // Refresh tokens have minimal claims for security
        auto token = jwt::create()
            .set_issuer(ISSUER)
            .set_type("JWT")
            .set_subject(user_id)                    // sub claim = user_id
            .set_payload_claim("typ", jwt::claim(std::string(TOKEN_TYPE_REFRESH)))
            .set_issued_at(now)                      // iat claim
            .set_expires_at(expires_at)              // exp claim
            .sign(jwt::algorithm::hs256{secret});

        return token;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create refresh token: " + std::string(e.what()));
    }
}

std::optional<TokenPayload> JwtUtils::validateToken(const std::string& token) {
    const auto& secret = getSecret();

    if (token.empty()) {
        return std::nullopt;
    }

    try {
        // Create verifier with HS256 algorithm
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer(ISSUER)
            .leeway(30);  // Allow 30 seconds of clock skew

        // Decode the token
        auto decoded = jwt::decode(token);

        // Verify signature and claims
        verifier.verify(decoded);

        // Extract payload
        TokenPayload payload;
        payload.user_id = decoded.get_subject();

        // Get role claim (only present in access tokens)
        if (decoded.has_payload_claim("role")) {
            payload.role = decoded.get_payload_claim("role").as_string();
        }

        // Get token type
        if (decoded.has_payload_claim("typ")) {
            payload.token_type = decoded.get_payload_claim("typ").as_string();
        }

        // Get timestamps
        payload.issued_at = decoded.get_issued_at();
        payload.expires_at = decoded.get_expires_at();

        return payload;

    } catch (const jwt::error::token_verification_exception& e) {
        // Token validation failed (invalid signature, expired, etc.)
        return std::nullopt;
    } catch (const std::exception& e) {
        // Other errors (malformed token, etc.)
        return std::nullopt;
    }
}

TokenPayload JwtUtils::decodeToken(const std::string& token) {
    if (token.empty()) {
        throw std::invalid_argument("Token cannot be empty");
    }

    try {
        // Decode without verification
        auto decoded = jwt::decode(token);

        TokenPayload payload;
        payload.user_id = decoded.get_subject();

        if (decoded.has_payload_claim("role")) {
            payload.role = decoded.get_payload_claim("role").as_string();
        }

        if (decoded.has_payload_claim("typ")) {
            payload.token_type = decoded.get_payload_claim("typ").as_string();
        }

        payload.issued_at = decoded.get_issued_at();
        payload.expires_at = decoded.get_expires_at();

        return payload;

    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to decode token: " + std::string(e.what()));
    }
}

bool JwtUtils::isExpired(const std::string& token) {
    if (token.empty()) {
        return true;
    }

    try {
        auto decoded = jwt::decode(token);
        auto expires_at = decoded.get_expires_at();
        return std::chrono::system_clock::now() >= expires_at;
    } catch (const std::exception&) {
        // Invalid token is considered expired
        return true;
    }
}

std::chrono::system_clock::time_point JwtUtils::getTokenExpiry(const std::string& token) {
    if (token.empty()) {
        throw std::invalid_argument("Token cannot be empty");
    }

    try {
        auto decoded = jwt::decode(token);
        return decoded.get_expires_at();
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to get token expiry: " + std::string(e.what()));
    }
}

std::string JwtUtils::extractUserId(const std::string& token) {
    if (token.empty()) {
        return "";
    }

    try {
        auto decoded = jwt::decode(token);
        return decoded.get_subject();
    } catch (const std::exception&) {
        return "";
    }
}

bool JwtUtils::isRefreshToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    try {
        auto decoded = jwt::decode(token);
        if (decoded.has_payload_claim("typ")) {
            return decoded.get_payload_claim("typ").as_string() == TOKEN_TYPE_REFRESH;
        }
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

void JwtUtils::setSecret(const std::string& secret) {
    // Minimum secret length for HS256 security
    constexpr int MIN_SECRET_LENGTH = 32;

    if (secret.length() < MIN_SECRET_LENGTH) {
        throw std::invalid_argument(
            "JWT secret must be at least " + std::to_string(MIN_SECRET_LENGTH) + " characters"
        );
    }

    std::lock_guard<std::mutex> lock(secret_mutex);
    jwt_secret_ = secret;
    secret_configured_ = true;
}

bool JwtUtils::isSecretConfigured() {
    std::lock_guard<std::mutex> lock(secret_mutex);
    return secret_configured_;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

const std::string& JwtUtils::getSecret() {
    std::lock_guard<std::mutex> lock(secret_mutex);

    if (!secret_configured_) {
        throw std::runtime_error(
            "JWT secret not configured. Call JwtUtils::setSecret() during application startup."
        );
    }

    return jwt_secret_;
}

} // namespace wtl::core::auth
