/**
 * @file ConnectionPool.h
 * @brief PostgreSQL connection pool for WaitingTheLongest application
 *
 * PURPOSE:
 * Manages a pool of PostgreSQL database connections for efficient
 * and thread-safe database access. Prevents connection exhaustion
 * and reduces connection overhead.
 *
 * USAGE:
 * // Acquire a connection
 * auto conn = ConnectionPool::getInstance().acquire();
 *
 * // Use the connection
 * pqxx::work txn(*conn);
 * auto result = txn.exec("SELECT * FROM dogs");
 * txn.commit();
 *
 * // Release when done (or let RAII handle it)
 * ConnectionPool::getInstance().release(conn);
 *
 * DEPENDENCIES:
 * - pqxx (PostgreSQL C++ client)
 * - Config (configuration loading)
 *
 * MODIFICATION GUIDE:
 * - Pool size is configurable via config.json
 * - Connection timeout and retry logic can be adjusted
 * - Add new pool statistics as needed in getStats()
 *
 * @author Agent 1 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>

// Third-party includes
#include <json/json.h>
#include <pqxx/pqxx>

namespace wtl::core::db {

/**
 * @class ConnectionPool
 * @brief Thread-safe singleton PostgreSQL connection pool
 *
 * Manages a fixed pool of database connections. Connections are
 * acquired and released by callers. When the pool is exhausted,
 * callers block until a connection becomes available (with timeout).
 */
class ConnectionPool {
public:
    // Type alias for connection pointer
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance of ConnectionPool
     * @return Reference to the ConnectionPool singleton
     *
     * Thread-safe initialization via Meyer's singleton pattern.
     * Must call initialize() before first use.
     */
    static ConnectionPool& getInstance();

    // Delete copy/move constructors and assignment operators
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the connection pool
     *
     * @param connection_string PostgreSQL connection string
     * @param pool_size Number of connections to create
     * @param connection_timeout_seconds Timeout for acquiring connections
     * @return true if initialization successful, false otherwise
     *
     * Creates the specified number of connections. Should be called
     * once at application startup after config is loaded.
     *
     * @example
     * auto conn_str = Config::getInstance().getDatabaseConnectionString();
     * int pool_size = Config::getInstance().getInt("database.pool_size", 10);
     * ConnectionPool::getInstance().initialize(conn_str, pool_size, 30);
     */
    bool initialize(const std::string& connection_string,
                    int pool_size = 10,
                    int connection_timeout_seconds = 30);

    /**
     * @brief Initialize the connection pool from Config
     *
     * @return true if initialization successful, false otherwise
     *
     * Reads all configuration from Config singleton.
     */
    bool initializeFromConfig();

    /**
     * @brief Check if pool has been initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const;

    /**
     * @brief Shutdown the connection pool
     *
     * Waits for all connections to be returned, then closes them.
     * Should be called before application exit.
     */
    void shutdown();

    // =========================================================================
    // CONNECTION MANAGEMENT
    // =========================================================================

    /**
     * @brief Acquire a connection from the pool
     *
     * @return Shared pointer to a pqxx connection
     * @throws std::runtime_error if pool not initialized or timeout exceeded
     *
     * Blocks if no connections are available. Times out after the
     * configured timeout period.
     *
     * @example
     * auto conn = ConnectionPool::getInstance().acquire();
     * pqxx::work txn(*conn);
     * // ... use connection ...
     * ConnectionPool::getInstance().release(conn);
     */
    ConnectionPtr acquire();

    /**
     * @brief Acquire a connection with custom timeout
     *
     * @param timeout_seconds Maximum seconds to wait for a connection
     * @return Shared pointer to a pqxx connection, or nullptr on timeout
     *
     * Returns nullptr instead of throwing on timeout.
     */
    ConnectionPtr acquireWithTimeout(int timeout_seconds);

    /**
     * @brief Release a connection back to the pool
     *
     * @param conn The connection to release
     *
     * Returns the connection to the pool for reuse.
     * If connection is broken, it will be replaced with a new one.
     */
    void release(ConnectionPtr conn);

    // =========================================================================
    // POOL STATISTICS
    // =========================================================================

    /**
     * @brief Get pool statistics
     *
     * @return JSON object with pool statistics
     *
     * Returns:
     * - pool_size: Total connections in pool
     * - active: Connections currently in use
     * - available: Connections available for use
     * - waiting: Threads waiting for a connection
     * - total_acquired: Total acquisitions since startup
     * - total_released: Total releases since startup
     * - total_timeouts: Total timeout errors
     * - total_errors: Total connection errors
     * - avg_wait_time_ms: Average wait time for connections
     */
    Json::Value getStats() const;

    /**
     * @brief Get number of available connections
     * @return Number of connections available in the pool
     */
    size_t getAvailableCount() const;

    /**
     * @brief Get number of active (in-use) connections
     * @return Number of connections currently in use
     */
    size_t getActiveCount() const;

    /**
     * @brief Get total pool size
     * @return Total number of connections in the pool
     */
    size_t getPoolSize() const;

    // =========================================================================
    // SELF-HEALING
    // =========================================================================

    /**
     * @brief Reset all connections in the pool
     *
     * @return Number of connections reset
     *
     * Closes all connections and creates new ones. Used for self-healing
     * when database connectivity issues are detected.
     *
     * WARNING: This will wait for all active connections to be released.
     */
    int resetAll();

    /**
     * @brief Test pool connectivity
     *
     * @return true if pool can connect to database, false otherwise
     *
     * Acquires a connection and runs a simple query to verify connectivity.
     */
    bool testConnectivity();

    /**
     * @brief Refresh a specific connection
     *
     * @param conn The connection to refresh
     * @return true if refresh successful, false otherwise
     *
     * Replaces a broken connection with a new one.
     */
    bool refreshConnection(ConnectionPtr& conn);

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    ConnectionPool() = default;
    ~ConnectionPool();

    /**
     * @brief Create a new database connection
     *
     * @return Shared pointer to new connection, or nullptr on failure
     */
    ConnectionPtr createConnection();

    /**
     * @brief Check if a connection is still valid
     *
     * @param conn The connection to check
     * @return true if connection is valid, false otherwise
     */
    bool isConnectionValid(const ConnectionPtr& conn) const;

    /**
     * @brief Record wait time for statistics
     *
     * @param wait_time_ms Wait time in milliseconds
     */
    void recordWaitTime(long long wait_time_ms);

    // Connection pool storage
    std::queue<ConnectionPtr> available_connections_;

    // Track active connections
    std::unordered_map<pqxx::connection*, ConnectionPtr> active_connections_;

    // Configuration
    std::string connection_string_;
    int pool_size_{10};
    int connection_timeout_seconds_{30};

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};

    // Statistics
    std::atomic<uint64_t> total_acquired_{0};
    std::atomic<uint64_t> total_released_{0};
    std::atomic<uint64_t> total_timeouts_{0};
    std::atomic<uint64_t> total_errors_{0};
    std::atomic<uint64_t> total_wait_time_ms_{0};
    std::atomic<uint64_t> waiting_count_{0};

    // Thread synchronization
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};

/**
 * @class ScopedConnection
 * @brief RAII wrapper for automatic connection release
 *
 * Automatically releases the connection back to the pool
 * when the object goes out of scope.
 *
 * @example
 * {
 *     ScopedConnection conn(ConnectionPool::getInstance());
 *     pqxx::work txn(*conn);
 *     // ... use connection ...
 * } // Connection automatically released here
 */
class ScopedConnection {
public:
    /**
     * @brief Construct a ScopedConnection
     *
     * @param pool Reference to the connection pool
     * @throws std::runtime_error if connection cannot be acquired
     */
    explicit ScopedConnection(ConnectionPool& pool);

    /**
     * @brief Destructor - releases connection back to pool
     */
    ~ScopedConnection();

    // Delete copy operations
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    // Allow move operations
    ScopedConnection(ScopedConnection&& other) noexcept;
    ScopedConnection& operator=(ScopedConnection&& other) noexcept;

    /**
     * @brief Get the underlying pqxx connection
     * @return Reference to the pqxx connection
     */
    pqxx::connection& operator*();

    /**
     * @brief Get pointer to the underlying pqxx connection
     * @return Pointer to the pqxx connection
     */
    pqxx::connection* operator->();

    /**
     * @brief Get the connection pointer
     * @return Shared pointer to the connection
     */
    ConnectionPool::ConnectionPtr get() const;

    /**
     * @brief Check if connection is valid
     * @return true if connection is valid
     */
    explicit operator bool() const;

private:
    ConnectionPool* pool_;
    ConnectionPool::ConnectionPtr connection_;
};

} // namespace wtl::core::db
