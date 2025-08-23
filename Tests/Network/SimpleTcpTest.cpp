#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/NetworkTypes.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

using namespace Helianthus::Network::Asio;
using namespace Helianthus::Network;

class SimpleTcpTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ServerContext = std::make_shared<IoContext>();
        ClientContext = std::make_shared<IoContext>();

        // 启动服务器事件循环
        ServerThread = std::thread([this]() { ServerContext->Run(); });

        // 启动客户端事件循环
        ClientThread = std::thread([this]() { ClientContext->Run(); });

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void TearDown() override
    {
        if (ServerContext)
        {
            ServerContext->Stop();
        }
        if (ClientContext)
        {
            ClientContext->Stop();
        }

        if (ServerThread.joinable())
        {
            ServerThread.join();
        }
        if (ClientThread.joinable())
        {
            ClientThread.join();
        }
    }

    std::shared_ptr<IoContext> ServerContext;
    std::shared_ptr<IoContext> ClientContext;
    std::thread ServerThread;
    std::thread ClientThread;
    // 持有已接受的服务器连接，防止过早析构
    std::shared_ptr<AsyncTcpSocket> AcceptedServerSocket;
    // 持有服务器端接收缓冲区，确保在异步完成前不被释放
    std::shared_ptr<std::vector<char>> ServerRecvBuffer;

    static constexpr uint16_t TestPort = 12350;
};

TEST_F(SimpleTcpTest, BasicConnection)
{
    std::atomic<bool> ServerReady = false;
    std::atomic<bool> ClientConnected = false;

    // 创建服务器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort);

    auto BindResult = Acceptor->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    // 服务器接受连接
    Acceptor->AsyncAccept(
        [this, ServerReady = std::ref(ServerReady)](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
        {
            EXPECT_EQ(Err, NetworkError::NONE);
            EXPECT_NE(ServerSocket, nullptr);
            ServerReady.get() = true;
            // 保存连接，保持生命周期
            this->AcceptedServerSocket = ServerSocket;
        });

    // 等待服务器准备就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 创建客户端连接
    auto ClientSocket = std::make_shared<AsyncTcpSocket>(ClientContext);
    auto ConnectResult = ClientSocket->Connect(ServerAddr);
    ASSERT_EQ(ConnectResult, NetworkError::NONE);
    ClientConnected = true;

    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(ServerReady);
    EXPECT_TRUE(ClientConnected);
}

TEST_F(SimpleTcpTest, SimpleSendReceive)
{
    std::atomic<bool> ServerReady = false;
    std::atomic<bool> MessageReceived = false;
    std::string ReceivedMessage;

    // 创建服务器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort + 1);

    auto BindResult = Acceptor->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    // 服务器接受连接
    Acceptor->AsyncAccept(
        [this, ServerReady = std::ref(ServerReady), MessageReceived = std::ref(MessageReceived), ReceivedMessage = std::ref(ReceivedMessage)](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
        {
            EXPECT_EQ(Err, NetworkError::NONE);
            EXPECT_NE(ServerSocket, nullptr);
            ServerReady.get() = true;
            // 保存连接，保持生命周期
            this->AcceptedServerSocket = ServerSocket;

            // 开始接收数据
            std::cout << "Starting AsyncReceive on accepted socket" << std::endl;
            // 在夹具上保存缓冲区，保证生命周期
            this->ServerRecvBuffer = std::make_shared<std::vector<char>>(1024);
            ServerSocket->AsyncReceive(this->ServerRecvBuffer->data(),
                                      1024,
                                      [this, MessageReceived = std::ref(MessageReceived), ReceivedMessage = std::ref(ReceivedMessage)](NetworkError Err, size_t Bytes, NetworkAddress)
                                      {
                                          std::cout << "AsyncReceive callback called with error: " << static_cast<int>(Err) << ", bytes: " << Bytes << std::endl;
                                          if (Err == NetworkError::NONE && Bytes > 0)
                                          {
                                              ReceivedMessage.get() = std::string(this->ServerRecvBuffer->data(), Bytes);
                                              MessageReceived.get() = true;
                                              std::cout << "Message received: " << ReceivedMessage.get() << std::endl;
                                          }
                                      });
        });

    // 等待服务器准备就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 创建客户端连接
    auto ClientSocket = std::make_shared<AsyncTcpSocket>(ClientContext);
    auto ConnectResult = ClientSocket->Connect(ServerAddr);
    ASSERT_EQ(ConnectResult, NetworkError::NONE);

    // 等待连接建立和 AsyncReceive 注册
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 发送测试消息
    std::string TestMessage = "Hello, Server!";
    ClientSocket->AsyncSend(TestMessage.data(),
                            TestMessage.size(),
                            ServerAddr,
                            [](NetworkError Err, size_t Bytes) { EXPECT_EQ(Err, NetworkError::NONE); });
    
    // 立即等待一小段时间确保数据传输
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 等待消息接收
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(ServerReady);
    EXPECT_TRUE(MessageReceived);
    EXPECT_EQ(ReceivedMessage, TestMessage);
}
