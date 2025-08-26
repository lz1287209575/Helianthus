#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

#include "Shared/MessageQueue/MessagePersistence.h"
#include "Shared/Common/Logger.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

using namespace Helianthus::MessageQueue;

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
    MQPersistence.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 简单析构测试 ===");
    
    // 测试1：创建和析构FileBasedPersistence（不初始化）
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：创建和析构FileBasedPersistence（不初始化）");
    {
        auto filePersistence = std::make_unique<FileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始析构FileBasedPersistence");
        filePersistence.reset();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence析构完成");
    }
    
    // 测试2：创建、初始化和析构FileBasedPersistence（带超时）
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：创建、初始化和析构FileBasedPersistence（带超时）");
    {
        auto filePersistence = std::make_unique<FileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");
        
        PersistenceConfig config;
        config.Type = PersistenceType::FILE_BASED;
        config.DataDirectory = "./test_simple_destructor_data";
        config.QueueDataFile = "queue_data.bin";
        config.MessageDataFile = "messages.bin";
        config.IndexFile = "index.bin";
        
        // 创建数据目录
        std::filesystem::create_directories(config.DataDirectory);
        
        // 初始化
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化FileBasedPersistence");
        auto initResult = filePersistence->Initialize(config);
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence初始化完成，结果: {}", static_cast<int>(initResult));
        
        if (initResult == QueueResult::SUCCESS) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始析构FileBasedPersistence");
            filePersistence.reset();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence析构完成");
        }
    }
    
    // 测试3：在独立线程中创建、初始化和析构FileBasedPersistence（带超时）
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：在独立线程中创建、初始化和析构FileBasedPersistence（带超时）");
    {
        std::thread destructorThread([]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始创建FileBasedPersistence");
            auto filePersistence = std::make_unique<FileBasedPersistence>();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：FileBasedPersistence创建成功");
            
            PersistenceConfig config;
            config.Type = PersistenceType::FILE_BASED;
            config.DataDirectory = "./test_simple_destructor_thread_data";
            config.QueueDataFile = "queue_data.bin";
            config.MessageDataFile = "messages.bin";
            config.IndexFile = "index.bin";
            
            // 创建数据目录
            std::filesystem::create_directories(config.DataDirectory);
            
            // 初始化
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始初始化FileBasedPersistence");
            auto initResult = filePersistence->Initialize(config);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：FileBasedPersistence初始化完成，结果: {}", static_cast<int>(initResult));
            
            if (initResult == QueueResult::SUCCESS) {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始析构FileBasedPersistence");
                filePersistence.reset();
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：FileBasedPersistence析构完成");
            }
            
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：析构测试完成");
        });
        
        // 等待析构线程完成，最多等待5秒
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待析构线程完成...");
        auto startTime = std::chrono::steady_clock::now();
        while (destructorThread.joinable()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
            
            if (elapsed.count() > 5000) {  // 5秒超时
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "析构线程超时");
                return 1;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待join开始");
        destructorThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "join完成");
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "析构线程完成");
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 简单析构测试完成 ===");
    
    return 0;
}
