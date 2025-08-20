#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include "../../Common/Logger.h"
#include "../DatabaseTypes.h"
#include "../IDatabase.h"
#include "hiredis.h"

namespace Helianthus::Database::Redis
{
// Redis-specific data types
enum class RedisDataType : uint8_t
{
    STRING = 0,
    HASH = 1,
    LIST = 2,
    SET = 3,
    SORTED_SET = 4,
    STREAM = 5
};

// Redis command result
struct RedisResult
{
    Common::ResultCode Code = Common::ResultCode::SUCCESS;
    std::string ErrorMessage;
    std::vector<std::string> Values;
    std::map<std::string, std::string> HashValues;
    bool BoolValue = false;
    int64_t IntValue = 0;
    double DoubleValue = 0.0;

    bool IsSuccess() const
    {
        return Code == Common::ResultCode::SUCCESS;
    }
    bool HasValues() const
    {
        return !Values.empty();
    }
    bool HasHashValues() const
    {
        return !HashValues.empty();
    }
};

// Redis connection implementation
class RedisConnection : public IConnection, public std::enable_shared_from_this<RedisConnection>
{
public:
    explicit RedisConnection(const RedisConfig& Config);
    virtual ~RedisConnection();

    // IConnection interface
    Common::ResultCode Connect() override;
    void Disconnect() override;
    bool IsConnected() const override;

    DatabaseResult ExecuteQuery(const std::string& Query,
                                const ParameterMap& Parameters = {}) override;
    DatabaseResult ExecuteStoredProcedure(const std::string& ProcedureName,
                                          const ParameterMap& Parameters = {}) override;

    std::shared_ptr<ITransaction>
    BeginTransaction(IsolationLevel Level = IsolationLevel::READ_COMMITTED) override;

    ConnectionInfo GetConnectionInfo() const override;
    Common::TimestampMs GetLastActiveTime() const override;
    void UpdateLastActiveTime() override;

    std::string EscapeString(const std::string& Input) const override;
    bool Ping() override;

    // Redis-specific methods
    redisContext* GetRedisHandle() const
    {
        return RedisContext;
    }
    const RedisConfig& GetConfig() const
    {
        return Config;
    }

    // String operations
    RedisResult Set(const std::string& Key, const std::string& Value, uint32_t ExpireSeconds = 0);
    RedisResult Get(const std::string& Key);
    RedisResult Delete(const std::string& Key);
    RedisResult Exists(const std::string& Key);
    RedisResult Expire(const std::string& Key, uint32_t Seconds);

    // Hash operations
    RedisResult HashSet(const std::string& Key, const std::string& Field, const std::string& Value);
    RedisResult HashGet(const std::string& Key, const std::string& Field);
    RedisResult HashGetAll(const std::string& Key);
    RedisResult HashDelete(const std::string& Key, const std::string& Field);
    RedisResult HashExists(const std::string& Key, const std::string& Field);

    // List operations
    RedisResult ListPush(const std::string& Key, const std::string& Value, bool PushLeft = true);
    RedisResult ListPop(const std::string& Key, bool PopLeft = true);
    RedisResult ListGet(const std::string& Key, int32_t Index);
    RedisResult ListRange(const std::string& Key, int32_t Start, int32_t Stop);
    RedisResult ListLength(const std::string& Key);

    // Set operations
    RedisResult SetAdd(const std::string& Key, const std::string& Member);
    RedisResult SetRemove(const std::string& Key, const std::string& Member);
    RedisResult SetMembers(const std::string& Key);
    RedisResult SetIsMember(const std::string& Key, const std::string& Member);
    RedisResult SetCount(const std::string& Key);

    // Sorted set operations
    RedisResult SortedSetAdd(const std::string& Key, double Score, const std::string& Member);
    RedisResult SortedSetRemove(const std::string& Key, const std::string& Member);
    RedisResult
    SortedSetRange(const std::string& Key, int32_t Start, int32_t Stop, bool WithScores = false);
    RedisResult SortedSetScore(const std::string& Key, const std::string& Member);
    RedisResult SortedSetCount(const std::string& Key);

    // Atomic operations
    RedisResult Increment(const std::string& Key, int64_t Delta = 1);
    RedisResult Decrement(const std::string& Key, int64_t Delta = 1);

    // Batch operations
    RedisResult ExecutePipeline(const std::vector<std::string>& Commands);

private:
    RedisConfig Config;
    redisContext* RedisContext;
    mutable std::mutex ConnectionMutex;

    // Connection state
    bool IsConnectedFlag;
    Common::TimestampMs LastActiveTime;
    uint64_t QueryCount;
    uint64_t ErrorCount;

    // Helper methods
    RedisResult ExecuteCommand(const std::string& Command);
    RedisResult ExecuteCommand(const char* Format, ...);
    RedisResult ProcessReply(redisReply* Reply);
    DatabaseResult ConvertToDbResult(const RedisResult& RedisRes);
    std::string GetRedisError() const;
    void CleanupConnection();
    std::string BuildConnectionString() const;

public:
    // Public command execution for internal use
    RedisResult ExecuteCommandInternal(const std::string& Command);
    RedisResult ExecuteCommandInternal(const char* Format, ...);
};

// Redis transaction implementation (multi/exec)
class RedisTransaction : public ITransaction
{
public:
    explicit RedisTransaction(std::shared_ptr<RedisConnection> Connection);
    virtual ~RedisTransaction();

    // ITransaction interface
    Common::ResultCode Begin() override;
    Common::ResultCode Commit() override;
    Common::ResultCode Rollback() override;
    bool IsActive() const override;

    DatabaseResult ExecuteQuery(const std::string& Query,
                                const ParameterMap& Parameters = {}) override;
    DatabaseResult ExecuteStoredProcedure(const std::string& ProcedureName,
                                          const ParameterMap& Parameters = {}) override;

    IsolationLevel GetIsolationLevel() const override
    {
        return IsolationLevel::READ_COMMITTED;
    }
    std::shared_ptr<IConnection> GetConnection() const override
    {
        return Connection;
    }

    // Redis-specific transaction methods
    Common::ResultCode AddCommand(const std::string& Command);
    Common::ResultCode Watch(const std::string& Key);
    Common::ResultCode Unwatch();

private:
    std::shared_ptr<RedisConnection> Connection;
    bool IsActiveFlag;
    std::vector<std::string> QueuedCommands;
    mutable std::mutex TransactionMutex;
};

}  // namespace Helianthus::Database::Redis