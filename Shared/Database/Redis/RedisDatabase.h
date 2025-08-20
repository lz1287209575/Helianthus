#pragma once

#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>

#include "../IConnectionPool.h"
#include "../IDatabase.h"
#include "RedisConnection.h"

namespace Helianthus::Database::Redis
{
// Redis database implementation with connection pooling
class RedisDatabase : public IDatabase
{
public:
    explicit RedisDatabase(const RedisConfig& Config);
    virtual ~RedisDatabase();

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
        return DatabaseType::REDIS;
    }
    ConnectionInfo GetConnectionInfo() const override;
    std::string GetDatabaseVersion() const override;

    bool IsHealthy() const override;
    Common::ResultCode TestConnection() override;

    std::string EscapeString(const std::string& Input) const override;
    std::string BuildConnectionString() const override;

    // Redis-specific convenience methods
    RedisResult Set(const std::string& Key, const std::string& Value, uint32_t ExpireSeconds = 0);
    RedisResult Get(const std::string& Key);
    RedisResult Delete(const std::string& Key);
    RedisResult HashSet(const std::string& Key, const std::string& Field, const std::string& Value);
    RedisResult HashGet(const std::string& Key, const std::string& Field);
    RedisResult HashGetAll(const std::string& Key);
    RedisResult ListPush(const std::string& Key, const std::string& Value, bool PushLeft = true);
    RedisResult ListPop(const std::string& Key, bool PopLeft = true);
    RedisResult SetAdd(const std::string& Key, const std::string& Member);
    RedisResult SetMembers(const std::string& Key);
    RedisResult Increment(const std::string& Key, int64_t Delta = 1);

    // Cache-specific methods
    Common::ResultCode
    SetCache(const std::string& Key, const std::string& Value, uint32_t ExpireSeconds = 0);
    std::string GetCache(const std::string& Key, bool& Found);
    Common::ResultCode DeleteCache(const std::string& Key);
    Common::ResultCode SetCacheHash(const std::string& Key,
                                    const std::map<std::string, std::string>& HashData,
                                    uint32_t ExpireSeconds = 0);
    std::map<std::string, std::string> GetCacheHash(const std::string& Key, bool& Found);

    // Session management
    Common::ResultCode SetSession(const std::string& SessionId,
                                  const std::string& Data,
                                  uint32_t ExpireSeconds = 3600);
    std::string GetSession(const std::string& SessionId, bool& Found);
    Common::ResultCode DeleteSession(const std::string& SessionId);
    Common::ResultCode ExtendSession(const std::string& SessionId, uint32_t ExpireSeconds = 3600);

    // Counter operations
    int64_t IncrementCounter(const std::string& CounterName, int64_t Delta = 1);
    int64_t GetCounter(const std::string& CounterName);
    Common::ResultCode ResetCounter(const std::string& CounterName);

    // Pub/Sub operations (basic implementation)
    Common::ResultCode Publish(const std::string& Channel, const std::string& Message);
    Common::ResultCode Subscribe(const std::string& Channel,
                                 std::function<void(const std::string&)> MessageHandler);
    Common::ResultCode Unsubscribe(const std::string& Channel);

private:
    // Configuration
    RedisConfig Config;

    // Connection pool management
    mutable std::mutex PoolMutex;
    std::queue<std::shared_ptr<RedisConnection>> AvailableConnections;
    std::vector<std::shared_ptr<RedisConnection>> AllConnections;
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

    // Pub/Sub management
    std::map<std::string, std::function<void(const std::string&)>> SubscriptionHandlers;
    std::mutex SubscriptionMutex;
    std::thread SubscriptionThread;
    std::atomic<bool> SubscriptionActive;

    // Helper methods
    std::shared_ptr<RedisConnection> CreateConnection();
    void InitializeConnectionPool();
    void CleanupConnectionPool();
    void WorkerThreadFunction();
    void SubscriptionThreadFunction();
    void ScheduleTask(std::function<void()> Task);
    bool ValidateConnection(std::shared_ptr<RedisConnection> Connection);
};

}  // namespace Helianthus::Database::Redis