#include "MongoConnection.h"

#include <iostream>

#include <bsoncxx/json.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/uri.hpp>

namespace Helianthus::Database::MongoDB
{
MongoConnection::MongoConnection(const MongoDbConfig& Config)
    : Config(Config), IsConnectedFlag(false), LastActiveTime(0), QueryCount(0), ErrorCount(0)
{
    std::cout << "[MongoConnection] 创建 MongoDB 连接" << std::endl;
}

MongoConnection::~MongoConnection()
{
    Disconnect();
}

Common::ResultCode MongoConnection::Connect()
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);

    if (IsConnectedFlag)
    {
        return Common::ResultCode::SUCCESS;
    }

    try
    {
        // 构建连接字符串
        std::string ConnectionString = "mongodb://";
        if (!Config.Username.empty())
        {
            ConnectionString += Config.Username;
            if (!Config.Password.empty())
            {
                ConnectionString += ":" + Config.Password;
            }
            ConnectionString += "@";
        }
        ConnectionString += Config.Host + ":" + std::to_string(Config.Port);
        if (!Config.Database.empty())
        {
            ConnectionString += "/" + Config.Database;
        }

        // 创建客户端
        mongocxx::uri Uri(ConnectionString);
        MongoClient = std::make_unique<mongocxx::client>(Uri);

        // 测试连接
        if (Ping())
        {
            IsConnectedFlag = true;
            LastActiveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();

            std::cout << "[MongoConnection] 连接成功: " << Config.Host << ":" << Config.Port
                      << std::endl;
            return Common::ResultCode::SUCCESS;
        }
        else
        {
            std::cout << "[MongoConnection] 连接失败: Ping 失败" << std::endl;
            return Common::ResultCode::CONNECTION_ERROR;
        }
    }
    catch (const std::exception& Ex)
    {
        std::cout << "[MongoConnection] 连接异常: " << Ex.what() << std::endl;
        ErrorCount++;
        return Common::ResultCode::CONNECTION_ERROR;
    }
}

void MongoConnection::Disconnect()
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);

    if (IsConnectedFlag)
    {
        MongoClient.reset();
        IsConnectedFlag = false;
        std::cout << "[MongoConnection] 连接已断开" << std::endl;
    }
}

bool MongoConnection::IsConnected() const
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    return IsConnectedFlag && MongoClient != nullptr;
}

DatabaseResult MongoConnection::ExecuteQuery(const std::string& Query,
                                             const ParameterMap& Parameters)
{
    // MongoDB 不支持 SQL 查询，这里返回错误
    return DatabaseResult{.Code = Common::ResultCode::NOT_SUPPORTED,
                          .ErrorMessage = "MongoDB does not support SQL queries"};
}

DatabaseResult MongoConnection::ExecuteStoredProcedure(const std::string& ProcedureName,
                                                       const ParameterMap& Parameters)
{
    // MongoDB 不支持存储过程，这里返回错误
    return DatabaseResult{.Code = Common::ResultCode::NOT_SUPPORTED,
                          .ErrorMessage = "MongoDB does not support stored procedures"};
}

std::shared_ptr<ITransaction> MongoConnection::BeginTransaction(IsolationLevel Level)
{
    // MongoDB 4.0+ 支持事务，但这里简化实现
    return nullptr;
}

ConnectionInfo MongoConnection::GetConnectionInfo() const
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);

    return ConnectionInfo{.DatabaseType = DatabaseType::MONGODB,
                          .Host = Config.Host,
                          .Port = Config.Port,
                          .Database = Config.Database,
                          .Username = Config.Username,
                          .IsConnected = IsConnectedFlag,
                          .QueryCount = QueryCount,
                          .ErrorCount = ErrorCount,
                          .LastActiveTime = LastActiveTime};
}

Common::TimestampMs MongoConnection::GetLastActiveTime() const
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    return LastActiveTime;
}

void MongoConnection::UpdateLastActiveTime()
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    LastActiveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
}

std::string MongoConnection::EscapeString(const std::string& Input) const
{
    // MongoDB 使用 BSON，不需要转义字符串
    return Input;
}

bool MongoConnection::Ping()
{
    try
    {
        if (!MongoClient)
        {
            return false;
        }

        // 执行 ping 命令
        auto Database = MongoClient->database("admin");
        auto Result = Database.run_command(bsoncxx::from_json("{\"ping\": 1}"));
        return true;
    }
    catch (const std::exception& Ex)
    {
        std::cout << "[MongoConnection] Ping 失败: " << Ex.what() << std::endl;
        return false;
    }
}

// MongoDB 特定方法实现
MongoResult MongoConnection::InsertOne(const std::string& Collection, const BsonDocument& Document)
{
    try
    {
        if (!IsConnected())
        {
            return MongoResult{.Code = Common::ResultCode::CONNECTION_ERROR,
                               .ErrorMessage = "Not connected to MongoDB"};
        }

        auto Database = MongoClient->database(Config.Database);
        auto Coll = Database.collection(Collection);

        auto Result = Coll.insert_one(Document.view());

        UpdateLastActiveTime();
        QueryCount++;

        return MongoResult{.Code = Common::ResultCode::SUCCESS,
                           .UpsertedId = Result->inserted_id().get_oid().value.to_string()};
    }
    catch (const std::exception& Ex)
    {
        ErrorCount++;
        return MongoResult{.Code = Common::ResultCode::QUERY_ERROR,
                           .ErrorMessage = GetMongoError(Ex)};
    }
}

MongoResult MongoConnection::FindOne(const std::string& Collection, const BsonDocument& Filter)
{
    try
    {
        if (!IsConnected())
        {
            return MongoResult{.Code = Common::ResultCode::CONNECTION_ERROR,
                               .ErrorMessage = "Not connected to MongoDB"};
        }

        auto Database = MongoClient->database(Config.Database);
        auto Coll = Database.collection(Collection);

        auto Result = Coll.find_one(Filter.view());

        UpdateLastActiveTime();
        QueryCount++;

        if (Result)
        {
            return MongoResult{.Code = Common::ResultCode::SUCCESS,
                               .Documents = {Result->view()},
                               .MatchedCount = 1};
        }
        else
        {
            return MongoResult{.Code = Common::ResultCode::SUCCESS, .MatchedCount = 0};
        }
    }
    catch (const std::exception& Ex)
    {
        ErrorCount++;
        return MongoResult{.Code = Common::ResultCode::QUERY_ERROR,
                           .ErrorMessage = GetMongoError(Ex)};
    }
}

DatabaseResult MongoConnection::ConvertToDbResult(const MongoResult& MongoRes)
{
    DatabaseResult Result;
    Result.Code = MongoRes.Code;
    Result.ErrorMessage = MongoRes.ErrorMessage;

    // 转换文档为字符串（简化实现）
    for (const auto& Doc : MongoRes.Documents)
    {
        Result.Rows.push_back({bsoncxx::to_json(Doc.view())});
    }

    Result.RowsAffected = MongoRes.ModifiedCount;
    return Result;
}

std::string MongoConnection::GetMongoError(const std::exception& Ex) const
{
    return std::string("MongoDB error: ") + Ex.what();
}
}  // namespace Helianthus::Database::MongoDB
