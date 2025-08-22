#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
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

class AsyncConnectTest : public ::testing::Test
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

// 测试 AsyncConnect 基本功能
TEST_F(AsyncConnectTest, AsyncConnectBasic)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> ConnectCompleted{false};
    std::atomic<bool> StopCalled{false};
    Helianthus::Network::NetworkError ConnectError = Helianthus::Network::NetworkError::NONE;
    
    // 创建异步 TCP 套接字
    auto AsyncSocket = std::make_shared<AsyncTcpSocket>(ContextPtr);
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr, &StopCalled]() {
        ContextPtr->Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 尝试连接到本地回环地址（预期会失败，因为没有服务器）
    Helianthus::Network::NetworkAddress ServerAddr("127.0.0.1", 12345);
    
    ContextPtr->Post([ContextPtr, &ServerAddr, &ConnectCompleted, &ConnectError, AsyncSocket]() {
        AsyncSocket->AsyncConnect(ServerAddr, 
            [&ConnectCompleted, &ConnectError](Helianthus::Network::NetworkError Error) {
                ConnectError = Error;
                ConnectCompleted.store(true);
                std::cout << "AsyncConnect 完成，错误: " << static_cast<int>(Error) << std::endl;
            });
    });
    
    // 等待连接完成
    int WaitCount = 0;
    while (!ConnectCompleted.load() && WaitCount < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 清理
    ContextPtr->Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(ConnectCompleted.load()) << "连接操作应该完成";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    
    std::cout << "AsyncConnect 基本测试完成" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 AsyncConnect 取消功能
TEST_F(AsyncConnectTest, AsyncConnectCancel)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> ConnectCancelled{false};
    std::atomic<bool> StopCalled{false};
    Helianthus::Network::NetworkError CancelError = Helianthus::Network::NetworkError::NONE;
    
    // 创建异步 TCP 套接字
    auto AsyncSocket = std::make_shared<AsyncTcpSocket>(ContextPtr);
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr, &StopCalled]() {
        ContextPtr->Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 尝试连接到本地回环地址
    Helianthus::Network::NetworkAddress ServerAddr("127.0.0.1", 12346);
    
    ContextPtr->Post([ContextPtr, &ServerAddr, &ConnectCancelled, &CancelError, AsyncSocket]() {
        AsyncSocket->AsyncConnect(ServerAddr, 
            [&ConnectCancelled, &CancelError](Helianthus::Network::NetworkError Error) {
                CancelError = Error;
                ConnectCancelled.store(true);
                std::cout << "AsyncConnect 取消完成，错误: " << static_cast<int>(Error) << std::endl;
            });
        
        // 立即取消连接
        AsyncSocket->Close();
    });
    
    // 等待操作完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 清理
    ContextPtr->Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(ConnectCancelled.load()) << "连接操作应该被取消";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    
    std::cout << "AsyncConnect 取消测试完成" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

// 测试 AsyncConnect 与 AsyncSend/AsyncReceive 的集成
TEST_F(AsyncConnectTest, AsyncConnectWithIO)
{
#ifdef _WIN32
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> ConnectCompleted{false};
    std::atomic<bool> StopCalled{false};
    Helianthus::Network::NetworkError ConnectError = Helianthus::Network::NetworkError::NONE;
    
    // 创建异步 TCP 套接字
    auto AsyncSocket = std::make_shared<AsyncTcpSocket>(ContextPtr);
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr, &StopCalled]() {
        ContextPtr->Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 尝试连接到本地回环地址
    Helianthus::Network::NetworkAddress ServerAddr("127.0.0.1", 12347);
    
    ContextPtr->Post([ContextPtr, &ServerAddr, &ConnectCompleted, &ConnectError, AsyncSocket]() {
        AsyncSocket->AsyncConnect(ServerAddr, 
            [&ConnectCompleted, &ConnectError, AsyncSocket](Helianthus::Network::NetworkError Error) {
                ConnectError = Error;
                ConnectCompleted.store(true);
                std::cout << "AsyncConnect 完成，错误: " << static_cast<int>(Error) << std::endl;
                
                // 如果连接成功，尝试发送数据
                if (Error == Helianthus::Network::NetworkError::NONE) {
                    std::string TestData = "Hello, AsyncConnect!";
                    AsyncSocket->AsyncSend(TestData.data(), TestData.size(),
                        [](Helianthus::Network::NetworkError SendError, size_t Bytes) {
                            std::cout << "AsyncSend 完成，错误: " << static_cast<int>(SendError) 
                                     << "，字节: " << Bytes << std::endl;
                        });
                }
            });
    });
    
    // 等待连接完成
    int WaitCount = 0;
    while (!ConnectCompleted.load() && WaitCount < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 清理
    ContextPtr->Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(ConnectCompleted.load()) << "连接操作应该完成";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    
    std::cout << "AsyncConnect 与 I/O 集成测试完成" << std::endl;
#else
    GTEST_SKIP() << "此测试仅在 Windows 平台上运行";
#endif
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
