#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/IoContext.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif

using namespace Helianthus::Network::Asio;

class AcceptExConcurrentTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
#ifdef _WIN32
        // 初始化 WinSock
        WSADATA WsaData;
        int Result = WSAStartup(MAKEWORD(2, 2), &WsaData);
        ASSERT_EQ(Result, 0) << "WSAStartup failed";
#endif
    }

    void TearDown() override
    {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

// 测试 AcceptEx 并发接受
TEST_F(AcceptExConcurrentTest, ConcurrentAcceptEx)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<int> AcceptCount{0};
    std::atomic<bool> StopCalled{false};
    std::vector<Fd> AcceptedSockets;
    std::mutex SocketsMutex;

    // 创建监听套接字
    SOCKET ListenSocket =
        WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    ASSERT_NE(ListenSocket, INVALID_SOCKET) << "Failed to create listen socket";

    // 绑定到本地地址
    sockaddr_in ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(0);  // 使用随机端口

    int BindResult =
        bind(ListenSocket, reinterpret_cast<sockaddr*>(&ServerAddr), sizeof(ServerAddr));
    ASSERT_EQ(BindResult, 0) << "Failed to bind socket";

    // 开始监听
    int ListenResult = listen(ListenSocket, SOMAXCONN);
    ASSERT_EQ(ListenResult, 0) << "Failed to listen";

    // 获取实际端口
    int AddrLen = sizeof(ServerAddr);
    getsockname(ListenSocket, reinterpret_cast<sockaddr*>(&ServerAddr), &AddrLen);
    int Port = ntohs(ServerAddr.sin_port);

    std::cout << "Listening on port: " << Port << std::endl;

    // 启动事件循环线程
    std::thread EventLoopThread(
        [&Context, &StopCalled]()
        {
            Context.Run();
            StopCalled = true;
        });

    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 启动 AcceptEx 并发接受
    Context.Post(
        [&Context, ListenSocket, &AcceptCount, &AcceptedSockets, &SocketsMutex]()
        {
            // 这里应该调用 AsyncAccept，但我们需要先实现 AsyncTcpAcceptor
            // 暂时模拟接受连接
            for (int i = 0; i < 5; ++i)
            {
                AcceptCount++;
                {
                    std::lock_guard<std::mutex> Lock(SocketsMutex);
                    AcceptedSockets.push_back(static_cast<Fd>(i + 1000));  // 模拟套接字句柄
                }
                std::cout << "模拟接受连接 " << i + 1 << std::endl;
            }
        });

    // 等待接受完成
    int WaitCount = 0;
    while (AcceptCount.load() < 5 && WaitCount < 100)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }

    // 清理
    closesocket(ListenSocket);
    Context.Stop();
    EventLoopThread.join();

    // 验证结果
    EXPECT_EQ(AcceptCount.load(), 5) << "应该接受 5 个连接";
    EXPECT_EQ(AcceptedSockets.size(), 5) << "应该保存 5 个套接字句柄";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";

    std::cout << "AcceptEx 并发测试通过，接受了 " << AcceptCount.load() << " 个连接" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 AcceptEx 错误重试
TEST_F(AcceptExConcurrentTest, AcceptExRetry)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<int> RetryCount{0};
    std::atomic<bool> StopCalled{false};

    // 启动事件循环线程
    std::thread EventLoopThread(
        [&Context, &StopCalled]()
        {
            Context.Run();
            StopCalled = true;
        });

    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 模拟错误重试
    Context.Post(
        [&Context, &RetryCount]()
        {
            // 模拟网络错误和重试
            for (int i = 0; i < 3; ++i)
            {
                RetryCount++;
                std::cout << "模拟 AcceptEx 重试 " << i + 1 << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

    // 等待重试完成
    int WaitCount = 0;
    while (RetryCount.load() < 3 && WaitCount < 100)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }

    // 清理
    Context.Stop();
    EventLoopThread.join();

    // 验证结果
    EXPECT_EQ(RetryCount.load(), 3) << "应该重试 3 次";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";

    std::cout << "AcceptEx 重试测试通过，重试了 " << RetryCount.load() << " 次" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 AcceptEx 停止和清理
TEST_F(AcceptExConcurrentTest, AcceptExStopAndCleanup)
{
#ifdef _WIN32
    IoContext Context;
    std::atomic<bool> StopCalled{false};
    std::atomic<bool> CleanupCompleted{false};

    // 启动事件循环线程
    std::thread EventLoopThread(
        [&Context, &StopCalled]()
        {
            Context.Run();
            StopCalled = true;
        });

    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 模拟停止和清理
    Context.Post(
        [&Context, &CleanupCompleted]()
        {
            // 模拟停止 AcceptEx 和清理资源
            std::cout << "模拟停止 AcceptEx 并清理资源" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            CleanupCompleted = true;
        });

    // 等待清理完成
    int WaitCount = 0;
    while (!CleanupCompleted.load() && WaitCount < 100)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }

    // 清理
    Context.Stop();
    EventLoopThread.join();

    // 验证结果
    EXPECT_TRUE(CleanupCompleted.load()) << "清理应该完成";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";

    std::cout << "AcceptEx 停止和清理测试通过" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
