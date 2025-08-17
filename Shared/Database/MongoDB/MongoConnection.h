#pragma once

#include "../IDatabase.h"
#include "../DatabaseTypes.h"
#include "../../Common/Logger.h"
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <memory>
#include <mutex>

namespace Helianthus::Database::MongoDB
{
    // MongoDB-specific data types
    using BsonDocument = bsoncxx::document::value;
    using BsonBuilder = bsoncxx::builder::stream::document;

    // MongoDB result
    struct MongoResult
    {
        Common::ResultCode Code = Common::ResultCode::SUCCESS;
        std::string ErrorMessage;
        std::vector<BsonDocument> Documents;
        uint64_t MatchedCount = 0;
        uint64_t ModifiedCount = 0;
        uint64_t UpsertedCount = 0;
        std::string UpsertedId;

        bool IsSuccess() const { return Code == Common::ResultCode::SUCCESS; }
        bool HasDocuments() const { return !Documents.empty(); }
    };

    // MongoDB connection implementation
    class MongoConnection : public IConnection
    {
    public:
        explicit MongoConnection(const MongoDbConfig& Config);
        virtual ~MongoConnection();

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

        // MongoDB-specific methods
        mongocxx::client& GetMongoClient() { return *MongoClient; }
        mongocxx::database GetDatabase() { return MongoClient->database(Config.Database); }
        const MongoDbConfig& GetConfig() const { return Config; }

        // Document operations
        MongoResult InsertOne(const std::string& Collection, const BsonDocument& Document);
        MongoResult InsertMany(const std::string& Collection, const std::vector<BsonDocument>& Documents);
        MongoResult FindOne(const std::string& Collection, const BsonDocument& Filter);
        MongoResult FindMany(const std::string& Collection, const BsonDocument& Filter, int32_t Limit = 0, int32_t Skip = 0);
        MongoResult UpdateOne(const std::string& Collection, const BsonDocument& Filter, const BsonDocument& Update, bool Upsert = false);
        MongoResult UpdateMany(const std::string& Collection, const BsonDocument& Filter, const BsonDocument& Update, bool Upsert = false);
        MongoResult DeleteOne(const std::string& Collection, const BsonDocument& Filter);
        MongoResult DeleteMany(const std::string& Collection, const BsonDocument& Filter);
        MongoResult Count(const std::string& Collection, const BsonDocument& Filter);

        // Aggregation operations
        MongoResult Aggregate(const std::string& Collection, const std::vector<BsonDocument>& Pipeline);

        // Index operations
        MongoResult CreateIndex(const std::string& Collection, const BsonDocument& Keys, const BsonDocument& Options = {});
        MongoResult DropIndex(const std::string& Collection, const std::string& IndexName);
        MongoResult ListIndexes(const std::string& Collection);

        // Collection operations
        MongoResult CreateCollection(const std::string& Collection, const BsonDocument& Options = {});
        MongoResult DropCollection(const std::string& Collection);
        MongoResult ListCollections();

    private:
        MongoDbConfig Config;
        std::unique_ptr<mongocxx::client> MongoClient;
        mutable std::mutex ConnectionMutex;

        // Connection state
        bool IsConnectedFlag;
        Common::TimestampMs LastActiveTime;
        uint64_t QueryCount;
        uint64_t ErrorCount;

        // Helper methods
        DatabaseResult ConvertToDbResult(const MongoResult& MongoRes);
        std::string GetMongoError(const std::exception& Ex) const;
        std::string BuildConnectionString() const;
        void CleanupConnection();
    };

    // MongoDB transaction implementation
    class MongoTransaction : public ITransaction
    {
    public:
        explicit MongoTransaction(std::shared_ptr<MongoConnection> Connection);
        virtual ~MongoTransaction();

        // ITransaction interface
        Common::ResultCode Begin() override;
        Common::ResultCode Commit() override;
        Common::ResultCode Rollback() override;
        bool IsActive() const override;

        DatabaseResult ExecuteQuery(const std::string& Query, const ParameterMap& Parameters = {}) override;
        DatabaseResult ExecuteStoredProcedure(const std::string& ProcedureName, const ParameterMap& Parameters = {}) override;

        IsolationLevel GetIsolationLevel() const override { return IsolationLevel::READ_COMMITTED; }
        std::shared_ptr<IConnection> GetConnection() const override { return Connection; }

        // MongoDB-specific transaction methods
        MongoResult InsertOne(const std::string& Collection, const BsonDocument& Document);
        MongoResult UpdateOne(const std::string& Collection, const BsonDocument& Filter, const BsonDocument& Update);
        MongoResult DeleteOne(const std::string& Collection, const BsonDocument& Filter);

    private:
        std::shared_ptr<MongoConnection> Connection;
        std::unique_ptr<mongocxx::client_session> Session;
        bool IsActiveFlag;
        mutable std::mutex TransactionMutex;
    };

    // BSON helper utilities
    class BsonHelper
    {
    public:
        static BsonDocument FromJson(const std::string& Json);
        static std::string ToJson(const BsonDocument& Document);
        static BsonDocument FromParameterMap(const ParameterMap& Parameters);
        static ParameterMap ToParameterMap(const BsonDocument& Document);
        static ResultRow ToResultRow(const BsonDocument& Document);
        static BsonDocument FromResultRow(const ResultRow& Row);
        
        // Builder helpers
        static BsonBuilder CreateDocument();
        static BsonDocument CreateFilter(const std::string& Key, const DatabaseValue& Value);
        static BsonDocument CreateUpdate(const std::map<std::string, DatabaseValue>& Updates);
    };

} // namespace Helianthus::Database::MongoDB