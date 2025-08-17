#pragma once

#include "IDatabase.h"
#include "DatabaseConfig.h"
#include "../Common/Types.h"
#include <memory>

namespace Helianthus::Database
{
    // Database factory implementation
    class DatabaseFactory : public IDatabaseFactory
    {
    public:
        DatabaseFactory() = default;
        virtual ~DatabaseFactory() = default;

        // IDatabaseFactory interface
        std::shared_ptr<IDatabase> CreateMySqlDatabase(const MySqlConfig& Config) override;
        std::shared_ptr<IDatabase> CreateMongoDbDatabase(const MongoDbConfig& Config) override;
        std::shared_ptr<IDatabase> CreateRedisDatabase(const RedisConfig& Config) override;

        bool IsSupported(DatabaseType Type) const override;
        std::vector<DatabaseType> GetSupportedTypes() const override;

        // Extended factory methods with configuration names
        std::shared_ptr<IDatabase> CreateDatabase(DatabaseType Type, const std::string& ConnectionName = "default");
        std::shared_ptr<IDatabase> CreateDatabaseFromConfig(const std::string& ConnectionName = "default");

        // Utility methods
        static std::shared_ptr<DatabaseFactory> CreateDefault();
        static Common::ResultCode RegisterCustomFactory(DatabaseType Type, 
            std::function<std::shared_ptr<IDatabase>(const std::string&)> FactoryFunction);

    private:
        static std::map<DatabaseType, std::function<std::shared_ptr<IDatabase>(const std::string&)>> CustomFactories;
    };

    // Database manager - manages multiple database connections
    class DatabaseManager
    {
    public:
        DatabaseManager();
        virtual ~DatabaseManager();

        // Lifecycle
        Common::ResultCode Initialize(const std::string& ConfigFilePath = "");
        void Shutdown();
        bool IsInitialized() const;

        // Database management
        Common::ResultCode RegisterDatabase(const std::string& Name, DatabaseType Type, const std::string& ConnectionName = "default");
        std::shared_ptr<IDatabase> GetDatabase(const std::string& Name);
        bool HasDatabase(const std::string& Name) const;
        void RemoveDatabase(const std::string& Name);

        // Convenience methods for default databases
        std::shared_ptr<IDatabase> GetMySqlDatabase(const std::string& ConnectionName = "default");
        std::shared_ptr<IDatabase> GetMongoDbDatabase(const std::string& ConnectionName = "default");
        std::shared_ptr<IDatabase> GetRedisDatabase(const std::string& ConnectionName = "default");

        // Health monitoring
        bool AreAllDatabasesHealthy() const;
        std::map<std::string, bool> GetDatabaseHealthStatus() const;
        Common::ResultCode TestAllDatabases();

        // Configuration management
        DatabaseConfigManager& GetConfigManager();
        Common::ResultCode ReloadConfiguration();

        // Statistics
        struct DatabaseStats
        {
            std::string Name;
            DatabaseType Type;
            bool IsHealthy;
            uint32_t ActiveConnections;
            uint32_t TotalConnections;
            uint64_t QueryCount;
            uint64_t ErrorCount;
        };

        std::vector<DatabaseStats> GetAllDatabaseStats() const;

    private:
        std::shared_ptr<DatabaseFactory> Factory;
        std::map<std::string, std::shared_ptr<IDatabase>> Databases;
        std::map<std::string, DatabaseType> DatabaseTypes;
        std::map<std::string, std::string> DatabaseConnectionNames;
        std::unique_ptr<DatabaseConfigManager> ConfigManager;
        
        mutable std::mutex DatabasesMutex;
        std::atomic<bool> IsInitializedFlag;

        // Helper methods
        std::shared_ptr<IDatabase> CreateDatabaseInternal(DatabaseType Type, const std::string& ConnectionName);
    };

    // Global database manager instance
    class GlobalDatabaseManager
    {
    public:
        static DatabaseManager& GetInstance();
        static Common::ResultCode Initialize(const std::string& ConfigFilePath = "");
        static void Shutdown();

        // Convenience accessors
        static std::shared_ptr<IDatabase> GetMySql(const std::string& ConnectionName = "default");
        static std::shared_ptr<IDatabase> GetMongoDb(const std::string& ConnectionName = "default");
        static std::shared_ptr<IDatabase> GetRedis(const std::string& ConnectionName = "default");

    private:
        static std::unique_ptr<DatabaseManager> Instance;
        static std::mutex InstanceMutex;
        static bool IsInitialized;
    };

} // namespace Helianthus::Database