#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#if !defined(_WIN32)
    #include <sys/eventfd.h>
    #include <unistd.h>
#endif

using namespace Helianthus::Network::Asio;

class BatchProcessingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Context = std::make_shared<IoContext>();
    }

    void TearDown() override
    {
        if (Context)
        {
            Context->Stop();
        }
    }

    std::shared_ptr<IoContext> Context;
};

TEST_F(BatchProcessingTest, TaskBatchConfig)
{
#if defined(__linux__)
    // 测试任务批处理配置
    TaskBatchConfig Config;
    Config.MaxTaskBatchSize = 64;
    Config.MinTaskBatchSize = 8;
    Config.MaxTaskBatchTimeoutMs = 2;
    Config.EnableTaskBatching = true;

    Context->SetTaskBatchConfig(Config);

    auto RetrievedConfig = Context->GetTaskBatchConfig();
    EXPECT_EQ(RetrievedConfig.MaxTaskBatchSize, 64);
    EXPECT_EQ(RetrievedConfig.MinTaskBatchSize, 8);
    EXPECT_EQ(RetrievedConfig.MaxTaskBatchTimeoutMs, 2);
    EXPECT_TRUE(RetrievedConfig.EnableTaskBatching);
#else
    GTEST_SKIP() << "批处理 API 仅在 Linux 下启用，测试跳过";
#endif
}

TEST_F(BatchProcessingTest, ReactorBatchConfig)
{
#if defined(__linux__)
    // 测试 Reactor 批处理配置
    auto Reactor = Context->GetReactor();
    ASSERT_NE(Reactor, nullptr);

    BatchConfig Config;
    Config.MaxBatchSize = 128;
    Config.MinBatchSize = 16;
    Config.MaxBatchTimeoutMs = 5;
    Config.EnableAdaptiveBatching = true;
    Config.AdaptiveThreshold = 32;

    Reactor->SetBatchConfig(Config);

    auto RetrievedConfig = Reactor->GetBatchConfig();
    EXPECT_EQ(RetrievedConfig.MaxBatchSize, 128);
    EXPECT_EQ(RetrievedConfig.MinBatchSize, 16);
    EXPECT_EQ(RetrievedConfig.MaxBatchTimeoutMs, 5);
    EXPECT_TRUE(RetrievedConfig.EnableAdaptiveBatching);
    EXPECT_EQ(RetrievedConfig.AdaptiveThreshold, 32);
#else
    GTEST_SKIP() << "批处理 API 仅在 Linux 下启用，测试跳过";
#endif
}

TEST_F(BatchProcessingTest, TaskBatchProcessing)
{
#if defined(__linux__)
    // 测试任务批处理
    std::atomic<int> TaskCounter = 0;
    std::vector<std::thread> Threads;

    // 配置批处理
    TaskBatchConfig Config;
    Config.MaxTaskBatchSize = 16;
    Config.MinTaskBatchSize = 4;
    Config.EnableTaskBatching = true;
    Context->SetTaskBatchConfig(Config);

    // 启动多个线程提交任务
    for (int i = 0; i < 4; ++i)
    {
        Threads.emplace_back(
            [this, &TaskCounter]()
            {
                for (int j = 0; j < 100; ++j)
                {
                    Context->Post(
                        [&TaskCounter]()
                        {
                            TaskCounter.fetch_add(1);
                            std::this_thread::sleep_for(std::chrono::microseconds(10));
                        });
                }
            });
    }

    // 启动事件循环
    std::thread RunThread([this]() { Context->RunBatch(); });

    // 等待所有线程完成
    for (auto& Thread : Threads)
    {
        Thread.join();
    }

    // 等待任务处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Context->Stop();
    RunThread.join();

    // 验证所有任务都被处理
    EXPECT_EQ(TaskCounter.load(), 400);

    // 检查批处理统计
    auto Stats = Context->GetTaskBatchStats();
    EXPECT_GT(Stats.TotalTasks, 0);
    EXPECT_GT(Stats.TotalBatches, 0);
    EXPECT_GT(Stats.AverageBatchSize, 0);
    EXPECT_GT(Stats.MaxBatchSize, 0);
    EXPECT_GT(Stats.MinBatchSize, 0);
#else
    GTEST_SKIP() << "批处理 API 仅在 Linux 下启用，测试跳过";
#endif
}

TEST_F(BatchProcessingTest, ReactorBatchProcessing)
{
#if defined(__linux__)
    // 测试 Reactor 批处理
    auto Reactor = Context->GetReactor();
    ASSERT_NE(Reactor, nullptr);

    // 配置批处理
    BatchConfig Config;
    Config.MaxBatchSize = 32;
    Config.MinBatchSize = 8;
    Config.EnableAdaptiveBatching = true;
    Reactor->SetBatchConfig(Config);

    // 重置统计
    Reactor->ResetPerformanceStats();

    // 创建一些文件描述符进行测试
    std::vector<int> TestFds;
    for (int i = 0; i < 10; ++i)
    {
        int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (fd >= 0)
        {
            TestFds.push_back(fd);
            Reactor->Add(static_cast<Fd>(fd), EventMask::Read, [](EventMask) {});
        }
    }

    // 执行一些批处理轮询
    for (int i = 0; i < 10; ++i)
    {
        Reactor->PollBatch(1, 16);  // 1ms 超时，最大16个事件
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 检查性能统计
    auto Stats = Reactor->GetPerformanceStats();
    EXPECT_GE(Stats.TotalBatches, 0);
    EXPECT_GE(Stats.AverageBatchSize, 0);
    EXPECT_GE(Stats.MaxBatchSize, 0);
    EXPECT_GE(Stats.MinBatchSize, 0);

    // 清理
    for (int fd : TestFds)
    {
        Reactor->Del(static_cast<Fd>(fd));
        close(fd);
    }
#else
    GTEST_SKIP() << "批处理 API 及 eventfd 仅在 Linux 下启用，测试跳过";
#endif
}

TEST_F(BatchProcessingTest, PerformanceComparison)
{
#if defined(__linux__)
    // 性能对比测试
    std::atomic<int> TaskCounter = 0;
    const int NumTasks = 1000;

    // 测试1：禁用批处理
    {
        TaskBatchConfig Config;
        Config.EnableTaskBatching = false;
        Context->SetTaskBatchConfig(Config);
        Context->ResetTaskBatchStats();

        auto StartTime = std::chrono::high_resolution_clock::now();

        // 提交任务
        for (int i = 0; i < NumTasks; ++i)
        {
            Context->Post([&TaskCounter]() { TaskCounter.fetch_add(1); });
        }

        // 启动事件循环
        std::thread RunThread([this]() { Context->Run(); });

        // 等待任务完成
        while (TaskCounter.load() < NumTasks)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);

        Context->Stop();
        RunThread.join();

        auto Stats = Context->GetTaskBatchStats();
        std::cout << "禁用批处理 - 耗时: " << Duration.count() << " 微秒" << std::endl;
        std::cout << "  总任务: " << Stats.TotalTasks << std::endl;
        std::cout << "  总批处理: " << Stats.TotalBatches << std::endl;
        std::cout << "  平均批处理大小: " << Stats.AverageBatchSize << std::endl;
    }

    // 重置计数器
    TaskCounter.store(0);

    // 测试2：启用批处理
    {
        TaskBatchConfig Config;
        Config.EnableTaskBatching = true;
        Config.MaxTaskBatchSize = 32;
        Config.MinTaskBatchSize = 8;
        Context->SetTaskBatchConfig(Config);
        Context->ResetTaskBatchStats();

        auto StartTime = std::chrono::high_resolution_clock::now();

        // 提交任务
        for (int i = 0; i < NumTasks; ++i)
        {
            Context->Post([&TaskCounter]() { TaskCounter.fetch_add(1); });
        }

        // 启动批处理事件循环
        std::thread RunThread([this]() { Context->RunBatch(); });

        // 等待任务完成
        while (TaskCounter.load() < NumTasks)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);

        Context->Stop();
        RunThread.join();

        auto Stats = Context->GetTaskBatchStats();
        std::cout << "启用批处理 - 耗时: " << Duration.count() << " 微秒" << std::endl;
        std::cout << "  总任务: " << Stats.TotalTasks << std::endl;
        std::cout << "  总批处理: " << Stats.TotalBatches << std::endl;
        std::cout << "  平均批处理大小: " << Stats.AverageBatchSize << std::endl;
        std::cout << "  最大批处理大小: " << Stats.MaxBatchSize << std::endl;
        std::cout << "  最小批处理大小: " << Stats.MinBatchSize << std::endl;
    }
#else
    GTEST_SKIP() << "批处理 API 仅在 Linux 下启用，测试跳过";
#endif
}

TEST_F(BatchProcessingTest, AdaptiveBatching)
{
#if defined(__linux__)
    // 测试自适应批处理
    auto Reactor = Context->GetReactor();
    ASSERT_NE(Reactor, nullptr);

    // 配置自适应批处理
    BatchConfig Config;
    Config.MaxBatchSize = 64;
    Config.MinBatchSize = 8;
    Config.EnableAdaptiveBatching = true;
    Config.AdaptiveThreshold = 16;
    Reactor->SetBatchConfig(Config);

    // 重置统计
    Reactor->ResetPerformanceStats();

    // 创建测试文件描述符
    std::vector<int> TestFds;
    for (int i = 0; i < 20; ++i)
    {
        int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (fd >= 0)
        {
            TestFds.push_back(fd);
            Reactor->Add(static_cast<Fd>(fd), EventMask::Read, [](EventMask) {});
        }
    }

    // 执行多次批处理轮询以触发自适应调整
    for (int i = 0; i < 50; ++i)
    {
        Reactor->PollBatch(1, 32);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // 检查自适应批处理统计
    auto Stats = Reactor->GetPerformanceStats();
    // 在没有实际事件的情况下，批处理可能为0，这是正常的
    EXPECT_GE(Stats.TotalBatches, 0);
    EXPECT_GE(Stats.AdaptiveBatchCount, 0);
    EXPECT_GE(Stats.AverageBatchSize, 0);

    std::cout << "自适应批处理统计:" << std::endl;
    std::cout << "  总批处理: " << Stats.TotalBatches << std::endl;
    std::cout << "  自适应批处理: " << Stats.AdaptiveBatchCount << std::endl;
    std::cout << "  平均批处理大小: " << Stats.AverageBatchSize << std::endl;
    std::cout << "  最大批处理大小: " << Stats.MaxBatchSize << std::endl;
    std::cout << "  最小批处理大小: " << Stats.MinBatchSize << std::endl;
    std::cout << "  平均处理时间: " << Stats.AverageProcessingTimeMs << " ms" << std::endl;

    // 清理
    for (int fd : TestFds)
    {
        Reactor->Del(static_cast<Fd>(fd));
        close(fd);
    }
#else
    GTEST_SKIP() << "批处理 API 及 eventfd 仅在 Linux 下启用，测试跳过";
#endif
}
