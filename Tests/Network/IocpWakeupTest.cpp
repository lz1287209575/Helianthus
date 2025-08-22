#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>

using namespace Helianthus::Network::Asio;

class IocpWakeupTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 设置测试环境
    }

    void TearDown() override
    {
        // 清理测试环境
    }
};

// 测试 IOCP 唤醒机制
TEST_F(IocpWakeupTest, WakeupMechanism)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<bool> TaskExecuted{false};
    std::atomic<bool> StopCalled{false};
    
    // 启动事件循环线程
    std::thread EventLoopThread([&Context, &StopCalled]() {
        Context.Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 从另一个线程提交任务
    std::thread PostThread([&Context, &TaskExecuted]() {
        Context.Post([&TaskExecuted]() {
            TaskExecuted = true;
        });
    });
    
    // 等待任务执行
    PostThread.join();
    
    // 等待任务执行完成
    int WaitCount = 0;
    while (!TaskExecuted && WaitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 停止事件循环
    Context.Stop();
    
    // 等待事件循环停止
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(TaskExecuted.load()) << "任务应该被执行";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    
    std::cout << "IOCP 唤醒机制测试通过" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试跨线程立即停止
TEST_F(IocpWakeupTest, ImmediateStop)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<bool> StopCalled{false};
    
    // 启动事件循环线程
    std::thread EventLoopThread([&Context, &StopCalled]() {
        Context.Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 从另一个线程立即停止
    std::thread StopThread([&Context]() {
        Context.Stop();
    });
    
    StopThread.join();
    
    // 等待事件循环停止
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该立即停止";
    
    std::cout << "立即停止测试通过" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试多个任务提交
TEST_F(IocpWakeupTest, MultipleTasks)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<int> TaskCount{0};
    std::atomic<bool> StopCalled{false};
    
    // 启动事件循环线程
    std::thread EventLoopThread([&Context, &StopCalled]() {
        Context.Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 从多个线程提交任务
    std::vector<std::thread> PostThreads;
    for (int i = 0; i < 5; ++i) {
        PostThreads.emplace_back([&Context, &TaskCount, i]() {
            Context.Post([&TaskCount, i]() {
                TaskCount++;
                std::cout << "任务 " << i << " 执行完成" << std::endl;
            });
        });
    }
    
    // 等待所有任务提交完成
    for (auto& thread : PostThreads) {
        thread.join();
    }
    
    // 等待任务执行
    int WaitCount = 0;
    while (TaskCount.load() < 5 && WaitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 停止事件循环
    Context.Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_EQ(TaskCount.load(), 5) << "所有任务都应该被执行";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    
    std::cout << "多任务测试通过，执行了 " << TaskCount.load() << " 个任务" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
