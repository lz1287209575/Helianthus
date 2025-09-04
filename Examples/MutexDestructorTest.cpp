#include "Shared/Common/LogCategories.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/Logger.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

// 模拟FileBasedPersistence的锁结构
class MockFileBasedPersistence
{
private:
    std::shared_mutex IndexMutex;
    std::shared_mutex QueueDataMutex;
    std::mutex FileMutex;
    std::unordered_map<std::string, int> QueueData;
    std::unordered_map<std::string, std::unordered_map<int, int>> QueueMessageIndex;

public:
    MockFileBasedPersistence()
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence构造函数");
    }

    ~MockFileBasedPersistence()
    {
        H_LOG(
            MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence析构函数开始");

        // 模拟析构时的操作
        {
            std::shared_lock<std::shared_mutex> indexLock(IndexMutex);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "析构时获取IndexMutex读锁");
        }

        {
            std::shared_lock<std::shared_mutex> queueLock(QueueDataMutex);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "析构时获取QueueDataMutex读锁");
        }

        {
            std::lock_guard<std::mutex> fileLock(FileMutex);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "析构时获取FileMutex锁");
        }

        H_LOG(
            MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence析构函数完成");
    }

    void TestOperation()
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "执行测试操作");

        {
            std::shared_lock<std::shared_mutex> indexLock(IndexMutex);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "获取IndexMutex读锁");
        }

        {
            std::shared_lock<std::shared_mutex> queueLock(QueueDataMutex);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "获取QueueDataMutex读锁");
        }

        {
            std::lock_guard<std::mutex> fileLock(FileMutex);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "获取FileMutex锁");
        }

        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试操作完成");
    }
};

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

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 锁析构测试 ===");

    // 测试1：基本锁析构
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：基本锁析构");
    {
        std::shared_mutex testMutex;
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建testMutex");

        {
            std::shared_lock<std::shared_mutex> lock(testMutex);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "获取testMutex读锁");
        }

        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "testMutex即将析构");
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "testMutex析构完成");

    // 测试2：MockFileBasedPersistence析构
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：MockFileBasedPersistence析构");
    {
        auto mockPersistence = std::make_unique<MockFileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence创建成功");

        mockPersistence->TestOperation();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试操作完成");

        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始析构MockFileBasedPersistence");
        mockPersistence.reset();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence析构完成");
    }

    // 测试3：在独立线程中析构
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：在独立线程中析构");
    {
        std::thread destructorThread(
            []()
            {
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：开始创建MockFileBasedPersistence");
                auto mockPersistence = std::make_unique<MockFileBasedPersistence>();
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：MockFileBasedPersistence创建成功");

                mockPersistence->TestOperation();
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：测试操作完成");

                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：开始析构MockFileBasedPersistence");
                mockPersistence.reset();
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：MockFileBasedPersistence析构完成");
            });

        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待析构线程完成");
        destructorThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "析构线程完成");
    }

    // 测试4：模拟FileBasedPersistence的完整生命周期
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "测试4：模拟FileBasedPersistence的完整生命周期");
    {
        std::thread lifecycleThread(
            []()
            {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始生命周期测试");

                auto mockPersistence = std::make_unique<MockFileBasedPersistence>();
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：MockFileBasedPersistence创建成功");

                // 模拟多次操作
                for (int i = 0; i < 5; ++i)
                {
                    mockPersistence->TestOperation();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：开始析构MockFileBasedPersistence");
                mockPersistence.reset();
                H_LOG(MQ,
                      Helianthus::Common::LogVerbosity::Display,
                      "线程内：MockFileBasedPersistence析构完成");

                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：生命周期测试完成");
            });

        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待生命周期线程完成");
        lifecycleThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "生命周期线程完成");
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 锁析构测试完成 ===");

    return 0;
}
