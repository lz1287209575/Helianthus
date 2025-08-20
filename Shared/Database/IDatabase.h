#pragma once

#include <functional>
#include <memory>

#include "../Common/Types.h"
#include "DatabaseTypes.h"

namespace Helianthus::Database
{
// Forward declarations
class IConnection;
class ITransaction;

// Database callback types
using QueryCallback = std::function<void(const DatabaseResult&)>;
using ConnectionCallback = std::function<void(Common::ResultCode)>;

// Main database interface
class IDatabase
{
public:
    virtual ~IDatabase() = default;

    // Database lifecycle
    virtual Common::ResultCode Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // Connection management
    virtual std::shared_ptr<IConnection> GetConnection() = 0;
    virtual void ReturnConnection(std::shared_ptr<IConnection> CONNECTION) = 0;
    virtual uint32_t GetActiveConnectionCount() const = 0;
    virtual uint32_t GetTotalConnectionCount() const = 0;

    // Synchronous operations
    virtual DatabaseResult ExecuteQuery(const std::string& QUERY,
                                        const ParameterMap& PARAMETERS = {}) = 0;
    virtual DatabaseResult ExecuteStoredProcedure(const std::string& PROCEDURE_NAME,
                                                  const ParameterMap& PARAMETERS = {}) = 0;

    // Asynchronous operations
    virtual void ExecuteQueryAsync(const std::string& QUERY,
                                   QueryCallback CALLBACK,
                                   const ParameterMap& PARAMETERS = {}) = 0;
    virtual void ExecuteStoredProcedureAsync(const std::string& PROCEDURE_NAME,
                                             QueryCallback CALLBACK,
                                             const ParameterMap& PARAMETERS = {}) = 0;

    // Transaction support
    virtual std::shared_ptr<ITransaction>
    BeginTransaction(IsolationLevel LEVEL = IsolationLevel::READ_COMMITTED) = 0;

    // Database information
    virtual DatabaseType GetDatabaseType() const = 0;
    virtual ConnectionInfo GetConnectionInfo() const = 0;
    virtual std::string GetDatabaseVersion() const = 0;

    // Health check
    virtual bool IsHealthy() const = 0;
    virtual Common::ResultCode TestConnection() = 0;

    // Utility methods
    virtual std::string EscapeString(const std::string& INPUT) const = 0;
    virtual std::string BuildConnectionString() const = 0;
};

// Database connection interface
class IConnection
{
public:
    virtual ~IConnection() = default;

    // Connection lifecycle
    virtual Common::ResultCode Connect() = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;

    // Query execution
    virtual DatabaseResult ExecuteQuery(const std::string& QUERY,
                                        const ParameterMap& PARAMETERS = {}) = 0;
    virtual DatabaseResult ExecuteStoredProcedure(const std::string& PROCEDURE_NAME,
                                                  const ParameterMap& PARAMETERS = {}) = 0;

    // Transaction support
    virtual std::shared_ptr<ITransaction>
    BeginTransaction(IsolationLevel LEVEL = IsolationLevel::READ_COMMITTED) = 0;

    // Connection information
    virtual ConnectionInfo GetConnectionInfo() const = 0;
    virtual Common::TimestampMs GetLastActiveTime() const = 0;
    virtual void UpdateLastActiveTime() = 0;

    // Utility methods
    virtual std::string EscapeString(const std::string& INPUT) const = 0;
    virtual bool Ping() = 0;
};

// Transaction interface
class ITransaction
{
public:
    virtual ~ITransaction() = default;

    // Transaction lifecycle
    virtual Common::ResultCode Begin() = 0;
    virtual Common::ResultCode Commit() = 0;
    virtual Common::ResultCode Rollback() = 0;
    virtual bool IsActive() const = 0;

    // Query execution within transaction
    virtual DatabaseResult ExecuteQuery(const std::string& QUERY,
                                        const ParameterMap& PARAMETERS = {}) = 0;
    virtual DatabaseResult ExecuteStoredProcedure(const std::string& PROCEDURE_NAME,
                                                  const ParameterMap& PARAMETERS = {}) = 0;

    // Transaction information
    virtual IsolationLevel GetIsolationLevel() const = 0;
    virtual std::shared_ptr<IConnection> GetConnection() const = 0;
};

// Database factory interface
class IDatabaseFactory
{
public:
    virtual ~IDatabaseFactory() = default;

    // Factory methods
    virtual std::shared_ptr<IDatabase> CreateMySqlDatabase(const MySqlConfig& CONFIG) = 0;
    virtual std::shared_ptr<IDatabase> CreateMongoDbDatabase(const MongoDbConfig& CONFIG) = 0;
    virtual std::shared_ptr<IDatabase> CreateRedisDatabase(const RedisConfig& CONFIG) = 0;

    // Utility methods
    virtual bool IsSupported(DatabaseType TYPE) const = 0;
    virtual std::vector<DatabaseType> GetSupportedTypes() const = 0;
};

}  // namespace Helianthus::Database