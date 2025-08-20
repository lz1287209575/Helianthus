#pragma once

#include <chrono>
#include <memory>

#include "../Common/Types.h"
#include "DatabaseTypes.h"
#include "IDatabase.h"

namespace Helianthus::Database
{
// Connection pool configuration
struct ConnectionPoolConfig
{
    uint32_t MinConnections = 5;
    uint32_t MaxConnections = 100;
    uint32_t ConnectionTimeoutMs = 30000;
    uint32_t IdleTimeoutMs = 300000;        // 5 minutes
    uint32_t ValidationIntervalMs = 60000;  // 1 minute
    bool TestOnBorrow = true;
    bool TestOnReturn = false;
    bool TestWhileIdle = true;
    uint32_t MaxRetries = 3;
    uint32_t RetryDelayMs = 1000;
};

// Connection pool statistics
struct ConnectionPoolStats
{
    uint32_t ActiveConnections = 0;
    uint32_t IdleConnections = 0;
    uint32_t TotalConnections = 0;
    uint32_t MaxConnections = 0;
    uint64_t TotalBorrowedConnections = 0;
    uint64_t TotalReturnedConnections = 0;
    uint64_t TotalCreatedConnections = 0;
    uint64_t TotalDestroyedConnections = 0;
    uint64_t TotalFailedConnections = 0;
    Common::TimestampMs LastValidationTime = 0;
};

// Connection pool interface
class IConnectionPool
{
public:
    virtual ~IConnectionPool() = default;

    // Pool lifecycle
    virtual Common::ResultCode Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // Connection management
    virtual std::shared_ptr<IConnection> BorrowConnection() = 0;
    virtual std::shared_ptr<IConnection> BorrowConnection(std::chrono::milliseconds TIMEOUT) = 0;
    virtual void ReturnConnection(std::shared_ptr<IConnection> CONNECTION) = 0;
    virtual void InvalidateConnection(std::shared_ptr<IConnection> CONNECTION) = 0;

    // Pool management
    virtual Common::ResultCode ValidatePool() = 0;
    virtual void ClearPool() = 0;
    virtual void ClearIdleConnections() = 0;

    // Pool information
    virtual ConnectionPoolStats GetStats() const = 0;
    virtual ConnectionPoolConfig GetConfig() const = 0;
    virtual void UpdateConfig(const ConnectionPoolConfig& CONFIG) = 0;

    // Health monitoring
    virtual bool IsHealthy() const = 0;
    virtual Common::ResultCode TestPool() = 0;
};

// Connection wrapper for pool management
class PooledConnection
{
public:
    virtual ~PooledConnection() = default;

    // Connection access
    virtual std::shared_ptr<IConnection> GetConnection() const = 0;
    virtual bool IsValid() const = 0;
    virtual Common::TimestampMs GetCreationTime() const = 0;
    virtual Common::TimestampMs GetLastUsedTime() const = 0;
    virtual uint64_t GetUsageCount() const = 0;

    // Pool management
    virtual void MarkAsUsed() = 0;
    virtual void MarkAsReturned() = 0;
    virtual bool IsIdle() const = 0;
    virtual bool IsExpired(uint32_t IDLE_TIMEOUT_MS) const = 0;
};

// Connection pool factory
class IConnectionPoolFactory
{
public:
    virtual ~IConnectionPoolFactory() = default;

    // Factory methods
    virtual std::shared_ptr<IConnectionPool>
    CreateMySqlPool(const MySqlConfig& DB_CONFIG, const ConnectionPoolConfig& POOL_CONFIG) = 0;

    virtual std::shared_ptr<IConnectionPool>
    CreateMongoDbPool(const MongoDbConfig& DB_CONFIG, const ConnectionPoolConfig& POOL_CONFIG) = 0;

    virtual std::shared_ptr<IConnectionPool>
    CreateRedisPool(const RedisConfig& DB_CONFIG, const ConnectionPoolConfig& POOL_CONFIG) = 0;
};

}  // namespace Helianthus::Database