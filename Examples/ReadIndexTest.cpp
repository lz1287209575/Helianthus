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
    MQPersistence.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus ReadIndex测试 ===");
    
    // 测试1：创建正常的索引文件
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：创建正常的索引文件");
    {
        std::string testDir = "./test_readindex_normal";
        std::filesystem::create_directories(testDir);
        
        std::string indexPath = testDir + "/index.bin";
        std::ofstream indexFile(indexPath, std::ios::binary | std::ios::out);
        if (indexFile.is_open()) {
            uint32_t version = 1;
            uint32_t queueCount = 0;
            indexFile.write(reinterpret_cast<const char*>(&version), sizeof(version));
            indexFile.write(reinterpret_cast<const char*>(&queueCount), sizeof(queueCount));
            indexFile.close();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "正常索引文件创建成功");
        }
    }
    
    // 测试2：创建损坏的索引文件
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：创建损坏的索引文件");
    {
        std::string testDir = "./test_readindex_corrupted";
        std::filesystem::create_directories(testDir);
        
        std::string indexPath = testDir + "/index.bin";
        std::ofstream indexFile(indexPath, std::ios::binary | std::ios::out);
        if (indexFile.is_open()) {
            // 写入一些随机数据，模拟损坏的索引文件
            uint32_t version = 1;
            uint32_t queueCount = 999999; // 一个很大的数字，可能导致无限循环
            indexFile.write(reinterpret_cast<const char*>(&version), sizeof(version));
            indexFile.write(reinterpret_cast<const char*>(&queueCount), sizeof(queueCount));
            indexFile.close();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "损坏的索引文件创建成功");
        }
    }
    
    // 测试3：测试正常索引文件的读取
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：测试正常索引文件的读取");
    {
        auto filePersistence = std::make_unique<FileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");
        
        PersistenceConfig config;
        config.Type = PersistenceType::FILE_BASED;
        config.DataDirectory = "./test_readindex_normal";
        config.QueueDataFile = "queue_data.bin";
        config.MessageDataFile = "messages.bin";
        config.IndexFile = "index.bin";
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化FileBasedPersistence（正常索引）");
        auto initResult = filePersistence->Initialize(config);
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence初始化完成，结果: {}", static_cast<int>(initResult));
        
        if (initResult == QueueResult::SUCCESS) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "正常索引文件读取成功");
        } else {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "正常索引文件读取失败");
        }
    }
    
    // 测试4：测试损坏索引文件的读取（带超时）
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试4：测试损坏索引文件的读取（带超时）");
    {
        auto filePersistence = std::make_unique<FileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");
        
        PersistenceConfig config;
        config.Type = PersistenceType::FILE_BASED;
        config.DataDirectory = "./test_readindex_corrupted";
        config.QueueDataFile = "queue_data.bin";
        config.MessageDataFile = "messages.bin";
        config.IndexFile = "index.bin";
        
        QueueResult initResult = QueueResult::INTERNAL_ERROR;
        std::thread initThread([&filePersistence, &config, &initResult]() {
            try {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始初始化FileBasedPersistence（损坏索引）");
                initResult = filePersistence->Initialize(config);
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：FileBasedPersistence初始化完成，结果: {}", static_cast<int>(initResult));
            } catch (const std::exception& e) {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "线程内：Initialize异常: {}", e.what());
                initResult = QueueResult::INTERNAL_ERROR;
            }
        });
        
        // 等待初始化完成，最多等待3秒
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待损坏索引文件读取完成...");
        auto startTime = std::chrono::steady_clock::now();
        while (initThread.joinable()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
            
            if (elapsed.count() > 3000) {  // 3秒超时
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "损坏索引文件读取超时");
                return 1;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待join开始");
        initThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "join完成");
        
        if (initResult == QueueResult::SUCCESS) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "损坏索引文件读取成功（意外）");
        } else {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "损坏索引文件读取失败（预期）");
        }
    }
    
    // 测试5：创建空的索引文件
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试5：创建空的索引文件");
    {
        std::string testDir = "./test_readindex_empty";
        std::filesystem::create_directories(testDir);
        
        std::string indexPath = testDir + "/index.bin";
        std::ofstream indexFile(indexPath, std::ios::binary | std::ios::out);
        if (indexFile.is_open()) {
            // 创建完全空的文件
            indexFile.close();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "空索引文件创建成功");
        }
    }
    
    // 测试6：测试空索引文件的读取
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试6：测试空索引文件的读取");
    {
        auto filePersistence = std::make_unique<FileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");
        
        PersistenceConfig config;
        config.Type = PersistenceType::FILE_BASED;
        config.DataDirectory = "./test_readindex_empty";
        config.QueueDataFile = "queue_data.bin";
        config.MessageDataFile = "messages.bin";
        config.IndexFile = "index.bin";
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化FileBasedPersistence（空索引）");
        auto initResult = filePersistence->Initialize(config);
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence初始化完成，结果: {}", static_cast<int>(initResult));
        
        if (initResult == QueueResult::SUCCESS) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "空索引文件读取成功");
        } else {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "空索引文件读取失败");
        }
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== ReadIndex测试完成 ===");
    
    return 0;
}
