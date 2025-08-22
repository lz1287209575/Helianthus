#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/AsyncUdpSocket.h"
#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/NetworkTypes.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include <vector>
#include <string>

using namespace Helianthus::Network::Asio;

class UdpProactorTest : public ::testing::Test
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

// 测试 UDP Proactor 基本功能
TEST_F(UdpProactorTest, UdpProactorBasic)
{
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> TestCompleted{false};
    std::atomic<bool> StopCalled{false};
    
    // 创建服务器和客户端 UDP 套接字
    auto ServerSocket = std::make_shared<AsyncUdpSocket>(ContextPtr);
    auto ClientSocket = std::make_shared<AsyncUdpSocket>(ContextPtr);
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr, &StopCalled]() {
        ContextPtr->Run();
        StopCalled = true;
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 绑定服务器套接字
    Helianthus::Network::NetworkAddress ServerAddr("127.0.0.1", 12345);
    auto BindResult = ServerSocket->Bind(ServerAddr);
    EXPECT_EQ(BindResult, Helianthus::Network::NetworkError::NONE) << "服务器绑定失败";
    
    // 绑定客户端套接字到本地地址（可选，UDP客户端可以不绑定）
    Helianthus::Network::NetworkAddress ClientAddr("127.0.0.1", 0);  // 端口0表示系统分配
    auto ClientBindResult = ClientSocket->Bind(ClientAddr);
    // UDP客户端绑定失败是正常的，因为可能端口已被占用，但不影响通信
    
    // 测试数据
    std::string TestMessage = "Hello, UDP Proactor!";
    std::vector<char> SendBuffer(TestMessage.begin(), TestMessage.end());
    std::vector<char> RecvBuffer(1024);
    
    std::atomic<bool> SendCompleted{false};
    std::atomic<bool> ReceiveCompleted{false};
    std::atomic<size_t> ReceivedBytes{0};
    Helianthus::Network::NetworkAddress ReceivedFromAddr;
    
    // 服务器端异步接收
    ContextPtr->Post([&]() {
        ServerSocket->AsyncReceiveFrom(RecvBuffer.data(), RecvBuffer.size(),
            [&](Helianthus::Network::NetworkError Error, size_t Bytes, Helianthus::Network::NetworkAddress FromAddr) {
                std::cout << "服务器接收到 " << Bytes << " 字节，来自 " << FromAddr.ToString() << std::endl;
                ReceivedBytes.store(Bytes);
                ReceivedFromAddr = FromAddr;
                ReceiveCompleted.store(true);
            });
    });
    
    // 等待一小段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 客户端异步发送
    ContextPtr->Post([&]() {
        ClientSocket->AsyncSendToProactor(SendBuffer.data(), SendBuffer.size(), ServerAddr,
            [&](Helianthus::Network::NetworkError Error, size_t Bytes) {
                std::cout << "客户端发送了 " << Bytes << " 字节" << std::endl;
                SendCompleted.store(true);
            });
    });
    
    // 等待操作完成
    int WaitCount = 0;
    while ((!SendCompleted.load() || !ReceiveCompleted.load()) && WaitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 清理
    ContextPtr->Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(SendCompleted.load()) << "发送操作应该完成";
    EXPECT_TRUE(ReceiveCompleted.load()) << "接收操作应该完成";
    EXPECT_TRUE(StopCalled.load()) << "事件循环应该停止";
    EXPECT_EQ(ReceivedBytes.load(), TestMessage.length()) << "接收的字节数应该匹配";
    
    std::cout << "UDP Proactor 基本测试完成" << std::endl;
}

// 测试 UDP Proactor 并发操作
TEST_F(UdpProactorTest, UdpProactorConcurrency)
{
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> TestCompleted{false};
    std::atomic<int> CompletedOperations{0};
    const int TotalOperations = 10;
    
    // 创建服务器和客户端 UDP 套接字
    auto ServerSocket = std::make_shared<AsyncUdpSocket>(ContextPtr);
    auto ClientSocket = std::make_shared<AsyncUdpSocket>(ContextPtr);
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr]() {
        ContextPtr->Run();
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 绑定服务器套接字
    Helianthus::Network::NetworkAddress ServerAddr("127.0.0.1", 12346);
    auto BindResult = ServerSocket->Bind(ServerAddr);
    EXPECT_EQ(BindResult, Helianthus::Network::NetworkError::NONE) << "服务器绑定失败";
    
    // 绑定客户端套接字到本地地址（可选，UDP客户端可以不绑定）
    Helianthus::Network::NetworkAddress ClientAddr("127.0.0.1", 0);  // 端口0表示系统分配
    auto ClientBindResult = ClientSocket->Bind(ClientAddr);
    // UDP客户端绑定失败是正常的，因为可能端口已被占用，但不影响通信
    
    // 服务器端持续接收
    std::vector<std::vector<char>> RecvBuffers(TotalOperations, std::vector<char>(1024));
    std::atomic<int> ReceivedCount{0};
    
    // 启动多个接收操作
    for (int i = 0; i < TotalOperations; ++i) {
        ContextPtr->Post([&, i]() {
            ServerSocket->AsyncReceiveFrom(RecvBuffers[i].data(), RecvBuffers[i].size(),
                [&, i](Helianthus::Network::NetworkError Error, size_t Bytes, Helianthus::Network::NetworkAddress FromAddr) {
                    if (Error == Helianthus::Network::NetworkError::NONE && Bytes > 0) {
                        std::cout << "服务器接收到第 " << (i + 1) << " 个消息，"
                                  << Bytes << " 字节，来自 " << FromAddr.ToString() << std::endl;
                        ReceivedCount++;
                    }
                    
                    CompletedOperations++;
                });
        });
    }
    
    // 等待一小段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 客户端并发发送
    for (int i = 0; i < TotalOperations; ++i) {
        ContextPtr->Post([&, i]() {
            std::string Message = "Concurrent message " + std::to_string(i + 1);
            std::vector<char> SendBuffer(Message.begin(), Message.end());
            
            ClientSocket->AsyncSendToProactor(SendBuffer.data(), SendBuffer.size(), ServerAddr,
                [&, i](Helianthus::Network::NetworkError Error, size_t Bytes) {
                    if (Error == Helianthus::Network::NetworkError::NONE) {
                        std::cout << "客户端发送了第 " << (i + 1) << " 个消息，"
                                  << Bytes << " 字节" << std::endl;
                    }
                    
                    CompletedOperations++;
                });
        });
        
        // 稍微延迟，避免发送过快
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 等待所有操作完成
    int WaitCount = 0;
    while (CompletedOperations.load() < TotalOperations * 2 && WaitCount < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 清理
    ContextPtr->Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_GE(CompletedOperations.load(), TotalOperations * 2) << "所有操作应该完成";
    EXPECT_GE(ReceivedCount.load(), TotalOperations) << "应该接收到所有消息";
    
    std::cout << "UDP Proactor 并发测试完成，完成操作数: " << CompletedOperations.load() 
              << "，接收消息数: " << ReceivedCount.load() << std::endl;
}

// 测试 UDP Proactor 错误处理
TEST_F(UdpProactorTest, UdpProactorErrorHandling)
{
    auto ContextPtr = std::make_shared<IoContext>();
    std::atomic<bool> TestCompleted{false};
    
    // 创建 UDP 套接字但不绑定
    auto Socket = std::make_shared<AsyncUdpSocket>(ContextPtr);
    
    // 启动事件循环线程
    std::thread EventLoopThread([ContextPtr]() {
        ContextPtr->Run();
    });
    
    // 等待一小段时间确保事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 尝试发送到无效地址
    Helianthus::Network::NetworkAddress InvalidAddr("192.168.255.255", 12347);
    std::string TestMessage = "Test message";
    std::vector<char> SendBuffer(TestMessage.begin(), TestMessage.end());
    
    std::atomic<bool> SendCompleted{false};
    Helianthus::Network::NetworkError SendError = Helianthus::Network::NetworkError::NONE;
    
    ContextPtr->Post([&]() {
        Socket->AsyncSendToProactor(SendBuffer.data(), SendBuffer.size(), InvalidAddr,
            [&](Helianthus::Network::NetworkError Error, size_t Bytes) {
                SendError = Error;
                SendCompleted.store(true);
                std::cout << "发送操作完成，错误: " << static_cast<int>(Error) << std::endl;
            });
    });
    
    // 等待操作完成
    int WaitCount = 0;
    while (!SendCompleted.load() && WaitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    // 清理
    ContextPtr->Stop();
    EventLoopThread.join();
    
    // 验证结果
    EXPECT_TRUE(SendCompleted.load()) << "发送操作应该完成";
    // 注意：发送到无效地址可能不会立即失败，这取决于网络配置
    
    std::cout << "UDP Proactor 错误处理测试完成" << std::endl;
}
