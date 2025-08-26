#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <shared_mutex>

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
    logCfg.UseAsync = false;
    Helianthus::Common::Logger::Initialize(logCfg);
    
    // 设置MQ分类的最小级别
    MQ.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 线程Join测试 ===");
    
    // 测试1：基本线程join
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：基本线程join");
    {
        std::thread basicThread([]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "基本线程开始");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "基本线程结束");
        });
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待基本线程join");
        basicThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "基本线程join成功");
    }
    
    // 测试2：带锁的线程join
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：带锁的线程join");
    {
        std::mutex testMutex;
        std::thread lockThread([&testMutex]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "锁线程开始");
            std::lock_guard<std::mutex> lock(testMutex);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "锁线程结束");
        });
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待锁线程join");
        lockThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "锁线程join成功");
    }
    
    // 测试3：带shared_mutex的线程join
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：带shared_mutex的线程join");
    {
        std::shared_mutex sharedMutex;
        std::thread sharedThread([&sharedMutex]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "shared_mutex线程开始");
            std::shared_lock<std::shared_mutex> lock(sharedMutex);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "shared_mutex线程结束");
        });
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待shared_mutex线程join");
        sharedThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "shared_mutex线程join成功");
    }
    
    // 测试4：模拟FileBasedPersistence的线程join
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试4：模拟FileBasedPersistence的线程join");
    {
        std::shared_mutex indexMutex;
        std::shared_mutex queueDataMutex;
        std::mutex fileMutex;
        
        std::thread persistenceThread([&indexMutex, &queueDataMutex, &fileMutex]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "持久化线程开始");
            
            // 模拟FileBasedPersistence中的锁操作
            {
                std::shared_lock<std::shared_mutex> indexLock(indexMutex);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            {
                std::shared_lock<std::shared_mutex> queueLock(queueDataMutex);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            {
                std::lock_guard<std::mutex> fileLock(fileMutex);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "持久化线程结束");
        });
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待持久化线程join");
        persistenceThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "持久化线程join成功");
    }
    
    // 测试5：包含MessageQueue头文件的线程join
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试5：包含MessageQueue头文件的线程join");
    {
        std::thread headerThread([]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "头文件线程开始");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "头文件线程结束");
        });
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待头文件线程join");
        headerThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "头文件线程join成功");
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 线程Join测试完成 ===");
    
    return 0;
}
