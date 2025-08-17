#include "MySqlConnection.h"
#include <chrono>
#include <sstream>
#include <algorithm>

namespace Helianthus::Database::MySQL
{
    MySqlConnection::MySqlConnection(const MySqlConfig& Config)
        : Config(Config)
        , MySqlConnectionHandle(nullptr)
        , IsConnectedFlag(false)
        , LastActiveTime(0)
        , QueryCount(0)
        , ErrorCount(0)
    {
        MySqlConnectionHandle = mysql_init(nullptr);
        if (!MySqlConnectionHandle)
        {
            HELIANTHUS_LOG_ERROR("Failed to initialize MySQL connection");
        }
    }

    MySqlConnection::~MySqlConnection()
    {
        Disconnect();
        CleanupConnection();
    }

    Common::ResultCode MySqlConnection::Connect()
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);

        if (IsConnectedFlag)
        {
            return Common::ResultCode::ALREADY_INITIALIZED;
        }

        if (!MySqlConnectionHandle)
        {
            return Common::ResultCode::NOT_INITIALIZED;
        }

        // Set connection options
        unsigned int Timeout = Config.ConnectionTimeout;
        mysql_options(MySqlConnectionHandle, MYSQL_OPT_CONNECT_TIMEOUT, &Timeout);
        mysql_options(MySqlConnectionHandle, MYSQL_OPT_READ_TIMEOUT, &Config.ReadTimeout);
        mysql_options(MySqlConnectionHandle, MYSQL_OPT_WRITE_TIMEOUT, &Config.WriteTimeout);

        if (Config.EnableSsl)
        {
            mysql_ssl_set(MySqlConnectionHandle, nullptr, nullptr, nullptr, nullptr, nullptr);
        }

        // Set character set
        mysql_options(MySqlConnectionHandle, MYSQL_SET_CHARSET_NAME, Config.CharacterSet.c_str());

        // Attempt connection
        MYSQL* Result = mysql_real_connect(
            MySqlConnectionHandle,
            Config.Host.c_str(),
            Config.Username.c_str(),
            Config.Password.c_str(),
            Config.Database.c_str(),
            Config.Port,
            nullptr,
            CLIENT_MULTI_STATEMENTS
        );

        if (!Result)
        {
            ErrorCount++;
            HELIANTHUS_LOG_ERROR("Failed to connect to MySQL: {}", GetMySqlError());
            return Common::ResultCode::FAILED;
        }

        IsConnectedFlag = true;
        UpdateLastActiveTime();
        
        HELIANTHUS_LOG_INFO("Successfully connected to MySQL database: {}:{}/{}", 
                           Config.Host, Config.Port, Config.Database);
        
        return Common::ResultCode::SUCCESS;
    }

    void MySqlConnection::Disconnect()
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);

        if (IsConnectedFlag && MySqlConnectionHandle)
        {
            mysql_close(MySqlConnectionHandle);
            IsConnectedFlag = false;
            HELIANTHUS_LOG_INFO("Disconnected from MySQL database");
        }
    }

    bool MySqlConnection::IsConnected() const
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        return IsConnectedFlag && MySqlConnectionHandle != nullptr;
    }

    DatabaseResult MySqlConnection::ExecuteQuery(const std::string& Query, const ParameterMap& Parameters)
    {
        if (!IsConnected())
        {
            DatabaseResult Result;
            Result.Code = Common::ResultCode::NOT_INITIALIZED;
            Result.ErrorMessage = "Not connected to database";
            return Result;
        }

        std::string PreparedQuery = PrepareQuery(Query, Parameters);
        UpdateLastActiveTime();
        QueryCount++;

        return ExecuteQueryInternal(PreparedQuery);
    }

    DatabaseResult MySqlConnection::ExecuteStoredProcedure(const std::string& ProcedureName, const ParameterMap& Parameters)
    {
        std::stringstream QueryStream;
        QueryStream << "CALL " << ProcedureName << "(";

        bool First = true;
        for (const auto& [Key, Value] : Parameters)
        {
            if (!First) QueryStream << ", ";
            QueryStream << ":" << Key;
            First = false;
        }

        QueryStream << ")";
        return ExecuteQuery(QueryStream.str(), Parameters);
    }

    std::shared_ptr<ITransaction> MySqlConnection::BeginTransaction(IsolationLevel Level)
    {
        if (!IsConnected())
        {
            return nullptr;
        }

        auto Transaction = std::make_shared<MySqlTransaction>(
            std::static_pointer_cast<MySqlConnection>(shared_from_this()), Level);
        
        if (Transaction->Begin() == Common::ResultCode::SUCCESS)
        {
            return Transaction;
        }

        return nullptr;
    }

    ConnectionInfo MySqlConnection::GetConnectionInfo() const
    {
        ConnectionInfo Info;
        Info.Type = DatabaseType::MYSQL;
        Info.ConnectionString = BuildConnectionString();
        Info.IsConnected = IsConnected();
        Info.LastActiveTime = LastActiveTime;
        Info.QueryCount = QueryCount;
        Info.ErrorCount = ErrorCount;
        return Info;
    }

    Common::TimestampMs MySqlConnection::GetLastActiveTime() const
    {
        return LastActiveTime;
    }

    void MySqlConnection::UpdateLastActiveTime()
    {
        LastActiveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string MySqlConnection::EscapeString(const std::string& Input) const
    {
        if (!MySqlConnectionHandle)
        {
            return Input;
        }

        std::vector<char> Buffer(Input.length() * 2 + 1);
        unsigned long Length = mysql_real_escape_string(
            MySqlConnectionHandle, Buffer.data(), Input.c_str(), Input.length());
        
        return std::string(Buffer.data(), Length);
    }

    bool MySqlConnection::Ping()
    {
        if (!IsConnected())
        {
            return false;
        }

        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        return mysql_ping(MySqlConnectionHandle) == 0;
    }

    // Private helper methods
    DatabaseResult MySqlConnection::ExecuteQueryInternal(const std::string& Query)
    {
        DatabaseResult Result;
        
        std::lock_guard<std::mutex> Lock(ConnectionMutex);

        if (mysql_real_query(MySqlConnectionHandle, Query.c_str(), Query.length()) != 0)
        {
            ErrorCount++;
            Result.Code = Common::ResultCode::FAILED;
            Result.ErrorMessage = GetMySqlError();
            return Result;
        }

        Result.AffectedRows = mysql_affected_rows(MySqlConnectionHandle);
        Result.LastInsertId = mysql_insert_id(MySqlConnectionHandle);

        // Process result set
        MYSQL_RES* MySqlResult = mysql_store_result(MySqlConnectionHandle);
        if (MySqlResult)
        {
            Result.Data = ProcessResultSet(MySqlResult);
            mysql_free_result(MySqlResult);
        }

        Result.Code = Common::ResultCode::SUCCESS;
        return Result;
    }

    std::string MySqlConnection::PrepareQuery(const std::string& Query, const ParameterMap& Parameters) const
    {
        std::string PreparedQuery = Query;

        for (const auto& [Key, Value] : Parameters)
        {
            std::string Placeholder = ":" + Key;
            std::string ValueString;

            std::visit([&ValueString, this](const auto& V) {
                using T = std::decay_t<decltype(V)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>)
                {
                    ValueString = "NULL";
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    ValueString = "'" + EscapeString(V) + "'";
                }
                else if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
                {
                    // Handle BLOB data
                    ValueString = "'"; // TODO: Implement proper BLOB handling
                    ValueString += "'";
                }
                else
                {
                    ValueString = std::to_string(V);
                }
            }, Value);

            // Replace all occurrences of placeholder
            size_t Pos = 0;
            while ((Pos = PreparedQuery.find(Placeholder, Pos)) != std::string::npos)
            {
                PreparedQuery.replace(Pos, Placeholder.length(), ValueString);
                Pos += ValueString.length();
            }
        }

        return PreparedQuery;
    }

    ResultSet MySqlConnection::ProcessResultSet(MYSQL_RES* Result) const
    {
        ResultSet ResultSetData;
        
        if (!Result)
        {
            return ResultSetData;
        }

        unsigned int NumFields = mysql_num_fields(Result);
        MYSQL_FIELD* Fields = mysql_fetch_fields(Result);

        MYSQL_ROW Row;
        while ((Row = mysql_fetch_row(Result)))
        {
            ResultRow ResultRowData;
            unsigned long* Lengths = mysql_fetch_lengths(Result);

            for (unsigned int I = 0; I < NumFields; I++)
            {
                std::string ColumnName = Fields[I].name;
                DatabaseValue Value = ConvertMySqlValue(Row[I], Fields[I].type);
                ResultRowData[ColumnName] = Value;
            }

            ResultSetData.push_back(ResultRowData);
        }

        return ResultSetData;
    }

    DatabaseValue MySqlConnection::ConvertMySqlValue(const char* Value, enum_field_types Type) const
    {
        if (!Value)
        {
            return nullptr;
        }

        switch (Type)
        {
            case MYSQL_TYPE_TINY:
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_LONG:
                return static_cast<int32_t>(std::stoi(Value));

            case MYSQL_TYPE_LONGLONG:
                return static_cast<int64_t>(std::stoll(Value));

            case MYSQL_TYPE_FLOAT:
                return std::stof(Value);

            case MYSQL_TYPE_DOUBLE:
                return std::stod(Value);

            case MYSQL_TYPE_BIT:
                return std::string(Value) == "1";

            case MYSQL_TYPE_BLOB:
            case MYSQL_TYPE_TINY_BLOB:
            case MYSQL_TYPE_MEDIUM_BLOB:
            case MYSQL_TYPE_LONG_BLOB:
                // TODO: Implement proper BLOB handling
                return std::vector<uint8_t>(Value, Value + strlen(Value));

            default:
                return std::string(Value);
        }
    }

    std::string MySqlConnection::GetMySqlError() const
    {
        if (!MySqlConnectionHandle)
        {
            return "MySQL connection not initialized";
        }

        return std::string(mysql_error(MySqlConnectionHandle));
    }

    void MySqlConnection::CleanupConnection()
    {
        if (MySqlConnectionHandle)
        {
            mysql_close(MySqlConnectionHandle);
            MySqlConnectionHandle = nullptr;
        }
    }

    std::string MySqlConnection::BuildConnectionString() const
    {
        std::stringstream Ss;
        Ss << "mysql://" << Config.Username << "@" << Config.Host << ":" << Config.Port << "/" << Config.Database;
        return Ss.str();
    }

    // MySqlTransaction implementation
    MySqlTransaction::MySqlTransaction(std::shared_ptr<MySqlConnection> Connection, IsolationLevel Level)
        : Connection(Connection)
        , TransactionIsolationLevel(Level)
        , IsActiveFlag(false)
    {
    }

    MySqlTransaction::~MySqlTransaction()
    {
        if (IsActiveFlag)
        {
            Rollback();
        }
    }

    Common::ResultCode MySqlTransaction::Begin()
    {
        std::lock_guard<std::mutex> Lock(TransactionMutex);

        if (IsActiveFlag)
        {
            return Common::ResultCode::ALREADY_INITIALIZED;
        }

        // Set isolation level
        std::string IsolationQuery = "SET TRANSACTION ISOLATION LEVEL " + GetIsolationLevelString();
        DatabaseResult IsolationResult = Connection->ExecuteQuery(IsolationQuery);
        if (!IsolationResult.IsSuccess())
        {
            return IsolationResult.Code;
        }

        // Begin transaction
        DatabaseResult BeginResult = Connection->ExecuteQuery("START TRANSACTION");
        if (BeginResult.IsSuccess())
        {
            IsActiveFlag = true;
        }

        return BeginResult.Code;
    }

    Common::ResultCode MySqlTransaction::Commit()
    {
        std::lock_guard<std::mutex> Lock(TransactionMutex);

        if (!IsActiveFlag)
        {
            return Common::ResultCode::INVALID_STATE;
        }

        DatabaseResult Result = Connection->ExecuteQuery("COMMIT");
        IsActiveFlag = false;
        return Result.Code;
    }

    Common::ResultCode MySqlTransaction::Rollback()
    {
        std::lock_guard<std::mutex> Lock(TransactionMutex);

        if (!IsActiveFlag)
        {
            return Common::ResultCode::INVALID_STATE;
        }

        DatabaseResult Result = Connection->ExecuteQuery("ROLLBACK");
        IsActiveFlag = false;
        return Result.Code;
    }

    bool MySqlTransaction::IsActive() const
    {
        std::lock_guard<std::mutex> Lock(TransactionMutex);
        return IsActiveFlag;
    }

    DatabaseResult MySqlTransaction::ExecuteQuery(const std::string& Query, const ParameterMap& Parameters)
    {
        if (!IsActiveFlag)
        {
            DatabaseResult Result;
            Result.Code = Common::ResultCode::INVALID_STATE;
            Result.ErrorMessage = "Transaction not active";
            return Result;
        }

        return Connection->ExecuteQuery(Query, Parameters);
    }

    DatabaseResult MySqlTransaction::ExecuteStoredProcedure(const std::string& ProcedureName, const ParameterMap& Parameters)
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

    std::string MySqlTransaction::GetIsolationLevelString() const
    {
        switch (TransactionIsolationLevel)
        {
            case IsolationLevel::READ_UNCOMMITTED:
                return "READ UNCOMMITTED";
            case IsolationLevel::READ_COMMITTED:
                return "READ COMMITTED";
            case IsolationLevel::REPEATABLE_READ:
                return "REPEATABLE READ";
            case IsolationLevel::SERIALIZABLE:
                return "SERIALIZABLE";
            default:
                return "READ COMMITTED";
        }
    }

} // namespace Helianthus::Database::MySQL