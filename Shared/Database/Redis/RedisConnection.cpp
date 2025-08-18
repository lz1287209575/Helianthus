#include "RedisConnection.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cstdarg>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

namespace Helianthus::Database::Redis
{
    RedisConnection::RedisConnection(const RedisConfig& Config)
        : Config(Config)
        , RedisContext(nullptr)
        , IsConnectedFlag(false)
        , LastActiveTime(0)
        , QueryCount(0)
        , ErrorCount(0)
    {
    }

    RedisConnection::~RedisConnection()
    {
        Disconnect();
        CleanupConnection();
    }

    Common::ResultCode RedisConnection::Connect()
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);

        if (IsConnectedFlag)
        {
            return Common::ResultCode::ALREADY_INITIALIZED;
        }

        // Create connection with timeout
        struct timeval Timeout = { static_cast<time_t>(Config.ConnectionTimeout), 0 };
        RedisContext = redisConnectWithTimeout(Config.Host.c_str(), Config.Port, Timeout);

        if (!RedisContext || RedisContext->err)
        {
            ErrorCount++;
            std::string ErrorMsg = RedisContext ? RedisContext->errstr : "Failed to allocate redis context";
            HELIANTHUS_LOG_ERROR("Failed to connect to Redis: " + ErrorMsg);
            CleanupConnection();
            return Common::ResultCode::FAILED;
        }

        // Set password if provided
        if (!Config.Password.empty())
        {
            redisReply* Reply = static_cast<redisReply*>(redisCommand(RedisContext, "AUTH %s", Config.Password.c_str()));
            if (!Reply || Reply->type == REDIS_REPLY_ERROR)
            {
                ErrorCount++;
                std::string ErrorMsg = Reply ? Reply->str : "Authentication failed";
                HELIANTHUS_LOG_ERROR("Redis authentication failed: " + ErrorMsg);
                if (Reply) freeReplyObject(Reply);
                CleanupConnection();
                return Common::ResultCode::PERMISSION_DENIED;
            }
            freeReplyObject(Reply);
        }

        // Select database
        if (Config.Database != 0)
        {
            redisReply* Reply = static_cast<redisReply*>(redisCommand(RedisContext, "SELECT %u", Config.Database));
            if (!Reply || Reply->type == REDIS_REPLY_ERROR)
            {
                ErrorCount++;
                std::string ErrorMsg = Reply ? Reply->str : "Database selection failed";
                HELIANTHUS_LOG_ERROR("Redis database selection failed: " + ErrorMsg);
                if (Reply) freeReplyObject(Reply);
                CleanupConnection();
                return Common::ResultCode::FAILED;
            }
            freeReplyObject(Reply);
        }

        IsConnectedFlag = true;
        UpdateLastActiveTime();
        
        HELIANTHUS_LOG_INFO("Successfully connected to Redis: " + Config.Host + ":" + 
                           std::to_string(Config.Port) + "/" + std::to_string(Config.Database));
        
        return Common::ResultCode::SUCCESS;
    }

    void RedisConnection::Disconnect()
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);

        if (IsConnectedFlag && RedisContext)
        {
            redisFree(RedisContext);
            RedisContext = nullptr;
            IsConnectedFlag = false;
            HELIANTHUS_LOG_INFO("Disconnected from Redis database");
        }
    }

    bool RedisConnection::IsConnected() const
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        return IsConnectedFlag && RedisContext != nullptr;
    }

    DatabaseResult RedisConnection::ExecuteQuery(const std::string& Query, const ParameterMap& Parameters)
    {
        if (!IsConnected())
        {
            DatabaseResult Result;
            Result.Code = Common::ResultCode::NOT_INITIALIZED;
            Result.ErrorMessage = "Not connected to Redis";
            return Result;
        }

        UpdateLastActiveTime();
        QueryCount++;

        RedisResult RedisRes = ExecuteCommand(Query);
        return ConvertToDbResult(RedisRes);
    }

    DatabaseResult RedisConnection::ExecuteStoredProcedure(const std::string& ProcedureName, const ParameterMap& Parameters)
    {
        // Redis doesn't have stored procedures, but we can implement Lua scripts
        std::string Script = "return redis.call('" + ProcedureName + "'";
        
        for (const auto& [Key, Value] : Parameters)
        {
            Script += ", ";
            std::visit([&Script](const auto& V) {
                using T = std::decay_t<decltype(V)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>)
                {
                    Script += "null";
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    Script += "'" + V + "'";
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    Script += V ? "true" : "false";
                }
                else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
                {
                    Script += std::to_string(V);
                }
                else if constexpr (std::is_integral_v<T>)
                {
                    Script += std::to_string(V);
                }
                else
                {
                    Script += "null";
                }
            }, Value);
        }
        
        Script += ")";

        RedisResult RedisRes = ExecuteCommand("EVAL", Script.c_str(), "0");
        return ConvertToDbResult(RedisRes);
    }

    std::shared_ptr<ITransaction> RedisConnection::BeginTransaction(IsolationLevel Level)
    {
        if (!IsConnected())
        {
            return nullptr;
        }

        auto Transaction = std::make_shared<RedisTransaction>(
            std::static_pointer_cast<RedisConnection>(shared_from_this()));
        
        if (Transaction->Begin() == Common::ResultCode::SUCCESS)
        {
            return Transaction;
        }

        return nullptr;
    }

    ConnectionInfo RedisConnection::GetConnectionInfo() const
    {
        ConnectionInfo Info;
        Info.Type = DatabaseType::REDIS;
        Info.ConnectionString = BuildConnectionString();
        Info.IsConnected = IsConnected();
        Info.LastActiveTime = LastActiveTime;
        Info.QueryCount = QueryCount;
        Info.ErrorCount = ErrorCount;
        return Info;
    }

    Common::TimestampMs RedisConnection::GetLastActiveTime() const
    {
        return LastActiveTime;
    }

    void RedisConnection::UpdateLastActiveTime()
    {
        LastActiveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string RedisConnection::EscapeString(const std::string& Input) const
    {
        // Redis doesn't need SQL-style escaping, but we can escape special characters
        std::string Escaped = Input;
        std::replace(Escaped.begin(), Escaped.end(), '\n', ' ');
        std::replace(Escaped.begin(), Escaped.end(), '\r', ' ');
        return Escaped;
    }

    bool RedisConnection::Ping()
    {
        if (!IsConnected())
        {
            return false;
        }

        RedisResult Result = ExecuteCommand("PING");
        return Result.IsSuccess();
    }

    // Redis-specific operations
    RedisResult RedisConnection::Set(const std::string& Key, const std::string& Value, uint32_t ExpireSeconds)
    {
        if (ExpireSeconds > 0)
        {
            return ExecuteCommand("SETEX %s %u %s", Key.c_str(), ExpireSeconds, Value.c_str());
        }
        else
        {
            return ExecuteCommand("SET %s %s", Key.c_str(), Value.c_str());
        }
    }

    RedisResult RedisConnection::Get(const std::string& Key)
    {
        return ExecuteCommand("GET %s", Key.c_str());
    }

    RedisResult RedisConnection::Delete(const std::string& Key)
    {
        return ExecuteCommand("DEL %s", Key.c_str());
    }

    RedisResult RedisConnection::Exists(const std::string& Key)
    {
        return ExecuteCommand("EXISTS %s", Key.c_str());
    }

    RedisResult RedisConnection::Expire(const std::string& Key, uint32_t Seconds)
    {
        return ExecuteCommand("EXPIRE %s %u", Key.c_str(), Seconds);
    }

    // Hash operations
    RedisResult RedisConnection::HashSet(const std::string& Key, const std::string& Field, const std::string& Value)
    {
        return ExecuteCommand("HSET %s %s %s", Key.c_str(), Field.c_str(), Value.c_str());
    }

    RedisResult RedisConnection::HashGet(const std::string& Key, const std::string& Field)
    {
        return ExecuteCommand("HGET %s %s", Key.c_str(), Field.c_str());
    }

    RedisResult RedisConnection::HashGetAll(const std::string& Key)
    {
        return ExecuteCommand("HGETALL %s", Key.c_str());
    }

    RedisResult RedisConnection::HashDelete(const std::string& Key, const std::string& Field)
    {
        return ExecuteCommand("HDEL %s %s", Key.c_str(), Field.c_str());
    }

    RedisResult RedisConnection::HashExists(const std::string& Key, const std::string& Field)
    {
        return ExecuteCommand("HEXISTS %s %s", Key.c_str(), Field.c_str());
    }

    // List operations
    RedisResult RedisConnection::ListPush(const std::string& Key, const std::string& Value, bool PushLeft)
    {
        const char* Command = PushLeft ? "LPUSH" : "RPUSH";
        return ExecuteCommand("%s %s %s", Command, Key.c_str(), Value.c_str());
    }

    RedisResult RedisConnection::ListPop(const std::string& Key, bool PopLeft)
    {
        const char* Command = PopLeft ? "LPOP" : "RPOP";
        return ExecuteCommand("%s %s", Command, Key.c_str());
    }

    RedisResult RedisConnection::ListGet(const std::string& Key, int32_t Index)
    {
        return ExecuteCommand("LINDEX %s %d", Key.c_str(), Index);
    }

    RedisResult RedisConnection::ListRange(const std::string& Key, int32_t Start, int32_t Stop)
    {
        return ExecuteCommand("LRANGE %s %d %d", Key.c_str(), Start, Stop);
    }

    RedisResult RedisConnection::ListLength(const std::string& Key)
    {
        return ExecuteCommand("LLEN %s", Key.c_str());
    }

    // Set operations
    RedisResult RedisConnection::SetAdd(const std::string& Key, const std::string& Member)
    {
        return ExecuteCommand("SADD %s %s", Key.c_str(), Member.c_str());
    }

    RedisResult RedisConnection::SetRemove(const std::string& Key, const std::string& Member)
    {
        return ExecuteCommand("SREM %s %s", Key.c_str(), Member.c_str());
    }

    RedisResult RedisConnection::SetMembers(const std::string& Key)
    {
        return ExecuteCommand("SMEMBERS %s", Key.c_str());
    }

    RedisResult RedisConnection::SetIsMember(const std::string& Key, const std::string& Member)
    {
        return ExecuteCommand("SISMEMBER %s %s", Key.c_str(), Member.c_str());
    }

    RedisResult RedisConnection::SetCount(const std::string& Key)
    {
        return ExecuteCommand("SCARD %s", Key.c_str());
    }

    // Sorted set operations
    RedisResult RedisConnection::SortedSetAdd(const std::string& Key, double Score, const std::string& Member)
    {
        return ExecuteCommand("ZADD %s %f %s", Key.c_str(), Score, Member.c_str());
    }

    RedisResult RedisConnection::SortedSetRemove(const std::string& Key, const std::string& Member)
    {
        return ExecuteCommand("ZREM %s %s", Key.c_str(), Member.c_str());
    }

    RedisResult RedisConnection::SortedSetRange(const std::string& Key, int32_t Start, int32_t Stop, bool WithScores)
    {
        if (WithScores)
        {
            return ExecuteCommand("ZRANGE %s %d %d WITHSCORES", Key.c_str(), Start, Stop);
        }
        else
        {
            return ExecuteCommand("ZRANGE %s %d %d", Key.c_str(), Start, Stop);
        }
    }

    RedisResult RedisConnection::SortedSetScore(const std::string& Key, const std::string& Member)
    {
        return ExecuteCommand("ZSCORE %s %s", Key.c_str(), Member.c_str());
    }

    RedisResult RedisConnection::SortedSetCount(const std::string& Key)
    {
        return ExecuteCommand("ZCARD %s", Key.c_str());
    }

    // Atomic operations
    RedisResult RedisConnection::Increment(const std::string& Key, int64_t Delta)
    {
        if (Delta == 1)
        {
            return ExecuteCommand("INCR %s", Key.c_str());
        }
        else
        {
            return ExecuteCommand("INCRBY %s %lld", Key.c_str(), Delta);
        }
    }

    RedisResult RedisConnection::Decrement(const std::string& Key, int64_t Delta)
    {
        if (Delta == 1)
        {
            return ExecuteCommand("DECR %s", Key.c_str());
        }
        else
        {
            return ExecuteCommand("DECRBY %s %lld", Key.c_str(), Delta);
        }
    }

    RedisResult RedisConnection::ExecutePipeline(const std::vector<std::string>& Commands)
    {
        RedisResult Result;
        
        if (!IsConnected())
        {
            Result.Code = Common::ResultCode::NOT_INITIALIZED;
            Result.ErrorMessage = "Not connected to Redis";
            return Result;
        }

        std::lock_guard<std::mutex> Lock(ConnectionMutex);

        // Execute commands in pipeline
        for (const auto& Command : Commands)
        {
            redisAppendCommand(RedisContext, Command.c_str());
        }

        // Get replies
        for (size_t I = 0; I < Commands.size(); I++)
        {
            redisReply* Reply;
            if (redisGetReply(RedisContext, reinterpret_cast<void**>(&Reply)) == REDIS_OK)
            {
                RedisResult CmdResult = ProcessReply(Reply);
                if (CmdResult.IsSuccess())
                {
                    Result.Values.insert(Result.Values.end(), CmdResult.Values.begin(), CmdResult.Values.end());
                }
                freeReplyObject(Reply);
            }
        }

        Result.Code = Common::ResultCode::SUCCESS;
        return Result;
    }

    // Private helper methods
    RedisResult RedisConnection::ExecuteCommand(const std::string& Command)
    {
        return ExecuteCommand(Command.c_str());
    }

    RedisResult RedisConnection::ExecuteCommand(const char* Format, ...)
    {
        RedisResult Result;
        
        if (!IsConnected())
        {
            Result.Code = Common::ResultCode::NOT_INITIALIZED;
            Result.ErrorMessage = "Not connected to Redis";
            return Result;
        }

        std::lock_guard<std::mutex> Lock(ConnectionMutex);

        va_list Args;
        va_start(Args, Format);
        redisReply* Reply = static_cast<redisReply*>(redisvCommand(RedisContext, Format, Args));
        va_end(Args);

        if (!Reply)
        {
            ErrorCount++;
            Result.Code = Common::ResultCode::FAILED;
            Result.ErrorMessage = GetRedisError();
            return Result;
        }

        Result = ProcessReply(Reply);
        freeReplyObject(Reply);
        return Result;
    }

    RedisResult RedisConnection::ProcessReply(redisReply* Reply)
    {
        RedisResult Result;

        if (!Reply)
        {
            Result.Code = Common::ResultCode::FAILED;
            Result.ErrorMessage = "Null reply";
            return Result;
        }

        switch (Reply->type)
        {
            case REDIS_REPLY_STRING:
                Result.Values.push_back(std::string(Reply->str, Reply->len));
                break;

            case REDIS_REPLY_ARRAY:
                for (size_t I = 0; I < Reply->elements; I++)
                {
                    if (Reply->element[I]->type == REDIS_REPLY_STRING)
                    {
                        Result.Values.push_back(std::string(Reply->element[I]->str, Reply->element[I]->len));
                    }
                }
                break;

            case REDIS_REPLY_INTEGER:
                Result.IntValue = Reply->integer;
                Result.BoolValue = Reply->integer != 0;
                break;

            case REDIS_REPLY_STATUS:
                Result.Values.push_back(std::string(Reply->str, Reply->len));
                break;

            case REDIS_REPLY_NIL:
                // Null reply is success but with no value
                break;

            case REDIS_REPLY_ERROR:
                Result.Code = Common::ResultCode::FAILED;
                Result.ErrorMessage = std::string(Reply->str, Reply->len);
                break;

            default:
                Result.Code = Common::ResultCode::FAILED;
                Result.ErrorMessage = "Unknown reply type";
                break;
        }

        return Result;
    }

    DatabaseResult RedisConnection::ConvertToDbResult(const RedisResult& RedisRes)
    {
        DatabaseResult DbResult;
        DbResult.Code = RedisRes.Code;
        DbResult.ErrorMessage = RedisRes.ErrorMessage;

        if (RedisRes.IsSuccess())
        {
            // Convert Redis values to database result format
            for (const auto& Value : RedisRes.Values)
            {
                ResultRow Row;
                Row["value"] = Value;
                DbResult.Data.push_back(Row);
            }

            if (!RedisRes.HashValues.empty())
            {
                ResultRow Row;
                for (const auto& [Key, Value] : RedisRes.HashValues)
                {
                    Row[Key] = Value;
                }
                DbResult.Data.push_back(Row);
            }
        }

        return DbResult;
    }

    std::string RedisConnection::GetRedisError() const
    {
        if (!RedisContext)
        {
            return "Redis context not initialized";
        }

        return std::string(RedisContext->errstr);
    }

    void RedisConnection::CleanupConnection()
    {
        if (RedisContext)
        {
            redisFree(RedisContext);
            RedisContext = nullptr;
        }
    }

    std::string RedisConnection::BuildConnectionString() const
    {
        std::stringstream Ss;
        Ss << "redis://" << Config.Host << ":" << Config.Port << "/" << Config.Database;
        return Ss.str();
    }

    // Public command execution methods for internal use
    RedisResult RedisConnection::ExecuteCommandInternal(const std::string& Command)
    {
        return ExecuteCommand(Command.c_str());
    }

    RedisResult RedisConnection::ExecuteCommandInternal(const char* Format, ...)
    {
        RedisResult Result;
        
        if (!IsConnected())
        {
            Result.Code = Common::ResultCode::NOT_INITIALIZED;
            Result.ErrorMessage = "Not connected to Redis";
            return Result;
        }

        std::lock_guard<std::mutex> Lock(ConnectionMutex);

        va_list Args;
        va_start(Args, Format);
        redisReply* Reply = static_cast<redisReply*>(redisvCommand(RedisContext, Format, Args));
        va_end(Args);

        if (!Reply)
        {
            ErrorCount++;
            Result.Code = Common::ResultCode::FAILED;
            Result.ErrorMessage = GetRedisError();
            return Result;
        }

        Result = ProcessReply(Reply);
        freeReplyObject(Reply);
        return Result;
    }

    // RedisTransaction implementation
    RedisTransaction::RedisTransaction(std::shared_ptr<RedisConnection> Connection)
        : Connection(Connection)
        , IsActiveFlag(false)
    {
    }

    RedisTransaction::~RedisTransaction()
    {
        if (IsActiveFlag)
        {
            Rollback();
        }
    }

    Common::ResultCode RedisTransaction::Begin()
    {
        std::lock_guard<std::mutex> Lock(TransactionMutex);

        if (IsActiveFlag)
        {
            return Common::ResultCode::ALREADY_INITIALIZED;
        }

        RedisResult Result = Connection->ExecuteCommandInternal("MULTI");
        if (Result.IsSuccess())
        {
            IsActiveFlag = true;
            QueuedCommands.clear();
        }

        return Result.Code;
    }

    Common::ResultCode RedisTransaction::Commit()
    {
        std::lock_guard<std::mutex> Lock(TransactionMutex);

        if (!IsActiveFlag)
        {
            return Common::ResultCode::INVALID_STATE;
        }

        RedisResult Result = Connection->ExecuteCommandInternal("EXEC");
        IsActiveFlag = false;
        QueuedCommands.clear();
        return Result.Code;
    }

    Common::ResultCode RedisTransaction::Rollback()
    {
        std::lock_guard<std::mutex> Lock(TransactionMutex);

        if (!IsActiveFlag)
        {
            return Common::ResultCode::INVALID_STATE;
        }

        RedisResult Result = Connection->ExecuteCommandInternal("DISCARD");
        IsActiveFlag = false;
        QueuedCommands.clear();
        return Result.Code;
    }

    bool RedisTransaction::IsActive() const
    {
        std::lock_guard<std::mutex> Lock(TransactionMutex);
        return IsActiveFlag;
    }

    DatabaseResult RedisTransaction::ExecuteQuery(const std::string& Query, const ParameterMap& Parameters)
    {
        if (!IsActiveFlag)
        {
            DatabaseResult Result;
            Result.Code = Common::ResultCode::INVALID_STATE;
            Result.ErrorMessage = "Transaction not active";
            return Result;
        }

        QueuedCommands.push_back(Query);
        return Connection->ExecuteQuery(Query, Parameters);
    }

    DatabaseResult RedisTransaction::ExecuteStoredProcedure(const std::string& ProcedureName, const ParameterMap& Parameters)
    {
        if (!IsActiveFlag)
        {
            DatabaseResult Result;
            Result.Code = Common::ResultCode::INVALID_STATE;
            Result.ErrorMessage = "Transaction not active";
            return Result;
        }

        return Connection->ExecuteStoredProcedure(ProcedureName, Parameters);
    }

    Common::ResultCode RedisTransaction::AddCommand(const std::string& Command)
    {
        if (!IsActiveFlag)
        {
            return Common::ResultCode::INVALID_STATE;
        }

        QueuedCommands.push_back(Command);
        RedisResult Result = Connection->ExecuteCommandInternal(Command);
        return Result.Code;
    }

    Common::ResultCode RedisTransaction::Watch(const std::string& Key)
    {
        RedisResult Result = Connection->ExecuteCommandInternal("WATCH %s", Key.c_str());
        return Result.Code;
    }

    Common::ResultCode RedisTransaction::Unwatch()
    {
        RedisResult Result = Connection->ExecuteCommandInternal("UNWATCH");
        return Result.Code;
    }

} // namespace Helianthus::Database::Redis