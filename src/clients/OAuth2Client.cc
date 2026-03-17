/**
 * @file OAuth2Client.cc
 * @brief Implementation of OAuth2 authentication client
 * @see OAuth2Client.h for documentation
 */

#include "clients/OAuth2Client.h"
#include "clients/HttpClient.h"

#include <sstream>

#include "core/debug/ErrorCapture.h"

namespace wtl::clients {

// ============================================================================
// CONSTRUCTION
// ============================================================================

OAuth2Client::OAuth2Client(const std::string& token_url,
                           const std::string& client_id,
                           const std::string& client_secret)
    : http_client_(std::make_unique<HttpClient>()) {
    config_.token_url = token_url;
    config_.client_id = client_id;
    config_.client_secret = client_secret;
    config_.grant_type = OAuth2GrantType::CLIENT_CREDENTIALS;
    config_.auto_refresh = true;
    config_.refresh_buffer_seconds = 300;
}

OAuth2Client::OAuth2Client(const OAuth2Config& config)
    : config_(config)
    , http_client_(std::make_unique<HttpClient>()) {
}

OAuth2Client::OAuth2Client()
    : http_client_(std::make_unique<HttpClient>()) {
}

OAuth2Client::~OAuth2Client() = default;

// ============================================================================
// CONFIGURATION
// ============================================================================

void OAuth2Client::configure(const OAuth2Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void OAuth2Client::setTokenUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.token_url = url;
}

void OAuth2Client::setCredentials(const std::string& client_id,
                                   const std::string& client_secret) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.client_id = client_id;
    config_.client_secret = client_secret;
}

void OAuth2Client::setScope(const std::string& scope) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.scope = scope;
}

void OAuth2Client::setGrantType(OAuth2GrantType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.grant_type = type;
}

void OAuth2Client::setAutoRefresh(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.auto_refresh = enabled;
}

void OAuth2Client::setTokenStorage(TokenStorageCallback store, TokenLoadCallback load) {
    std::lock_guard<std::mutex> lock(mutex_);
    store_callback_ = std::move(store);
    load_callback_ = std::move(load);

    // Try to load existing token
    if (load_callback_) {
        auto loaded = load_callback_();
        if (loaded) {
            token_ = *loaded;
        }
    }
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

bool OAuth2Client::authenticate() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.token_url.empty() || config_.client_id.empty()) {
        last_error_ = "Missing token URL or client ID";
        return false;
    }

    std::unordered_map<std::string, std::string> params;
    params["grant_type"] = grantTypeToString(config_.grant_type);
    params["client_id"] = config_.client_id;
    params["client_secret"] = config_.client_secret;

    if (!config_.scope.empty()) {
        params["scope"] = config_.scope;
    }

    return requestToken(params);
}

bool OAuth2Client::authenticateWithCode(const std::string& code) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.token_url.empty() || config_.client_id.empty() || code.empty()) {
        last_error_ = "Missing token URL, client ID, or authorization code";
        return false;
    }

    std::unordered_map<std::string, std::string> params;
    params["grant_type"] = "authorization_code";
    params["code"] = code;
    params["client_id"] = config_.client_id;
    params["client_secret"] = config_.client_secret;

    if (!config_.redirect_uri.empty()) {
        params["redirect_uri"] = config_.redirect_uri;
    }

    return requestToken(params);
}

bool OAuth2Client::refreshToken() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (token_.refresh_token.empty()) {
        last_error_ = "No refresh token available";
        return false;
    }

    std::unordered_map<std::string, std::string> params;
    params["grant_type"] = "refresh_token";
    params["refresh_token"] = token_.refresh_token;
    params["client_id"] = config_.client_id;
    params["client_secret"] = config_.client_secret;

    return requestToken(params);
}

std::string OAuth2Client::getAccessToken() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if token is valid
    if (!token_.isValid()) {
        // Try to refresh if we have a refresh token
        if (!token_.refresh_token.empty() && config_.auto_refresh) {
            // Temporarily unlock for the request
            mutex_.unlock();
            bool refreshed = refreshToken();
            mutex_.lock();

            if (!refreshed) {
                return "";
            }
        } else {
            return "";
        }
    }

    // Check if token is about to expire
    if (token_.isExpired(config_.refresh_buffer_seconds) &&
        !token_.refresh_token.empty() &&
        config_.auto_refresh) {
        // Try to refresh proactively
        mutex_.unlock();
        refreshToken();  // Ignore failure, return current token
        mutex_.lock();
    }

    return token_.access_token;
}

OAuth2Token OAuth2Client::getToken() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return token_;
}

std::string OAuth2Client::getAuthorizationHeader() {
    std::string token = getAccessToken();
    if (token.empty()) {
        return "";
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return token_.token_type + " " + token;
}

bool OAuth2Client::isAuthenticated() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return token_.isValid();
}

void OAuth2Client::clearToken() {
    std::lock_guard<std::mutex> lock(mutex_);
    token_ = OAuth2Token{};
}

// ============================================================================
// AUTHORIZATION CODE FLOW HELPERS
// ============================================================================

std::string OAuth2Client::buildAuthorizationUrl(const std::string& authorize_url,
                                                  const std::string& state) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream url;
    url << authorize_url;
    url << "?response_type=code";
    url << "&client_id=" << config_.client_id;

    if (!config_.redirect_uri.empty()) {
        url << "&redirect_uri=" << config_.redirect_uri;
    }

    if (!config_.scope.empty()) {
        url << "&scope=" << config_.scope;
    }

    if (!state.empty()) {
        url << "&state=" << state;
    }

    return url.str();
}

// ============================================================================
// STATUS
// ============================================================================

Json::Value OAuth2Client::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value status;
    status["is_authenticated"] = token_.isValid();
    status["has_refresh_token"] = !token_.refresh_token.empty();
    status["auto_refresh"] = config_.auto_refresh;

    if (!token_.access_token.empty()) {
        status["token_type"] = token_.token_type;
        status["scope"] = token_.scope;
        status["expires_in"] = token_.expires_in;
        status["seconds_remaining"] = token_.getSecondsRemaining();
        status["is_expired"] = token_.isExpired();
    }

    if (!last_error_.empty()) {
        status["last_error"] = last_error_;
    }

    return status;
}

std::string OAuth2Client::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool OAuth2Client::requestToken(const std::unordered_map<std::string, std::string>& params) {
    // Build form-encoded body
    std::ostringstream body;
    bool first = true;
    for (const auto& [key, value] : params) {
        if (!first) body << "&";
        first = false;
        body << key << "=" << value;  // Should URL encode, but OAuth servers handle this
    }

    // Create temporary client for the request
    HttpClient client(config_.token_url);
    client.setTimeout(30);

    try {
        // Prepare headers
        HttpClient::Headers headers;
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["Accept"] = "application/json";

        // Make POST request
        auto response = client.postForm("", params, headers);

        if (!response.isOk()) {
            last_error_ = "Token request failed: HTTP " +
                          std::to_string(response.status_code);

            // Try to extract error from response body
            auto json = response.json();
            if (json.isMember("error")) {
                last_error_ += " - " + json["error"].asString();
                if (json.isMember("error_description")) {
                    last_error_ += ": " + json["error_description"].asString();
                }
            }

            WTL_CAPTURE_ERROR(
                wtl::core::debug::ErrorCategory::AUTHENTICATION,
                "OAuth2 token request failed",
                {{"token_url", config_.token_url},
                 {"status_code", std::to_string(response.status_code)},
                 {"error", last_error_}}
            );

            return false;
        }

        // Parse token response
        auto json = response.json();
        if (json.isNull() || !json.isMember("access_token")) {
            last_error_ = "Invalid token response: missing access_token";
            return false;
        }

        token_ = OAuth2Token::fromJson(json);

        // Store token if callback is set
        storeToken();

        last_error_.clear();
        return true;

    } catch (const std::exception& e) {
        last_error_ = std::string("Token request exception: ") + e.what();

        std::unordered_map<std::string, std::string> meta{{"token_url", config_.token_url}};
        WTL_CAPTURE_EXCEPTION(e, wtl::core::debug::ErrorCategory::AUTHENTICATION, meta);

        return false;
    }
}

void OAuth2Client::storeToken() {
    if (store_callback_ && token_.isValid()) {
        try {
            store_callback_(token_);
        } catch (const std::exception& e) {
            WTL_CAPTURE_WARNING(
                wtl::core::debug::ErrorCategory::AUTHENTICATION,
                "Failed to store OAuth2 token: " + std::string(e.what()),
                {}
            );
        }
    }
}

void OAuth2Client::loadToken() {
    if (load_callback_) {
        try {
            auto loaded = load_callback_();
            if (loaded) {
                token_ = *loaded;
            }
        } catch (const std::exception& e) {
            WTL_CAPTURE_WARNING(
                wtl::core::debug::ErrorCategory::AUTHENTICATION,
                "Failed to load OAuth2 token: " + std::string(e.what()),
                {}
            );
        }
    }
}

} // namespace wtl::clients
