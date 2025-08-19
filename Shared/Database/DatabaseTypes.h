#pragma once

#include "Common/Types.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>

namespace Helianthus::Database
{
    // Database type definitions
    enum class DatabaseType : uint8_t
    {
        MYSQL = 0,
        MONGODB = 1,
        REDIS = 2
    };

    // Database value types
    using DatabaseValue = std::variant<
        std::nullptr_t,
        bool,
        int32_t,
        int64_t,
        uint32_t,
        uint64_t,
        float,
        double,
        std::string,
        std::vector<char>  // BLOB data
    >;

    // Parameter map for queries
    using ParameterMap = std::map<std::string, DatabaseValue>;
    
    // Result row as key-value pairs
    using ResultRow = std::map<std::string, DatabaseValue>;
    
    // Result set as vector of rows
    using ResultSet = std::vector<ResultRow>;

    // Database configuration structures
    struct MySqlConfig
    {
        std::string Host = "localhost";
        uint16_t Port = 3306;
        std::string Database;
        std::string Username;
        std::string Password;
        uint32_t ConnectionTimeout = 30;
        uint32_t ReadTimeout = 30;
        uint32_t WriteTimeout = 30;
        bool EnableSsl = false;
        std::string CharacterSet = "utf8mb4";
        uint32_t MaxConnections = 100;
        uint32_t MinConnections = 5;
    };

    struct MongoDbConfig
    {
        std::string Host = "localhost";
        uint16_t Port = 27017;
        std::string Database;
        std::string Username;
        std::string Password;
        uint32_t ConnectionTimeout = 30;
        bool EnableSsl = false;
        std::string AuthDatabase = "admin";
        uint32_t MaxConnections = 100;
        uint32_t MinConnections = 5;
    };

    struct RedisConfig
    {
        std::string Host = "localhost";
        uint16_t Port = 6379;
        std::string Password;
        uint32_t Database = 0;
        uint32_t ConnectionTimeout = 30;
        bool EnableSsl = false;
        uint32_t MaxConnections = 100;
        uint32_t MinConnections = 5;
        uint32_t KeyExpireSeconds = 3600; // Default TTL
    };

    // Transaction isolation levels
    enum class IsolationLevel : uint8_t
    {
        READ_UNCOMMITTED = 0,
        READ_COMMITTED = 1,
        REPEATABLE_READ = 2,
        SERIALIZABLE = 3
    };

    // Database operation result
    struct DatabaseResult
    {
        Common::ResultCode Code = Common::ResultCode::SUCCESS;
        std::string ErrorMessage;
        uint64_t AffectedRows = 0;
        uint64_t LastInsertId = 0;
        ResultSet Data;

        bool IsSuccess() const 
        { 
            return Code == Common::ResultCode::SUCCESS; 
        }
        bool HasData() const 
        { 
            return !Data.empty(); 
        }
    };

    // Connection information
    struct ConnectionInfo
    {
        DatabaseType Type;
        std::string ConnectionString;
        bool IsConnected = false;
        Common::TimestampMs LastActiveTime = 0;
        uint64_t QueryCount = 0;
        uint64_t ErrorCount = 0;
    };

} // namespace Helianthus::Database