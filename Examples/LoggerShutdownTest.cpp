#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

#include "Shared/Common/Logger.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

int main()
{
    // 初始化日志系统
    Helianthus::Common::Logger::LoggerConfig logCfg;
    logCfg.Level = Helianthus::Common::LogLevel::INFO;
    logCfg.EnableConsole = true;
    logCfg.EnableFile = false;
    logCfg.UseAsync = false;  // 使用同步模式避免异步问题
    Helianthus::Common::Logger::Initialize(logCfg);
    
    // 设置MQ分类的最小级别
    MQ.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
    MQPersistence.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus Logger析构测试 ===");
    
    // 测试1：基本日志操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：基本日志操作");
    H_LOG(MQPersistence, Helianthus::Common::LogVerbosity::Display, "测试1：MQPersistence日志");
    
    // 测试2：在独立线程中进行日志操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：在独立线程中进行日志操作");
    {
        std::thread logThread([]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始日志测试");
            
            for (int i = 0; i < 10; ++i) {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：日志消息 {}", i);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：日志测试完成");
        });
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待日志线程完成");
        logThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "日志线程完成");
    }
    
    // 测试3：手动调用Logger::Shutdown
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：手动调用Logger::Shutdown");
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始手动关闭Logger");
        Helianthus::Common::Logger::Shutdown();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Logger手动关闭完成");
    }
    
    // 测试4：重新初始化Logger
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试4：重新初始化Logger");
    {
        Helianthus::Common::Logger::Initialize(logCfg);
        MQ.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
        MQPersistence.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Logger重新初始化成功");
    }
    
    // 测试5：在独立线程中关闭Logger
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试5：在独立线程中关闭Logger");
    {
        std::thread shutdownThread([]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始关闭Logger");
            Helianthus::Common::Logger::Shutdown();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：Logger关闭完成");
        });
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待Logger关闭线程完成");
        shutdownThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Logger关闭线程完成");
    }
    
    // 测试6：重新初始化Logger并测试程序退出
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试6：重新初始化Logger并测试程序退出");
    {
        Helianthus::Common::Logger::Initialize(logCfg);
        MQ.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
        MQPersistence.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Logger重新初始化成功");
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "程序即将退出，测试自动析构");
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Logger析构测试完成 ===");
    
    return 0;
}
