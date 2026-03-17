/**
 * @file OAuth2Client.h
 * @brief OAuth2 authentication client for external API access
 *
 * PURPOSE:
 * Handles OAuth2 authentication flows for APIs that require it,
 * such as RescueGroups API. Supports client credentials and
 * authorization code grant types with automatic token refresh.
 *
 * USAGE:
 * OAuth2Client oauth("https://api.rescuegroups.org/v5/oauth2/token",
 *                    "client_id", "client_secret");
 * if (oauth.authenticate()) {
 *     std::string token = oauth.getAccessToken();
 * }
 *
 * DEPENDENCIES:
 * - HttpClient for token requests
 * - jsoncpp for token parsing
 *
 * MODIFICATION GUIDE:
 * - Add new grant types as needed
 * - Extend token storage mechanisms
 * - Add additional OAuth2 flows
 *
 * @author Agent 19 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

// Third-party includes
#include <json/json.h>

// Forward declarations
namespace wtl::clients {
    class HttpClient;
}

namespace wtl::clients {

/**
 * @enum OAuth2GrantType
 * @brief Supported OAuth2 grant types
 */
enum class OAuth2GrantType {
    CLIENT_CREDENTIALS,    ///< Client credentials (server-to-server)
    AUTHORIZATION_CODE,    ///< Authorization code (user authorization)
    REFRESH_TOKEN          ///< Refresh token
};

/**
 * @brief Convert grant type to string
 * @param type Grant type
 * @return String representation
 */
inline std::string grantTypeToString(OAuth2GrantType type) {
    switch (type) {
        case OAuth2GrantType::CLIENT_CREDENTIALS: return "client_credentials";
        case OAuth2GrantType::AUTHORIZATION_CODE: return "authorization_code";
        case OAuth2GrantType::REFRESH_TOKEN:      return "refresh_token";
        default:                                  return "unknown";
    }
}

/**
 * @struct OAuth2Token
 * @brief Represents an OAuth2 access token
 */
struct OAuth2Token {
    std::string access_token;          ///< The access token
    std::string token_type;            ///< Token type (usually "Bearer")
    std::string refresh_token;         ///< Refresh token if provided
    std::string scope;                 ///< Token scope
    int expires_in{0};                 ///< Seconds until expiration
    std::chrono::system_clock::time_point issued_at;   ///< When token was issued
    std::chrono::system_clock::time_point expires_at;  ///< When token expires

    /**
     * @brief Check if token is expired
     * @param buffer_seconds Buffer before actual expiration (default 60)
     * @return true if expired or will expire within buffer
     */
    bool isExpired(int buffer_seconds = 60) const {
        auto now = std::chrono::system_clock::now();
        auto buffer = std::chrono::seconds(buffer_seconds);
        return now >= (expires_at - buffer);
    }

    /**
     * @brief Check if token is valid (non-empty and not expired)
     * @return true if valid
     */
    bool isValid() const {
        return !access_token.empty() && !isExpired();
    }

    /**
     * @brief Get time remaining until expiration
     * @return Seconds remaining, 0 if expired
     */
    int getSecondsRemaining() const {
        auto now = std::chrono::system_clock::now();
        if (now >= expires_at) return 0;
        return static_cast<int>(
            std::chrono::duration_cast<std::chrono::seconds>(expires_at - now).count()
        );
    }

    /**
     * @brief Get Authorization header value
     * @return "Bearer <token>" string
     */
    std::string getAuthorizationHeader() const {
        return token_type + " " + access_token;
    }

    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["access_token"] = access_token;
        json["token_type"] = token_type;
        json["refresh_token"] = refresh_token;
        json["scope"] = scope;
        json["expires_in"] = expires_in;
        json["seconds_remaining"] = getSecondsRemaining();
        json["is_valid"] = isValid();
        return json;
    }

    /**
     * @brief Create from JSON (token response)
     * @param json Token response JSON
     * @return OAuth2Token
     */
    static OAuth2Token fromJson(const Json::Value& json) {
        OAuth2Token token;
        token.access_token = json.get("access_token", "").asString();
        token.token_type = json.get("token_type", "Bearer").asString();
        token.refresh_token = json.get("refresh_token", "").asString();
        token.scope = json.get("scope", "").asString();
        token.expires_in = json.get("expires_in", 3600).asInt();
        token.issued_at = std::chrono::system_clock::now();
        token.expires_at = token.issued_at + std::chrono::seconds(token.expires_in);
        return token;
    }
};

/**
 * @struct OAuth2Config
 * @brief Configuration for OAuth2 client
 */
struct OAuth2Config {
    std::string token_url;             ///< Token endpoint URL
    std::string client_id;             ///< Client ID
    std::string client_secret;         ///< Client secret
    OAuth2GrantType grant_type{OAuth2GrantType::CLIENT_CREDENTIALS};
    std::string scope;                 ///< Requested scope
    std::string redirect_uri;          ///< Redirect URI for auth code flow
    bool auto_refresh{true};           ///< Auto-refresh before expiration
    int refresh_buffer_seconds{300};   ///< Seconds before expiry to refresh

    /**
     * @brief Create from JSON
     * @param json Configuration JSON
     * @return OAuth2Config
     */
    static OAuth2Config fromJson(const Json::Value& json) {
        OAuth2Config config;
        config.token_url = json.get("token_url", "").asString();
        config.client_id = json.get("client_id", "").asString();
        config.client_secret = json.get("client_secret", "").asString();
        config.scope = json.get("scope", "").asString();
        config.redirect_uri = json.get("redirect_uri", "").asString();
        config.auto_refresh = json.get("auto_refresh", true).asBool();
        config.refresh_buffer_seconds = json.get("refresh_buffer_seconds", 300).asInt();

        std::string grant = json.get("grant_type", "client_credentials").asString();
        if (grant == "authorization_code") {
            config.grant_type = OAuth2GrantType::AUTHORIZATION_CODE;
        } else {
            config.grant_type = OAuth2GrantType::CLIENT_CREDENTIALS;
        }

        return config;
    }

    /**
     * @brief Convert to JSON (excludes secrets)
     * @return JSON representation
     */
    Json::Value toJson() const {
        Json::Value json;
        json["token_url"] = token_url;
        json["client_id"] = client_id;
        // Don't include client_secret in JSON output
        json["grant_type"] = grantTypeToString(grant_type);
        json["scope"] = scope;
        json["redirect_uri"] = redirect_uri;
        json["auto_refresh"] = auto_refresh;
        json["refresh_buffer_seconds"] = refresh_buffer_seconds;
        return json;
    }
};

/**
 * @class OAuth2Client
 * @brief OAuth2 authentication client
 *
 * Handles OAuth2 token acquisition and refresh for API authentication.
 * Supports client credentials and authorization code flows.
 */
class OAuth2Client {
public:
    // Type for token storage callback
    using TokenStorageCallback = std::function<void(const OAuth2Token&)>;
    using TokenLoadCallback = std::function<std::optional<OAuth2Token>()>;

    // =========================================================================
    // CONSTRUCTION
    // =========================================================================

    /**
     * @brief Construct with token URL and credentials
     *
     * @param token_url OAuth2 token endpoint
     * @param client_id Client ID
     * @param client_secret Client secret
     */
    OAuth2Client(const std::string& token_url,
                 const std::string& client_id,
                 const std::string& client_secret);

    /**
     * @brief Construct with configuration
     * @param config OAuth2 configuration
     */
    explicit OAuth2Client(const OAuth2Config& config);

    /**
     * @brief Default constructor
     */
    OAuth2Client();

    /**
     * @brief Destructor
     */
    ~OAuth2Client();

    // =========================================================================
    // CONFIGURATION
    // =========================================================================

    /**
     * @brief Configure the client
     * @param config Configuration to apply
     */
    void configure(const OAuth2Config& config);

    /**
     * @brief Set token URL
     * @param url Token endpoint URL
     */
    void setTokenUrl(const std::string& url);

    /**
     * @brief Set client credentials
     * @param client_id Client ID
     * @param client_secret Client secret
     */
    void setCredentials(const std::string& client_id,
                        const std::string& client_secret);

    /**
     * @brief Set requested scope
     * @param scope OAuth2 scope
     */
    void setScope(const std::string& scope);

    /**
     * @brief Set grant type
     * @param type Grant type
     */
    void setGrantType(OAuth2GrantType type);

    /**
     * @brief Enable/disable auto-refresh
     * @param enabled Whether to auto-refresh
     */
    void setAutoRefresh(bool enabled);

    /**
     * @brief Set token storage callbacks for persistence
     * @param store Callback to store token
     * @param load Callback to load token
     */
    void setTokenStorage(TokenStorageCallback store, TokenLoadCallback load);

    // =========================================================================
    // AUTHENTICATION
    // =========================================================================

    /**
     * @brief Authenticate and obtain access token
     *
     * @return true if authentication successful
     *
     * For client_credentials: Exchanges credentials for token
     * For authorization_code: Use authenticateWithCode() instead
     */
    bool authenticate();

    /**
     * @brief Authenticate with authorization code
     *
     * @param code Authorization code from OAuth2 redirect
     * @return true if authentication successful
     *
     * Used after user completes OAuth2 authorization flow.
     */
    bool authenticateWithCode(const std::string& code);

    /**
     * @brief Refresh the access token
     *
     * @return true if refresh successful
     *
     * Uses stored refresh_token to obtain new access_token.
     */
    bool refreshToken();

    /**
     * @brief Get valid access token, refreshing if needed
     *
     * @return Access token string, empty if not authenticated
     *
     * If auto_refresh is enabled and token is expired/expiring,
     * automatically refreshes before returning.
     */
    std::string getAccessToken();

    /**
     * @brief Get current token
     * @return Current OAuth2Token
     */
    OAuth2Token getToken() const;

    /**
     * @brief Get Authorization header value
     * @return "Bearer <token>" or empty if not authenticated
     */
    std::string getAuthorizationHeader();

    /**
     * @brief Check if currently authenticated
     * @return true if have valid token
     */
    bool isAuthenticated() const;

    /**
     * @brief Clear stored token (logout)
     */
    void clearToken();

    // =========================================================================
    // AUTHORIZATION CODE FLOW HELPERS
    // =========================================================================

    /**
     * @brief Build authorization URL for user redirect
     *
     * @param authorize_url OAuth2 authorize endpoint
     * @param state State parameter for CSRF protection
     * @return Full authorization URL
     *
     * Used to start authorization code flow.
     */
    std::string buildAuthorizationUrl(const std::string& authorize_url,
                                       const std::string& state = "") const;

    // =========================================================================
    // STATUS
    // =========================================================================

    /**
     * @brief Get status information
     * @return JSON with authentication status
     */
    Json::Value getStatus() const;

    /**
     * @brief Get last error message
     * @return Error message or empty string
     */
    std::string getLastError() const;

private:
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================

    /**
     * @brief Request token from endpoint
     * @param params Request parameters
     * @return true if successful
     */
    bool requestToken(const std::unordered_map<std::string, std::string>& params);

    /**
     * @brief Store token (call storage callback)
     */
    void storeToken();

    /**
     * @brief Load token (call load callback)
     */
    void loadToken();

    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    OAuth2Config config_;
    OAuth2Token token_;
    std::string last_error_;

    // Token storage callbacks
    TokenStorageCallback store_callback_;
    TokenLoadCallback load_callback_;

    // HTTP client for token requests
    std::unique_ptr<HttpClient> http_client_;

    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace wtl::clients
