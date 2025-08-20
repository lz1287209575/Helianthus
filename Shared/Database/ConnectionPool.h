#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "../Common/Logger.h"
#include "IConnectionPool.h"
#include "IDatabase.h"

namespace Helianthus::Database
{
// Pooled connection wrapper implementation
class PooledConnectionImpl : public PooledConnection
{
public:
    explicit PooledConnectionImpl(std::shared_ptr<IConnection> Connection);
    virtual ~PooledConnectionImpl() = default;

    // PooledConnection interface
    std::shared_ptr<IConnection> GetConnection() const override;
    bool IsValid() const override;
    Common::TimestampMs GetCreationTime() const override;
    Common::TimestampMs GetLastUsedTime() const override;
    uint64_t GetUsageCount() const override;

    void MarkAsUsed() override;
    void MarkAsReturned() override;
    bool IsIdle() const override;
    bool IsExpired(uint32_t IdleTimeoutMs) const override;

private:
    std::shared_ptr<IConnection> Connection;
    Common::TimestampMs CreationTime;
    Common::TimestampMs LastUsedTime;
    uint64_t UsageCount;
    bool IsInUse;
    mutable std::mutex Mutex;

    Common::TimestampMs GetCurrentTimeMs() const;
};

// Generic connection pool implementation
template <typename ConnectionType>
class ConnectionPoolImpl : public IConnectionPool
{
public:
    using ConnectionFactory = std::function<std::shared_ptr<ConnectionType>()>;

    explicit ConnectionPoolImpl(ConnectionFactory Factory, const ConnectionPoolConfig& Config);
    virtual ~ConnectionPoolImpl();

    // IConnectionPool interface
    Common::ResultCode Initialize() override;
    void Shutdown() override;
    bool IsInitialized() const override;

    std::shared_ptr<IConnection> BorrowConnection() override;
    std::shared_ptr<IConnection> BorrowConnection(std::chrono::milliseconds Timeout) override;
    void ReturnConnection(std::shared_ptr<IConnection> Connection) override;
    void InvalidateConnection(std::shared_ptr<IConnection> Connection) override;

    Common::ResultCode ValidatePool() override;
    void ClearPool() override;
    void ClearIdleConnections() override;

    ConnectionPoolStats GetStats() const override;
    ConnectionPoolConfig GetConfig() const override;
    void UpdateConfig(const ConnectionPoolConfig& Config) override;

    bool IsHealthy() const override;
    Common::ResultCode TestPool() override;

private:
    // Configuration and factory
    ConnectionFactory Factory;
    ConnectionPoolConfig Config;

    // Pool state
    std::atomic<bool> IsInitializedFlag;
    std::atomic<bool> ShutdownRequested;

    // Connection management
    mutable std::mutex PoolMutex;
    std::condition_variable PoolCondition;
    std::queue<std::shared_ptr<PooledConnectionImpl>> AvailableConnections;
    std::vector<std::shared_ptr<PooledConnectionImpl>> AllConnections;

    // Statistics
    mutable std::mutex StatsMutex;
    ConnectionPoolStats Stats;

    // Background maintenance
    std::thread MaintenanceThread;
    std::mutex MaintenanceMutex;
    std::condition_variable MaintenanceCondition;

    // Helper methods
    std::shared_ptr<PooledConnectionImpl> CreateConnection();
    void RemoveConnection(std::shared_ptr<PooledConnectionImpl> PooledConn);
    bool ValidateConnection(std::shared_ptr<PooledConnectionImpl> PooledConn);
    void MaintenanceWorker();
    void UpdateStats();
    bool ShouldCreateConnection() const;
    void CleanupExpiredConnections();
};

// MySQL connection pool specialization
class MySqlConnectionPool : public ConnectionPoolImpl<IConnection>
{
public:
    MySqlConnectionPool(const MySqlConfig& DbConfig, const ConnectionPoolConfig& PoolConfig);
    virtual ~MySqlConnectionPool() = default;

private:
    MySqlConfig DbConfig;
    std::shared_ptr<IConnection> CreateMySqlConnection();
};

}  // namespace Helianthus::Database