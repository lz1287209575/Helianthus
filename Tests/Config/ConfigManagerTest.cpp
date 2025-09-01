#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "Shared/Config/ConfigManager.h"

using namespace Helianthus::Config;

class ConfigManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Manager = std::make_unique<ConfigManager>();
        Manager->Initialize("test_config");
    }

    void TearDown() override
    {
        if (Manager)
        {
            Manager->Shutdown();
        }
    }

    std::unique_ptr<ConfigManager> Manager;
};

// 基本功能测试
TEST_F(ConfigManagerTest, BasicInitialization)
{
    EXPECT_TRUE(Manager->IsInitialized());
    EXPECT_EQ(Manager->GetConfigItemCount(), 0);
}

TEST_F(ConfigManagerTest, SetAndGetString)
{
    EXPECT_TRUE(Manager->SetString("test.key", "test_value"));
    EXPECT_EQ(Manager->GetString("test.key"), "test_value");
    EXPECT_EQ(Manager->GetString("test.key", "default"), "test_value");
    EXPECT_EQ(Manager->GetString("nonexistent.key", "default"), "default");
}

TEST_F(ConfigManagerTest, SetAndGetInt)
{
    EXPECT_TRUE(Manager->SetInt("test.int", 42));
    EXPECT_EQ(Manager->GetInt("test.int"), 42);
    EXPECT_EQ(Manager->GetInt("test.int", 100), 42);
    EXPECT_EQ(Manager->GetInt("nonexistent.int", 100), 100);
}

TEST_F(ConfigManagerTest, SetAndGetFloat)
{
    EXPECT_TRUE(Manager->SetFloat("test.float", 3.14));
    EXPECT_DOUBLE_EQ(Manager->GetFloat("test.float"), 3.14);
    EXPECT_DOUBLE_EQ(Manager->GetFloat("test.float", 2.71), 3.14);
    EXPECT_DOUBLE_EQ(Manager->GetFloat("nonexistent.float", 2.71), 2.71);
}

TEST_F(ConfigManagerTest, SetAndGetBool)
{
    EXPECT_TRUE(Manager->SetBool("test.bool", true));
    EXPECT_TRUE(Manager->GetBool("test.bool"));
    EXPECT_TRUE(Manager->GetBool("test.bool", false));
    
    EXPECT_TRUE(Manager->SetBool("test.bool2", false));
    EXPECT_FALSE(Manager->GetBool("test.bool2"));
    EXPECT_FALSE(Manager->GetBool("test.bool2", true));
    
    EXPECT_FALSE(Manager->GetBool("nonexistent.bool", false));
}

TEST_F(ConfigManagerTest, SetAndGetArray)
{
    std::vector<std::string> TestArray = {"item1", "item2", "item3"};
    EXPECT_TRUE(Manager->SetArray("test.array", TestArray));
    
    auto RetrievedArray = Manager->GetArray("test.array");
    EXPECT_EQ(RetrievedArray.size(), 3);
    EXPECT_EQ(RetrievedArray[0], "item1");
    EXPECT_EQ(RetrievedArray[1], "item2");
    EXPECT_EQ(RetrievedArray[2], "item3");
}

TEST_F(ConfigManagerTest, SetAndGetObject)
{
    std::unordered_map<std::string, std::string> TestObject = {
        {"key1", "value1"},
        {"key2", "value2"}
    };
    EXPECT_TRUE(Manager->SetObject("test.object", TestObject));
    
    auto RetrievedObject = Manager->GetObject("test.object");
    EXPECT_EQ(RetrievedObject.size(), 2);
    EXPECT_EQ(RetrievedObject["key1"], "value1");
    EXPECT_EQ(RetrievedObject["key2"], "value2");
}

// 配置项管理测试
TEST_F(ConfigManagerTest, AddAndRemoveConfigItem)
{
    std::string TestValue = "test_value";
    ConfigItem Item("test.item", ConfigValue(TestValue), "Test description");
    EXPECT_TRUE(Manager->AddConfigItem(Item));
    EXPECT_TRUE(Manager->HasConfigItem("test.item"));
    
    ConfigItem RetrievedItem = Manager->GetConfigItem("test.item");
    EXPECT_EQ(RetrievedItem.Key, "test.item");
    EXPECT_EQ(RetrievedItem.Value.AsString(), "test_value");
    EXPECT_EQ(RetrievedItem.Description, "Test description");
    
    EXPECT_TRUE(Manager->RemoveConfigItem("test.item"));
    EXPECT_FALSE(Manager->HasConfigItem("test.item"));
}

TEST_F(ConfigManagerTest, GetAllKeys)
{
    Manager->SetString("key1", "value1");
    Manager->SetString("key2", "value2");
    Manager->SetString("key3", "value3");
    
    auto Keys = Manager->GetAllKeys();
    EXPECT_EQ(Keys.size(), 3);
    EXPECT_TRUE(std::find(Keys.begin(), Keys.end(), "key1") != Keys.end());
    EXPECT_TRUE(std::find(Keys.begin(), Keys.end(), "key2") != Keys.end());
    EXPECT_TRUE(std::find(Keys.begin(), Keys.end(), "key3") != Keys.end());
}

// 配置验证测试
TEST_F(ConfigManagerTest, ConfigValidation)
{
    Manager->SetString("valid.key", "valid_value");
    EXPECT_TRUE(Manager->ValidateConfig());
    EXPECT_TRUE(Manager->ValidateConfigItem("valid.key"));
}

TEST_F(ConfigManagerTest, CustomValidator)
{
    bool ValidatorCalled = false;
    ConfigValidator Validator = [&](const std::string& Key, const ConfigValue& Value) {
        ValidatorCalled = true;
        return Value.AsString() == "expected_value";
    };
    
    Manager->AddValidator("test.key", Validator);
    
    EXPECT_TRUE(Manager->SetString("test.key", "expected_value"));
    EXPECT_TRUE(Manager->ValidateConfigItem("test.key"));
    
    EXPECT_FALSE(Manager->SetString("test.key", "wrong_value"));
    EXPECT_TRUE(ValidatorCalled);
}

// 配置变更回调测试
TEST_F(ConfigManagerTest, ChangeCallback)
{
    std::string ChangedKey;
    ConfigValue OldValue, NewValue;
    bool CallbackCalled = false;
    
    ConfigChangeCallback Callback = [&](const std::string& Key, const ConfigValue& Old, const ConfigValue& New) {
        ChangedKey = Key;
        OldValue = Old;
        NewValue = New;
        CallbackCalled = true;
    };
    
    Manager->AddChangeCallback("test.key", Callback);
    
    Manager->SetString("test.key", "initial_value");
    Manager->SetString("test.key", "new_value");
    
    EXPECT_TRUE(CallbackCalled);
    EXPECT_EQ(ChangedKey, "test.key");
    EXPECT_EQ(OldValue.AsString(), "initial_value");
    EXPECT_EQ(NewValue.AsString(), "new_value");
}

TEST_F(ConfigManagerTest, GlobalChangeCallback)
{
    std::vector<std::string> ChangedKeys;
    bool CallbackCalled = false;
    
    ConfigChangeCallback Callback = [&](const std::string& Key, const ConfigValue& Old, const ConfigValue& New) {
        ChangedKeys.push_back(Key);
        CallbackCalled = true;
    };
    
    Manager->AddGlobalChangeCallback(Callback);
    
    Manager->SetString("key1", "value1");
    Manager->SetString("key2", "value2");
    
    EXPECT_TRUE(CallbackCalled);
    EXPECT_EQ(ChangedKeys.size(), 2);
    EXPECT_EQ(ChangedKeys[0], "key1");
    EXPECT_EQ(ChangedKeys[1], "key2");
}

// 配置锁定测试
TEST_F(ConfigManagerTest, ConfigLocking)
{
    EXPECT_FALSE(Manager->IsConfigLocked());
    
    Manager->LockConfig();
    EXPECT_TRUE(Manager->IsConfigLocked());
    EXPECT_FALSE(Manager->SetString("test.key", "value"));
    
    Manager->UnlockConfig();
    EXPECT_FALSE(Manager->IsConfigLocked());
    EXPECT_TRUE(Manager->SetString("test.key", "value"));
}

// 配置模板测试
TEST_F(ConfigManagerTest, LoadDefaultConfig)
{
    Manager->LoadDefaultConfig();
    
    EXPECT_EQ(Manager->GetString("app.name"), "Helianthus");
    EXPECT_EQ(Manager->GetString("app.version"), "1.0.0");
    EXPECT_EQ(Manager->GetString("app.environment"), "development");
    EXPECT_TRUE(Manager->GetBool("app.debug"));
    EXPECT_EQ(Manager->GetInt("app.port"), 8080);
    EXPECT_EQ(Manager->GetString("app.host"), "localhost");
}

TEST_F(ConfigManagerTest, LoadMessageQueueConfig)
{
    Manager->LoadMessageQueueConfig();
    
    EXPECT_EQ(Manager->GetInt("messagequeue.max_size"), 10000);
    EXPECT_EQ(Manager->GetInt("messagequeue.max_size_bytes"), 100 * 1024 * 1024);
    EXPECT_EQ(Manager->GetInt("messagequeue.max_consumers"), 100);
    EXPECT_EQ(Manager->GetInt("messagequeue.max_producers"), 100);
    EXPECT_EQ(Manager->GetInt("messagequeue.message_ttl_ms"), 300000);
    EXPECT_EQ(Manager->GetInt("messagequeue.queue_ttl_ms"), 0);
    EXPECT_TRUE(Manager->GetBool("messagequeue.enable_dead_letter"));
    EXPECT_EQ(Manager->GetString("messagequeue.dead_letter_queue"), "dead_letter");
    EXPECT_EQ(Manager->GetInt("messagequeue.max_retries"), 3);
    EXPECT_EQ(Manager->GetInt("messagequeue.retry_delay_ms"), 1000);
    EXPECT_TRUE(Manager->GetBool("messagequeue.enable_retry_backoff"));
    EXPECT_DOUBLE_EQ(Manager->GetFloat("messagequeue.retry_backoff_multiplier"), 2.0);
    EXPECT_EQ(Manager->GetInt("messagequeue.max_retry_delay_ms"), 60000);
    EXPECT_EQ(Manager->GetInt("messagequeue.dead_letter_ttl_ms"), 86400000);
    EXPECT_FALSE(Manager->GetBool("messagequeue.enable_priority"));
    EXPECT_TRUE(Manager->GetBool("messagequeue.enable_batching"));
    EXPECT_EQ(Manager->GetInt("messagequeue.batch_size"), 100);
    EXPECT_EQ(Manager->GetInt("messagequeue.batch_timeout_ms"), 1000);
}

TEST_F(ConfigManagerTest, LoadNetworkConfig)
{
    Manager->LoadNetworkConfig();
    
    EXPECT_EQ(Manager->GetInt("network.max_connections"), 1000);
    EXPECT_EQ(Manager->GetInt("network.connection_timeout_ms"), 30000);
    EXPECT_EQ(Manager->GetInt("network.read_timeout_ms"), 60000);
    EXPECT_EQ(Manager->GetInt("network.write_timeout_ms"), 60000);
    EXPECT_EQ(Manager->GetInt("network.keep_alive_interval_ms"), 30000);
    EXPECT_EQ(Manager->GetInt("network.max_message_size"), 10 * 1024 * 1024);
    EXPECT_TRUE(Manager->GetBool("network.enable_compression"));
    EXPECT_FALSE(Manager->GetBool("network.enable_encryption"));
    EXPECT_EQ(Manager->GetString("network.compression_algorithm"), "gzip");
    EXPECT_EQ(Manager->GetString("network.encryption_algorithm"), "aes-256-gcm");
    EXPECT_EQ(Manager->GetInt("network.thread_pool_size"), 4);
    EXPECT_EQ(Manager->GetInt("network.max_pending_requests"), 1000);
}

TEST_F(ConfigManagerTest, LoadLoggingConfig)
{
    Manager->LoadLoggingConfig();
    
    EXPECT_EQ(Manager->GetString("logging.level"), "info");
    EXPECT_EQ(Manager->GetString("logging.format"), "json");
    EXPECT_EQ(Manager->GetString("logging.output"), "console");
    EXPECT_EQ(Manager->GetString("logging.file_path"), "logs/helianthus.log");
    EXPECT_EQ(Manager->GetInt("logging.max_file_size_mb"), 100);
    EXPECT_EQ(Manager->GetInt("logging.max_files"), 10);
    EXPECT_TRUE(Manager->GetBool("logging.enable_rotation"));
    EXPECT_TRUE(Manager->GetBool("logging.enable_timestamp"));
    EXPECT_TRUE(Manager->GetBool("logging.enable_thread_id"));
    EXPECT_TRUE(Manager->GetBool("logging.enable_color"));
}

TEST_F(ConfigManagerTest, LoadMonitoringConfig)
{
    Manager->LoadMonitoringConfig();
    
    EXPECT_TRUE(Manager->GetBool("monitoring.enable_metrics"));
    EXPECT_EQ(Manager->GetInt("monitoring.metrics_port"), 9090);
    EXPECT_EQ(Manager->GetString("monitoring.metrics_path"), "/metrics");
    EXPECT_TRUE(Manager->GetBool("monitoring.enable_health_check"));
    EXPECT_EQ(Manager->GetInt("monitoring.health_check_interval_ms"), 30000);
    EXPECT_FALSE(Manager->GetBool("monitoring.enable_tracing"));
    EXPECT_EQ(Manager->GetString("monitoring.tracing_endpoint"), "http://localhost:14268/api/traces");
    EXPECT_FALSE(Manager->GetBool("monitoring.enable_profiling"));
    EXPECT_EQ(Manager->GetInt("monitoring.profiling_port"), 6060);
}

// 配置导出测试
TEST_F(ConfigManagerTest, ExportToJson)
{
    Manager->SetString("key1", "value1");
    Manager->SetInt("key2", 42);
    Manager->SetBool("key3", true);
    
    std::string Json = Manager->ExportToJson();
    EXPECT_FALSE(Json.empty());
    EXPECT_TRUE(Json.find("\"key1\"") != std::string::npos);
    EXPECT_TRUE(Json.find("\"value1\"") != std::string::npos);
    EXPECT_TRUE(Json.find("\"key2\"") != std::string::npos);
    EXPECT_TRUE(Json.find("42") != std::string::npos);
    EXPECT_TRUE(Json.find("\"key3\"") != std::string::npos);
    EXPECT_TRUE(Json.find("true") != std::string::npos);
}

TEST_F(ConfigManagerTest, ExportToYaml)
{
    Manager->SetString("key1", "value1");
    Manager->SetInt("key2", 42);
    
    std::string Yaml = Manager->ExportToYaml();
    EXPECT_FALSE(Yaml.empty());
    EXPECT_TRUE(Yaml.find("key1: value1") != std::string::npos);
    EXPECT_TRUE(Yaml.find("key2: 42") != std::string::npos);
}

TEST_F(ConfigManagerTest, ExportToIni)
{
    Manager->SetString("key1", "value1");
    Manager->SetInt("key2", 42);
    
    std::string Ini = Manager->ExportToIni();
    EXPECT_FALSE(Ini.empty());
    EXPECT_TRUE(Ini.find("key1 = value1") != std::string::npos);
    EXPECT_TRUE(Ini.find("key2 = 42") != std::string::npos);
}

// 修改跟踪测试
TEST_F(ConfigManagerTest, ModifiedKeysTracking)
{
    Manager->SetString("key1", "value1");
    Manager->SetString("key2", "value2");
    
    auto ModifiedKeys = Manager->GetModifiedKeys();
    EXPECT_EQ(ModifiedKeys.size(), 2);
    EXPECT_TRUE(std::find(ModifiedKeys.begin(), ModifiedKeys.end(), "key1") != ModifiedKeys.end());
    EXPECT_TRUE(std::find(ModifiedKeys.begin(), ModifiedKeys.end(), "key2") != ModifiedKeys.end());
    
    Manager->ClearModifiedFlags();
    ModifiedKeys = Manager->GetModifiedKeys();
    EXPECT_EQ(ModifiedKeys.size(), 0);
}

// 热重载测试 (已移除)
TEST_F(ConfigManagerTest, HotReload)
{
    // 热重载功能已从简化版本中移除
    EXPECT_TRUE(true); // 占位符测试
}

// 全局配置访问测试
TEST_F(ConfigManagerTest, GlobalConfigAccess)
{
    EXPECT_TRUE(Global::InitializeConfig("test_global_config"));
    
    EXPECT_TRUE(Global::SetString("global.key", "global_value"));
    EXPECT_EQ(Global::GetString("global.key"), "global_value");
    EXPECT_EQ(Global::GetString("global.key", "default"), "global_value");
    
    EXPECT_TRUE(Global::SetInt("global.int", 123));
    EXPECT_EQ(Global::GetInt("global.int"), 123);
    EXPECT_EQ(Global::GetInt("global.int", 456), 123);
    
    EXPECT_TRUE(Global::SetFloat("global.float", 3.14159));
    EXPECT_DOUBLE_EQ(Global::GetFloat("global.float"), 3.14159);
    EXPECT_DOUBLE_EQ(Global::GetFloat("global.float", 2.71828), 3.14159);
    
    EXPECT_TRUE(Global::SetBool("global.bool", true));
    EXPECT_TRUE(Global::GetBool("global.bool"));
    EXPECT_TRUE(Global::GetBool("global.bool", false));
    
    EXPECT_TRUE(Global::ValidateConfig());
    
    Global::ShutdownConfig();
}

// 边界条件测试
TEST_F(ConfigManagerTest, EmptyKey)
{
    EXPECT_FALSE(Manager->SetString("", "value"));
    EXPECT_FALSE(Manager->HasConfigItem(""));
}

TEST_F(ConfigManagerTest, InvalidKey)
{
    EXPECT_FALSE(Manager->SetString("invalid key", "value"));
    EXPECT_FALSE(Manager->SetString("key@invalid", "value"));
    EXPECT_FALSE(Manager->SetString("key#invalid", "value"));
}

TEST_F(ConfigManagerTest, UninitializedManager)
{
    ConfigManager UninitManager;
    
    EXPECT_FALSE(UninitManager.IsInitialized());
    EXPECT_FALSE(UninitManager.SetString("key", "value"));
    EXPECT_EQ(UninitManager.GetString("key"), "");
    EXPECT_EQ(UninitManager.GetConfigItemCount(), 0);
}

// 类型转换测试
TEST_F(ConfigManagerTest, TypeConversion)
{
    // 字符串转数字
    Manager->SetString("string_int", "42");
    EXPECT_EQ(Manager->GetInt("string_int"), 42);
    
    Manager->SetString("string_float", "3.14");
    EXPECT_DOUBLE_EQ(Manager->GetFloat("string_float"), 3.14);
    
    // 数字转字符串
    Manager->SetInt("int_string", 123);
    EXPECT_EQ(Manager->GetString("int_string"), "123");
    
    Manager->SetFloat("float_string", 2.718);
    EXPECT_EQ(Manager->GetString("float_string"), "2.718");
    
    // 布尔值转换
    Manager->SetString("bool_true", "true");
    EXPECT_TRUE(Manager->GetBool("bool_true"));
    
    Manager->SetString("bool_false", "false");
    EXPECT_FALSE(Manager->GetBool("bool_false"));
    
    Manager->SetString("bool_1", "1");
    EXPECT_TRUE(Manager->GetBool("bool_1"));
    
    Manager->SetString("bool_0", "0");
    EXPECT_FALSE(Manager->GetBool("bool_0"));
    
    Manager->SetString("bool_yes", "yes");
    EXPECT_TRUE(Manager->GetBool("bool_yes"));
    
    Manager->SetString("bool_on", "on");
    EXPECT_TRUE(Manager->GetBool("bool_on"));
}

// 配置值验证测试
TEST_F(ConfigManagerTest, ConfigValueValidation)
{
    ConfigValue ValidString("valid");
    EXPECT_TRUE(ValidString.IsValid());
    
    std::string EmptyStr = "";
    ConfigValue EmptyString(EmptyStr);
    EXPECT_FALSE(EmptyString.IsValid());
    
    ConfigValue ValidInt(static_cast<int64_t>(42));
    EXPECT_TRUE(ValidInt.IsValid());
    
    ConfigValue ValidFloat(3.14);
    EXPECT_TRUE(ValidFloat.IsValid());
    
    ConfigValue ValidBool(true);
    EXPECT_TRUE(ValidBool.IsValid());
    
    ConfigValue ValidArray(std::vector<std::string>{"item1", "item2"});
    EXPECT_TRUE(ValidArray.IsValid());
    
    ConfigValue ValidObject(std::unordered_map<std::string, std::string>{{"key", "value"}});
    EXPECT_TRUE(ValidObject.IsValid());
}

// 配置模板类测试
TEST_F(ConfigManagerTest, ConfigTemplate)
{
    ConfigTemplate::LoadMessageQueueDefaults(*Manager);
    EXPECT_EQ(Manager->GetInt("messagequeue.max_size"), 10000);
    
    ConfigTemplate::LoadNetworkDefaults(*Manager);
    EXPECT_EQ(Manager->GetInt("network.max_connections"), 1000);
    
    ConfigTemplate::LoadLoggingDefaults(*Manager);
    EXPECT_EQ(Manager->GetString("logging.level"), "info");
    
    ConfigTemplate::LoadMonitoringDefaults(*Manager);
    EXPECT_TRUE(Manager->GetBool("monitoring.enable_metrics"));
    
    ConfigTemplate::LoadSecurityDefaults(*Manager);
    EXPECT_FALSE(Manager->GetBool("security.enable_ssl"));
    
    ConfigTemplate::LoadPerformanceDefaults(*Manager);
    EXPECT_EQ(Manager->GetInt("performance.thread_pool_size"), 4);
}
