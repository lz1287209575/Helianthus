#pragma once

#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>

#include "IConnectionPool.h"
#include "IDatabase.h"
#include "MySqlConnection.h"

namespace Helianthus::Database::MySQL
{
// MySQL database implementation with connection pooling
class MySqlDatabase : public IDatabase
{
public:
    explicit MySqlDatabase(const MySqlConfig& Config);
    virtual ~MySqlDatabase();

    // IDatabase interface
    Common::ResultCode Initialize() override;
    void Shutdown() override;
    bool IsInitialized() const override;

    std::shared_ptr<IConnection> GetConnection() override;
    void ReturnConnection(std::shared_ptr<IConnection> Connection) override;
    uint32_t GetActiveConnectionCount() const override;
    uint32_t GetTotalConnectionCount() const override;

    DatabaseResult ExecuteQuery(const std::string& Query,
                                const ParameterMap& Parameters = {}) override;
    DatabaseResult ExecuteStoredProcedure(const std::string& ProcedureName,
                                          const ParameterMap& Parameters = {}) override;

    void ExecuteQueryAsync(const std::string& Query,
                           QueryCallback Callback,
                           const ParameterMap& Parameters = {}) override;
    void ExecuteStoredProcedureAsync(const std::string& ProcedureName,
                                     QueryCallback Callback,
                                     const ParameterMap& Parameters = {}) override;

    std::shared_ptr<ITransaction>
    BeginTransaction(IsolationLevel Level = IsolationLevel::READ_COMMITTED) override;

    DatabaseType GetDatabaseType() const override
    {
        return DatabaseType::MYSQL;
    }
    ConnectionInfo GetConnectionInfo() const override;
    std::string GetDatabaseVersion() const override;

    bool IsHealthy() const override;
    Common::ResultCode TestConnection() override;

    std::string EscapeString(const std::string& Input) const override;
    std::string BuildConnectionString() const override;

private:
    // Configuration
    MySqlConfig Config;

    // Connection pool management
    mutable std::mutex PoolMutex;
    std::queue<std::shared_ptr<MySqlConnection>> AvailableConnections;
    std::vector<std::shared_ptr<MySqlConnection>> AllConnections;
    uint32_t ActiveConnectionCount;

    // Async execution
    std::vector<std::thread> WorkerThreads;
    std::queue<std::function<void()>> TaskQueue;
    std::mutex TaskMutex;
    std::condition_variable TaskCondition;
    std::atomic<bool> ShutdownRequested;

    // State
    std::atomic<bool> IsInitializedFlag;
    mutable std::string CachedVersion;

    // Helper methods
    std::shared_ptr<MySqlConnection> CreateConnection();
    void InitializeConnectionPool();
    void CleanupConnectionPool();
    void WorkerThreadFunction();
    void ScheduleTask(std::function<void()> Task);
    bool ValidateConnection(std::shared_ptr<MySqlConnection> Connection);
};

}  // namespace Helianthus::Database::MySQL