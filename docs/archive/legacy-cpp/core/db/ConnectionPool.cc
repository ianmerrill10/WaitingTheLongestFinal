/**
 * @file ConnectionPool.cc
 * @brief Implementation of PostgreSQL connection pool
 * @see ConnectionPool.h for documentation
 */

#include "core/db/ConnectionPool.h"
#include "core/utils/Config.h"

// Standard library includes
#include <iostream>
#include <thread>

namespace wtl::core::db {

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

ConnectionPool& ConnectionPool::getInstance() {
    // Meyer's singleton - thread-safe in C++11 and later
    static ConnectionPool instance;
    return instance;
}

ConnectionPool::~ConnectionPool() {
    // Ensure pool is shutdown properly
    if (initialized_.load()) {
        shutdown();
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool ConnectionPool::initialize(const std::string& connection_string,
                                 int pool_size,
                                 int connection_timeout_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already initialized
    if (initialized_.load()) {
        std::cerr << "[ConnectionPool] Warning: Pool already initialized"
                  << std::endl;
        return true;
    }

    // Validate parameters
    if (connection_string.empty()) {
        std::cerr << "[ConnectionPool] Error: Empty connection string"
                  << std::endl;
        return false;
    }

    if (pool_size < 1 || pool_size > 100) {
        std::cerr << "[ConnectionPool] Error: Invalid pool size: " << pool_size
                  << " (must be 1-100)" << std::endl;
        return false;
    }

    // Store configuration
    connection_string_ = connection_string;
    pool_size_ = pool_size;
    connection_timeout_seconds_ = connection_timeout_seconds;

    std::cout << "[ConnectionPool] Initializing with pool_size=" << pool_size_
              << ", timeout=" << connection_timeout_seconds_ << "s"
              << std::endl;

    // Create connections
    int created = 0;
    int failed = 0;

    for (int i = 0; i < pool_size_; ++i) {
        try {
            auto conn = createConnection();
            if (conn) {
                available_connections_.push(conn);
                ++created;
            } else {
                ++failed;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ConnectionPool] Failed to create connection " << i
                      << ": " << e.what() << std::endl;
            ++failed;
        }
    }

    // Check if we created at least one connection
    if (created == 0) {
        std::cerr << "[ConnectionPool] Error: Failed to create any connections"
                  << std::endl;
        return false;
    }

    if (failed > 0) {
        std::cerr << "[ConnectionPool] Warning: Failed to create " << failed
                  << " of " << pool_size_ << " connections" << std::endl;
    }

    initialized_.store(true);
    shutting_down_.store(false);

    std::cout << "[ConnectionPool] Initialized with " << created
              << " connections" << std::endl;

    return true;
}

bool ConnectionPool::initializeFromConfig() {
    auto& config = wtl::core::utils::Config::getInstance();

    if (!config.isLoaded()) {
        std::cerr << "[ConnectionPool] Error: Configuration not loaded"
                  << std::endl;
        return false;
    }

    std::string conn_str = config.getDatabaseConnectionString();
    int pool_size = config.getInt("database.pool_size", 10);
    int timeout = config.getInt("database.connection_timeout_seconds", 30);

    return initialize(conn_str, pool_size, timeout);
}

bool ConnectionPool::isInitialized() const {
    return initialized_.load();
}

void ConnectionPool::shutdown() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!initialized_.load()) {
        return;
    }

    std::cout << "[ConnectionPool] Shutting down..." << std::endl;

    // Signal shutdown
    shutting_down_.store(true);

    // Wake up any waiting threads
    condition_.notify_all();

    // Wait for active connections to be released (with timeout)
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(connection_timeout_seconds_);

    while (!active_connections_.empty()) {
        if (condition_.wait_until(lock, deadline) == std::cv_status::timeout) {
            std::cerr << "[ConnectionPool] Warning: Shutdown timeout with "
                      << active_connections_.size()
                      << " connections still active" << std::endl;
            break;
        }
    }

    // Close all available connections
    while (!available_connections_.empty()) {
        available_connections_.pop();
    }

    // Clear any remaining active connections
    active_connections_.clear();

    initialized_.store(false);

    std::cout << "[ConnectionPool] Shutdown complete" << std::endl;
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

ConnectionPool::ConnectionPtr ConnectionPool::acquire() {
    auto conn = acquireWithTimeout(connection_timeout_seconds_);

    if (!conn) {
        ++total_timeouts_;
        throw std::runtime_error(
            "[ConnectionPool] Timeout waiting for database connection");
    }

    return conn;
}

ConnectionPool::ConnectionPtr ConnectionPool::acquireWithTimeout(
    int timeout_seconds) {

    auto start_time = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(mutex_);

    // Check if pool is initialized
    if (!initialized_.load() || shutting_down_.load()) {
        std::cerr << "[ConnectionPool] Error: Pool not initialized or "
                  << "shutting down" << std::endl;
        return nullptr;
    }

    // Increment waiting count for statistics
    ++waiting_count_;

    // Wait for an available connection
    auto deadline = start_time + std::chrono::seconds(timeout_seconds);

    while (available_connections_.empty()) {
        if (shutting_down_.load()) {
            --waiting_count_;
            return nullptr;
        }

        if (condition_.wait_until(lock, deadline) == std::cv_status::timeout) {
            --waiting_count_;
            return nullptr;
        }
    }

    // Get a connection from the pool
    ConnectionPtr conn = available_connections_.front();
    available_connections_.pop();

    // Verify connection is still valid
    if (!isConnectionValid(conn)) {
        std::cerr << "[ConnectionPool] Connection invalid, creating new one"
                  << std::endl;

        // Try to create a new connection
        conn = createConnection();
        if (!conn) {
            // Put a placeholder back to maintain pool size
            ++total_errors_;
            --waiting_count_;
            return nullptr;
        }
    }

    // Track as active
    active_connections_[conn.get()] = conn;
    ++total_acquired_;
    --waiting_count_;

    // Record wait time
    auto end_time = std::chrono::steady_clock::now();
    auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    recordWaitTime(wait_ms);

    return conn;
}

void ConnectionPool::release(ConnectionPtr conn) {
    if (!conn) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Remove from active connections
    auto it = active_connections_.find(conn.get());
    if (it != active_connections_.end()) {
        active_connections_.erase(it);
    }

    // If shutting down, don't return to pool
    if (shutting_down_.load()) {
        condition_.notify_all();
        return;
    }

    // Check if connection is still valid
    if (isConnectionValid(conn)) {
        // Return to pool
        available_connections_.push(conn);
    } else {
        // Create a replacement connection
        std::cerr << "[ConnectionPool] Connection broken, creating replacement"
                  << std::endl;
        auto new_conn = createConnection();
        if (new_conn) {
            available_connections_.push(new_conn);
        } else {
            ++total_errors_;
        }
    }

    ++total_released_;

    // Notify waiting threads
    condition_.notify_one();
}

// ============================================================================
// POOL STATISTICS
// ============================================================================

Json::Value ConnectionPool::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value stats;

    stats["pool_size"] = pool_size_;
    stats["available"] = static_cast<int>(available_connections_.size());
    stats["active"] = static_cast<int>(active_connections_.size());
    stats["waiting"] = static_cast<Json::UInt64>(waiting_count_.load());
    stats["total_acquired"] = static_cast<Json::UInt64>(total_acquired_.load());
    stats["total_released"] = static_cast<Json::UInt64>(total_released_.load());
    stats["total_timeouts"] = static_cast<Json::UInt64>(total_timeouts_.load());
    stats["total_errors"] = static_cast<Json::UInt64>(total_errors_.load());

    // Calculate average wait time
    uint64_t acquired = total_acquired_.load();
    uint64_t total_wait = total_wait_time_ms_.load();
    if (acquired > 0) {
        stats["avg_wait_time_ms"] = static_cast<double>(total_wait) /
                                    static_cast<double>(acquired);
    } else {
        stats["avg_wait_time_ms"] = 0.0;
    }

    stats["initialized"] = initialized_.load();
    stats["shutting_down"] = shutting_down_.load();

    return stats;
}

size_t ConnectionPool::getAvailableCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_connections_.size();
}

size_t ConnectionPool::getActiveCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_connections_.size();
}

size_t ConnectionPool::getPoolSize() const {
    return static_cast<size_t>(pool_size_);
}

// ============================================================================
// SELF-HEALING
// ============================================================================

int ConnectionPool::resetAll() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!initialized_.load()) {
        return 0;
    }

    std::cout << "[ConnectionPool] Resetting all connections..." << std::endl;

    // Wait for all active connections to be released
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(connection_timeout_seconds_);

    while (!active_connections_.empty()) {
        if (condition_.wait_until(lock, deadline) == std::cv_status::timeout) {
            std::cerr << "[ConnectionPool] Reset timeout: "
                      << active_connections_.size()
                      << " connections still active" << std::endl;
            break;
        }
    }

    // Close all available connections
    int reset_count = 0;
    while (!available_connections_.empty()) {
        available_connections_.pop();
        ++reset_count;
    }

    // Create new connections
    int created = 0;
    for (int i = 0; i < pool_size_; ++i) {
        try {
            auto conn = createConnection();
            if (conn) {
                available_connections_.push(conn);
                ++created;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ConnectionPool] Failed to create connection during "
                      << "reset: " << e.what() << std::endl;
        }
    }

    std::cout << "[ConnectionPool] Reset complete. Closed " << reset_count
              << ", created " << created << " connections" << std::endl;

    // Wake up any waiting threads
    condition_.notify_all();

    return reset_count;
}

bool ConnectionPool::testConnectivity() {
    try {
        auto conn = acquireWithTimeout(5);  // Short timeout for test
        if (!conn) {
            return false;
        }

        // Run a simple query
        pqxx::nontransaction txn(*conn);
        auto result = txn.exec("SELECT 1");

        release(conn);

        return result.size() == 1;
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionPool] Connectivity test failed: "
                  << e.what() << std::endl;
        return false;
    }
}

bool ConnectionPool::refreshConnection(ConnectionPtr& conn) {
    if (!conn) {
        return false;
    }

    try {
        // Create a new connection
        auto new_conn = createConnection();
        if (!new_conn) {
            return false;
        }

        // Replace the old connection
        conn = new_conn;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionPool] Failed to refresh connection: "
                  << e.what() << std::endl;
        ++total_errors_;
        return false;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

ConnectionPool::ConnectionPtr ConnectionPool::createConnection() {
    try {
        auto conn = std::make_shared<pqxx::connection>(connection_string_);

        if (conn->is_open()) {
            return conn;
        } else {
            std::cerr << "[ConnectionPool] Connection created but not open"
                      << std::endl;
            return nullptr;
        }
    } catch (const pqxx::broken_connection& e) {
        std::cerr << "[ConnectionPool] Broken connection: " << e.what()
                  << std::endl;
        return nullptr;
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionPool] Failed to create connection: "
                  << e.what() << std::endl;
        return nullptr;
    }
}

bool ConnectionPool::isConnectionValid(const ConnectionPtr& conn) const {
    if (!conn) {
        return false;
    }

    try {
        return conn->is_open();
    } catch (...) {
        return false;
    }
}

void ConnectionPool::recordWaitTime(long long wait_time_ms) {
    // Use relaxed memory ordering since this is just for statistics
    total_wait_time_ms_.fetch_add(static_cast<uint64_t>(wait_time_ms),
                                   std::memory_order_relaxed);
}

// ============================================================================
// SCOPED CONNECTION IMPLEMENTATION
// ============================================================================

ScopedConnection::ScopedConnection(ConnectionPool& pool)
    : pool_(&pool)
    , connection_(pool.acquire()) {
}

ScopedConnection::~ScopedConnection() {
    if (pool_ && connection_) {
        pool_->release(connection_);
    }
}

ScopedConnection::ScopedConnection(ScopedConnection&& other) noexcept
    : pool_(other.pool_)
    , connection_(std::move(other.connection_)) {
    other.pool_ = nullptr;
}

ScopedConnection& ScopedConnection::operator=(ScopedConnection&& other) noexcept {
    if (this != &other) {
        // Release current connection if any
        if (pool_ && connection_) {
            pool_->release(connection_);
        }

        // Take ownership from other
        pool_ = other.pool_;
        connection_ = std::move(other.connection_);
        other.pool_ = nullptr;
    }
    return *this;
}

pqxx::connection& ScopedConnection::operator*() {
    if (!connection_) {
        throw std::runtime_error("ScopedConnection: No connection available");
    }
    return *connection_;
}

pqxx::connection* ScopedConnection::operator->() {
    if (!connection_) {
        throw std::runtime_error("ScopedConnection: No connection available");
    }
    return connection_.get();
}

ConnectionPool::ConnectionPtr ScopedConnection::get() const {
    return connection_;
}

ScopedConnection::operator bool() const {
    return connection_ != nullptr && connection_->is_open();
}

} // namespace wtl::core::db
