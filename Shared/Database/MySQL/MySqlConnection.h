#pragma once

#include "../IDatabase.h"
#include "../DatabaseTypes.h"
#include "../../Common/Logger.h"
#include <mysql.h>
#include <memory>
#include <mutex>

namespace Helianthus::Database::MySQL
{
    // Forward declarations
    class MySqlTransaction;

    // MySQL connection implementation
    class MySqlConnection : public IConnection, public std::enable_shared_from_this<MySqlConnection>
    {
    public:
        explicit MySqlConnection(const MySqlConfig& Config);
        virtual ~MySqlConnection();

        // IConnection interface
        Common::ResultCode Connect() override;
        void Disconnect() override;
        bool IsConnected() const override;

        DatabaseResult ExecuteQuery(const std::string& Query, const ParameterMap& Parameters = {}) override;
        DatabaseResult ExecuteStoredProcedure(const std::string& ProcedureName, const ParameterMap& Parameters = {}) override;

        std::shared_ptr<ITransaction> BeginTransaction(IsolationLevel Level = IsolationLevel::READ_COMMITTED) override;

        ConnectionInfo GetConnectionInfo() const override;
        Common::TimestampMs GetLastActiveTime() const override;
        void UpdateLastActiveTime() override;

        std::string EscapeString(const std::string& Input) const override;
        bool Ping() override;

        // MySQL specific methods
        MYSQL* GetMySqlHandle() const { return MySqlConnectionHandle; }
        const MySqlConfig& GetConfig() const { return Config; }

    private:
        // Configuration and connection
        MySqlConfig Config;
        MYSQL* MySqlConnectionHandle;
        mutable std::mutex ConnectionMutex;

        // Connection state
        bool IsConnectedFlag;
        Common::TimestampMs LastActiveTime;
        uint64_t QueryCount;
        uint64_t ErrorCount;

        // Helper methods
        DatabaseResult ExecuteQueryInternal(const std::string& Query);
        std::string PrepareQuery(const std::string& Query, const ParameterMap& Parameters) const;
        ResultSet ProcessResultSet(MYSQL_RES* Result) const;
        DatabaseValue ConvertMySqlValue(const char* Value, enum_field_types Type) const;
        std::string GetMySqlError() const;
        Common::ResultCode SetupConnection();
        void CleanupConnection();
        std::string BuildConnectionString() const;
    };

    // MySQL transaction implementation
    class MySqlTransaction : public ITransaction
    {
    public:
        explicit MySqlTransaction(std::shared_ptr<MySqlConnection> Connection, IsolationLevel Level);
        virtual ~MySqlTransaction();

        // ITransaction interface
        Common::ResultCode Begin() override;
        Common::ResultCode Commit() override;
        Common::ResultCode Rollback() override;
        bool IsActive() const override;

        DatabaseResult ExecuteQuery(const std::string& Query, const ParameterMap& Parameters = {}) override;
        DatabaseResult ExecuteStoredProcedure(const std::string& ProcedureName, const ParameterMap& Parameters = {}) override;

        IsolationLevel GetIsolationLevel() const override { return TransactionIsolationLevel; }
        std::shared_ptr<IConnection> GetConnection() const override { return Connection; }

    private:
        std::shared_ptr<MySqlConnection> Connection;
        IsolationLevel TransactionIsolationLevel;
        bool IsActiveFlag;
        mutable std::mutex TransactionMutex;

        std::string GetIsolationLevelString() const;
    };

} // namespace Helianthus::Database::MySQL