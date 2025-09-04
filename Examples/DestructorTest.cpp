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

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 析构测试 ===");

    // 测试1：创建和析构FileBasedPersistence（不初始化）
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "测试1：创建和析构FileBasedPersistence（不初始化）");
    {
        auto filePersistence = std::make_unique<FileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");

        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始析构FileBasedPersistence");
        filePersistence.reset();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence析构成功");
    }

    // 测试2：创建、初始化和析构FileBasedPersistence
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "测试2：创建、初始化和析构FileBasedPersistence");
    {
        auto filePersistence = std::make_unique<FileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence创建成功");

        // 配置
        PersistenceConfig config;
        config.Type = PersistenceType::FILE_BASED;
        config.DataDirectory = "./test_destructor_data";
        config.QueueDataFile = "queues.dat";
        config.MessageDataFile = "messages.dat";
        config.IndexFile = "index.dat";

        // 创建数据目录
        std::filesystem::create_directories(config.DataDirectory);

        // 初始化
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化FileBasedPersistence");
        auto initResult = filePersistence->Initialize(config);
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Display,
              "FileBasedPersistence初始化完成，结果: {}",
              static_cast<int>(initResult));

        if (initResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始析构FileBasedPersistence");
            filePersistence.reset();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "FileBasedPersistence析构成功");
        }
    }

    // 测试3：在独立线程中创建和析构FileBasedPersistence
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "测试3：在独立线程中创建和析构FileBasedPersistence");
    {
        std::thread destructorThread(
            []()
            {
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：开始创建FileBasedPersistence");
                auto filePersistence = std::make_unique<FileBasedPersistence>();
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：FileBasedPersistence创建成功");

                // 配置
                PersistenceConfig config;
                config.Type = PersistenceType::FILE_BASED;
                config.DataDirectory = "./test_destructor_thread_data";
                config.QueueDataFile = "queues.dat";
                config.MessageDataFile = "messages.dat";
                config.IndexFile = "index.dat";

                // 创建数据目录
                std::filesystem::create_directories(config.DataDirectory);

                // 初始化
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：开始初始化FileBasedPersistence");
                auto initResult = filePersistence->Initialize(config);
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：FileBasedPersistence初始化完成，结果: {}",
                      static_cast<int>(initResult));

                if (initResult == QueueResult::SUCCESS)
                {
                    H_LOG(MQ,
                          Helianthus::Common::LogVerbosity::Display,
                          "线程内：开始析构FileBasedPersistence");
                    filePersistence.reset();
                    H_LOG(MQ,
                          Helianthus::Common::LogVerbosity::Display,
                          "线程内：FileBasedPersistence析构成功");
                }

                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：析构测试完成");
            });

        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待析构线程完成");
        destructorThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "析构线程完成");
    }

    // 测试4：测试PersistenceManager的析构
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试4：测试PersistenceManager的析构");
    {
        auto persistenceMgr = std::make_unique<PersistenceManager>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "PersistenceManager创建成功");

        // 配置
        PersistenceConfig config;
        config.Type = PersistenceType::FILE_BASED;
        config.DataDirectory = "./test_destructor_mgr_data";
        config.QueueDataFile = "queues.dat";
        config.MessageDataFile = "messages.dat";
        config.IndexFile = "index.dat";

        // 创建数据目录
        std::filesystem::create_directories(config.DataDirectory);

        // 初始化
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化PersistenceManager");
        auto initResult = persistenceMgr->Initialize(config);
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Display,
              "PersistenceManager初始化完成，结果: {}",
              static_cast<int>(initResult));

        if (initResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始析构PersistenceManager");
            persistenceMgr.reset();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "PersistenceManager析构成功");
        }
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 析构测试完成 ===");

    return 0;
}
