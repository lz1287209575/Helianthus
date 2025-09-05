#include "DatabaseConfig.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "../Common/Logger.h"

namespace Helianthus::Database
{
// DatabaseConfigManager implementation
Common::ResultCode DatabaseConfigManager::LoadFromFile(const std::string& FilePath)
{
    std::ifstream File(FilePath);
    if (!File.is_open())
    {
        HELIANTHUS_LOG_ERROR("Failed to open config file: {}", FilePath);
        return Common::ResultCode::NOT_FOUND;
    }

    std::stringstream Buffer;
    Buffer << File.rdbuf();
    File.close();

    return LoadFromJson(Buffer.str());
}

Common::ResultCode DatabaseConfigManager::LoadFromJson(const std::string& JsonContent)
{
    // TODO: Implement JSON parsing
    // For now, set default configuration
    SetDefaultMySqlConfig();
    SetDefaultMongoDbConfig();
    SetDefaultRedisConfig();
    SetDefaultPoolConfig();

    HELIANTHUS_LOG_INFO("Loaded database configuration from JSON");
    return Common::ResultCode::SUCCESS;
}

Common::ResultCode DatabaseConfigManager::LoadFromEnvironment(const std::string& Prefix)
{
    // MySQL environment variables
    std::string MySqlHost = std::getenv((Prefix + "MYSQL_HOST").c_str()) ?: "localhost";
    std::string MySqlPort = std::getenv((Prefix + "MYSQL_PORT").c_str()) ?: "3306";
    std::string MySqlDb = std::getenv((Prefix + "MYSQL_DATABASE").c_str()) ?: "helianthus";
    std::string MySqlUser = std::getenv((Prefix + "MYSQL_USERNAME").c_str()) ?: "root";
    std::string MySqlPass = std::getenv((Prefix + "MYSQL_PASSWORD").c_str()) ?: "";

    SetValue("mysql.default", "host", MySqlHost);
    SetValue("mysql.default", "port", static_cast<uint32_t>(std::stoul(MySqlPort)));
    SetValue("mysql.default", "database", MySqlDb);
    SetValue("mysql.default", "username", MySqlUser);
    SetValue("mysql.default", "password", MySqlPass);

    // MongoDB environment variables
    std::string MongoHost = std::getenv((Prefix + "MONGO_HOST").c_str()) ?: "localhost";
    std::string MongoPort = std::getenv((Prefix + "MONGO_PORT").c_str()) ?: "27017";
    std::string MongoDb = std::getenv((Prefix + "MONGO_DATABASE").c_str()) ?: "helianthus";
    std::string MongoUser = std::getenv((Prefix + "MONGO_USERNAME").c_str()) ?: "";
    std::string MongoPass = std::getenv((Prefix + "MONGO_PASSWORD").c_str()) ?: "";

    SetValue("mongodb.default", "host", MongoHost);
    SetValue("mongodb.default", "port", static_cast<uint32_t>(std::stoul(MongoPort)));
    SetValue("mongodb.default", "database", MongoDb);
    SetValue("mongodb.default", "username", MongoUser);
    SetValue("mongodb.default", "password", MongoPass);

    // Redis environment variables
    std::string RedisHost = std::getenv((Prefix + "REDIS_HOST").c_str()) ?: "localhost";
    std::string RedisPort = std::getenv((Prefix + "REDIS_PORT").c_str()) ?: "6379";
    std::string RedisPass = std::getenv((Prefix + "REDIS_PASSWORD").c_str()) ?: "";
    std::string RedisDb = std::getenv((Prefix + "REDIS_DATABASE").c_str()) ?: "0";

    SetValue("redis.default", "host", RedisHost);
    SetValue("redis.default", "port", static_cast<uint32_t>(std::stoul(RedisPort)));
    SetValue("redis.default", "password", RedisPass);
    SetValue("redis.default", "database", static_cast<uint32_t>(std::stoul(RedisDb)));

    HELIANTHUS_LOG_INFO("Loaded database configuration from environment variables");
    return Common::ResultCode::SUCCESS;
}

Common::ResultCode DatabaseConfigManager::SaveToFile(const std::string& FilePath) const
{
    std::ofstream File(FilePath);
    if (!File.is_open())
    {
        HELIANTHUS_LOG_ERROR("Failed to create config file: {}", FilePath);
        return Common::ResultCode::FAILED;
    }

    File << SaveToJson();
    File.close();

    HELIANTHUS_LOG_INFO("Saved database configuration to file: {}", FilePath);
    return Common::ResultCode::SUCCESS;
}

std::string DatabaseConfigManager::SaveToJson() const
{
    // 若尚未设置任何配置，生成一份默认配置再序列化，确保包含 mysql 等节
    if (Config.empty())
    {
        // 需要移除 const
        auto* self = const_cast<DatabaseConfigManager*>(this);
        self->SetDefaultMySqlConfig();
        self->SetDefaultMongoDbConfig();
        self->SetDefaultRedisConfig();
        self->SetDefaultPoolConfig();
    }

    // TODO: Implement JSON serialization
    // For now, return a simple representation
    std::stringstream Json;
    Json << "{\n";

    bool FirstSection = true;
    for (const auto& [SectionName, Section] : Config)
    {
        if (!FirstSection)
            Json << ",\n";
        Json << "  \"" << SectionName << "\": {\n";

        bool FirstKey = true;
        for (const auto& [Key, Value] : Section)
        {
            if (!FirstKey)
                Json << ",\n";
            Json << "    \"" << Key << "\": ";

            std::visit(
                [&Json](const auto& V)
                {
                    using T = std::decay_t<decltype(V)>;
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        Json << "\"" << V << "\"";
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        Json << (V ? "true" : "false");
                    }
                    else
                    {
                        Json << V;
                    }
                },
                Value);

            FirstKey = false;
        }

        Json << "\n  }";
        FirstSection = false;
    }

    Json << "\n}";
    return Json.str();
}

MySqlConfig DatabaseConfigManager::GetMySqlConfig(const std::string& ConnectionName) const
{
    std::string SectionName = "mysql." + ConnectionName;
    auto SectionIt = Config.find(SectionName);

    if (SectionIt == Config.end())
    {
        HELIANTHUS_LOG_WARN("MySQL configuration not found for connection: {}, using defaults",
                            ConnectionName);
        return MySqlConfig{};
    }

    return ParseMySqlConfig(SectionIt->second);
}

MongoDbConfig DatabaseConfigManager::GetMongoDbConfig(const std::string& ConnectionName) const
{
    std::string SectionName = "mongodb." + ConnectionName;
    auto SectionIt = Config.find(SectionName);

    if (SectionIt == Config.end())
    {
        HELIANTHUS_LOG_WARN("MongoDB configuration not found for connection: {}, using defaults",
                            ConnectionName);
        return MongoDbConfig{};
    }

    return ParseMongoDbConfig(SectionIt->second);
}

RedisConfig DatabaseConfigManager::GetRedisConfig(const std::string& ConnectionName) const
{
    std::string SectionName = "redis." + ConnectionName;
    auto SectionIt = Config.find(SectionName);

    if (SectionIt == Config.end())
    {
        HELIANTHUS_LOG_WARN("Redis configuration not found for connection: {}, using defaults",
                            ConnectionName);
        return RedisConfig{};
    }

    return ParseRedisConfig(SectionIt->second);
}

ConnectionPoolConfig DatabaseConfigManager::GetPoolConfig(const std::string& ConnectionName) const
{
    std::string SectionName = "pool." + ConnectionName;
    auto SectionIt = Config.find(SectionName);

    if (SectionIt == Config.end())
    {
        HELIANTHUS_LOG_WARN("Pool configuration not found for connection: {}, using defaults",
                            ConnectionName);
        return ConnectionPoolConfig{};
    }

    return ParsePoolConfig(SectionIt->second);
}

template <typename T>
T DatabaseConfigManager::GetValue(const std::string& Section,
                                  const std::string& Key,
                                  const T& DefaultValue) const
{
    auto SectionIt = Config.find(Section);
    if (SectionIt == Config.end())
    {
        return DefaultValue;
    }

    return GetConfigValue(SectionIt->second, Key, DefaultValue);
}

void DatabaseConfigManager::SetValue(const std::string& Section,
                                     const std::string& Key,
                                     const ConfigValue& Value)
{
    Config[Section][Key] = Value;
}

bool DatabaseConfigManager::HasSection(const std::string& Section) const
{
    return Config.find(Section) != Config.end();
}

bool DatabaseConfigManager::HasKey(const std::string& Section, const std::string& Key) const
{
    auto SectionIt = Config.find(Section);
    if (SectionIt == Config.end())
    {
        return false;
    }

    return SectionIt->second.find(Key) != SectionIt->second.end();
}

Common::ResultCode DatabaseConfigManager::ValidateConfiguration() const
{
    ValidationErrors.clear();

    // Validate all configured connections
    for (const auto& [SectionName, Section] : Config)
    {
        if (SectionName.find("mysql.") == 0)
        {
            std::string ConnectionName = SectionName.substr(6);
            ValidateMySqlConfig(ConnectionName);
        }
        else if (SectionName.find("mongodb.") == 0)
        {
            std::string ConnectionName = SectionName.substr(8);
            ValidateMongoDbConfig(ConnectionName);
        }
        else if (SectionName.find("redis.") == 0)
        {
            std::string ConnectionName = SectionName.substr(6);
            ValidateRedisConfig(ConnectionName);
        }
        else if (SectionName.find("pool.") == 0)
        {
            std::string ConnectionName = SectionName.substr(5);
            ValidatePoolConfig(ConnectionName);
        }
    }

    return ValidationErrors.empty() ? Common::ResultCode::SUCCESS
                                    : Common::ResultCode::INVALID_PARAMETER;
}

std::vector<std::string> DatabaseConfigManager::GetValidationErrors() const
{
    return ValidationErrors;
}

void DatabaseConfigManager::Clear()
{
    Config.clear();
    ValidationErrors.clear();
}

ConfigMap DatabaseConfigManager::GetAllConfig() const
{
    return Config;
}

std::vector<std::string> DatabaseConfigManager::GetSectionNames() const
{
    std::vector<std::string> SectionNames;
    for (const auto& [SectionName, Section] : Config)
    {
        SectionNames.push_back(SectionName);
    }
    return SectionNames;
}

std::vector<std::string> DatabaseConfigManager::GetKeyNames(const std::string& Section) const
{
    std::vector<std::string> KeyNames;
    auto SectionIt = Config.find(Section);
    if (SectionIt != Config.end())
    {
        for (const auto& [Key, Value] : SectionIt->second)
        {
            KeyNames.push_back(Key);
        }
    }
    return KeyNames;
}

// Private helper methods
MySqlConfig DatabaseConfigManager::ParseMySqlConfig(const ConfigSection& Section) const
{
    MySqlConfig Config;
    Config.Host = GetConfigValue(Section, "host", Config.Host);
    Config.Port = static_cast<uint16_t>(
        GetConfigValue<uint32_t>(Section, "port", static_cast<uint32_t>(Config.Port)));
    Config.Database = GetConfigValue(Section, "database", Config.Database);
    Config.Username = GetConfigValue(Section, "username", Config.Username);
    Config.Password = GetConfigValue(Section, "password", Config.Password);
    Config.ConnectionTimeout =
        GetConfigValue(Section, "connection_timeout", Config.ConnectionTimeout);
    Config.ReadTimeout = GetConfigValue(Section, "read_timeout", Config.ReadTimeout);
    Config.WriteTimeout = GetConfigValue(Section, "write_timeout", Config.WriteTimeout);
    Config.EnableSsl = GetConfigValue(Section, "enable_ssl", Config.EnableSsl);
    Config.CharacterSet = GetConfigValue(Section, "character_set", Config.CharacterSet);
    Config.MaxConnections = GetConfigValue(Section, "max_connections", Config.MaxConnections);
    Config.MinConnections = GetConfigValue(Section, "min_connections", Config.MinConnections);
    return Config;
}

MongoDbConfig DatabaseConfigManager::ParseMongoDbConfig(const ConfigSection& Section) const
{
    MongoDbConfig Config;
    Config.Host = GetConfigValue(Section, "host", Config.Host);
    Config.Port = static_cast<uint16_t>(
        GetConfigValue<uint32_t>(Section, "port", static_cast<uint32_t>(Config.Port)));
    Config.Database = GetConfigValue(Section, "database", Config.Database);
    Config.Username = GetConfigValue(Section, "username", Config.Username);
    Config.Password = GetConfigValue(Section, "password", Config.Password);
    Config.ConnectionTimeout =
        GetConfigValue(Section, "connection_timeout", Config.ConnectionTimeout);
    Config.EnableSsl = GetConfigValue(Section, "enable_ssl", Config.EnableSsl);
    Config.AuthDatabase = GetConfigValue(Section, "auth_database", Config.AuthDatabase);
    Config.MaxConnections = GetConfigValue(Section, "max_connections", Config.MaxConnections);
    Config.MinConnections = GetConfigValue(Section, "min_connections", Config.MinConnections);
    return Config;
}

RedisConfig DatabaseConfigManager::ParseRedisConfig(const ConfigSection& Section) const
{
    RedisConfig Config;
    Config.Host = GetConfigValue(Section, "host", Config.Host);
    Config.Port = static_cast<uint16_t>(
        GetConfigValue<uint32_t>(Section, "port", static_cast<uint32_t>(Config.Port)));
    Config.Password = GetConfigValue(Section, "password", Config.Password);
    Config.Database = GetConfigValue(Section, "database", Config.Database);
    Config.ConnectionTimeout =
        GetConfigValue(Section, "connection_timeout", Config.ConnectionTimeout);
    Config.EnableSsl = GetConfigValue(Section, "enable_ssl", Config.EnableSsl);
    Config.MaxConnections = GetConfigValue(Section, "max_connections", Config.MaxConnections);
    Config.MinConnections = GetConfigValue(Section, "min_connections", Config.MinConnections);
    Config.KeyExpireSeconds =
        GetConfigValue(Section, "key_expire_seconds", Config.KeyExpireSeconds);
    return Config;
}

ConnectionPoolConfig DatabaseConfigManager::ParsePoolConfig(const ConfigSection& Section) const
{
    ConnectionPoolConfig Config;
    Config.MinConnections = GetConfigValue(Section, "min_connections", Config.MinConnections);
    Config.MaxConnections = GetConfigValue(Section, "max_connections", Config.MaxConnections);
    Config.ConnectionTimeoutMs =
        GetConfigValue(Section, "connection_timeout_ms", Config.ConnectionTimeoutMs);
    Config.IdleTimeoutMs = GetConfigValue(Section, "idle_timeout_ms", Config.IdleTimeoutMs);
    Config.ValidationIntervalMs =
        GetConfigValue(Section, "validation_interval_ms", Config.ValidationIntervalMs);
    Config.TestOnBorrow = GetConfigValue(Section, "test_on_borrow", Config.TestOnBorrow);
    Config.TestOnReturn = GetConfigValue(Section, "test_on_return", Config.TestOnReturn);
    Config.TestWhileIdle = GetConfigValue(Section, "test_while_idle", Config.TestWhileIdle);
    Config.MaxRetries = GetConfigValue(Section, "max_retries", Config.MaxRetries);
    Config.RetryDelayMs = GetConfigValue(Section, "retry_delay_ms", Config.RetryDelayMs);
    return Config;
}

template <typename T>
T DatabaseConfigManager::GetConfigValue(const ConfigSection& Section,
                                        const std::string& Key,
                                        const T& DefaultValue) const
{
    auto KeyIt = Section.find(Key);
    if (KeyIt == Section.end())
    {
        return DefaultValue;
    }

    try
    {
        return std::get<T>(KeyIt->second);
    }
    catch (const std::bad_variant_access&)
    {
        HELIANTHUS_LOG_WARN("Type mismatch for config key: {}, using default", Key);
        return DefaultValue;
    }
}


void DatabaseConfigManager::SetDefaultMySqlConfig()
{
    SetValue("mysql.default", "host", std::string("localhost"));
    SetValue("mysql.default", "port", uint32_t(3306));
    SetValue("mysql.default", "database", std::string("helianthus"));
    SetValue("mysql.default", "username", std::string("root"));
    SetValue("mysql.default", "password", std::string(""));
    SetValue("mysql.default", "connection_timeout", uint32_t(30));
    SetValue("mysql.default", "read_timeout", uint32_t(30));
    SetValue("mysql.default", "write_timeout", uint32_t(30));
    SetValue("mysql.default", "enable_ssl", false);
    SetValue("mysql.default", "character_set", std::string("utf8mb4"));
    SetValue("mysql.default", "max_connections", uint32_t(100));
    SetValue("mysql.default", "min_connections", uint32_t(5));
}

void DatabaseConfigManager::SetDefaultMongoDbConfig()
{
    SetValue("mongodb.default", "host", std::string("localhost"));
    SetValue("mongodb.default", "port", uint32_t(27017));
    SetValue("mongodb.default", "database", std::string("helianthus"));
    SetValue("mongodb.default", "username", std::string(""));
    SetValue("mongodb.default", "password", std::string(""));
    SetValue("mongodb.default", "connection_timeout", uint32_t(30));
    SetValue("mongodb.default", "enable_ssl", false);
    SetValue("mongodb.default", "auth_database", std::string("admin"));
    SetValue("mongodb.default", "max_connections", uint32_t(100));
    SetValue("mongodb.default", "min_connections", uint32_t(5));
}

void DatabaseConfigManager::SetDefaultRedisConfig()
{
    SetValue("redis.default", "host", std::string("localhost"));
    SetValue("redis.default", "port", uint32_t(6379));
    SetValue("redis.default", "password", std::string(""));
    SetValue("redis.default", "database", uint32_t(0));
    SetValue("redis.default", "connection_timeout", uint32_t(30));
    SetValue("redis.default", "enable_ssl", false);
    SetValue("redis.default", "max_connections", uint32_t(100));
    SetValue("redis.default", "min_connections", uint32_t(5));
    SetValue("redis.default", "key_expire_seconds", uint32_t(3600));
}

void DatabaseConfigManager::SetDefaultPoolConfig()
{
    SetValue("pool.default", "min_connections", uint32_t(5));
    SetValue("pool.default", "max_connections", uint32_t(100));
    SetValue("pool.default", "connection_timeout_ms", uint32_t(30000));
    SetValue("pool.default", "idle_timeout_ms", uint32_t(300000));
    SetValue("pool.default", "validation_interval_ms", uint32_t(60000));
    SetValue("pool.default", "test_on_borrow", true);
    SetValue("pool.default", "test_on_return", false);
    SetValue("pool.default", "test_while_idle", true);
    SetValue("pool.default", "max_retries", uint32_t(3));
    SetValue("pool.default", "retry_delay_ms", uint32_t(1000));
}

bool DatabaseConfigManager::ValidateMySqlConfig(const std::string& ConnectionName) const
{
    MySqlConfig Config = GetMySqlConfig(ConnectionName);
    bool IsValid = true;

    if (Config.Host.empty())
    {
        ValidationErrors.push_back("MySQL host is empty for connection: " + ConnectionName);
        IsValid = false;
    }

    if (Config.Port == 0)
    {
        ValidationErrors.push_back("MySQL port is invalid for connection: " + ConnectionName);
        IsValid = false;
    }

    if (Config.Database.empty())
    {
        ValidationErrors.push_back("MySQL database is empty for connection: " + ConnectionName);
        IsValid = false;
    }

    return IsValid;
}

bool DatabaseConfigManager::ValidateMongoDbConfig(const std::string& ConnectionName) const
{
    MongoDbConfig Config = GetMongoDbConfig(ConnectionName);
    bool IsValid = true;

    if (Config.Host.empty())
    {
        ValidationErrors.push_back("MongoDB host is empty for connection: " + ConnectionName);
        IsValid = false;
    }

    if (Config.Port == 0)
    {
        ValidationErrors.push_back("MongoDB port is invalid for connection: " + ConnectionName);
        IsValid = false;
    }

    if (Config.Database.empty())
    {
        ValidationErrors.push_back("MongoDB database is empty for connection: " + ConnectionName);
        IsValid = false;
    }

    return IsValid;
}

bool DatabaseConfigManager::ValidateRedisConfig(const std::string& ConnectionName) const
{
    RedisConfig Config = GetRedisConfig(ConnectionName);
    bool IsValid = true;

    if (Config.Host.empty())
    {
        ValidationErrors.push_back("Redis host is empty for connection: " + ConnectionName);
        IsValid = false;
    }

    if (Config.Port == 0)
    {
        ValidationErrors.push_back("Redis port is invalid for connection: " + ConnectionName);
        IsValid = false;
    }

    return IsValid;
}

bool DatabaseConfigManager::ValidatePoolConfig(const std::string& ConnectionName) const
{
    ConnectionPoolConfig Config = GetPoolConfig(ConnectionName);
    bool IsValid = true;

    if (Config.MinConnections > Config.MaxConnections)
    {
        ValidationErrors.push_back("Pool min connections > max connections for: " + ConnectionName);
        IsValid = false;
    }

    if (Config.MaxConnections == 0)
    {
        ValidationErrors.push_back("Pool max connections is zero for: " + ConnectionName);
        IsValid = false;
    }

    return IsValid;
}

// GlobalDatabaseConfig implementation
std::unique_ptr<DatabaseConfigManager> GlobalDatabaseConfig::Instance = nullptr;
std::mutex GlobalDatabaseConfig::InstanceMutex;
bool GlobalDatabaseConfig::IsInitialized = false;

DatabaseConfigManager& GlobalDatabaseConfig::GetInstance()
{
    std::lock_guard<std::mutex> Lock(InstanceMutex);
    if (!Instance)
    {
        Instance = std::make_unique<DatabaseConfigManager>();
    }
    return *Instance;
}

Common::ResultCode GlobalDatabaseConfig::Initialize(const std::string& ConfigFilePath)
{
    std::lock_guard<std::mutex> Lock(InstanceMutex);

    if (IsInitialized)
    {
        return Common::ResultCode::ALREADY_INITIALIZED;
    }

    Instance = std::make_unique<DatabaseConfigManager>();

    Common::ResultCode Result = Common::ResultCode::SUCCESS;
    if (!ConfigFilePath.empty())
    {
        Result = Instance->LoadFromFile(ConfigFilePath);
    }
    else
    {
        Result = Instance->LoadFromEnvironment();
    }

    if (Result == Common::ResultCode::SUCCESS)
    {
        IsInitialized = true;
        HELIANTHUS_LOG_INFO("Global database configuration initialized");
    }

    return Result;
}

void GlobalDatabaseConfig::Shutdown()
{
    std::lock_guard<std::mutex> Lock(InstanceMutex);
    Instance.reset();
    IsInitialized = false;
    HELIANTHUS_LOG_INFO("Global database configuration shutdown");
}

// Explicit template instantiations
template bool
DatabaseConfigManager::GetValue<bool>(const std::string&, const std::string&, const bool&) const;
template int32_t DatabaseConfigManager::GetValue<int32_t>(const std::string&,
                                                          const std::string&,
                                                          const int32_t&) const;
template uint32_t DatabaseConfigManager::GetValue<uint32_t>(const std::string&,
                                                            const std::string&,
                                                            const uint32_t&) const;
template uint64_t DatabaseConfigManager::GetValue<uint64_t>(const std::string&,
                                                            const std::string&,
                                                            const uint64_t&) const;
template float
DatabaseConfigManager::GetValue<float>(const std::string&, const std::string&, const float&) const;
template double DatabaseConfigManager::GetValue<double>(const std::string&,
                                                        const std::string&,
                                                        const double&) const;
template std::string DatabaseConfigManager::GetValue<std::string>(const std::string&,
                                                                  const std::string&,
                                                                  const std::string&) const;

}  // namespace Helianthus::Database