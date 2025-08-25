#include <gtest/gtest.h>
#include "Shared/Config/ConfigManager.h"
#include "Shared/Config/JsonConfigProvider.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace Helianthus::Config;

class ConfigSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 创建测试配置文件
        TestConfigPath = "test_config.json";
        CreateTestConfigFile();
    }

    void TearDown() override
    {
        // 清理测试文件
        if (std::filesystem::exists(TestConfigPath))
        {
            std::filesystem::remove(TestConfigPath);
        }
        if (std::filesystem::exists("test_config_modified.json"))
        {
            std::filesystem::remove("test_config_modified.json");
        }
        
        // 确保配置管理器已关闭
        ConfigManager::Instance().Shutdown();
    }

    void CreateTestConfigFile()
    {
        std::ofstream File(TestConfigPath);
        File << R"({
    "server": {
        "host": "127.0.0.1",
        "port": 8080,
        "enable_ssl": false
    },
    "database": {
        "host": "localhost",
        "port": 3306,
        "name": "test_db"
    },
    "logging": {
        "level": "info",
        "enable_console": true
    }
})";
        File.close();
    }

    std::string TestConfigPath;
};

// 测试JsonConfigProvider基本功能
TEST_F(ConfigSystemTest, JsonConfigProviderBasicFunctionality)
{
    JsonConfigProvider Provider;
    
    // 测试加载配置
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    ASSERT_TRUE(Provider.IsValid());
    
    // 测试获取配置值
    auto ServerHost = Provider.GetValue("server.host");
    ASSERT_TRUE(ServerHost.has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(*ServerHost));
    EXPECT_EQ(std::get<std::string>(*ServerHost), "127.0.0.1");
    
    auto ServerPort = Provider.GetValue("server.port");
    ASSERT_TRUE(ServerPort.has_value());
    ASSERT_TRUE(std::holds_alternative<int32_t>(*ServerPort));
    EXPECT_EQ(std::get<int32_t>(*ServerPort), 8080);
    
    auto EnableSSL = Provider.GetValue("server.enable_ssl");
    ASSERT_TRUE(EnableSSL.has_value());
    ASSERT_TRUE(std::holds_alternative<bool>(*EnableSSL));
    EXPECT_FALSE(std::get<bool>(*EnableSSL));
    
    // 测试不存在的键
    auto NonExistent = Provider.GetValue("nonexistent.key");
    EXPECT_FALSE(NonExistent.has_value());
    
    // 测试HasKey
    EXPECT_TRUE(Provider.HasKey("server.host"));
    EXPECT_FALSE(Provider.HasKey("nonexistent.key"));
}

// 测试JsonConfigProvider设置配置值
TEST_F(ConfigSystemTest, JsonConfigProviderSetValue)
{
    JsonConfigProvider Provider;
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    
    // 设置现有键的值
    ASSERT_TRUE(Provider.SetValue("server.port", 9090));
    auto NewPort = Provider.GetValue("server.port");
    ASSERT_TRUE(NewPort.has_value());
    EXPECT_EQ(std::get<int32_t>(*NewPort), 9090);
    
    // 设置新键的值
    ASSERT_TRUE(Provider.SetValue("server.timeout", 30));
    auto Timeout = Provider.GetValue("server.timeout");
    ASSERT_TRUE(Timeout.has_value());
    EXPECT_EQ(std::get<int32_t>(*Timeout), 30);
    
    // 设置嵌套新键的值
    ASSERT_TRUE(Provider.SetValue("cache.redis.host", std::string("redis-server")));
    auto RedisHost = Provider.GetValue("cache.redis.host");
    ASSERT_TRUE(RedisHost.has_value());
    EXPECT_EQ(std::get<std::string>(*RedisHost), "redis-server");
}

// 测试JsonConfigProvider获取所有键
TEST_F(ConfigSystemTest, JsonConfigProviderGetAllKeys)
{
    JsonConfigProvider Provider;
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    
    auto AllKeys = Provider.GetAllKeys();
    EXPECT_GT(AllKeys.size(), 0);
    
    // 检查一些预期的键
    bool HasServerHost = std::find(AllKeys.begin(), AllKeys.end(), "server.host") != AllKeys.end();
    bool HasServerPort = std::find(AllKeys.begin(), AllKeys.end(), "server.port") != AllKeys.end();
    bool HasDatabaseHost = std::find(AllKeys.begin(), AllKeys.end(), "database.host") != AllKeys.end();
    
    EXPECT_TRUE(HasServerHost);
    EXPECT_TRUE(HasServerPort);
    EXPECT_TRUE(HasDatabaseHost);
}

// 测试JsonConfigProvider获取配置节
TEST_F(ConfigSystemTest, JsonConfigProviderGetSection)
{
    JsonConfigProvider Provider;
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    
    auto ServerSection = Provider.GetSection("server");
    EXPECT_GT(ServerSection.size(), 0);
    
    // 检查服务器配置节的内容
    EXPECT_TRUE(ServerSection.find("server.host") != ServerSection.end());
    EXPECT_TRUE(ServerSection.find("server.port") != ServerSection.end());
    EXPECT_TRUE(ServerSection.find("server.enable_ssl") != ServerSection.end());
    
    auto DatabaseSection = Provider.GetSection("database");
    EXPECT_GT(DatabaseSection.size(), 0);
    
    // 检查数据库配置节的内容
    EXPECT_TRUE(DatabaseSection.find("database.host") != DatabaseSection.end());
    EXPECT_TRUE(DatabaseSection.find("database.port") != DatabaseSection.end());
}

// 测试JsonConfigProvider保存功能
TEST_F(ConfigSystemTest, JsonConfigProviderSave)
{
    JsonConfigProvider Provider;
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    
    // 修改配置
    ASSERT_TRUE(Provider.SetValue("server.port", 9090));
    ASSERT_TRUE(Provider.SetValue("new_setting", std::string("test_value")));
    
    // 保存到新文件
    std::string SavePath = "test_config_modified.json";
    ASSERT_TRUE(Provider.Save(SavePath));
    
    // 验证保存的文件
    JsonConfigProvider NewProvider;
    ASSERT_TRUE(NewProvider.Load(SavePath));
    
    auto SavedPort = NewProvider.GetValue("server.port");
    ASSERT_TRUE(SavedPort.has_value());
    EXPECT_EQ(std::get<int32_t>(*SavedPort), 9090);
    
    auto SavedNewSetting = NewProvider.GetValue("new_setting");
    ASSERT_TRUE(SavedNewSetting.has_value());
    EXPECT_EQ(std::get<std::string>(*SavedNewSetting), "test_value");
}

// 测试配置变更回调
TEST_F(ConfigSystemTest, JsonConfigProviderChangeCallback)
{
    JsonConfigProvider Provider;
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    
    bool CallbackCalled = false;
    std::string CallbackKey;
    ConfigValue CallbackOldValue;
    ConfigValue CallbackNewValue;
    
    // 注册变更回调
    Provider.RegisterChangeCallback("server.port", 
        [&](const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue) {
            CallbackCalled = true;
            CallbackKey = Key;
            CallbackOldValue = OldValue;
            CallbackNewValue = NewValue;
        });
    
    // 修改配置值
    ASSERT_TRUE(Provider.SetValue("server.port", 9090));
    
    // 验证回调被调用
    EXPECT_TRUE(CallbackCalled);
    EXPECT_EQ(CallbackKey, "server.port");
    EXPECT_EQ(std::get<int32_t>(CallbackOldValue), 8080);
    EXPECT_EQ(std::get<int32_t>(CallbackNewValue), 9090);
}

// 测试ConfigManager基本功能
TEST_F(ConfigSystemTest, ConfigManagerBasicFunctionality)
{
    // 使用GlobalConfig初始化
    ConfigManagerConfig Config;
    Config.EnableHotReload = false; // 禁用热更新以简化测试
    Config.EnableConfigValidation = false; // 禁用验证以简化测试
    
    ASSERT_TRUE(GlobalConfig::Initialize(TestConfigPath, ConfigFormat::JSON, Config));
    ASSERT_TRUE(ConfigManager::Instance().IsInitialized());
    
    // 测试获取配置值
    std::string ServerHost = GlobalConfig::Get<std::string>("server.host", "default");
    EXPECT_EQ(ServerHost, "127.0.0.1");
    
    int32_t ServerPort = GlobalConfig::Get<int32_t>("server.port", 0);
    EXPECT_EQ(ServerPort, 8080);
    
    bool EnableSSL = GlobalConfig::Get<bool>("server.enable_ssl", true);
    EXPECT_FALSE(EnableSSL);
    
    // 测试设置配置值
    ASSERT_TRUE(GlobalConfig::Set("server.port", 9090));
    int32_t NewPort = GlobalConfig::Get<int32_t>("server.port", 0);
    EXPECT_EQ(NewPort, 9090);
    
    // 测试检查键是否存在
    EXPECT_TRUE(GlobalConfig::Has("server.host"));
    EXPECT_FALSE(GlobalConfig::Has("nonexistent.key"));
    
    GlobalConfig::Shutdown();
}

// 测试配置验证功能
TEST_F(ConfigSystemTest, ConfigManagerValidation)
{
    ConfigManagerConfig Config;
    Config.EnableHotReload = false;
    Config.EnableConfigValidation = true;
    
    ASSERT_TRUE(GlobalConfig::Initialize(TestConfigPath, ConfigFormat::JSON, Config));
    
    // 注册端口验证器
    ConfigManager::Instance().RegisterValidator(R"(.*\.port)", 
        [](const std::string& Key, const ConfigValue& Value, std::string& ErrorMessage) -> bool {
            if (std::holds_alternative<int32_t>(Value))
            {
                int32_t Port = std::get<int32_t>(Value);
                if (Port < 1 || Port > 65535)
                {
                    ErrorMessage = "Port must be between 1 and 65535";
                    return false;
                }
                return true;
            }
            ErrorMessage = "Port must be an integer";
            return false;
        });
    
    // 测试有效端口
    EXPECT_TRUE(GlobalConfig::Set("server.port", 9090));
    EXPECT_EQ(GlobalConfig::Get<int32_t>("server.port", 0), 9090);
    
    // 测试无效端口
    EXPECT_FALSE(GlobalConfig::Set("server.port", 70000));
    EXPECT_NE(GlobalConfig::Get<int32_t>("server.port", 0), 70000); // 应该保持原值
    
    // 测试验证所有配置
    std::vector<std::string> ValidationErrors;
    EXPECT_TRUE(ConfigManager::Instance().ValidateAllConfigs(ValidationErrors));
    EXPECT_TRUE(ValidationErrors.empty());
    
    GlobalConfig::Shutdown();
}

// 测试全局前缀功能
TEST_F(ConfigSystemTest, ConfigManagerGlobalPrefix)
{
    ConfigManagerConfig Config;
    Config.EnableHotReload = false;
    Config.EnableConfigValidation = false;
    
    ASSERT_TRUE(GlobalConfig::Initialize(TestConfigPath, ConfigFormat::JSON, Config));
    
    // 设置全局前缀
    ConfigManager::Instance().SetGlobalPrefix("app");
    
    // 设置一个配置值（应该自动加上前缀）
    ASSERT_TRUE(GlobalConfig::Set("test.key", std::string("test_value")));
    
    // 直接从Provider检查是否添加了前缀
    auto DirectValue = ConfigManager::Instance().GetValue("test.key");
    ASSERT_TRUE(DirectValue.has_value());
    EXPECT_EQ(std::get<std::string>(*DirectValue), "test_value");
    
    // 获取所有键应该不包含前缀
    auto AllKeys = ConfigManager::Instance().GetAllKeys();
    bool HasTestKey = std::find(AllKeys.begin(), AllKeys.end(), "test.key") != AllKeys.end();
    EXPECT_TRUE(HasTestKey);
    
    GlobalConfig::Shutdown();
}

// 测试配置统计功能
TEST_F(ConfigSystemTest, ConfigManagerStats)
{
    ConfigManagerConfig Config;
    Config.EnableHotReload = false;
    Config.EnableConfigValidation = true;
    
    ASSERT_TRUE(GlobalConfig::Initialize(TestConfigPath, ConfigFormat::JSON, Config));
    
    // 获取初始统计信息
    auto InitialStats = ConfigManager::Instance().GetStats();
    EXPECT_GT(InitialStats.TotalKeys, 0);
    EXPECT_EQ(InitialStats.ReloadCount, 0);
    EXPECT_EQ(InitialStats.SaveCount, 0);
    
    // 执行重新加载
    ASSERT_TRUE(GlobalConfig::Reload());
    auto StatsAfterReload = ConfigManager::Instance().GetStats();
    EXPECT_EQ(StatsAfterReload.ReloadCount, 1);
    
    // 执行保存
    ASSERT_TRUE(GlobalConfig::Save());
    auto StatsAfterSave = ConfigManager::Instance().GetStats();
    EXPECT_EQ(StatsAfterSave.SaveCount, 1);
    
    GlobalConfig::Shutdown();
}

// 测试从字符串加载JSON配置
TEST_F(ConfigSystemTest, JsonConfigProviderLoadFromString)
{
    JsonConfigProvider Provider;
    
    std::string JsonString = R"({
        "test": {
            "string_value": "hello",
            "int_value": 42,
            "bool_value": true,
            "double_value": 3.14
        }
    })";
    
    ASSERT_TRUE(Provider.LoadFromString(JsonString));
    ASSERT_TRUE(Provider.IsValid());
    
    // 验证加载的值
    auto StringValue = Provider.GetValue("test.string_value");
    ASSERT_TRUE(StringValue.has_value());
    EXPECT_EQ(std::get<std::string>(*StringValue), "hello");
    
    auto IntValue = Provider.GetValue("test.int_value");
    ASSERT_TRUE(IntValue.has_value());
    EXPECT_EQ(std::get<int32_t>(*IntValue), 42);
    
    auto BoolValue = Provider.GetValue("test.bool_value");
    ASSERT_TRUE(BoolValue.has_value());
    EXPECT_TRUE(std::get<bool>(*BoolValue));
    
    auto DoubleValue = Provider.GetValue("test.double_value");
    ASSERT_TRUE(DoubleValue.has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*DoubleValue), 3.14);
}

// 测试错误处理
TEST_F(ConfigSystemTest, ErrorHandling)
{
    JsonConfigProvider Provider;
    
    // 测试加载不存在的文件
    EXPECT_FALSE(Provider.Load("nonexistent_file.json"));
    EXPECT_FALSE(Provider.IsValid());
    EXPECT_FALSE(Provider.GetLastError().empty());
    
    // 测试加载无效JSON
    std::string InvalidJson = "{ invalid json }";
    EXPECT_FALSE(Provider.LoadFromString(InvalidJson));
    EXPECT_FALSE(Provider.IsValid());
    EXPECT_FALSE(Provider.GetLastError().empty());
    
    // 测试在无效状态下的操作
    EXPECT_FALSE(Provider.GetValue("any.key").has_value());
    EXPECT_FALSE(Provider.HasKey("any.key"));
    EXPECT_TRUE(Provider.GetAllKeys().empty());
    EXPECT_FALSE(Provider.SetValue("any.key", std::string("value")));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
