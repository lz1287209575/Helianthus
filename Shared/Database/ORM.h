#pragma once

#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "../Common/Types.h"
#include "DatabaseTypes.h"
#include "IDatabase.h"

namespace Helianthus::Database::ORM
{
// Forward declarations
template <typename T>
class Repository;

// Field metadata
struct FieldInfo
{
    std::string Name;
    std::string Type;
    bool IsPrimaryKey = false;
    bool IsAutoIncrement = false;
    bool IsNullable = true;
    std::string DefaultValue;
    uint32_t MaxLength = 0;
};

// Table metadata
struct TableInfo
{
    std::string Name;
    std::vector<FieldInfo> Fields;
    std::string PrimaryKeyField;
};

// Entity base interface
class IEntity
{
public:
    virtual ~IEntity() = default;
    virtual std::string GetTableName() const = 0;
    virtual ParameterMap ToParameterMap() const = 0;
    virtual void FromParameterMap(const ParameterMap& Parameters) = 0;
    virtual DatabaseValue GetPrimaryKeyValue() const = 0;
    virtual void SetPrimaryKeyValue(const DatabaseValue& Value) = 0;
    virtual TableInfo GetTableInfo() const = 0;
};

// Query builder
class QueryBuilder
{
public:
    QueryBuilder() = default;
    virtual ~QueryBuilder() = default;

    // SELECT operations
    QueryBuilder& Select(const std::vector<std::string>& Columns = {});
    QueryBuilder& From(const std::string& Table);
    QueryBuilder& Where(const std::string& Condition);
    QueryBuilder& WhereEquals(const std::string& Column, const DatabaseValue& Value);
    QueryBuilder& WhereIn(const std::string& Column, const std::vector<DatabaseValue>& Values);
    QueryBuilder&
    WhereBetween(const std::string& Column, const DatabaseValue& Start, const DatabaseValue& End);
    QueryBuilder& OrderBy(const std::string& Column, bool Ascending = true);
    QueryBuilder& GroupBy(const std::string& Column);
    QueryBuilder& Having(const std::string& Condition);
    QueryBuilder& Limit(uint32_t Count);
    QueryBuilder& Offset(uint32_t Count);

    // JOIN operations
    QueryBuilder& Join(const std::string& Table, const std::string& Condition);
    QueryBuilder& LeftJoin(const std::string& Table, const std::string& Condition);
    QueryBuilder& RightJoin(const std::string& Table, const std::string& Condition);
    QueryBuilder& InnerJoin(const std::string& Table, const std::string& Condition);

    // INSERT operations
    QueryBuilder& InsertInto(const std::string& Table);
    QueryBuilder& Values(const ParameterMap& Values);

    // UPDATE operations
    QueryBuilder& Update(const std::string& Table);
    QueryBuilder& Set(const std::string& Column, const DatabaseValue& Value);
    QueryBuilder& Set(const ParameterMap& Values);

    // DELETE operations
    QueryBuilder& DeleteFrom(const std::string& Table);

    // Build query
    std::string Build() const;
    ParameterMap GetParameters() const;
    void Clear();

private:
    enum class QueryType
    {
        SELECT,
        INSERT,
        UPDATE,
        DELETE
    };

    QueryType Type = QueryType::SELECT;
    std::vector<std::string> SelectColumns;
    std::string TableName;
    std::vector<std::string> WhereConditions;
    std::vector<std::string> JoinClauses;
    std::vector<std::string> OrderByColumns;
    std::vector<std::string> GroupByColumns;
    std::string HavingCondition;
    ParameterMap Parameters;
    ParameterMap UpdateValues;
    uint32_t LimitCount = 0;
    uint32_t OffsetCount = 0;

    std::string BuildSelect() const;
    std::string BuildInsert() const;
    std::string BuildUpdate() const;
    std::string BuildDelete() const;
    std::string ValueToString(const DatabaseValue& Value) const;
};

// Repository base class for CRUD operations
template <typename TEntity>
class Repository
{
    static_assert(std::is_base_of_v<IEntity, TEntity>, "TEntity must inherit from IEntity");

public:
    explicit Repository(std::shared_ptr<IDatabase> Database) : Database(Database) {}

    virtual ~Repository() = default;

    // CRUD operations
    virtual Common::ResultCode Create(const TEntity& Entity)
    {
        auto Query = QueryBuilder()
                         .InsertInto(Entity.GetTableName())
                         .Values(Entity.ToParameterMap())
                         .Build();

        auto Result = Database->ExecuteQuery(Query, QueryBuilder().GetParameters());
        return Result.Code;
    }

    virtual std::unique_ptr<TEntity> FindById(const DatabaseValue& Id)
    {
        TEntity TempEntity;
        auto TableInfo = TempEntity.GetTableInfo();

        auto Query = QueryBuilder()
                         .Select()
                         .From(TempEntity.GetTableName())
                         .WhereEquals(TableInfo.PrimaryKeyField, Id)
                         .Build();

        auto Result = Database->ExecuteQuery(Query, QueryBuilder().GetParameters());

        if (Result.IsSuccess() && Result.HasData())
        {
            auto Entity = std::make_unique<TEntity>();
            Entity->FromParameterMap(ConvertResultRowToParameterMap(Result.Data[0]));
            return Entity;
        }

        return nullptr;
    }

    virtual std::vector<std::unique_ptr<TEntity>> FindAll()
    {
        TEntity TempEntity;
        auto Query = QueryBuilder().Select().From(TempEntity.GetTableName()).Build();

        auto Result = Database->ExecuteQuery(Query);
        std::vector<std::unique_ptr<TEntity>> Entities;

        if (Result.IsSuccess())
        {
            for (const auto& Row : Result.Data)
            {
                auto Entity = std::make_unique<TEntity>();
                Entity->FromParameterMap(ConvertResultRowToParameterMap(Row));
                Entities.push_back(std::move(Entity));
            }
        }

        return Entities;
    }

    virtual std::vector<std::unique_ptr<TEntity>> FindWhere(const std::string& Condition,
                                                            const ParameterMap& Parameters = {})
    {
        TEntity TempEntity;
        auto Query =
            QueryBuilder().Select().From(TempEntity.GetTableName()).Where(Condition).Build();

        auto Result = Database->ExecuteQuery(Query, Parameters);
        std::vector<std::unique_ptr<TEntity>> Entities;

        if (Result.IsSuccess())
        {
            for (const auto& Row : Result.Data)
            {
                auto Entity = std::make_unique<TEntity>();
                Entity->FromParameterMap(ConvertResultRowToParameterMap(Row));
                Entities.push_back(std::move(Entity));
            }
        }

        return Entities;
    }

    virtual Common::ResultCode Update(const TEntity& Entity)
    {
        auto TableInfo = Entity.GetTableInfo();
        auto Parameters = Entity.ToParameterMap();

        auto Query = QueryBuilder()
                         .Update(Entity.GetTableName())
                         .Set(Parameters)
                         .WhereEquals(TableInfo.PrimaryKeyField, Entity.GetPrimaryKeyValue())
                         .Build();

        auto Result = Database->ExecuteQuery(Query, QueryBuilder().GetParameters());
        return Result.Code;
    }

    virtual Common::ResultCode Delete(const DatabaseValue& Id)
    {
        TEntity TempEntity;
        auto TableInfo = TempEntity.GetTableInfo();

        auto Query = QueryBuilder()
                         .DeleteFrom(TempEntity.GetTableName())
                         .WhereEquals(TableInfo.PrimaryKeyField, Id)
                         .Build();

        auto Result = Database->ExecuteQuery(Query, QueryBuilder().GetParameters());
        return Result.Code;
    }

    virtual Common::ResultCode DeleteWhere(const std::string& Condition,
                                           const ParameterMap& Parameters = {})
    {
        TEntity TempEntity;
        auto Query = QueryBuilder().DeleteFrom(TempEntity.GetTableName()).Where(Condition).Build();

        auto Result = Database->ExecuteQuery(Query, Parameters);
        return Result.Code;
    }

    // Pagination support
    virtual std::vector<std::unique_ptr<TEntity>>
    FindPage(uint32_t Page, uint32_t PageSize, const std::string& OrderBy = "")
    {
        TEntity TempEntity;
        auto QueryBdr = QueryBuilder()
                            .Select()
                            .From(TempEntity.GetTableName())
                            .Limit(PageSize)
                            .Offset(Page * PageSize);

        if (!OrderBy.empty())
        {
            QueryBdr.OrderBy(OrderBy);
        }

        auto Query = QueryBdr.Build();
        auto Result = Database->ExecuteQuery(Query);
        std::vector<std::unique_ptr<TEntity>> Entities;

        if (Result.IsSuccess())
        {
            for (const auto& Row : Result.Data)
            {
                auto Entity = std::make_unique<TEntity>();
                Entity->FromParameterMap(ConvertResultRowToParameterMap(Row));
                Entities.push_back(std::move(Entity));
            }
        }

        return Entities;
    }

    virtual uint64_t Count()
    {
        TEntity TempEntity;
        auto Query = "SELECT COUNT(*) as count FROM " + TempEntity.GetTableName();
        auto Result = Database->ExecuteQuery(Query);

        if (Result.IsSuccess() && Result.HasData())
        {
            auto CountValue = Result.Data[0].find("count");
            if (CountValue != Result.Data[0].end())
            {
                return std::get<uint64_t>(CountValue->second);
            }
        }

        return 0;
    }

    // Transaction support
    virtual Common::ResultCode CreateInTransaction(const std::vector<TEntity>& Entities)
    {
        auto Transaction = Database->BeginTransaction();
        if (!Transaction)
        {
            return Common::ResultCode::FAILED;
        }

        for (const auto& Entity : Entities)
        {
            auto Query = QueryBuilder()
                             .InsertInto(Entity.GetTableName())
                             .Values(Entity.ToParameterMap())
                             .Build();

            auto Result = Transaction->ExecuteQuery(Query, QueryBuilder().GetParameters());
            if (!Result.IsSuccess())
            {
                Transaction->Rollback();
                return Result.Code;
            }
        }

        return Transaction->Commit();
    }

protected:
    std::shared_ptr<IDatabase> Database;

    ParameterMap ConvertResultRowToParameterMap(const ResultRow& Row)
    {
        ParameterMap Parameters;
        for (const auto& [Key, Value] : Row)
        {
            Parameters[Key] = Value;
        }
        return Parameters;
    }
};

// Entity factory for creating tables
class EntityFactory
{
public:
    static Common::ResultCode CreateTable(std::shared_ptr<IDatabase> Database,
                                          const IEntity& Entity);
    static Common::ResultCode DropTable(std::shared_ptr<IDatabase> Database,
                                        const std::string& TableName);
    static Common::ResultCode CreateIndex(std::shared_ptr<IDatabase> Database,
                                          const std::string& TableName,
                                          const std::vector<std::string>& Columns,
                                          const std::string& IndexName = "");
    static std::string GenerateCreateTableSQL(const TableInfo& TableInfo, DatabaseType DbType);
    static std::string GenerateCreateIndexSQL(const std::string& TableName,
                                              const std::vector<std::string>& Columns,
                                              const std::string& IndexName,
                                              DatabaseType DbType);
};

// Migration helper
class Migration
{
public:
    virtual ~Migration() = default;
    virtual Common::ResultCode Up(std::shared_ptr<IDatabase> Database) = 0;
    virtual Common::ResultCode Down(std::shared_ptr<IDatabase> Database) = 0;
    virtual std::string GetVersion() const = 0;
    virtual std::string GetDescription() const = 0;
};

class MigrationRunner
{
public:
    explicit MigrationRunner(std::shared_ptr<IDatabase> Database);
    virtual ~MigrationRunner() = default;

    Common::ResultCode AddMigration(std::unique_ptr<Migration> Migration);
    Common::ResultCode RunMigrations();
    Common::ResultCode RollbackMigration(const std::string& Version);
    std::vector<std::string> GetAppliedMigrations();

private:
    std::shared_ptr<IDatabase> Database;
    std::vector<std::unique_ptr<Migration>> Migrations;

    Common::ResultCode CreateMigrationTable();
    bool IsMigrationApplied(const std::string& Version);
    Common::ResultCode RecordMigration(const std::string& Version);
    Common::ResultCode RemoveMigration(const std::string& Version);
};

}  // namespace Helianthus::Database::ORM