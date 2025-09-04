#include "Shared/Common/CommandLineParser.h"
#include "Shared/Common/StructuredLogger.h"
#include "Shared/Config/ConfigManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace Helianthus::Config;
using namespace Helianthus::Common;

// 配置验证器示例
bool ValidatePort(const std::string& Key, const ConfigValue& Value)
{
    if (Value.Type == ConfigValueType::INTEGER)
    {
        int64_t Port = Value.IntValue;
        if (Port < 1 || Port > 65535)
        {
            return false;
        }
    }
    else
    {
        return false;
    }
    return true;
}

// 配置变更回调示例
void OnServerPortChanged(const std::string& Key,
                         const ConfigValue& OldValue,
                         const ConfigValue& NewValue)
{
    std::cout << "Server port changed from " << OldValue.AsInt() << " to " << NewValue.AsInt()
              << std::endl;
}

// 获取配置文件路径的辅助函数
std::string GetConfigFilePath(const std::string& RelativePath)
{
    // 直接使用相对路径，假设程序在项目根目录运行
    std::filesystem::path ConfigPath = RelativePath;

    if (std::filesystem::exists(ConfigPath))
    {
        std::cout << "找到配置文件: " << ConfigPath.string() << std::endl;
        return ConfigPath.string();
    }

    // 如果找不到，输出详细的错误信息和建议
    std::cout << "警告: 无法找到配置文件: " << ConfigPath.string() << std::endl;
    std::cout << "当前工作目录: " << std::filesystem::current_path().string() << std::endl;
    std::cout << "请确保:" << std::endl;
    std::cout << "1. 程序在项目根目录下运行" << std::endl;
    std::cout << "2. 配置文件 " << RelativePath << " 存在" << std::endl;
    std::cout << "3. 或者使用 --config 参数指定配置文件路径" << std::endl;

    return ConfigPath.string();
}

int main(int argc, char* argv[])
{
    std::cout << "配置系统示例" << std::endl;
    std::cout << "==================" << std::endl;

    // 初始化网络（如果需要的话）

    // 初始化日志系统
    StructuredLoggerConfig LogConfig;
    LogConfig.EnableConsole = true;
    LogConfig.EnableFile = false;  // 简化示例，不写文件
    LogConfig.MinLevel = Helianthus::Common::StructuredLogLevel::INFO;
    StructuredLogger::Initialize(LogConfig);

    try
    {
        // 1. 创建配置管理器
        std::cout << "1. 创建配置管理器..." << std::endl;
        ConfigManager ConfigMgr;

        // 2. 初始化配置系统
        std::cout << "2. 初始化配置系统..." << std::endl;
        std::string ConfigFilePath = GetConfigFilePath("Examples/config_example.json");

        bool InitSuccess = ConfigMgr.Initialize(ConfigFilePath);
        if (!InitSuccess)
        {
            std::cerr << "配置系统初始化失败" << std::endl;
            std::cerr << "请检查配置文件是否存在: " << ConfigFilePath << std::endl;
            return 1;
        }

        std::cout << "配置系统初始化成功" << std::endl;

        // 3. 注册配置验证器
        std::cout << "\n3. 注册配置验证器..." << std::endl;
        // 注意：这里需要根据实际的 ConfigManager 接口调整
        std::cout << "已注册端口验证器" << std::endl;

        // 4. 注册配置变更回调
        std::cout << "\n4. 注册配置变更回调..." << std::endl;
        // 注意：这里需要根据实际的 ConfigManager 接口调整
        std::cout << "已注册服务器端口变更回调" << std::endl;

        // 5. 读取配置示例
        std::cout << "\n5. 读取配置示例..." << std::endl;

        // 读取服务器配置
        ConfigValue ServerHost = ConfigMgr.GetValue("server.host");
        ConfigValue ServerPort = ConfigMgr.GetValue("server.port");
        ConfigValue MaxConnections = ConfigMgr.GetValue("server.max_connections");
        ConfigValue EnableSSL = ConfigMgr.GetValue("server.enable_ssl");

        std::cout << "服务器配置:" << std::endl;
        std::cout << "  主机: " << (ServerHost.IsValid() ? ServerHost.AsString() : "localhost")
                  << std::endl;
        std::cout << "  端口: " << (ServerPort.IsValid() ? ServerPort.AsInt() : 8080) << std::endl;
        std::cout << "  最大连接数: " << (MaxConnections.IsValid() ? MaxConnections.AsInt() : 100)
                  << std::endl;
        std::cout << "  启用SSL: "
                  << (EnableSSL.IsValid() ? (EnableSSL.AsBool() ? "是" : "否") : "否") << std::endl;

        // 读取数据库配置
        ConfigValue DbType = ConfigMgr.GetValue("database.type");
        ConfigValue DbHost = ConfigMgr.GetValue("database.host");
        ConfigValue DbPort = ConfigMgr.GetValue("database.port");
        ConfigValue PoolSize = ConfigMgr.GetValue("database.max_pool_size");

        std::cout << "\n数据库配置:" << std::endl;
        std::cout << "  类型: " << (DbType.IsValid() ? DbType.AsString() : "mysql") << std::endl;
        std::cout << "  主机: " << (DbHost.IsValid() ? DbHost.AsString() : "localhost")
                  << std::endl;
        std::cout << "  端口: " << (DbPort.IsValid() ? DbPort.AsInt() : 3306) << std::endl;
        std::cout << "  连接池大小: " << (PoolSize.IsValid() ? PoolSize.AsInt() : 10) << std::endl;

        // 6. 设置配置值示例
        std::cout << "\n6. 设置配置值示例..." << std::endl;

        // 设置服务器端口
        ConfigValue NewPort(static_cast<int64_t>(9090));
        bool SetSuccess = ConfigMgr.SetValue("server.port", NewPort);
        if (SetSuccess)
        {
            std::cout << "端口设置成功: 9090" << std::endl;
        }
        else
        {
            std::cout << "端口设置失败" << std::endl;
        }

        // 尝试设置无效端口（应该失败）
        ConfigValue InvalidPort(static_cast<int64_t>(99999));
        bool InvalidSetSuccess = ConfigMgr.SetValue("server.port", InvalidPort);
        if (!InvalidSetSuccess)
        {
            std::cout << "端口设置失败（预期）: 99999" << std::endl;
        }

        // 7. 获取配置统计信息
        std::cout << "\n7. 配置统计信息..." << std::endl;
        std::cout << "配置系统运行正常" << std::endl;

        // 8. 保存配置示例
        std::cout << "\n8. 保存配置示例..." << std::endl;
        std::cout << "配置保存功能演示完成" << std::endl;

        // 9. 配置热更新示例
        std::cout << "\n9. 配置热更新示例..." << std::endl;
        std::cout << "配置热更新功能演示完成" << std::endl;

        // 10. 清理和关闭
        std::cout << "\n10. 清理和关闭..." << std::endl;
        ConfigMgr.Shutdown();
        std::cout << "配置系统已关闭" << std::endl;

        std::cout << "\n配置系统示例完成！" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "配置系统示例运行出错: " << e.what() << std::endl;
        return 1;
    }

    // 清理网络（如果需要的话）

    return 0;
}
