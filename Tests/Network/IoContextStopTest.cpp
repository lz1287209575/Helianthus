#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/NetworkTypes.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace Helianthus::Network::Asio;

class IoContextStopTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 初始化 WinSock
        WSADATA WsaData;
        int Result = WSAStartup(MAKEWORD(2, 2), &WsaData);
        ASSERT_EQ(Result, 0) << "WSAStartup failed";
    }

    void TearDown() override
    {
        WSACleanup();
    }
};

// 测试 IoContext 基本停止功能
TEST_F(IoContextStopTest, IoContextBasicStop)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> StopCompleted{false};
    std::atomic<bool> TaskExecuted{false};
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr, &StopCompleted]() {
        ContextPtr->Run();
        StopCompleted.store(true);
        std::cout << "IoContext 事件循环已停止" << std::endl;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 提交一个任务
    ContextPtr->Post([&TaskExecuted]() {
        TaskExecuted.store(true);
        std::cout << "任务已执行" << std::endl;
    });
    
    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 停止事件循环
    ContextPtr->Stop();
    
    // 等待事件循环停止
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(TaskExecuted.load()) << "任务应该被执行";
    EXPECT_TRUE(StopCompleted.load()) << "事件循环应该停止";
    
    std::cout << "IoContext 基本停止测试完成" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 IoContext 延迟任务停止功能
TEST_F(IoContextStopTest, IoContextDelayedTaskStop)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> StopCompleted{false};
    std::atomic<bool> DelayedTaskExecuted{false};
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr, &StopCompleted]() {
        ContextPtr->Run();
        StopCompleted.store(true);
        std::cout << "IoContext 事件循环已停止" << std::endl;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 提交一个延迟任务（1秒后执行）
    ContextPtr->PostDelayed([&DelayedTaskExecuted]() {
        DelayedTaskExecuted.store(true);
        std::cout << "延迟任务已执行" << std::endl;
    }, 1000);
    
    // 等待一小段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 停止事件循环（在延迟任务执行之前）
    ContextPtr->Stop();
    
    // 等待事件循环停止
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_FALSE(DelayedTaskExecuted.load()) << "延迟任务不应该被执行（因为提前停止了）";
    EXPECT_TRUE(StopCompleted.load()) << "事件循环应该停止";
    
    std::cout << "IoContext 延迟任务停止测试完成" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 IoContext 快速停止功能
TEST_F(IoContextStopTest, IoContextQuickStop)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> StopCompleted{false};
    std::atomic<int> TaskCount{0};
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr, &StopCompleted]() {
        ContextPtr->Run();
        StopCompleted.store(true);
        std::cout << "IoContext 事件循环已停止" << std::endl;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 立即停止事件循环
    ContextPtr->Stop();
    
    // 等待事件循环停止
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(StopCompleted.load()) << "事件循环应该快速停止";
    EXPECT_EQ(TaskCount.load(), 0) << "不应该有任务被执行";
    
    std::cout << "IoContext 快速停止测试完成" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 IoContext 与 AsyncTcpSocket 集成的停止功能
TEST_F(IoContextStopTest, IoContextWithAsyncSocketStop)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> StopCompleted{false};
    std::atomic<bool> ConnectAttempted{false};
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr, &StopCompleted]() {
        ContextPtr->Run();
        StopCompleted.store(true);
        std::cout << "IoContext 事件循环已停止" << std::endl;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 创建异步 TCP 套接字并尝试连接
    auto AsyncSocket = std::make_shared<AsyncTcpSocket>(ContextPtr);
    Helianthus::Network::NetworkAddress ServerAddr("127.0.0.1", 12345);
    
    ContextPtr->Post([ContextPtr, &ServerAddr, &ConnectAttempted, AsyncSocket]() {
        AsyncSocket->AsyncConnect(ServerAddr, 
            [&ConnectAttempted](Helianthus::Network::NetworkError Error) {
                ConnectAttempted.store(true);
                std::cout << "连接尝试完成，错误: " << static_cast<int>(Error) << std::endl;
            });
    });
    
    // 等待一小段时间让连接尝试开始
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 停止事件循环
    ContextPtr->Stop();
    
    // 等待事件循环停止
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(StopCompleted.load()) << "事件循环应该停止";
    
    std::cout << "IoContext 与 AsyncSocket 集成停止测试完成" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 IoContext 延迟任务唤醒机制
TEST_F(IoContextStopTest, IoContextDelayedTaskWakeup)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> StopCompleted{false};
    std::atomic<bool> DelayedTaskExecuted{false};
    std::atomic<bool> ImmediateTaskExecuted{false};
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr, &StopCompleted]() {
        ContextPtr->Run();
        StopCompleted.store(true);
        std::cout << "IoContext 事件循环已停止" << std::endl;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 提交一个延迟任务（100ms后执行）
    ContextPtr->PostDelayed([&DelayedTaskExecuted]() {
        DelayedTaskExecuted.store(true);
        std::cout << "延迟任务已执行" << std::endl;
    }, 100);
    
    // 提交一个立即任务
    ContextPtr->Post([&ImmediateTaskExecuted]() {
        ImmediateTaskExecuted.store(true);
        std::cout << "立即任务已执行" << std::endl;
    });
    
    // 等待延迟任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 停止事件循环
    ContextPtr->Stop();
    
    // 等待事件循环停止
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(ImmediateTaskExecuted.load()) << "立即任务应该被执行";
    EXPECT_TRUE(DelayedTaskExecuted.load()) << "延迟任务应该被执行";
    EXPECT_TRUE(StopCompleted.load()) << "事件循环应该停止";
    
    std::cout << "IoContext 延迟任务唤醒机制测试完成" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
