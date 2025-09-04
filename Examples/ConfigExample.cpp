#include "Shared/Common/Logger.h"
#include "Shared/Config/ConfigManager.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

using namespace Helianthus::Config;

int main()
{
    std::cout << "=== Helianthus Configuration Management Example ===" << std::endl;

    // 1. 初始化配置管理器
    std::cout << "\n1. 初始化配置管理器..." << std::endl;
    auto ConfigManager = std::make_unique<Helianthus::Config::ConfigManager>();

    if (!ConfigManager->Initialize("config"))
    {
        std::cerr << "Failed to initialize config manager" << std::endl;
        return 1;
    }

    std::cout << "配置管理器初始化成功" << std::endl;

    // 2. 加载默认配置
    std::cout << "\n2. 加载默认配置..." << std::endl;
    ConfigManager->LoadDefaultConfig();
    ConfigManager->LoadMessageQueueConfig();
    ConfigManager->LoadNetworkConfig();
    ConfigManager->LoadLoggingConfig();
    ConfigManager->LoadMonitoringConfig();

    std::cout << "默认配置加载完成，配置项数量: " << ConfigManager->GetConfigItemCount()
              << std::endl;

    // 3. 从文件加载配置
    std::cout << "\n3. 从文件加载配置..." << std::endl;
    if (ConfigManager->LoadFromFile("config/helianthus.conf"))
    {
        std::cout << "配置文件加载成功" << std::endl;
    }
    else
    {
        std::cout << "配置文件加载失败，使用默认配置" << std::endl;
    }

    // 4. 从环境变量加载配置
    std::cout << "\n4. 从环境变量加载配置..." << std::endl;
    ConfigManager->LoadFromEnvironment();
    std::cout << "环境变量配置加载完成" << std::endl;

    // 5. 演示配置访问
    std::cout << "\n5. 配置访问演示..." << std::endl;

    // 基本配置
    std::cout << "应用名称: " << ConfigManager->GetString("app.name", "Unknown") << std::endl;
    std::cout << "应用版本: " << ConfigManager->GetString("app.version", "Unknown") << std::endl;
    std::cout << "应用环境: " << ConfigManager->GetString("app.environment", "Unknown")
              << std::endl;
    std::cout << "调试模式: " << (ConfigManager->GetBool("app.debug", false) ? "开启" : "关闭")
              << std::endl;
    std::cout << "服务端口: " << ConfigManager->GetInt("app.port", 8080) << std::endl;
    std::cout << "服务主机: " << ConfigManager->GetString("app.host", "localhost") << std::endl;

    // 消息队列配置
    std::cout << "\n消息队列配置:" << std::endl;
    std::cout << "  最大消息数: " << ConfigManager->GetInt("messagequeue.max_size", 10000)
              << std::endl;
    std::cout << "  最大字节数: "
              << ConfigManager->GetInt("messagequeue.max_size_bytes", 100 * 1024 * 1024)
              << std::endl;
    std::cout << "  最大消费者: " << ConfigManager->GetInt("messagequeue.max_consumers", 100)
              << std::endl;
    std::cout << "  最大生产者: " << ConfigManager->GetInt("messagequeue.max_producers", 100)
              << std::endl;
    std::cout << "  消息TTL: " << ConfigManager->GetInt("messagequeue.message_ttl_ms", 300000)
              << "ms" << std::endl;
    std::cout << "  启用死信队列: "
              << (ConfigManager->GetBool("messagequeue.enable_dead_letter", true) ? "是" : "否")
              << std::endl;
    std::cout << "  启用批处理: "
              << (ConfigManager->GetBool("messagequeue.enable_batching", true) ? "是" : "否")
              << std::endl;
    std::cout << "  批处理大小: " << ConfigManager->GetInt("messagequeue.batch_size", 100)
              << std::endl;

    // 网络配置
    std::cout << "\n网络配置:" << std::endl;
    std::cout << "  最大连接数: " << ConfigManager->GetInt("network.max_connections", 1000)
              << std::endl;
    std::cout << "  连接超时: " << ConfigManager->GetInt("network.connection_timeout_ms", 30000)
              << "ms" << std::endl;
    std::cout << "  读取超时: " << ConfigManager->GetInt("network.read_timeout_ms", 60000) << "ms"
              << std::endl;
    std::cout << "  写入超时: " << ConfigManager->GetInt("network.write_timeout_ms", 60000) << "ms"
              << std::endl;
    std::cout << "  启用压缩: "
              << (ConfigManager->GetBool("network.enable_compression", true) ? "是" : "否")
              << std::endl;
    std::cout << "  启用加密: "
              << (ConfigManager->GetBool("network.enable_encryption", false) ? "是" : "否")
              << std::endl;
    std::cout << "  压缩算法: " << ConfigManager->GetString("network.compression_algorithm", "gzip")
              << std::endl;
    std::cout << "  加密算法: "
              << ConfigManager->GetString("network.encryption_algorithm", "aes-256-gcm")
              << std::endl;

    // 日志配置
    std::cout << "\n日志配置:" << std::endl;
    std::cout << "  日志级别: " << ConfigManager->GetString("logging.level", "info") << std::endl;
    std::cout << "  日志格式: " << ConfigManager->GetString("logging.format", "json") << std::endl;
    std::cout << "  日志输出: " << ConfigManager->GetString("logging.output", "console")
              << std::endl;
    std::cout << "  日志文件: "
              << ConfigManager->GetString("logging.file_path", "logs/helianthus.log") << std::endl;
    std::cout << "  启用轮转: "
              << (ConfigManager->GetBool("logging.enable_rotation", true) ? "是" : "否")
              << std::endl;
    std::cout << "  最大文件大小: " << ConfigManager->GetInt("logging.max_file_size_mb", 100)
              << "MB" << std::endl;
    std::cout << "  最大文件数: " << ConfigManager->GetInt("logging.max_files", 10) << std::endl;

    // 监控配置
    std::cout << "\n监控配置:" << std::endl;
    std::cout << "  启用指标: "
              << (ConfigManager->GetBool("monitoring.enable_metrics", true) ? "是" : "否")
              << std::endl;
    std::cout << "  指标端口: " << ConfigManager->GetInt("monitoring.metrics_port", 9090)
              << std::endl;
    std::cout << "  指标路径: " << ConfigManager->GetString("monitoring.metrics_path", "/metrics")
              << std::endl;
    std::cout << "  启用健康检查: "
              << (ConfigManager->GetBool("monitoring.enable_health_check", true) ? "是" : "否")
              << std::endl;
    std::cout << "  健康检查间隔: "
              << ConfigManager->GetInt("monitoring.health_check_interval_ms", 30000) << "ms"
              << std::endl;

    // 6. 演示配置修改
    std::cout << "\n6. 配置修改演示..." << std::endl;

    // 修改配置
    std::cout << "修改应用名称..." << std::endl;
    ConfigManager->SetString("app.name", "Helianthus-Modified");
    std::cout << "新的应用名称: " << ConfigManager->GetString("app.name") << std::endl;

    std::cout << "修改服务端口..." << std::endl;
    ConfigManager->SetInt("app.port", 9090);
    std::cout << "新的服务端口: " << ConfigManager->GetInt("app.port") << std::endl;

    std::cout << "修改调试模式..." << std::endl;
    ConfigManager->SetBool("app.debug", false);
    std::cout << "新的调试模式: " << (ConfigManager->GetBool("app.debug") ? "开启" : "关闭")
              << std::endl;

    // 7. 演示配置验证
    std::cout << "\n7. 配置验证演示..." << std::endl;

    // 添加自定义验证器
    ConfigValidator PortValidator = [](const std::string& Key, const ConfigValue& Value)
    {
        int64_t Port = Value.AsInt();
        return Port > 0 && Port <= 65535;
    };

    ConfigManager->AddValidator("app.port", PortValidator);

    // 测试有效端口
    if (ConfigManager->SetInt("app.port", 8080))
    {
        std::cout << "端口 8080 设置成功" << std::endl;
    }

    // 测试无效端口
    if (!ConfigManager->SetInt("app.port", 70000))
    {
        std::cout << "端口 70000 设置失败（验证器阻止）" << std::endl;
    }

    // 8. 演示配置变更回调
    std::cout << "\n8. 配置变更回调演示..." << std::endl;

    ConfigChangeCallback ChangeCallback =
        [](const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue)
    {
        std::cout << "配置变更: " << Key << " = " << OldValue.ToString() << " -> "
                  << NewValue.ToString() << std::endl;
    };

    ConfigManager->AddChangeCallback("app.name", ChangeCallback);

    // 触发配置变更
    ConfigManager->SetString("app.name", "Helianthus-Callback-Test");
    ConfigManager->SetString("app.name", "Helianthus-Final");

    // 9. 演示配置导出
    std::cout << "\n9. 配置导出演示..." << std::endl;

    // 导出为JSON
    std::string JsonConfig = ConfigManager->ExportToJson();
    std::cout << "JSON配置 (前200字符): " << JsonConfig.substr(0, 200) << "..." << std::endl;

    // 导出为YAML
    std::string YamlConfig = ConfigManager->ExportToYaml();
    std::cout << "YAML配置 (前200字符): " << YamlConfig.substr(0, 200) << "..." << std::endl;

    // 导出为INI
    std::string IniConfig = ConfigManager->ExportToIni();
    std::cout << "INI配置 (前200字符): " << IniConfig.substr(0, 200) << "..." << std::endl;

    // 10. 演示配置锁定
    std::cout << "\n10. 配置锁定演示..." << std::endl;

    ConfigManager->LockConfig();
    std::cout << "配置已锁定" << std::endl;

    if (!ConfigManager->SetString("app.name", "Locked-Name"))
    {
        std::cout << "无法修改锁定的配置" << std::endl;
    }

    ConfigManager->UnlockConfig();
    std::cout << "配置已解锁" << std::endl;

    if (ConfigManager->SetString("app.name", "Unlocked-Name"))
    {
        std::cout << "可以修改解锁的配置" << std::endl;
    }

    // 11. 演示修改跟踪
    std::cout << "\n11. 修改跟踪演示..." << std::endl;

    ConfigManager->SetString("track.key1", "value1");
    ConfigManager->SetString("track.key2", "value2");
    ConfigManager->SetInt("track.key3", 42);

    auto ModifiedKeys = ConfigManager->GetModifiedKeys();
    std::cout << "修改的配置项数量: " << ModifiedKeys.size() << std::endl;

    // 只显示前10个修改的配置项，避免输出过多内容
    size_t DisplayCount = std::min(ModifiedKeys.size(), size_t(10));
    for (size_t i = 0; i < DisplayCount; ++i)
    {
        std::cout << "  - " << ModifiedKeys[i] << std::endl;
    }

    if (ModifiedKeys.size() > 10)
    {
        std::cout << "  ... 还有 " << (ModifiedKeys.size() - 10) << " 个配置项" << std::endl;
    }

    ConfigManager->ClearModifiedFlags();
    std::cout << "修改标志已清除" << std::endl;

    // 12. 演示全局配置访问
    std::cout << "\n12. 全局配置访问演示..." << std::endl;

    if (Global::InitializeConfig("global_config"))
    {
        std::cout << "全局配置初始化成功" << std::endl;

        Global::SetString("global.test", "global_value");
        std::cout << "全局配置值: " << Global::GetString("global.test") << std::endl;

        Global::ShutdownConfig();
        std::cout << "全局配置已关闭" << std::endl;
    }

    // 13. 演示配置模板
    std::cout << "\n13. 配置模板演示..." << std::endl;

    ConfigTemplate::LoadSecurityDefaults(*ConfigManager);
    ConfigTemplate::LoadPerformanceDefaults(*ConfigManager);

    std::cout << "安全配置:" << std::endl;
    std::cout << "  启用SSL: " << (ConfigManager->GetBool("security.enable_ssl") ? "是" : "否")
              << std::endl;
    std::cout << "  证书文件: " << ConfigManager->GetString("security.cert_file") << std::endl;
    std::cout << "  密钥文件: " << ConfigManager->GetString("security.key_file") << std::endl;

    std::cout << "性能配置:" << std::endl;
    std::cout << "  线程池大小: " << ConfigManager->GetInt("performance.thread_pool_size")
              << std::endl;
    std::cout << "  最大连接数: " << ConfigManager->GetInt("performance.max_connections")
              << std::endl;
    std::cout << "  启用缓存: "
              << (ConfigManager->GetBool("performance.enable_caching") ? "是" : "否") << std::endl;

    // 14. 演示热重载 (已移除)
    std::cout << "\n14. 热重载演示 (已移除)..." << std::endl;
    std::cout << "热重载功能已从简化版本中移除" << std::endl;

    // 15. 保存配置到文件
    std::cout << "\n15. 保存配置到文件..." << std::endl;

    // 使用临时目录，避免 Bazel 沙箱限制
    const char* TempDir = std::getenv("TEST_TMPDIR");
    std::string FilePath = TempDir ? std::string(TempDir) + "/helianthus_modified.conf"
                                   : "/tmp/helianthus_modified.conf";

    if (ConfigManager->SaveToFile(FilePath))
    {
        std::cout << "配置已保存到 " << FilePath << std::endl;

        // 显示文件内容的前几行
        std::ifstream File(FilePath);
        if (File.is_open())
        {
            std::cout << "文件内容预览:" << std::endl;
            std::string Line;
            int LineCount = 0;
            while (std::getline(File, Line) && LineCount < 10)
            {
                std::cout << "  " << Line << std::endl;
                LineCount++;
            }
            if (LineCount >= 10)
            {
                std::cout << "  ... (更多内容)" << std::endl;
            }
            File.close();
        }
    }
    else
    {
        std::cout << "配置保存失败" << std::endl;
    }

    // 16. 配置统计
    std::cout << "\n16. 配置统计..." << std::endl;
    std::cout << "总配置项数量: " << ConfigManager->GetConfigItemCount() << std::endl;
    std::cout << "修改的配置项数量: " << ConfigManager->GetModifiedKeys().size() << std::endl;
    std::cout << "配置验证状态: " << (ConfigManager->ValidateConfig() ? "通过" : "失败")
              << std::endl;

    // 17. 清理
    std::cout << "\n17. 清理资源..." << std::endl;
    ConfigManager->Shutdown();
    std::cout << "配置管理器已关闭" << std::endl;

    std::cout << "\n=== 配置管理示例完成 ===" << std::endl;
    return 0;
}
