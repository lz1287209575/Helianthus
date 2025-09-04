#include "Shared/Common/StructuredLogger.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace Helianthus::Common;

int main()
{
    std::cout << "=== Helianthus 结构化日志测试 ===" << std::endl;

    // 初始化结构化日志
    StructuredLoggerConfig logConfig;
    logConfig.MinLevel = StructuredLogLevel::INFO;
    logConfig.EnableConsole = true;
    logConfig.EnableFile = true;
    logConfig.FilePath = "logs/structured_test.log";
    logConfig.EnableJsonOutput = true;

    StructuredLogger::Initialize(logConfig);

    std::cout << "✅ 结构化日志系统初始化成功" << std::endl;

    // 测试不同级别的日志
    StructuredLogger::Info("Test", "这是一条信息日志");
    StructuredLogger::Warn("Test", "这是一条警告日志");
    StructuredLogger::Error("Test", "这是一条错误日志");

    // 测试带字段的日志
    LogFields fields;
    fields.AddField("user_id", "12345");
    fields.AddField("action", "login");
    fields.AddField("ip_address", "192.168.1.100");
    fields.AddField("timestamp", static_cast<int64_t>(std::time(nullptr)));

    StructuredLogger::Info("UserActivity", "用户登录", fields);

    // 测试性能指标
    StructuredLogger::RecordMetric("response_time", 45.67);
    StructuredLogger::IncrementCounter("requests_total");
    StructuredLogger::RecordHistogram("request_size", 1024);

    // 测试全局字段
    StructuredLogger::SetGlobalField("service_name", "helianthus");
    StructuredLogger::SetGlobalField("version", "1.0.0");
    StructuredLogger::SetGlobalField("environment", "development");

    StructuredLogger::Info("System", "系统启动完成");

    // 等待一下让日志写入
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "✅ 结构化日志测试完成" << std::endl;
    std::cout << "请检查 logs/structured_test.log 文件" << std::endl;

    // 关闭日志系统
    StructuredLogger::Shutdown();

    // 再等待一下确保日志完全写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return 0;
}
