#include "Shared/Common/LogCategories.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/Logger.h"
#include "Shared/MessageQueue/MessagePersistence.h"

#include <chrono>
#include <fstream>
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

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 精确调试测试 ===");

    // 步骤1：创建FileBasedPersistence实例
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤1：创建FileBasedPersistence实例");
    auto filePersistence = std::make_unique<FileBasedPersistence>();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤1完成：FileBasedPersistence创建成功");

    // 步骤2：配置持久化设置
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤2：配置持久化设置");
    PersistenceConfig config;
    config.Type = PersistenceType::FILE_BASED;
    config.DataDirectory = "./test_precise_debug_data";
    config.QueueDataFile = "queues.dat";
    config.MessageDataFile = "messages.dat";
    config.IndexFile = "index.dat";
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤2完成：持久化配置设置完成");

    // 步骤3：手动创建数据目录和文件
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤3：手动创建数据目录和文件");
    try
    {
        std::filesystem::create_directories(config.DataDirectory);
        std::string indexFilePath = config.DataDirectory + "/" + config.IndexFile;
        std::ofstream indexFile(indexFilePath, std::ios::binary | std::ios::out);
        if (indexFile.is_open())
        {
            uint32_t version = 1;
            uint32_t queueCount = 0;
            indexFile.write(reinterpret_cast<const char*>(&version), sizeof(version));
            indexFile.write(reinterpret_cast<const char*>(&queueCount), sizeof(queueCount));
            indexFile.close();
        }
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤3完成：数据目录和文件创建成功");
    }
    catch (const std::exception& e)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "步骤3失败：{}", e.what());
        return 1;
    }

    // 步骤4：初始化FileBasedPersistence
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤4：开始初始化FileBasedPersistence");
    QueueResult initResult = QueueResult::INTERNAL_ERROR;

    std::thread initThread(
        [&filePersistence, &config, &initResult]()
        {
            try
            {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始调用Initialize");
                initResult = filePersistence->Initialize(config);
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：Initialize调用完成，结果: {}",
                      static_cast<int>(initResult));
            }
            catch (const std::exception& e)
            {
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Error,
                      "线程内：Initialize异常: {}",
                      e.what());
                initResult = QueueResult::INTERNAL_ERROR;
            }
        });

    // 等待初始化完成
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤4：等待初始化线程完成");
    auto startTime = std::chrono::steady_clock::now();
    while (initThread.joinable())
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);

        if (elapsed.count() > 10000)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "步骤4失败：初始化超时");
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤4：等待join开始");
    initThread.join();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤4完成：线程join成功");

    if (initResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "步骤4失败：初始化失败 code={}",
              static_cast<int>(initResult));
        return 1;
    }

    H_LOG(
        MQ, Helianthus::Common::LogVerbosity::Display, "步骤4完成：FileBasedPersistence初始化成功");

    // 步骤5：测试基本操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤5：测试基本操作");
    try
    {
        auto queues = filePersistence->ListPersistedQueues();
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Display,
              "步骤5完成：ListPersistedQueues成功，队列数量: {}",
              queues.size());
    }
    catch (const std::exception& e)
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "步骤5失败：ListPersistedQueues异常: {}",
              e.what());
        return 1;
    }

    // 步骤6：关闭FileBasedPersistence
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤6：开始关闭FileBasedPersistence");
    try
    {
        filePersistence->Shutdown();
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Display,
              "步骤6完成：FileBasedPersistence关闭成功");
    }
    catch (const std::exception& e)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "步骤6失败：Shutdown异常: {}", e.what());
        return 1;
    }

    // 步骤7：析构FileBasedPersistence
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤7：开始析构FileBasedPersistence");
    try
    {
        filePersistence.reset();
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Display,
              "步骤7完成：FileBasedPersistence析构成功");
    }
    catch (const std::exception& e)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "步骤7失败：析构异常: {}", e.what());
        return 1;
    }

    // 步骤8：程序退出
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "步骤8：程序即将退出");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 精确调试测试完成 ===");

    return 0;
}
