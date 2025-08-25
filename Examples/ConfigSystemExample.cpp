#include "Shared/Config/ConfigManager.h"
#include "Shared/Common/StructuredLogger.h"
#include "Shared/Common/CommandLineParser.h"
#include "Shared/Network/WinSockInit.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace Helianthus::Config;
using namespace Helianthus::Common;

// 配置验证器示例
bool ValidatePort(const std::string& Key, const ConfigValue& Value, std::string& ErrorMessage)
{
    if (std::holds_alternative<int32_t>(Value))
    {
        int32_t Port = std::get<int32_t>(Value);
        if (Port < 1 || Port > 65535)
        {
            ErrorMessage = "Port must be between 1 and 65535";
            return false;
        }
    }
    else
    {
        ErrorMessage = "Port must be an integer";
        return false;
    }
    return true;
}

// 配置变更回调示例
void OnServerPortChanged(const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue)
{
    std::cout << "Server port changed from " << std::get<int32_t>(OldValue) 
              << " to " << std::get<int32_t>(NewValue) << std::endl;
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

// 设置命令行参数
void SetupCommandLineArgs(CommandLineParser& Parser)
{
    // 配置文件路径
    HELIANTHUS_CLI_STRING(Parser, "c", "config", 
        "配置文件路径", false, "Examples/config_example.json");
    
    // 热更新间隔
    HELIANTHUS_CLI_INTEGER(Parser, "i", "interval", 
        "热更新检查间隔（毫秒）", false, "500");
    
    // 启用热更新
    HELIANTHUS_CLI_FLAG(Parser, "r", "hot-reload", 
        "启用配置文件热更新");
    
    // 启用配置验证
    HELIANTHUS_CLI_FLAG(Parser, "v", "validate", 
        "启用配置验证");
    
    // 详细输出
    HELIANTHUS_CLI_FLAG(Parser, "d", "verbose", 
        "启用详细输出");
    
    // 运行时间
    HELIANTHUS_CLI_INTEGER(Parser, "t", "time", 
        "程序运行时间（秒）", false, "10");
    
    // 保存配置
    HELIANTHUS_CLI_STRING(Parser, "s", "save", 
        "保存配置到指定文件", false, "");
    
    // 显示帮助
    HELIANTHUS_CLI_FLAG(Parser, "h", "help", 
        "显示帮助信息");
}

int main(int argc, char* argv[])
{
    // 设置命令行参数解析器
    CommandLineParser Parser;
    SetupCommandLineArgs(Parser);
    
    // 解析命令行参数
    if (!Parser.Parse(argc, argv))
    {
        if (!Parser.GetLastError().empty())
        {
            std::cerr << "参数解析错误: " << Parser.GetLastError() << std::endl;
            std::cerr << "使用 --help 查看帮助信息" << std::endl;
            return 1;
        }
        return 0; // 显示帮助后退出
    }
    
    // 检查解析是否有效
    if (!Parser.IsValid())
    {
        std::cerr << "参数解析失败: " << Parser.GetLastError() << std::endl;
        std::cerr << "使用 --help 查看帮助信息" << std::endl;
        return 1;
    }
    
    // 获取命令行参数
    std::string ConfigFile = Parser.GetString("config");
    int32_t HotReloadInterval = Parser.GetInteger("interval");
    bool EnableHotReload = Parser.HasFlag("hot-reload");
    bool EnableValidation = Parser.HasFlag("validate");
    bool Verbose = Parser.HasFlag("verbose");
    int32_t RunTime = Parser.GetInteger("time");
    std::string SaveFile = Parser.GetString("save");
    
    if (Verbose)
    {
        std::cout << "=== Helianthus配置系统示例 ===" << std::endl;
        std::cout << "命令行参数:" << std::endl;
        std::cout << "  配置文件: " << ConfigFile << std::endl;
        std::cout << "  热更新间隔: " << HotReloadInterval << "ms" << std::endl;
        std::cout << "  启用热更新: " << (EnableHotReload ? "是" : "否") << std::endl;
        std::cout << "  启用验证: " << (EnableValidation ? "是" : "否") << std::endl;
        std::cout << "  运行时间: " << RunTime << "秒" << std::endl;
        if (!SaveFile.empty())
        {
            std::cout << "  保存文件: " << SaveFile << std::endl;
        }
        std::cout << std::endl;
    }
    
    // 确保Windows Socket初始化
    Helianthus::Network::EnsureWinSockInitialized();
    
    // 初始化结构化日志系统
    StructuredLoggerConfig LogConfig;
    LogConfig.EnableConsole = true;
    LogConfig.EnableFile = false; // 简化示例，不写文件
    LogConfig.MinLevel = Verbose ? Helianthus::Common::StructuredLogLevel::DEBUG_LEVEL : Helianthus::Common::StructuredLogLevel::INFO;
    StructuredLogger::Initialize(LogConfig);
    
    try
    {
        // 1. 初始化配置系统
        if (Verbose)
        {
            std::cout << "1. 初始化配置系统..." << std::endl;
        }
        
        // 获取配置文件路径
        std::string ConfigFilePath = GetConfigFilePath(ConfigFile);
        
        ConfigManagerConfig Config;
        Config.EnableHotReload = EnableHotReload;
        Config.HotReloadInterval = std::chrono::milliseconds(HotReloadInterval);
        Config.EnableConfigValidation = EnableValidation;
        
        bool InitSuccess = GlobalConfig::Initialize(ConfigFilePath, ConfigFormat::AUTO_DETECT, Config);
        if (!InitSuccess)
        {
            std::cerr << "配置系统初始化失败: " << ConfigManager::Instance().GetLastError() << std::endl;
            std::cerr << "请检查配置文件是否存在: " << ConfigFilePath << std::endl;
            std::cerr << "当前工作目录: " << std::filesystem::current_path().string() << std::endl;
            std::cerr << "使用 --help 查看帮助信息" << std::endl;
            return 1;
        }
        
        if (Verbose)
        {
            std::cout << "配置系统初始化成功，配置源: " << ConfigManager::Instance().GetConfigSource() << std::endl;
        }
        
        // 2. 注册配置验证器
        if (Verbose)
        {
            std::cout << "\n2. 注册配置验证器..." << std::endl;
        }
        ConfigManager::Instance().RegisterValidator(R"(.*\.port)", ValidatePort);
        if (Verbose)
        {
            std::cout << "已注册端口验证器" << std::endl;
        }
        
        // 3. 注册配置变更回调
        if (Verbose)
        {
            std::cout << "\n3. 注册配置变更回调..." << std::endl;
        }
        ConfigManager::Instance().RegisterChangeCallback("server.port", OnServerPortChanged);
        if (Verbose)
        {
            std::cout << "已注册服务器端口变更回调" << std::endl;
        }
        
        // 4. 读取配置示例
        if (Verbose)
        {
            std::cout << "\n4. 读取配置示例..." << std::endl;
        }
        
        // 读取服务器配置
        std::string ServerHost = HELIANTHUS_CONFIG_GET("server.host", std::string("localhost"));
        int32_t ServerPort = HELIANTHUS_CONFIG_GET("server.port", 8080);
        int32_t MaxConnections = HELIANTHUS_CONFIG_GET("server.max_connections", 100);
        bool EnableSSL = HELIANTHUS_CONFIG_GET("server.enable_ssl", false);
        
        std::cout << "服务器配置:" << std::endl;
        std::cout << "  主机: " << ServerHost << std::endl;
        std::cout << "  端口: " << ServerPort << std::endl;
        std::cout << "  最大连接数: " << MaxConnections << std::endl;
        std::cout << "  启用SSL: " << (EnableSSL ? "是" : "否") << std::endl;
        
        // 读取数据库配置
        std::string DbType = HELIANTHUS_CONFIG_GET("database.type", std::string("mysql"));
        std::string DbHost = HELIANTHUS_CONFIG_GET("database.host", std::string("localhost"));
        int32_t DbPort = HELIANTHUS_CONFIG_GET("database.port", 3306);
        int32_t PoolSize = HELIANTHUS_CONFIG_GET("database.max_pool_size", 10);
        
        std::cout << "\n数据库配置:" << std::endl;
        std::cout << "  类型: " << DbType << std::endl;
        std::cout << "  主机: " << DbHost << std::endl;
        std::cout << "  端口: " << DbPort << std::endl;
        std::cout << "  连接池大小: " << PoolSize << std::endl;
        
        // 5. 获取配置节示例
        if (Verbose)
        {
            std::cout << "\n5. 获取配置节示例..." << std::endl;
            auto LoggingConfig = ConfigManager::Instance().GetSection("logging");
            std::cout << "日志配置节包含 " << LoggingConfig.size() << " 个配置项:" << std::endl;
            for (const auto& [Key, Value] : LoggingConfig)
            {
                std::cout << "  " << Key << " = ";
                std::visit([](const auto& Val) { std::cout << Val; }, Value);
                std::cout << std::endl;
            }
        }
        
        // 6. 修改配置示例
        if (Verbose)
        {
            std::cout << "\n6. 修改配置示例..." << std::endl;
        }
        
        // 尝试设置有效端口
        std::cout << "设置服务器端口为9090..." << std::endl;
        bool SetSuccess = HELIANTHUS_CONFIG_SET("server.port", 9090);
        if (SetSuccess)
        {
            std::cout << "端口设置成功，新端口: " << HELIANTHUS_CONFIG_GET("server.port", 8080) << std::endl;
        }
        else
        {
            std::cout << "端口设置失败: " << ConfigManager::Instance().GetLastError() << std::endl;
        }
        
        // 尝试设置无效端口（触发验证失败）
        if (Verbose)
        {
            std::cout << "\n尝试设置无效端口70000..." << std::endl;
            bool InvalidSetSuccess = HELIANTHUS_CONFIG_SET("server.port", 70000);
            if (!InvalidSetSuccess)
            {
                std::cout << "端口设置失败（预期）: " << ConfigManager::Instance().GetLastError() << std::endl;
            }
        }
        
        // 7. 配置统计信息
        if (Verbose)
        {
            std::cout << "\n7. 配置统计信息..." << std::endl;
            auto Stats = ConfigManager::Instance().GetStats();
            std::cout << "总配置键数: " << Stats.TotalKeys << std::endl;
            std::cout << "重载次数: " << Stats.ReloadCount << std::endl;
            std::cout << "保存次数: " << Stats.SaveCount << std::endl;
            std::cout << "验证错误数: " << Stats.ValidationErrors << std::endl;
        }
        
        // 8. 热更新测试（模拟）
        if (EnableHotReload)
        {
            std::cout << "\n8. 热更新功能测试..." << std::endl;
            std::cout << "配置系统正在后台监控文件变更..." << std::endl;
            std::cout << "您可以修改 " << ConfigFilePath << " 文件来测试热更新功能" << std::endl;
            std::cout << "程序将运行" << RunTime << "秒来演示热更新..." << std::endl;
            
            for (int i = 0; i < RunTime; ++i)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                // 每秒检查配置是否有变化
                int32_t CurrentPort = HELIANTHUS_CONFIG_GET("server.port", 8080);
                if (i == 0)
                {
                    std::cout << "当前端口: " << CurrentPort << " (如果修改配置文件，这个值会自动更新)" << std::endl;
                }
            }
        }
        
        // 9. 保存配置
        if (!SaveFile.empty())
        {
            std::cout << "\n9. 保存配置到指定文件..." << std::endl;
            bool SaveSuccess = ConfigManager::Instance().SaveConfig(SaveFile);
            if (SaveSuccess)
            {
                std::cout << "配置已保存到 " << SaveFile << std::endl;
            }
            else
            {
                std::cout << "配置保存失败: " << ConfigManager::Instance().GetLastError() << std::endl;
            }
        }
        
        std::cout << "\n=== 配置系统示例完成 ===" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "示例运行出错: " << e.what() << std::endl;
        
        // 记录错误日志
        LogFields ErrorFields;
        ErrorFields.AddField("error", e.what());
        StructuredLogger::Error("CONFIG_EXAMPLE", "Example execution failed", ErrorFields);
        
        return 1;
    }
    
    // 清理资源
    GlobalConfig::Shutdown();
    StructuredLogger::Shutdown();
    
    return 0;
}
