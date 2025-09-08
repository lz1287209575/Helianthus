#include "Shared/Config/JsonConfigProvider.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <variant>

#include <gtest/gtest.h>

using namespace Helianthus::Config;

class ConfigSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 创建测试配置文件
        TestConfigPath = "TestConfig.json";
        CreateTestConfigFile();
    }

    void TearDown() override
    {
        // 清理测试文件
        if (std::filesystem::exists(TestConfigPath))
        {
            std::filesystem::remove(TestConfigPath);
        }
        if (std::filesystem::exists("TestConfigModified.json"))
        {
            std::filesystem::remove("TestConfigModified.json");
        }
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
        "port": 5432,
        "name": "testdb"
    },
    "logging": {
        "level": "info",
        "file": "app.log"
    }
})";
        File.close();
    }

    std::string TestConfigPath;
};

// 测试基本配置加载
TEST_F(ConfigSystemTest, BasicConfigLoading)
{
    JsonConfigProvider Provider;
    
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    ASSERT_TRUE(Provider.IsValid());

    // 测试获取配置值
    auto HostValue = Provider.GetValue("server.host");
    ASSERT_TRUE(HostValue.has_value());
    EXPECT_EQ(std::get<std::string>(HostValue.value()), "127.0.0.1");
    
    auto PortValue = Provider.GetValue("server.port");
    ASSERT_TRUE(PortValue.has_value());
    EXPECT_EQ(std::get<int32_t>(PortValue.value()), 8080);
    
    auto SslValue = Provider.GetValue("server.enable_ssl");
    ASSERT_TRUE(SslValue.has_value());
    EXPECT_FALSE(std::get<bool>(SslValue.value()));
}

// 测试配置值设置
TEST_F(ConfigSystemTest, ConfigValueSetting)
{
    JsonConfigProvider Provider;
    
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    ASSERT_TRUE(Provider.IsValid());

    // 设置新值
    ASSERT_TRUE(Provider.SetValue("server.port", 9090));
    auto NewPortValue = Provider.GetValue("server.port");
    ASSERT_TRUE(NewPortValue.has_value());
    EXPECT_EQ(std::get<int32_t>(NewPortValue.value()), 9090);

    // 设置字符串值
    ASSERT_TRUE(Provider.SetValue("server.host", std::string("192.168.1.1")));
    auto NewHostValue = Provider.GetValue("server.host");
    ASSERT_TRUE(NewHostValue.has_value());
    EXPECT_EQ(std::get<std::string>(NewHostValue.value()), "192.168.1.1");
}

// 测试配置键存在性检查
TEST_F(ConfigSystemTest, ConfigKeyExistence)
{
    JsonConfigProvider Provider;
    
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    ASSERT_TRUE(Provider.IsValid());

    // 检查存在的键
    EXPECT_TRUE(Provider.HasKey("server.host"));
    EXPECT_TRUE(Provider.HasKey("server.port"));
    EXPECT_TRUE(Provider.HasKey("database.name"));

    // 检查不存在的键
    EXPECT_FALSE(Provider.HasKey("nonexistent.key"));
    EXPECT_FALSE(Provider.HasKey("server.nonexistent"));
}

// 测试获取所有键
TEST_F(ConfigSystemTest, GetAllKeys)
{
    JsonConfigProvider Provider;
    
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    ASSERT_TRUE(Provider.IsValid());

    auto Keys = Provider.GetAllKeys();
    EXPECT_GT(Keys.size(), 0);

    // 检查一些预期的键
    bool HasServerHost = std::find(Keys.begin(), Keys.end(), "server.host") != Keys.end();
    bool HasServerPort = std::find(Keys.begin(), Keys.end(), "server.port") != Keys.end();
    bool HasDatabaseName = std::find(Keys.begin(), Keys.end(), "database.name") != Keys.end();

    EXPECT_TRUE(HasServerHost);
    EXPECT_TRUE(HasServerPort);
    EXPECT_TRUE(HasDatabaseName);
}

// 测试配置变更回调
TEST_F(ConfigSystemTest, ConfigChangeCallback)
{
    JsonConfigProvider Provider;
    
    ASSERT_TRUE(Provider.Load(TestConfigPath));
    ASSERT_TRUE(Provider.IsValid());

    bool CallbackCalled = false;
    std::string CallbackKey;
    ConfigValue CallbackOldValue;
    ConfigValue CallbackNewValue;

    // 注册变更回调
    Provider.RegisterChangeCallback(
        "server.port",
        [&](const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue)
        {
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

// 测试从字符串加载配置
TEST_F(ConfigSystemTest, LoadFromString)
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
    EXPECT_EQ(std::get<std::string>(StringValue.value()), "hello");
    
    auto IntValue = Provider.GetValue("test.int_value");
    ASSERT_TRUE(IntValue.has_value());
    EXPECT_EQ(std::get<int32_t>(IntValue.value()), 42);
    
    auto BoolValue = Provider.GetValue("test.bool_value");
    ASSERT_TRUE(BoolValue.has_value());
    EXPECT_TRUE(std::get<bool>(BoolValue.value()));
    
    auto DoubleValue = Provider.GetValue("test.double_value");
    ASSERT_TRUE(DoubleValue.has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(DoubleValue.value()), 3.14);
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
    std::string InvalidJson = R"({"invalid": json})";
    EXPECT_FALSE(Provider.LoadFromString(InvalidJson));
    EXPECT_FALSE(Provider.IsValid());
    EXPECT_FALSE(Provider.GetLastError().empty());

    // 测试获取不存在的键
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