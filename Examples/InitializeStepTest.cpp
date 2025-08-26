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
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus Initialize步骤测试 ===");
    
    // 测试1：创建FileBasedPersistence实例
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：创建FileBasedPersistence实例");
    auto filePersistence = std::make_unique<FileBasedPersistence>();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");
    
    // 测试2：配置持久化设置
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：配置持久化设置");
    PersistenceConfig config;
    config.Type = PersistenceType::FILE_BASED;
    config.DataDirectory = "./test_initialize_step_data";
    config.QueueDataFile = "queues.dat";
    config.MessageDataFile = "messages.dat";
    config.IndexFile = "index.dat";
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "持久化配置设置完成");
    
    // 测试3：手动创建数据目录
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：手动创建数据目录");
    try {
        std::filesystem::create_directories(config.DataDirectory);
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "数据目录创建成功: {}", config.DataDirectory);
    } catch (const std::exception& e) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "数据目录创建失败: {}", e.what());
        return 1;
    }
    
    // 测试4：手动创建索引文件
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试4：手动创建索引文件");
    try {
        std::string indexFilePath = config.DataDirectory + "/" + config.IndexFile;
        std::ofstream indexFile(indexFilePath, std::ios::binary | std::ios::out);
        if (indexFile.is_open()) {
            uint32_t version = 1;
            uint32_t queueCount = 0;
            indexFile.write(reinterpret_cast<const char*>(&version), sizeof(version));
            indexFile.write(reinterpret_cast<const char*>(&queueCount), sizeof(queueCount));
            indexFile.close();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "索引文件创建成功: {}", indexFilePath);
        } else {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "索引文件创建失败");
            return 1;
        }
    } catch (const std::exception& e) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "索引文件创建异常: {}", e.what());
        return 1;
    }
    
    // 测试5：逐步测试Initialize方法
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试5：逐步测试Initialize方法");
    
    // 步骤5.1：检查是否已初始化
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤5.1：检查是否已初始化");
    if (filePersistence->IsInitialized()) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence已经初始化");
        return 0;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence未初始化，继续...");
    
    // 步骤5.2：设置配置
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤5.2：设置配置");
    // 这里我们需要直接访问FileBasedPersistence的私有成员，但这是不可能的
    // 所以我们只能通过公共接口来测试
    
    // 步骤5.3：测试Initialize方法（带超时）
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤5.3：测试Initialize方法（带超时）");
    
    QueueResult initResult = QueueResult::INTERNAL_ERROR;
    std::thread initThread([&filePersistence, &config, &initResult]() {
        try {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始调用Initialize");
            initResult = filePersistence->Initialize(config);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：Initialize调用完成，结果: {}", static_cast<int>(initResult));
        } catch (const std::exception& e) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "线程内：Initialize异常: {}", e.what());
            initResult = QueueResult::INTERNAL_ERROR;
        }
    });
    
    // 等待初始化完成，最多等待5秒
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待Initialize完成...");
    auto startTime = std::chrono::steady_clock::now();
    while (initThread.joinable()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        
        if (elapsed.count() > 5000) {  // 5秒超时
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "Initialize超时");
            return 1;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待join开始");
    initThread.join();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "join完成");
    
    if (initResult != QueueResult::SUCCESS) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "Initialize失败，结果: {}", static_cast<int>(initResult));
        return 1;
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Initialize成功");
    
    // 测试6：验证初始化结果
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试6：验证初始化结果");
    if (filePersistence->IsInitialized()) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "验证成功：FileBasedPersistence已初始化");
    } else {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "验证失败：FileBasedPersistence未初始化");
        return 1;
    }
    
    // 测试7：测试基本操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试7：测试基本操作");
    try {
        auto queues = filePersistence->ListPersistedQueues();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "ListPersistedQueues成功，队列数量: {}", queues.size());
    } catch (const std::exception& e) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "ListPersistedQueues异常: {}", e.what());
        return 1;
    }
    
    // 测试8：关闭FileBasedPersistence
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试8：关闭FileBasedPersistence");
    try {
        filePersistence->Shutdown();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "Shutdown成功");
    } catch (const std::exception& e) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "Shutdown异常: {}", e.what());
        return 1;
    }
    
    // 测试9：析构FileBasedPersistence
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试9：析构FileBasedPersistence");
    try {
        filePersistence.reset();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "析构成功");
    } catch (const std::exception& e) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "析构异常: {}", e.what());
        return 1;
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Initialize步骤测试完成 ===");
    
    return 0;
}
