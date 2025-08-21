#include <gtest/gtest.h>
#include "Shared/Network/Asio/IoContext.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

using namespace Helianthus::Network::Asio;

class WakeupMechanismTest : public ::testing::Test
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

TEST_F(WakeupMechanismTest, WakeupTypeConfiguration)
{
    // 测试唤醒类型配置
    auto OriginalType = Context->GetWakeupType();
    
    // 测试设置不同的唤醒类型
    Context->SetWakeupType(WakeupType::EventFd);
    EXPECT_EQ(Context->GetWakeupType(), WakeupType::EventFd);
    
    Context->SetWakeupType(WakeupType::Pipe);
    EXPECT_EQ(Context->GetWakeupType(), WakeupType::Pipe);
    
    // 恢复原始类型
    Context->SetWakeupType(OriginalType);
}

TEST_F(WakeupMechanismTest, CrossThreadWakeup)
{
    // 测试跨线程唤醒
    std::atomic<int> TaskCounter = 0;
    std::atomic<bool> ReadyToPost = false;
    
    // 启动事件循环线程
    std::thread RunThread([this]() {
        Context->Run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 从另一个线程提交任务
    std::thread PostThread([this, &TaskCounter, &ReadyToPost]() {
        // 等待主线程准备就绪
        while (!ReadyToPost.load())
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        // 提交多个任务
        for (int i = 0; i < 100; ++i)
        {
            Context->Post([&TaskCounter]() {
                TaskCounter.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            });
        }
    });
    
    // 标记准备就绪
    ReadyToPost.store(true);
    
    // 等待任务完成
    while (TaskCounter.load() < 100)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 停止事件循环
    Context->Stop();
    RunThread.join();
    PostThread.join();
    
    // 验证所有任务都被处理
    EXPECT_EQ(TaskCounter.load(), 100);
    
    // 检查唤醒统计
    auto Stats = Context->GetWakeupStats();
    EXPECT_GT(Stats.TotalWakeups, 0);
    EXPECT_GT(Stats.CrossThreadWakeups, 0);
}

TEST_F(WakeupMechanismTest, MultipleThreadWakeup)
{
    // 测试多线程唤醒
    std::atomic<int> TaskCounter = 0;
    const int NumThreads = 8;
    const int TasksPerThread = 50;
    
    // 启动事件循环线程
    std::thread RunThread([this]() {
        Context->Run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 启动多个线程提交任务
    std::vector<std::thread> PostThreads;
    for (int i = 0; i < NumThreads; ++i)
    {
        PostThreads.emplace_back([this, &TaskCounter, TasksPerThread]() {
            for (int j = 0; j < TasksPerThread; ++j)
            {
                Context->Post([&TaskCounter]() {
                    TaskCounter.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::microseconds(5));
                });
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& Thread : PostThreads)
    {
        Thread.join();
    }
    
    // 等待任务处理完成
    while (TaskCounter.load() < NumThreads * TasksPerThread)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 停止事件循环
    Context->Stop();
    RunThread.join();
    
    // 验证所有任务都被处理
    EXPECT_EQ(TaskCounter.load(), NumThreads * TasksPerThread);
    
    // 检查唤醒统计
    auto Stats = Context->GetWakeupStats();
    EXPECT_GT(Stats.TotalWakeups, 0);
    EXPECT_GT(Stats.CrossThreadWakeups, 0);
    EXPECT_GT(Stats.AverageWakeupLatencyMs, 0.0);
}

TEST_F(WakeupMechanismTest, WakeupLatency)
{
    // 测试唤醒延迟
    std::atomic<int> TaskCounter = 0;
    const int NumTasks = 1000;
    
    // 重置统计
    Context->ResetWakeupStats();
    
    // 启动事件循环线程
    std::thread RunThread([this]() {
        Context->Run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 提交大量任务来测试延迟
    for (int i = 0; i < NumTasks; ++i)
    {
        Context->Post([&TaskCounter]() {
            TaskCounter.fetch_add(1);
        });
    }
    
    // 等待任务处理完成
    while (TaskCounter.load() < NumTasks)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 停止事件循环
    Context->Stop();
    RunThread.join();
    
    // 检查唤醒统计
    auto Stats = Context->GetWakeupStats();
    EXPECT_EQ(Stats.TotalWakeups, NumTasks);
    EXPECT_GT(Stats.CrossThreadWakeups, 0);
    EXPECT_GE(Stats.AverageWakeupLatencyMs, 0.0);
    EXPECT_GE(Stats.MaxWakeupLatencyMs, 0);
    
    std::cout << "唤醒延迟统计:" << std::endl;
    std::cout << "  总唤醒次数: " << Stats.TotalWakeups << std::endl;
    std::cout << "  跨线程唤醒: " << Stats.CrossThreadWakeups << std::endl;
    std::cout << "  同线程唤醒: " << Stats.SameThreadWakeups << std::endl;
    std::cout << "  平均延迟: " << Stats.AverageWakeupLatencyMs << " ms" << std::endl;
    std::cout << "  最大延迟: " << Stats.MaxWakeupLatencyMs << " ms" << std::endl;
}

TEST_F(WakeupMechanismTest, WakeupStatsReset)
{
    // 测试唤醒统计重置
    std::atomic<int> TaskCounter = 0;
    
    // 提交一些任务
    for (int i = 0; i < 10; ++i)
    {
        Context->Post([&TaskCounter]() {
            TaskCounter.fetch_add(1);
        });
    }
    
    // 启动事件循环处理任务
    std::thread RunThread([this]() {
        Context->Run();
    });
    
    // 等待任务完成
    while (TaskCounter.load() < 10)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Context->Stop();
    RunThread.join();
    
    // 检查有统计数据
    auto StatsBefore = Context->GetWakeupStats();
    EXPECT_GT(StatsBefore.TotalWakeups, 0);
    
    // 重置统计
    Context->ResetWakeupStats();
    
    // 检查统计已重置
    auto StatsAfter = Context->GetWakeupStats();
    EXPECT_EQ(StatsAfter.TotalWakeups, 0);
    EXPECT_EQ(StatsAfter.CrossThreadWakeups, 0);
    EXPECT_EQ(StatsAfter.SameThreadWakeups, 0);
    EXPECT_EQ(StatsAfter.AverageWakeupLatencyMs, 0.0);
    EXPECT_EQ(StatsAfter.MaxWakeupLatencyMs, 0);
}

TEST_F(WakeupMechanismTest, WakeupFromOtherThread)
{
    // 测试 WakeupFromOtherThread 方法
    std::atomic<int> TaskCounter = 0;
    
    // 启动事件循环线程
    std::thread RunThread([this]() {
        Context->Run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 从另一个线程直接调用唤醒
    std::thread WakeupThread([this, &TaskCounter]() {
        for (int i = 0; i < 50; ++i)
        {
            // 提交任务
            Context->Post([&TaskCounter]() {
                TaskCounter.fetch_add(1);
            });
            
            // 直接唤醒
            Context->WakeupFromOtherThread();
            
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // 等待任务完成
    while (TaskCounter.load() < 50)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 停止事件循环
    Context->Stop();
    RunThread.join();
    WakeupThread.join();
    
    // 验证所有任务都被处理
    EXPECT_EQ(TaskCounter.load(), 50);
    
    // 检查唤醒统计
    auto Stats = Context->GetWakeupStats();
    EXPECT_GT(Stats.TotalWakeups, 0);
    EXPECT_GT(Stats.CrossThreadWakeups, 0);
}
