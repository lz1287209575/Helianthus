#include "Shared/Common/LogCategories.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/Logger.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main()
{
    // 初始化日志系统
    Helianthus::Common::Logger::LoggerConfig logCfg;
    logCfg.Level = Helianthus::Common::LogLevel::INFO;
    logCfg.EnableConsole = true;
    logCfg.EnableFile = false;
    logCfg.UseAsync = false;
    Helianthus::Common::Logger::Initialize(logCfg);

    // 设置MQ分类的最小级别
    MQ.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 简单日志测试 ===");

    // 测试1：基本日志输出
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：基本日志输出");

    // 测试2：多线程日志输出
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：多线程日志输出");

    std::thread logThread(
        []()
        {
            for (int i = 0; i < 10; ++i)
            {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程日志消息: {}", i);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

    logThread.join();

    // 测试3：不同级别的日志
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "测试3：不同级别的日志");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "这是一个警告消息");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "这是一个错误消息");

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 简单日志测试完成 ===");

    return 0;
}
