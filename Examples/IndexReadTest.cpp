#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>

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
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 索引读取测试 ===");
    
    // 测试1：创建FileBasedPersistence实例
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：创建FileBasedPersistence实例");
    
    auto filePersistence = std::make_unique<FileBasedPersistence>();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");
    
    // 测试2：配置持久化设置
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：配置持久化设置");
    
    PersistenceConfig config;
    config.Type = PersistenceType::FILE_BASED;
    config.DataDirectory = "./test_index_data";
    config.QueueDataFile = "queues.dat";
    config.MessageDataFile = "messages.dat";
    config.IndexFile = "index.dat";
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "持久化配置: dataDir={}", config.DataDirectory);
    
    // 测试3：手动创建索引文件
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：手动创建索引文件");
    
    try {
        std::filesystem::create_directories(config.DataDirectory);
        
        std::string indexFilePath = config.DataDirectory + "/" + config.IndexFile;
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建索引文件: {}", indexFilePath);
        
        std::ofstream indexFile(indexFilePath, std::ios::binary | std::ios::out);
        if (indexFile.is_open())
        {
            // 写入索引版本
            uint32_t version = 1;
            indexFile.write(reinterpret_cast<const char*>(&version), sizeof(version));
            
            // 写入队列数量
            uint32_t queueCount = 0;
            indexFile.write(reinterpret_cast<const char*>(&queueCount), sizeof(queueCount));
            
            indexFile.close();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "索引文件创建成功");
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "索引文件创建失败");
            return 1;
        }
    } catch (const std::exception& e) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "创建索引文件异常: {}", e.what());
        return 1;
    }
    
    // 测试4：初始化FileBasedPersistence（应该会读取我们创建的索引文件）
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试4：初始化FileBasedPersistence");
    
    QueueResult initResult = QueueResult::INTERNAL_ERROR;
    std::thread initThread([&filePersistence, &config, &initResult]() {
        try {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化FileBasedPersistence...");
            initResult = filePersistence->Initialize(config);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence初始化完成，结果: {}", static_cast<int>(initResult));
        } catch (const std::exception& e) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "FileBasedPersistence初始化异常: {}", e.what());
            initResult = QueueResult::INTERNAL_ERROR;
        }
    });
    
    // 等待初始化完成，最多等待10秒
    auto startTime = std::chrono::steady_clock::now();
    while (initThread.joinable()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        
        if (elapsed.count() > 10000) {  // 10秒超时
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
    
    // 测试5：关闭FileBasedPersistence
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试5：关闭FileBasedPersistence");
    
    filePersistence->Shutdown();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence关闭成功");
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 索引读取测试完成 ===");
    
    return 0;
}
