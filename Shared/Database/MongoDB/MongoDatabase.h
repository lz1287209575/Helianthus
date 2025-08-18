#pragma once

#include "../IDatabase.h"
#include "../DatabaseTypes.h"
#include "MongoConnection.h"
#include <memory>

namespace Helianthus::Database::MongoDB
{
    /**
     * @brief MongoDB 数据库实现
     * 
     * 提供 MongoDB 数据库的高级接口，包括连接池管理、
     * 文档操作、聚合查询等功能
     */
    class MongoDatabase : public IDatabase
    {
    public:
        explicit MongoDatabase(const MongoDbConfig& Config);
        virtual ~MongoDatabase();

        // IDatabase interface
        Common::ResultCode Initialize() override;
        void Shutdown() override;
        bool IsInitialized() const override;

        std::shared_ptr<IConnection> GetConnection() override;
        void ReleaseConnection(std::shared_ptr<IConnection> Connection) override;

        DatabaseResult ExecuteQuery(const std::string& Query, const ParameterMap& Parameters = {}) override;
        DatabaseResult ExecuteStoredProcedure(const std::string& ProcedureName, const ParameterMap& Parameters = {}) override;

        // MongoDB 特定方法
        MongoResult InsertDocument(const std::string& Collection, const BsonDocument& Document);
        MongoResult InsertDocuments(const std::string& Collection, const std::vector<BsonDocument>& Documents);
        MongoResult FindDocument(const std::string& Collection, const BsonDocument& Filter);
        MongoResult FindDocuments(const std::string& Collection, const BsonDocument& Filter, int32_t Limit = 0, int32_t Skip = 0);
        MongoResult UpdateDocument(const std::string& Collection, const BsonDocument& Filter, const BsonDocument& Update, bool Upsert = false);
        MongoResult UpdateDocuments(const std::string& Collection, const BsonDocument& Filter, const BsonDocument& Update, bool Upsert = false);
        MongoResult DeleteDocument(const std::string& Collection, const BsonDocument& Filter);
        MongoResult DeleteDocuments(const std::string& Collection, const BsonDocument& Filter);
        MongoResult CountDocuments(const std::string& Collection, const BsonDocument& Filter);

        // 聚合操作
        MongoResult Aggregate(const std::string& Collection, const std::vector<BsonDocument>& Pipeline);

        // 索引操作
        MongoResult CreateIndex(const std::string& Collection, const BsonDocument& Keys, const BsonDocument& Options = {});
        MongoResult DropIndex(const std::string& Collection, const std::string& IndexName);
        MongoResult ListIndexes(const std::string& Collection);

        // 集合操作
        MongoResult CreateCollection(const std::string& Collection, const BsonDocument& Options = {});
        MongoResult DropCollection(const std::string& Collection);
        MongoResult ListCollections();

        // 配置和状态
        const MongoDbConfig& GetConfig() const { return Config; }
        DatabaseStats GetStats() const override;

    private:
        MongoDbConfig Config;
        std::atomic<bool> InitializedFlag{false};
        std::shared_ptr<MongoConnection> Connection;

        // 统计信息
        mutable std::mutex StatsMutex;
        DatabaseStats Stats;
    };

} // namespace Helianthus::Database::MongoDB
