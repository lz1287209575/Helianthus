#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/NetworkTypes.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif

using namespace Helianthus::Network::Asio;

#ifdef _WIN32
class AcceptExConcurrencyTest : public ::testing::Test
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
#else
// 在非Windows平台上跳过这些测试
class AcceptExConcurrencyTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};
#endif

// 测试 AcceptEx 基本并发功能
TEST_F(AcceptExConcurrencyTest, AcceptExBasicConcurrency)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> StopCompleted{false};
    std::atomic<int> AcceptCount{0};
    std::atomic<int> ConnectionCount{0};

    // 启动事件循环线程
    std::thread EventLoopThread(
        [ContextPtr, &StopCompleted]()
        {
            ContextPtr->Run();
            StopCompleted.store(true);
            std::cout << "IoContext 事件循环已停止" << std::endl;
        });

    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 创建异步 TCP 接受器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ContextPtr);
    Helianthus::Network::NetworkAddress ListenAddr("127.0.0.1", 12348);

    // 启动监听
    ContextPtr->Post(
        [ContextPtr, &ListenAddr, &AcceptCount, Acceptor]()
        {
            // 绑定地址
            auto BindResult = Acceptor->Bind(ListenAddr);
            if (BindResult == Helianthus::Network::NetworkError::NONE)
            {
                // 开始异步接受连接
                Acceptor->AsyncAcceptEx(
                    [&AcceptCount](Helianthus::Network::NetworkError Error, Fd ClientSocket)
                    {
                        if (Error == Helianthus::Network::NetworkError::NONE)
                        {
                            AcceptCount.fetch_add(1);
                            std::cout << "接受连接，客户端套接字: " << ClientSocket << std::endl;

                            // 立即关闭客户端连接（测试用）
                            closesocket(static_cast<SOCKET>(ClientSocket));
                        }
                        else
                        {
                            std::cout << "接受连接失败，错误: " << static_cast<int>(Error)
                                      << std::endl;
                        }
                    });
            }
            else
            {
                std::cout << "绑定地址失败，错误: " << static_cast<int>(BindResult) << std::endl;
            }
        });

    // 等待监听启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 创建多个客户端连接线程来测试并发接受
    std::vector<std::thread> ClientThreads;
    const int ClientCount = 10;

    for (int i = 0; i < ClientCount; ++i)
    {
        ClientThreads.emplace_back(
            [&ListenAddr, &ConnectionCount, i]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));  // 错开连接时间

                SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (ClientSocket != INVALID_SOCKET)
                {
                    sockaddr_in ServerAddr{};
                    ServerAddr.sin_family = AF_INET;
                    ServerAddr.sin_port = htons(ListenAddr.Port);
                    inet_pton(AF_INET, ListenAddr.Ip.c_str(), &ServerAddr.sin_addr);

                    if (connect(ClientSocket,
                                reinterpret_cast<sockaddr*>(&ServerAddr),
                                sizeof(ServerAddr)) == 0)
                    {
                        ConnectionCount.fetch_add(1);
                        std::cout << "客户端 " << i << " 连接成功" << std::endl;
                    }
                    else
                    {
                        std::cout << "客户端 " << i << " 连接失败" << std::endl;
                    }

                    closesocket(ClientSocket);
                }
            });
    }

    // 等待所有客户端连接完成
    for (auto& Thread : ClientThreads)
    {
        Thread.join();
    }

    // 等待一段时间让服务器处理所有连接
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 停止事件循环
    ContextPtr->Stop();
    EventLoopThread.join();

    // 验证结果
    EXPECT_TRUE(StopCompleted.load()) << "事件循环应该停止";
    EXPECT_GT(AcceptCount.load(), 0) << "应该接受至少一个连接";
    EXPECT_GT(ConnectionCount.load(), 0) << "应该建立至少一个连接";

    std::cout << "AcceptEx 基本并发测试完成，接受连接数: " << AcceptCount.load()
              << "，客户端连接数: " << ConnectionCount.load() << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 AcceptEx 高并发性能
TEST_F(AcceptExConcurrencyTest, AcceptExHighConcurrency)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> StopCompleted{false};
    std::atomic<int> AcceptCount{0};
    std::atomic<int> ConnectionCount{0};

    // 启动事件循环线程
    std::thread EventLoopThread(
        [ContextPtr, &StopCompleted]()
        {
            ContextPtr->Run();
            StopCompleted.store(true);
            std::cout << "IoContext 事件循环已停止" << std::endl;
        });

    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 创建异步 TCP 接受器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ContextPtr);
    Helianthus::Network::NetworkAddress ListenAddr("127.0.0.1", 12349);

    // 启动监听
    ContextPtr->Post(
        [ContextPtr, &ListenAddr, &AcceptCount, Acceptor]()
        {
            // 绑定地址
            auto BindResult = Acceptor->Bind(ListenAddr);
            if (BindResult == Helianthus::Network::NetworkError::NONE)
            {
                // 开始异步接受连接
                Acceptor->AsyncAcceptEx(
                    [&AcceptCount](Helianthus::Network::NetworkError Error, Fd ClientSocket)
                    {
                        if (Error == Helianthus::Network::NetworkError::NONE)
                        {
                            AcceptCount.fetch_add(1);

                            // 立即关闭客户端连接（测试用）
                            closesocket(static_cast<SOCKET>(ClientSocket));
                        }
                    });
            }
            else
            {
                std::cout << "绑定地址失败，错误: " << static_cast<int>(BindResult) << std::endl;
            }
        });

    // 等待监听启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 创建大量客户端连接来测试高并发性能
    std::vector<std::thread> ClientThreads;
    const int ClientCount = 50;  // 增加客户端数量

    auto StartTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ClientCount; ++i)
    {
        ClientThreads.emplace_back(
            [&ListenAddr, &ConnectionCount, i]()
            {
                SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (ClientSocket != INVALID_SOCKET)
                {
                    sockaddr_in ServerAddr{};
                    ServerAddr.sin_family = AF_INET;
                    ServerAddr.sin_port = htons(ListenAddr.Port);
                    inet_pton(AF_INET, ListenAddr.Ip.c_str(), &ServerAddr.sin_addr);

                    if (connect(ClientSocket,
                                reinterpret_cast<sockaddr*>(&ServerAddr),
                                sizeof(ServerAddr)) == 0)
                    {
                        ConnectionCount.fetch_add(1);
                    }

                    closesocket(ClientSocket);
                }
            });
    }

    // 等待所有客户端连接完成
    for (auto& Thread : ClientThreads)
    {
        Thread.join();
    }

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);

    // 等待一段时间让服务器处理所有连接
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 停止事件循环
    ContextPtr->Stop();
    EventLoopThread.join();

    // 验证结果
    EXPECT_TRUE(StopCompleted.load()) << "事件循环应该停止";
    EXPECT_GT(AcceptCount.load(), 0) << "应该接受至少一个连接";
    EXPECT_GT(ConnectionCount.load(), 0) << "应该建立至少一个连接";

    std::cout << "AcceptEx 高并发测试完成，接受连接数: " << AcceptCount.load()
              << "，客户端连接数: " << ConnectionCount.load() << "，耗时: " << Duration.count()
              << "ms" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 AcceptEx 动态并发调整
TEST_F(AcceptExConcurrencyTest, AcceptExDynamicAdjustment)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> StopCompleted{false};
    std::atomic<int> AcceptCount{0};

    // 启动事件循环线程
    std::thread EventLoopThread(
        [ContextPtr, &StopCompleted]()
        {
            ContextPtr->Run();
            StopCompleted.store(true);
            std::cout << "IoContext 事件循环已停止" << std::endl;
        });

    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 创建异步 TCP 接受器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ContextPtr);
    Helianthus::Network::NetworkAddress ListenAddr("127.0.0.1", 12350);

    // 启动监听
    ContextPtr->Post(
        [ContextPtr, &ListenAddr, &AcceptCount, Acceptor]()
        {
            // 绑定地址
            auto BindResult = Acceptor->Bind(ListenAddr);
            if (BindResult == Helianthus::Network::NetworkError::NONE)
            {
                // 开始异步接受连接
                Acceptor->AsyncAcceptEx(
                    [&AcceptCount](Helianthus::Network::NetworkError Error, Fd ClientSocket)
                    {
                        if (Error == Helianthus::Network::NetworkError::NONE)
                        {
                            AcceptCount.fetch_add(1);

                            // 立即关闭客户端连接（测试用）
                            closesocket(static_cast<SOCKET>(ClientSocket));
                        }
                    });
            }
            else
            {
                std::cout << "绑定地址失败，错误: " << static_cast<int>(BindResult) << std::endl;
            }
        });

    // 等待监听启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 分阶段创建连接，测试动态调整
    std::vector<std::thread> ClientThreads;

    // 第一阶段：少量连接
    for (int i = 0; i < 5; ++i)
    {
        ClientThreads.emplace_back(
            [&ListenAddr, i]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(i * 50));

                SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (ClientSocket != INVALID_SOCKET)
                {
                    sockaddr_in ServerAddr{};
                    ServerAddr.sin_family = AF_INET;
                    ServerAddr.sin_port = htons(ListenAddr.Port);
                    inet_pton(AF_INET, ListenAddr.Ip.c_str(), &ServerAddr.sin_addr);

                    connect(
                        ClientSocket, reinterpret_cast<sockaddr*>(&ServerAddr), sizeof(ServerAddr));
                    closesocket(ClientSocket);
                }
            });
    }

    // 等待第一阶段完成
    for (auto& Thread : ClientThreads)
    {
        Thread.join();
    }
    ClientThreads.clear();

    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 第二阶段：大量连接
    for (int i = 0; i < 20; ++i)
    {
        ClientThreads.emplace_back(
            [&ListenAddr, i]()
            {
                SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (ClientSocket != INVALID_SOCKET)
                {
                    sockaddr_in ServerAddr{};
                    ServerAddr.sin_family = AF_INET;
                    ServerAddr.sin_port = htons(ListenAddr.Port);
                    inet_pton(AF_INET, ListenAddr.Ip.c_str(), &ServerAddr.sin_addr);

                    connect(
                        ClientSocket, reinterpret_cast<sockaddr*>(&ServerAddr), sizeof(ServerAddr));
                    closesocket(ClientSocket);
                }
            });
    }

    // 等待第二阶段完成
    for (auto& Thread : ClientThreads)
    {
        Thread.join();
    }

    // 等待一段时间让服务器处理所有连接
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 停止事件循环
    ContextPtr->Stop();
    EventLoopThread.join();

    // 验证结果
    EXPECT_TRUE(StopCompleted.load()) << "事件循环应该停止";
    EXPECT_GT(AcceptCount.load(), 0) << "应该接受至少一个连接";

    std::cout << "AcceptEx 动态调整测试完成，接受连接数: " << AcceptCount.load() << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
