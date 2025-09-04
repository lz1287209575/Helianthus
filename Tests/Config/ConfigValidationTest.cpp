#include "Shared/Config/ConfigManager.h"

#include <iostream>

#include <gtest/gtest.h>

using namespace Helianthus::Config;

class ConfigValidationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ConfigManager = std::make_unique<Helianthus::Config::ConfigManager>();
        ConfigManager->Initialize("test_config");
        ConfigManager->LoadDefaultConfig();
    }

    void TearDown() override
    {
        if (ConfigManager)
        {
            ConfigManager->Shutdown();
        }
    }

    std::unique_ptr<Helianthus::Config::ConfigManager> ConfigManager;
};

// 测试基本的配置验证功能
TEST_F(ConfigValidationTest, BasicValidation)
{
    std::cout << "开始基本验证测试..." << std::endl;

    // 设置一些基本配置
    ConfigManager->SetString("test.string", "value");
    ConfigManager->SetInt("test.int", 42);
    ConfigManager->SetBool("test.bool", true);

    std::cout << "配置设置完成，开始验证..." << std::endl;

    // 测试验证 - 这里可能会阻塞
    bool Result = ConfigManager->ValidateConfig();

    std::cout << "验证完成，结果: " << (Result ? "通过" : "失败") << std::endl;

    EXPECT_TRUE(Result);
}

// 测试单个配置项验证
TEST_F(ConfigValidationTest, SingleItemValidation)
{
    std::cout << "开始单个配置项验证测试..." << std::endl;

    ConfigManager->SetString("test.item", "value");

    std::cout << "开始验证单个配置项..." << std::endl;

    // 测试单个配置项验证
    bool Result = ConfigManager->ValidateConfigItem("test.item");

    std::cout << "单个配置项验证完成，结果: " << (Result ? "通过" : "失败") << std::endl;

    EXPECT_TRUE(Result);
}

// 测试空配置验证
TEST_F(ConfigValidationTest, EmptyConfigValidation)
{
    std::cout << "开始空配置验证测试..." << std::endl;

    // 创建一个新的空配置管理器
    auto EmptyConfigManager = std::make_unique<Helianthus::Config::ConfigManager>();
    EmptyConfigManager->Initialize("empty_config");

    std::cout << "开始验证空配置..." << std::endl;

    bool Result = EmptyConfigManager->ValidateConfig();

    std::cout << "空配置验证完成，结果: " << (Result ? "通过" : "失败") << std::endl;

    EmptyConfigManager->Shutdown();

    // 空配置应该是有效的
    EXPECT_TRUE(Result);
}

// 测试配置值验证
TEST_F(ConfigValidationTest, ConfigValueValidation)
{
    std::cout << "开始配置值验证测试..." << std::endl;

    // 测试不同类型的配置值
    ConfigValue StringValue("test");
    ConfigValue IntValue(static_cast<int64_t>(42));
    ConfigValue FloatValue(3.14);
    ConfigValue BoolValue(true);

    std::cout << "测试字符串值验证..." << std::endl;
    EXPECT_TRUE(StringValue.IsValid());

    std::cout << "测试整数值验证..." << std::endl;
    EXPECT_TRUE(IntValue.IsValid());

    std::cout << "测试浮点数值验证..." << std::endl;
    EXPECT_TRUE(FloatValue.IsValid());

    std::cout << "测试布尔值验证..." << std::endl;
    EXPECT_TRUE(BoolValue.IsValid());

    std::cout << "配置值验证测试完成" << std::endl;
}

// 测试验证器回调
TEST_F(ConfigValidationTest, ValidatorCallback)
{
    std::cout << "开始验证器回调测试..." << std::endl;

    bool CallbackCalled = false;

    // 添加一个简单的验证器
    ConfigValidator Validator = [&CallbackCalled](const std::string& Key,
                                                  const ConfigValue& Value) -> bool
    {
        std::cout << "验证器被调用: " << Key << std::endl;
        CallbackCalled = true;
        return true;
    };

    ConfigManager->AddValidator("test.validator", Validator);
    ConfigManager->SetString("test.validator", "value");

    std::cout << "开始验证带验证器的配置..." << std::endl;

    bool Result = ConfigManager->ValidateConfig();

    std::cout << "验证器回调测试完成，结果: " << (Result ? "通过" : "失败") << std::endl;

    EXPECT_TRUE(Result);
    EXPECT_TRUE(CallbackCalled);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
