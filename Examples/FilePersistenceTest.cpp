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
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 文件持久化测试 ===");
    
    // 测试1：创建FileBasedPersistence实例
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：创建FileBasedPersistence实例");
    
    try {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始创建FileBasedPersistence...");
        auto filePersistence = std::make_unique<FileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");
    } catch (const std::exception& e) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "FileBasedPersistence创建失败: {}", e.what());
        return 1;
    }
    
    // 测试2：配置持久化设置
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：配置持久化设置");
    
    PersistenceConfig config;
    config.Type = PersistenceType::FILE_BASED;
    config.DataDirectory = "./test_file_persistence_data";
    config.QueueDataFile = "queues.dat";
    config.MessageDataFile = "messages.dat";
    config.IndexFile = "index.dat";
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "持久化配置: dataDir={}, queueFile={}, messageFile={}, indexFile={}", 
          config.DataDirectory, config.QueueDataFile, config.MessageDataFile, config.IndexFile);
    
    // 测试3：创建并初始化FileBasedPersistence
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：创建并初始化FileBasedPersistence");
    
    QueueResult initResult = QueueResult::INTERNAL_ERROR;
    std::unique_ptr<FileBasedPersistence> filePersistence;
    
    std::thread initThread([&filePersistence, &config, &initResult]() {
        try {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "在独立线程中创建FileBasedPersistence...");
            filePersistence = std::make_unique<FileBasedPersistence>();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功，开始初始化...");
            initResult = filePersistence->Initialize(config);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence初始化完成，结果: {}", static_cast<int>(initResult));
        } catch (const std::exception& e) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "FileBasedPersistence操作异常: {}", e.what());
            initResult = QueueResult::INTERNAL_ERROR;
        }
    });
    
    // 等待初始化完成，最多等待15秒
    auto startTime = std::chrono::steady_clock::now();
    while (initThread.joinable()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        
        if (elapsed.count() > 15000) {  // 15秒超时
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "FileBasedPersistence初始化超时");
            return 1;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    initThread.join();
    
    if (initResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "FileBasedPersistence初始化失败 code={}", static_cast<int>(initResult));
        return 1;
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence初始化成功");
    
    // 测试4：关闭FileBasedPersistence
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试4：关闭FileBasedPersistence");
    
    if (filePersistence) {
        filePersistence->Shutdown();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence关闭成功");
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 文件持久化测试完成 ===");
    
    return 0;
}
