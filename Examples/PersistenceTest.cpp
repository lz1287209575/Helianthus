#include "Shared/Common/LogCategories.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/Logger.h"
#include "Shared/MessageQueue/MessagePersistence.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

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

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 持久化管理器测试 ===");

    // 创建持久化管理器
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建持久化管理器...");
    auto persistenceMgr = std::make_unique<PersistenceManager>();

    // 配置持久化设置
    PersistenceConfig config;
    config.Type = PersistenceType::FILE_BASED;
    config.DataDirectory = "./test_persistence_data";
    config.QueueDataFile = "queues.dat";
    config.MessageDataFile = "messages.dat";
    config.IndexFile = "index.dat";

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化持久化管理器...");

    // 添加超时机制
    QueueResult initResult = QueueResult::INTERNAL_ERROR;
    std::thread initThread([&persistenceMgr, &config, &initResult]()
                           { initResult = persistenceMgr->Initialize(config); });

    // 等待初始化完成，最多等待10秒
    auto startTime = std::chrono::steady_clock::now();
    while (initThread.joinable())
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);

        if (elapsed.count() > 10000)
        {  // 10秒超时
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "持久化管理器初始化超时");
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    initThread.join();

    if (initResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "持久化管理器初始化失败 code={}",
              static_cast<int>(initResult));
        return 1;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "持久化管理器初始化成功");

    // 测试队列配置保存和加载
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 测试队列配置保存和加载 ===");

    QueueConfig queueConfig;
    queueConfig.Name = "test_queue";
    queueConfig.Type = QueueType::STANDARD;
    queueConfig.MaxSize = 100;
    queueConfig.EnableDeadLetter = true;

    QueueStats queueStats;
    queueStats.TotalMessages = 10;
    queueStats.ProcessedMessages = 5;

    auto saveResult = persistenceMgr->SaveQueue(queueConfig.Name, queueConfig, queueStats);
    if (saveResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "队列配置保存成功");
    }
    else
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "队列配置保存失败 code={}",
              static_cast<int>(saveResult));
    }

    // 加载队列配置
    QueueConfig loadedConfig;
    QueueStats loadedStats;
    auto loadResult = persistenceMgr->LoadQueue(queueConfig.Name, loadedConfig, loadedStats);
    if (loadResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Display,
              "队列配置加载成功: name={}, maxSize={}",
              loadedConfig.Name,
              loadedConfig.MaxSize);
    }
    else
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "队列配置加载失败 code={}",
              static_cast<int>(loadResult));
    }

    // 关闭持久化管理器
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "关闭持久化管理器...");
    persistenceMgr->Shutdown();

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 持久化管理器测试完成 ===");

    return 0;
}
