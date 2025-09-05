#include "../../Shared/Database/DatabaseConfig.h"
#include "../../Shared/Database/DatabaseTypes.h"
#ifndef HELIANTHUS_DISABLE_MYSQL
#include "../../Shared/Database/MySQL/MySqlConnection.h"
#else
#include <gtest/gtest.h>
#endif
#include <gtest/gtest.h>
// #include "../../Shared/Database/Redis/RedisConnection.h"  // Temporarily disabled due to hiredis
// compilation issues
#include "../../Shared/Database/ORM.h"

using namespace Helianthus::Database;
using namespace Helianthus::Common;

// Test entity for ORM testing
class TestUser : public ORM::IEntity
{
public:
    uint64_t Id = 0;
    std::string Username;
    std::string Email;
    uint32_t Age = 0;
    bool IsActive = true;

    std::string GetTableName() const override
    {
        return "users";
    }

    ParameterMap ToParameterMap() const override
    {
        ParameterMap Parameters;
        if (Id != 0)
            Parameters["id"] = Id;
        Parameters["username"] = Username;
        Parameters["email"] = Email;
        Parameters["age"] = Age;
        Parameters["is_active"] = IsActive;
        return Parameters;
    }

    void FromParameterMap(const ParameterMap& Parameters) override
    {
        auto IdIt = Parameters.find("id");
        if (IdIt != Parameters.end())
        {
            Id = std::get<uint64_t>(IdIt->second);
        }

        auto UsernameIt = Parameters.find("username");
        if (UsernameIt != Parameters.end())
        {
            Username = std::get<std::string>(UsernameIt->second);
        }

        auto EmailIt = Parameters.find("email");
        if (EmailIt != Parameters.end())
        {
            Email = std::get<std::string>(EmailIt->second);
        }

        auto AgeIt = Parameters.find("age");
        if (AgeIt != Parameters.end())
        {
            Age = std::get<uint32_t>(AgeIt->second);
        }

        auto ActiveIt = Parameters.find("is_active");
        if (ActiveIt != Parameters.end())
        {
            IsActive = std::get<bool>(ActiveIt->second);
        }
    }

    DatabaseValue GetPrimaryKeyValue() const override
    {
        return Id;
    }

    void SetPrimaryKeyValue(const DatabaseValue& Value) override
    {
        Id = std::get<uint64_t>(Value);
    }

    ORM::TableInfo GetTableInfo() const override
    {
        ORM::TableInfo TableInfo;
        TableInfo.Name = "users";
        TableInfo.PrimaryKeyField = "id";

        TableInfo.Fields = {{"id", "BIGINT", true, true, false, "", 0},
                            {"username", "VARCHAR", false, false, false, "", 50},
                            {"email", "VARCHAR", false, false, false, "", 100},
                            {"age", "INT", false, false, true, "0", 0},
                            {"is_active", "BOOLEAN", false, false, false, "true", 0}};

        return TableInfo;
    }
};

class DatabaseConfigTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ConfigManager = std::make_unique<DatabaseConfigManager>();
    }

    void TearDown() override
    {
        ConfigManager.reset();
    }

    std::unique_ptr<DatabaseConfigManager> ConfigManager;
};

TEST_F(DatabaseConfigTest, LoadFromEnvironment)
{
    // Test loading configuration from environment variables
    ResultCode Result = ConfigManager->LoadFromEnvironment("TEST_");
    EXPECT_EQ(Result, ResultCode::SUCCESS);

    // Test default MySQL configuration
#ifdef HELIANTHUS_DISABLE_MYSQL
    GTEST_SKIP() << "MySQL headers not available; skipping MySQL config assertions";
#else
    MySqlConfig MySqlCfg = ConfigManager->GetMySqlConfig();
    EXPECT_EQ(MySqlCfg.Host, "localhost");
    EXPECT_EQ(MySqlCfg.Port, 3306);
    EXPECT_EQ(MySqlCfg.Database, "helianthus");
#endif
}

TEST_F(DatabaseConfigTest, ConfigurationValidation)
{
    // Set invalid configuration
    ConfigManager->SetValue("mysql.test", "host", std::string(""));
    ConfigManager->SetValue("mysql.test", "port", uint32_t(0));

    ResultCode Result = ConfigManager->ValidateConfiguration();
    EXPECT_EQ(Result, ResultCode::INVALID_PARAMETER);

    auto Errors = ConfigManager->GetValidationErrors();
    EXPECT_GT(Errors.size(), 0);
}

TEST_F(DatabaseConfigTest, JsonSerialization)
{
    // Test JSON serialization
    std::string Json = ConfigManager->SaveToJson();
    EXPECT_FALSE(Json.empty());
    EXPECT_TRUE(Json.find("mysql") != std::string::npos);
}

class QueryBuilderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Builder = std::make_unique<ORM::QueryBuilder>();
    }

    void TearDown() override
    {
        Builder.reset();
    }

    std::unique_ptr<ORM::QueryBuilder> Builder;
};

TEST_F(QueryBuilderTest, SelectQuery)
{
    std::string Query = Builder->Select({"id", "username", "email"})
                            .From("users")
                            .WhereEquals("is_active", true)
                            .OrderBy("username")
                            .Limit(10)
                            .Build();

    EXPECT_FALSE(Query.empty());
    EXPECT_TRUE(Query.find("SELECT") != std::string::npos);
    EXPECT_TRUE(Query.find("FROM users") != std::string::npos);
    EXPECT_TRUE(Query.find("WHERE") != std::string::npos);
    EXPECT_TRUE(Query.find("ORDER BY") != std::string::npos);
    EXPECT_TRUE(Query.find("LIMIT") != std::string::npos);
}

TEST_F(QueryBuilderTest, InsertQuery)
{
    ParameterMap Values = {{"username", std::string("testuser")},
                           {"email", std::string("test@example.com")},
                           {"age", uint32_t(25)}};

    std::string Query = Builder->InsertInto("users").Values(Values).Build();

    EXPECT_FALSE(Query.empty());
    EXPECT_TRUE(Query.find("INSERT INTO") != std::string::npos);
    EXPECT_TRUE(Query.find("users") != std::string::npos);
    EXPECT_TRUE(Query.find("VALUES") != std::string::npos);
}

TEST_F(QueryBuilderTest, UpdateQuery)
{
    std::string Query = Builder->Update("users")
                            .Set("email", std::string("newemail@example.com"))
                            .WhereEquals("id", uint64_t(1))
                            .Build();

    EXPECT_FALSE(Query.empty());
    EXPECT_TRUE(Query.find("UPDATE") != std::string::npos);
    EXPECT_TRUE(Query.find("SET") != std::string::npos);
    EXPECT_TRUE(Query.find("WHERE") != std::string::npos);
}

TEST_F(QueryBuilderTest, DeleteQuery)
{
    std::string Query = Builder->DeleteFrom("users").WhereEquals("is_active", false).Build();

    EXPECT_FALSE(Query.empty());
    EXPECT_TRUE(Query.find("DELETE FROM") != std::string::npos);
    EXPECT_TRUE(Query.find("WHERE") != std::string::npos);
}

// Mock database for testing Repository
class MockDatabase : public IDatabase
{
public:
    ResultCode Initialize() override
    {
        return ResultCode::SUCCESS;
    }
    void Shutdown() override {}
    bool IsInitialized() const override
    {
        return true;
    }

    std::shared_ptr<IConnection> GetConnection() override
    {
        return nullptr;
    }
    void ReturnConnection(std::shared_ptr<IConnection> Connection) override {}
    uint32_t GetActiveConnectionCount() const override
    {
        return 0;
    }
    uint32_t GetTotalConnectionCount() const override
    {
        return 1;
    }

    DatabaseResult ExecuteQuery(const std::string& Query,
                                const ParameterMap& Parameters = {}) override
    {
        LastQuery = Query;
        LastParameters = Parameters;

        DatabaseResult Result;
        Result.Code = ResultCode::SUCCESS;

        // Mock data for SELECT queries
        if (Query.find("SELECT") != std::string::npos)
        {
            ResultRow Row;
            Row["id"] = uint64_t(1);
            Row["username"] = std::string("testuser");
            Row["email"] = std::string("test@example.com");
            Row["age"] = uint32_t(25);
            Row["is_active"] = true;
            Result.Data.push_back(Row);
        }

        return Result;
    }

    DatabaseResult ExecuteStoredProcedure(const std::string& ProcedureName,
                                          const ParameterMap& Parameters = {}) override
    {
        return DatabaseResult{};
    }

    void ExecuteQueryAsync(const std::string& Query,
                           QueryCallback Callback,
                           const ParameterMap& Parameters = {}) override
    {
    }
    void ExecuteStoredProcedureAsync(const std::string& ProcedureName,
                                     QueryCallback Callback,
                                     const ParameterMap& Parameters = {}) override
    {
    }

    std::shared_ptr<ITransaction>
    BeginTransaction(IsolationLevel Level = IsolationLevel::READ_COMMITTED) override
    {
        return nullptr;
    }

    DatabaseType GetDatabaseType() const override
    {
        return DatabaseType::MYSQL;
    }
    ConnectionInfo GetConnectionInfo() const override
    {
        return ConnectionInfo{};
    }
    std::string GetDatabaseVersion() const override
    {
        return "test";
    }

    bool IsHealthy() const override
    {
        return true;
    }
    ResultCode TestConnection() override
    {
        return ResultCode::SUCCESS;
    }

    std::string EscapeString(const std::string& Input) const override
    {
        return Input;
    }
    std::string BuildConnectionString() const override
    {
        return "test://localhost";
    }

    std::string LastQuery;
    ParameterMap LastParameters;
};

class RepositoryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MockDb = std::make_shared<MockDatabase>();
        UserRepository = std::make_unique<ORM::Repository<TestUser>>(MockDb);
    }

    void TearDown() override
    {
        UserRepository.reset();
        MockDb.reset();
    }

    std::shared_ptr<MockDatabase> MockDb;
    std::unique_ptr<ORM::Repository<TestUser>> UserRepository;
};

TEST_F(RepositoryTest, CreateEntity)
{
    TestUser User;
    User.Username = "testuser";
    User.Email = "test@example.com";
    User.Age = 25;
    User.IsActive = true;

    ResultCode Result = UserRepository->Create(User);
    EXPECT_EQ(Result, ResultCode::SUCCESS);
    EXPECT_TRUE(MockDb->LastQuery.find("INSERT INTO") != std::string::npos);
    EXPECT_TRUE(MockDb->LastQuery.find("users") != std::string::npos);
}

TEST_F(RepositoryTest, FindById)
{
    auto User = UserRepository->FindById(uint64_t(1));
    EXPECT_NE(User, nullptr);
    EXPECT_EQ(User->Id, 1);
    EXPECT_EQ(User->Username, "testuser");
    EXPECT_EQ(User->Email, "test@example.com");
    EXPECT_TRUE(MockDb->LastQuery.find("SELECT") != std::string::npos);
    EXPECT_TRUE(MockDb->LastQuery.find("WHERE id") != std::string::npos);
}

TEST_F(RepositoryTest, FindAll)
{
    auto Users = UserRepository->FindAll();
    EXPECT_EQ(Users.size(), 1);
    EXPECT_TRUE(MockDb->LastQuery.find("SELECT") != std::string::npos);
    EXPECT_TRUE(MockDb->LastQuery.find("FROM users") != std::string::npos);
}

TEST_F(RepositoryTest, UpdateEntity)
{
    TestUser User;
    User.Id = 1;
    User.Username = "updateduser";
    User.Email = "updated@example.com";

    ResultCode Result = UserRepository->Update(User);
    EXPECT_EQ(Result, ResultCode::SUCCESS);
    EXPECT_TRUE(MockDb->LastQuery.find("UPDATE") != std::string::npos);
    EXPECT_TRUE(MockDb->LastQuery.find("SET") != std::string::npos);
    EXPECT_TRUE(MockDb->LastQuery.find("WHERE id") != std::string::npos);
}

TEST_F(RepositoryTest, DeleteEntity)
{
    ResultCode Result = UserRepository->Delete(uint64_t(1));
    EXPECT_EQ(Result, ResultCode::SUCCESS);
    EXPECT_TRUE(MockDb->LastQuery.find("DELETE FROM") != std::string::npos);
    EXPECT_TRUE(MockDb->LastQuery.find("WHERE id") != std::string::npos);
}

// Integration tests would require actual database connections
// These are placeholder tests for the basic functionality

class DatabaseTypesTest : public ::testing::Test
{
};

TEST_F(DatabaseTypesTest, MySqlConfigDefaults)
{
    MySqlConfig Config;
    EXPECT_EQ(Config.Host, "localhost");
    EXPECT_EQ(Config.Port, 3306);
    EXPECT_EQ(Config.ConnectionTimeout, 30);
    EXPECT_EQ(Config.CharacterSet, "utf8mb4");
    EXPECT_FALSE(Config.EnableSsl);
}

// TODO: Re-enable when Redis compilation is fixed
/*
TEST_F(DatabaseTypesTest, RedisConfigDefaults)
{
    RedisConfig Config;
    EXPECT_EQ(Config.Host, "localhost");
    EXPECT_EQ(Config.Port, 6379);
    EXPECT_EQ(Config.Database, 0);
    EXPECT_EQ(Config.KeyExpireSeconds, 3600);
    EXPECT_FALSE(Config.EnableSsl);
}
*/

TEST_F(DatabaseTypesTest, DatabaseValueVariant)
{
    DatabaseValue Value1 = std::string("test");
    DatabaseValue Value2 = int32_t(42);
    DatabaseValue Value3 = true;
    DatabaseValue Value4 = nullptr;

    EXPECT_TRUE(std::holds_alternative<std::string>(Value1));
    EXPECT_TRUE(std::holds_alternative<int32_t>(Value2));
    EXPECT_TRUE(std::holds_alternative<bool>(Value3));
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(Value4));
}

TEST_F(DatabaseTypesTest, DatabaseResultSuccess)
{
    DatabaseResult Result;
    Result.Code = ResultCode::SUCCESS;
    Result.AffectedRows = 1;

    EXPECT_TRUE(Result.IsSuccess());
    EXPECT_EQ(Result.AffectedRows, 1);
}

// Performance tests could be added here for connection pooling, query execution, etc.

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}