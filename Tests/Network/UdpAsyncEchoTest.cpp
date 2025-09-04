#include "Shared/Network/Asio/AsyncUdpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/MessageProtocol.h"
#include "Shared/Network/NetworkTypes.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace Helianthus::Network::Asio;
using namespace Helianthus::Network;

class UdpAsyncEchoTest : public ::testing::Test
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

    static constexpr uint16_t TestPort = 12350;
};

TEST_F(UdpAsyncEchoTest, SimpleEcho)
{
    std::atomic<bool> MessageReceived = false;
    std::atomic<bool> EchoReceived = false;
    std::string ReceivedMessage;

    // 创建服务器
    auto ServerSocket = std::make_shared<AsyncUdpSocket>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort);

    auto BindResult = ServerSocket->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    auto Protocol = std::make_shared<MessageProtocol>();

    Protocol->SetMessageHandler(
        [&MessageReceived, &ReceivedMessage, ServerSocket, Protocol](const std::string& Message)
        {
            ReceivedMessage = Message;
            MessageReceived = true;

            // 回显消息
            auto EchoData = MessageProtocol::EncodeMessage(Message);
            ServerSocket->AsyncSendTo(EchoData.data(),
                                      EchoData.size(),
                                      NetworkAddress("127.0.0.1", TestPort + 1),
                                      [](NetworkError, size_t)
                                      {
                                          // 发送完成
                                      });
        });

    // 开始接收数据
    auto StartReceive = std::make_shared<std::function<void()>>();
    *StartReceive = [ServerSocket, Protocol, StartReceive]()
    {
        auto Buffer = std::make_shared<std::vector<char>>(1024);
        ServerSocket->AsyncReceive(Buffer->data(),
                                   1024,
                                   [Protocol, StartReceive, Buffer](NetworkError Err, size_t Bytes)
                                   {
                                       if (Err == NetworkError::NONE && Bytes > 0)
                                       {
                                           Protocol->ProcessReceivedData(Buffer->data(), Bytes);
                                           (*StartReceive)();  // 继续接收
                                       }
                                   });
    };
    (*StartReceive)();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 创建客户端
    auto ClientSocket = std::make_shared<AsyncUdpSocket>(ClientContext);
    auto ClientProtocol = std::make_shared<MessageProtocol>();

    ClientProtocol->SetMessageHandler(
        [&EchoReceived](const std::string& Message)
        {
            EXPECT_EQ(Message, "Hello, UDP Echo Server!");
            EchoReceived = true;
        });

    // 绑定客户端到不同端口
    auto ClientBindResult = ClientSocket->Bind(NetworkAddress("127.0.0.1", TestPort + 1));
    ASSERT_EQ(ClientBindResult, NetworkError::NONE);

    // 发送测试消息
    std::string TestMessage = "Hello, UDP Echo Server!";
    auto MessageData = MessageProtocol::EncodeMessage(TestMessage);
    ClientSocket->AsyncSendTo(MessageData.data(),
                              MessageData.size(),
                              ServerAddr,
                              [](NetworkError Err, size_t) { EXPECT_EQ(Err, NetworkError::NONE); });

    // 开始接收回显
    auto StartClientReceive = std::make_shared<std::function<void()>>();
    *StartClientReceive = [ClientSocket, ClientProtocol, StartClientReceive]()
    {
        auto Buffer = std::make_shared<std::vector<char>>(1024);
        ClientSocket->AsyncReceive(
            Buffer->data(),
            1024,
            [ClientProtocol, StartClientReceive, Buffer](NetworkError Err, size_t Bytes)
            {
                if (Err == NetworkError::NONE && Bytes > 0)
                {
                    ClientProtocol->ProcessReceivedData(Buffer->data(), Bytes);
                    (*StartClientReceive)();  // 继续接收
                }
            });
    };
    (*StartClientReceive)();

    // 等待测试完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(MessageReceived);
    EXPECT_TRUE(EchoReceived);
    EXPECT_EQ(ReceivedMessage, TestMessage);
}

TEST_F(UdpAsyncEchoTest, MultipleMessages)
{
    // 测试多个消息的传输
    std::atomic<int> MessagesReceived = 0;
    std::atomic<int> EchoReceived = 0;
    std::vector<std::string> ReceivedMessages;
    std::mutex MessagesMutex;

    // 创建服务器
    auto ServerSocket = std::make_shared<AsyncUdpSocket>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort + 2);

    auto BindResult = ServerSocket->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    auto Protocol = std::make_shared<MessageProtocol>();

    Protocol->SetMessageHandler(
        [&MessagesReceived, &ReceivedMessages, &MessagesMutex, ServerSocket, Protocol](
            const std::string& Message)
        {
            {
                std::lock_guard<std::mutex> Lock(MessagesMutex);
                ReceivedMessages.push_back(Message);
            }
            MessagesReceived++;

            // 回显消息
            auto EchoData = MessageProtocol::EncodeMessage(Message);
            ServerSocket->AsyncSendTo(EchoData.data(),
                                      EchoData.size(),
                                      NetworkAddress("127.0.0.1", TestPort + 3),
                                      [](NetworkError, size_t)
                                      {
                                          // 发送完成
                                      });
        });

    // 开始接收数据
    auto StartReceive = std::make_shared<std::function<void()>>();
    *StartReceive = [ServerSocket, Protocol, StartReceive]()
    {
        auto Buffer = std::make_shared<std::vector<char>>(1024);
        ServerSocket->AsyncReceive(Buffer->data(),
                                   1024,
                                   [Protocol, StartReceive, Buffer](NetworkError Err, size_t Bytes)
                                   {
                                       if (Err == NetworkError::NONE && Bytes > 0)
                                       {
                                           Protocol->ProcessReceivedData(Buffer->data(), Bytes);
                                           (*StartReceive)();  // 继续接收
                                       }
                                   });
    };
    (*StartReceive)();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 创建客户端
    auto ClientSocket = std::make_shared<AsyncUdpSocket>(ClientContext);
    auto ClientProtocol = std::make_shared<MessageProtocol>();

    ClientProtocol->SetMessageHandler(
        [&EchoReceived](const std::string& Message)
        {
            // 验证消息格式
            EXPECT_TRUE(Message.find("UDPTest:") == 0);
            EchoReceived++;
        });

    // 绑定客户端到不同端口
    auto ClientBindResult = ClientSocket->Bind(NetworkAddress("127.0.0.1", TestPort + 3));
    ASSERT_EQ(ClientBindResult, NetworkError::NONE);

    // 发送多个测试消息
    std::vector<std::string> TestMessages = {
        "UDPTest:Message1", "UDPTest:Message2", "UDPTest:Message3", "UDPTest:Message4"};

    for (const auto& TestMessage : TestMessages)
    {
        auto MessageData = MessageProtocol::EncodeMessage(TestMessage);
        ClientSocket->AsyncSendTo(MessageData.data(),
                                  MessageData.size(),
                                  ServerAddr,
                                  [](NetworkError Err, size_t)
                                  { EXPECT_EQ(Err, NetworkError::NONE); });
    }

    // 开始接收回显
    auto StartClientReceive = std::make_shared<std::function<void()>>();
    *StartClientReceive = [ClientSocket, ClientProtocol, StartClientReceive]()
    {
        auto Buffer = std::make_shared<std::vector<char>>(1024);
        ClientSocket->AsyncReceive(
            Buffer->data(),
            1024,
            [ClientProtocol, StartClientReceive, Buffer](NetworkError Err, size_t Bytes)
            {
                if (Err == NetworkError::NONE && Bytes > 0)
                {
                    ClientProtocol->ProcessReceivedData(Buffer->data(), Bytes);
                    (*StartClientReceive)();  // 继续接收
                }
            });
    };
    (*StartClientReceive)();

    // 等待测试完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(MessagesReceived, TestMessages.size());
    EXPECT_EQ(EchoReceived, TestMessages.size());
    {
        std::lock_guard<std::mutex> Lock(MessagesMutex);
        EXPECT_EQ(ReceivedMessages.size(), TestMessages.size());
        for (size_t i = 0; i < TestMessages.size() && i < ReceivedMessages.size(); ++i)
        {
            EXPECT_EQ(ReceivedMessages[i], TestMessages[i]);
        }
    }
}

TEST_F(UdpAsyncEchoTest, LargeMessageEcho)
{
    // 测试大消息的传输
    std::atomic<bool> MessageReceived = false;
    std::atomic<bool> EchoReceived = false;
    std::string ReceivedMessage;

    // 创建服务器
    auto ServerSocket = std::make_shared<AsyncUdpSocket>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort + 4);

    auto BindResult = ServerSocket->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    auto Protocol = std::make_shared<MessageProtocol>();

    Protocol->SetMessageHandler(
        [&MessageReceived, &ReceivedMessage, ServerSocket, Protocol](const std::string& Message)
        {
            ReceivedMessage = Message;
            MessageReceived = true;

            // 回显消息
            auto EchoData = MessageProtocol::EncodeMessage(Message);
            ServerSocket->AsyncSendTo(EchoData.data(),
                                      EchoData.size(),
                                      NetworkAddress("127.0.0.1", TestPort + 5),
                                      [](NetworkError, size_t)
                                      {
                                          // 发送完成
                                      });
        });

    // 开始接收数据
    auto StartReceive = std::make_shared<std::function<void()>>();
    *StartReceive = [ServerSocket, Protocol, StartReceive]()
    {
        auto Buffer = std::make_shared<std::vector<char>>(8192);  // 更大的缓冲区
        ServerSocket->AsyncReceive(Buffer->data(),
                                   8192,
                                   [Protocol, StartReceive, Buffer](NetworkError Err, size_t Bytes)
                                   {
                                       if (Err == NetworkError::NONE && Bytes > 0)
                                       {
                                           Protocol->ProcessReceivedData(Buffer->data(), Bytes);
                                           (*StartReceive)();  // 继续接收
                                       }
                                   });
    };
    (*StartReceive)();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 创建客户端
    auto ClientSocket = std::make_shared<AsyncUdpSocket>(ClientContext);
    auto ClientProtocol = std::make_shared<MessageProtocol>();

    ClientProtocol->SetMessageHandler(
        [&EchoReceived](const std::string& Message)
        {
            // 验证大消息内容
            EXPECT_EQ(Message.size(), 2000);
            EXPECT_EQ(Message.substr(0, 10), "LargeData:");
            EchoReceived = true;
        });

    // 绑定客户端到不同端口
    auto ClientBindResult = ClientSocket->Bind(NetworkAddress("127.0.0.1", TestPort + 5));
    ASSERT_EQ(ClientBindResult, NetworkError::NONE);

    // 发送大消息
    std::string LargeMessage = "LargeData:" + std::string(1990, 'X');  // 2000字节消息
    auto MessageData = MessageProtocol::EncodeMessage(LargeMessage);
    ClientSocket->AsyncSendTo(MessageData.data(),
                              MessageData.size(),
                              ServerAddr,
                              [](NetworkError Err, size_t) { EXPECT_EQ(Err, NetworkError::NONE); });

    // 开始接收回显
    auto StartClientReceive = std::make_shared<std::function<void()>>();
    *StartClientReceive = [ClientSocket, ClientProtocol, StartClientReceive]()
    {
        auto Buffer = std::make_shared<std::vector<char>>(8192);
        ClientSocket->AsyncReceive(
            Buffer->data(),
            8192,
            [ClientProtocol, StartClientReceive, Buffer](NetworkError Err, size_t Bytes)
            {
                if (Err == NetworkError::NONE && Bytes > 0)
                {
                    ClientProtocol->ProcessReceivedData(Buffer->data(), Bytes);
                    (*StartClientReceive)();  // 继续接收
                }
            });
    };
    (*StartClientReceive)();

    // 等待测试完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(MessageReceived);
    EXPECT_TRUE(EchoReceived);
    EXPECT_EQ(ReceivedMessage, LargeMessage);
}
