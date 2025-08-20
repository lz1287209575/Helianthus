#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>

#include "../Common/Types.h"
#include "DatabaseTypes.h"
#include "IConnectionPool.h"

namespace Helianthus::Database
{
// Configuration value types
using ConfigValue = std::variant<bool, int32_t, uint32_t, uint64_t, float, double, std::string>;

// Configuration section
using ConfigSection = std::map<std::string, ConfigValue>;

// Full configuration map
using ConfigMap = std::map<std::string, ConfigSection>;

// Database configuration manager
class DatabaseConfigManager
{
public:
    DatabaseConfigManager() = default;
    virtual ~DatabaseConfigManager() = default;

    // Configuration loading
    Common::ResultCode LoadFromFile(const std::string& FilePath);
    Common::ResultCode LoadFromJson(const std::string& JsonContent);
    Common::ResultCode LoadFromEnvironment(const std::string& Prefix = "HELIANTHUS_DB_");

    // Configuration saving
    Common::ResultCode SaveToFile(const std::string& FilePath) const;
    std::string SaveToJson() const;

    // Database configuration retrieval
    MySqlConfig GetMySqlConfig(const std::string& ConnectionName = "default") const;
    MongoDbConfig GetMongoDbConfig(const std::string& ConnectionName = "default") const;
    RedisConfig GetRedisConfig(const std::string& ConnectionName = "default") const;
    ConnectionPoolConfig GetPoolConfig(const std::string& ConnectionName = "default") const;

    // Generic configuration access
    template <typename T>
    T GetValue(const std::string& Section,
               const std::string& Key,
               const T& DefaultValue = T{}) const;

    void SetValue(const std::string& Section, const std::string& Key, const ConfigValue& Value);
    bool HasSection(const std::string& Section) const;
    bool HasKey(const std::string& Section, const std::string& Key) const;

    // Configuration validation
    Common::ResultCode ValidateConfiguration() const;
    std::vector<std::string> GetValidationErrors() const;

    // Utility methods
    void Clear();
    ConfigMap GetAllConfig() const;
    std::vector<std::string> GetSectionNames() const;
    std::vector<std::string> GetKeyNames(const std::string& Section) const;

private:
    ConfigMap Config;
    mutable std::vector<std::string> ValidationErrors;

    // Helper methods
    MySqlConfig ParseMySqlConfig(const ConfigSection& Section) const;
    MongoDbConfig ParseMongoDbConfig(const ConfigSection& Section) const;
    RedisConfig ParseRedisConfig(const ConfigSection& Section) const;
    ConnectionPoolConfig ParsePoolConfig(const ConfigSection& Section) const;

    template <typename T>
    T GetConfigValue(const ConfigSection& Section,
                     const std::string& Key,
                     const T& DefaultValue) const;

    void SetDefaultMySqlConfig();
    void SetDefaultMongoDbConfig();
    void SetDefaultRedisConfig();
    void SetDefaultPoolConfig();

    bool ValidateMySqlConfig(const std::string& ConnectionName) const;
    bool ValidateMongoDbConfig(const std::string& ConnectionName) const;
    bool ValidateRedisConfig(const std::string& ConnectionName) const;
    bool ValidatePoolConfig(const std::string& ConnectionName) const;
};

// Global configuration instance
class GlobalDatabaseConfig
{
public:
    static DatabaseConfigManager& GetInstance();
    static Common::ResultCode Initialize(const std::string& ConfigFilePath = "");
    static void Shutdown();

private:
    static std::unique_ptr<DatabaseConfigManager> Instance;
    static std::mutex InstanceMutex;
    static bool IsInitialized;
};

// Configuration builder utility
class DatabaseConfigBuilder
{
public:
    DatabaseConfigBuilder() = default;
    virtual ~DatabaseConfigBuilder() = default;

    // MySQL configuration
    DatabaseConfigBuilder& WithMySql(const std::string& ConnectionName = "default");
    DatabaseConfigBuilder& MySqlHost(const std::string& Host);
    DatabaseConfigBuilder& MySqlPort(uint16_t Port);
    DatabaseConfigBuilder& MySqlDatabase(const std::string& Database);
    DatabaseConfigBuilder& MySqlCredentials(const std::string& Username,
                                            const std::string& Password);
    DatabaseConfigBuilder& MySqlSsl(bool EnableSsl);
    DatabaseConfigBuilder&
    MySqlTimeout(uint32_t ConnectionTimeout, uint32_t ReadTimeout, uint32_t WriteTimeout);

    // MongoDB configuration
    DatabaseConfigBuilder& WithMongoDb(const std::string& ConnectionName = "default");
    DatabaseConfigBuilder& MongoHost(const std::string& Host);
    DatabaseConfigBuilder& MongoPort(uint16_t Port);
    DatabaseConfigBuilder& MongoDatabase(const std::string& Database);
    DatabaseConfigBuilder& MongoCredentials(const std::string& Username,
                                            const std::string& Password);

    // Redis configuration
    DatabaseConfigBuilder& WithRedis(const std::string& ConnectionName = "default");
    DatabaseConfigBuilder& RedisHost(const std::string& Host);
    DatabaseConfigBuilder& RedisPort(uint16_t Port);
    DatabaseConfigBuilder& RedisPassword(const std::string& Password);
    DatabaseConfigBuilder& RedisDatabase(uint32_t Database);

    // Connection pool configuration
    DatabaseConfigBuilder& WithPool(const std::string& ConnectionName = "default");
    DatabaseConfigBuilder& PoolSize(uint32_t MinConnections, uint32_t MaxConnections);
    DatabaseConfigBuilder& PoolTimeouts(uint32_t ConnectionTimeoutMs, uint32_t IdleTimeoutMs);
    DatabaseConfigBuilder& PoolValidation(bool TestOnBorrow, bool TestOnReturn, bool TestWhileIdle);

    // Build and apply
    std::unique_ptr<DatabaseConfigManager> Build();
    Common::ResultCode ApplyTo(DatabaseConfigManager& ConfigManager);

private:
    ConfigMap ConfigData;
    std::string CurrentConnectionName;
    std::string CurrentSection;

    void EnsureSection(const std::string& Section);
};

}  // namespace Helianthus::Database