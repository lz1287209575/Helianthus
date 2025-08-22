#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/MessageProtocol.h"
#include "Shared/Network/NetworkTypes.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <mutex>

#include <gtest/gtest.h>

using namespace Helianthus::Network::Asio;
using namespace Helianthus::Network;

class TcpAsyncEchoTest : public ::testing::Test
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

    static constexpr uint16_t TestPort = 12346;
};

TEST_F(TcpAsyncEchoTest, SimpleEcho)
{
    std::atomic<bool> ServerReady = false;
    std::atomic<bool> MessageReceived = false;
    std::atomic<bool> EchoReceived = false;
    std::string ReceivedMessage;

    // 创建服务器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort);

    auto BindResult = Acceptor->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    // 服务器接受连接
    Acceptor->AsyncAccept(
        [&ServerReady, &MessageReceived, &ReceivedMessage](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
        {
            EXPECT_EQ(Err, NetworkError::NONE);
            EXPECT_NE(ServerSocket, nullptr);
            ServerReady = true;

            auto Protocol = std::make_shared<MessageProtocol>();

            Protocol->SetMessageHandler(
                [&MessageReceived, &ReceivedMessage, ServerSocket, Protocol](
                    const std::string& Message)
                {
                    ReceivedMessage = Message;
                    MessageReceived = true;

                    // 回显消息
                    auto EchoData = MessageProtocol::EncodeMessage(Message);
                    ServerSocket->AsyncSend(EchoData.data(),
                                            EchoData.size(),
                                            [](NetworkError, size_t)
                                            {
                                                // 发送完成
                                            });
                });

            // 开始接收数据（使用 shared_ptr 递归闭包，确保生命周期安全）
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
        });

    // 等待服务器准备就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 创建客户端连接
    auto ClientSocket = std::make_shared<AsyncTcpSocket>(ClientContext);
    auto ClientProtocol = std::make_shared<MessageProtocol>();

    ClientProtocol->SetMessageHandler(
        [&EchoReceived](const std::string& Message)
        {
            EXPECT_EQ(Message, "Hello, TCP Echo Server!");
            EchoReceived = true;
        });

    auto ConnectResult = ClientSocket->Connect(ServerAddr);
    ASSERT_EQ(ConnectResult, NetworkError::NONE);

    // 发送测试消息
    std::string TestMessage = "Hello, TCP Echo Server!";
    auto MessageData = MessageProtocol::EncodeMessage(TestMessage);
    ClientSocket->AsyncSend(MessageData.data(),
                            MessageData.size(),
                            [](NetworkError Err, size_t) { EXPECT_EQ(Err, NetworkError::NONE); });

    // 开始接收回显（使用 shared_ptr 递归闭包）
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

TEST_F(TcpAsyncEchoTest, FragmentedMessages)
{
    // 测试半包/粘包处理
    std::atomic<int> MessagesReceived = 0;
    std::vector<std::string> ReceivedMessages;
    std::mutex MessagesMutex;

    // 创建服务器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort + 2);

    auto BindResult = Acceptor->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    // 服务器接受连接
    Acceptor->AsyncAccept(
        [&MessagesReceived, &ReceivedMessages, &MessagesMutex](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
        {
            EXPECT_EQ(Err, NetworkError::NONE);
            EXPECT_NE(ServerSocket, nullptr);

            auto Protocol = std::make_shared<MessageProtocol>();

            Protocol->SetMessageHandler(
                [&MessagesReceived, &ReceivedMessages, &MessagesMutex](const std::string& Message)
                {
                    {
                        std::lock_guard<std::mutex> Lock(MessagesMutex);
                        ReceivedMessages.push_back(Message);
                    }
                    MessagesReceived++;
                });

            // 开始接收数据（使用 shared_ptr 递归闭包）
            auto StartReceive = std::make_shared<std::function<void()>>();
            *StartReceive = [ServerSocket, Protocol, StartReceive]()
            {
                auto Buffer = std::make_shared<std::vector<char>>(16);  // 小缓冲区模拟分片
                ServerSocket->AsyncReceive(Buffer->data(),
                                          16,
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
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 创建客户端连接
    auto ClientSocket = std::make_shared<AsyncTcpSocket>(ClientContext);
    auto ConnectResult = ClientSocket->Connect(ServerAddr);
    ASSERT_EQ(ConnectResult, NetworkError::NONE);

    // 发送多个消息，模拟粘包
    std::vector<std::string> TestMessages = {"Message 1",
                                             "This is a longer message 2",
                                             "Short msg 3",
                                             "Another message with different length 4"};

    // 将所有消息编码并拼接成一个大的数据包
    std::vector<char> CombinedData;
    for (const auto& Msg : TestMessages)
    {
        auto Encoded = MessageProtocol::EncodeMessage(Msg);
        CombinedData.insert(CombinedData.end(), Encoded.begin(), Encoded.end());
    }

    // 分片发送，模拟半包
    size_t BytesToSend = CombinedData.size();
    size_t Offset = 0;
    const size_t ChunkSize = 7;  // 小块发送

    auto SendNextChunk = std::make_shared<std::function<void()>>();
    *SendNextChunk = [ClientSocket, &CombinedData, &Offset, ChunkSize, BytesToSend, SendNextChunk]()
    {
        if (Offset >= BytesToSend)
            return;

        size_t CurrentChunkSize = std::min(ChunkSize, BytesToSend - Offset);
        ClientSocket->AsyncSend(CombinedData.data() + Offset,
                                CurrentChunkSize,
                                [&Offset, CurrentChunkSize, SendNextChunk](NetworkError Err, size_t)
                                {
                                    EXPECT_EQ(Err, NetworkError::NONE);
                                    Offset += CurrentChunkSize;
                                    // 继续发送下一块
                                    (*SendNextChunk)();
                                });
    };

    (*SendNextChunk)();

    // 等待所有消息被接收
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_EQ(MessagesReceived, 4);
    {
        std::lock_guard<std::mutex> Lock(MessagesMutex);
        EXPECT_EQ(ReceivedMessages.size(), 4);
        for (size_t I = 0; I < TestMessages.size() && I < ReceivedMessages.size(); ++I)
        {
            EXPECT_EQ(ReceivedMessages[I], TestMessages[I]);
        }
    }
}

TEST_F(TcpAsyncEchoTest, LargeMessageEcho)
{
    // 测试大消息的传输
    std::atomic<bool> ServerReady = false;
    std::atomic<bool> MessageReceived = false;
    std::atomic<bool> EchoReceived = false;
    std::string ReceivedMessage;

    // 创建服务器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort + 4);

    auto BindResult = Acceptor->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    // 服务器接受连接
    Acceptor->AsyncAccept(
        [&ServerReady, &MessageReceived, &ReceivedMessage](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
        {
            EXPECT_EQ(Err, NetworkError::NONE);
            EXPECT_NE(ServerSocket, nullptr);
            ServerReady = true;

            auto Protocol = std::make_shared<MessageProtocol>();

            Protocol->SetMessageHandler(
                [&MessageReceived, &ReceivedMessage, ServerSocket, Protocol](
                    const std::string& Message)
                {
                    ReceivedMessage = Message;
                    MessageReceived = true;

                    // 回显消息
                    auto EchoData = MessageProtocol::EncodeMessage(Message);
                    ServerSocket->AsyncSend(EchoData.data(),
                                            EchoData.size(),
                                            [](NetworkError, size_t)
                                            {
                                                // 发送完成
                                            });
                });

            // 开始接收数据
            auto StartReceive = std::make_shared<std::function<void()>>();
            *StartReceive = [ServerSocket, Protocol, StartReceive]()
            {
                auto Buffer = std::make_shared<std::vector<char>>(8192); // 更大的缓冲区
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
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 创建客户端连接
    auto ClientSocket = std::make_shared<AsyncTcpSocket>(ClientContext);
    auto ClientProtocol = std::make_shared<MessageProtocol>();

    ClientProtocol->SetMessageHandler(
        [&EchoReceived](const std::string& Message)
        {
            // 验证大消息内容
            EXPECT_EQ(Message.size(), 5000);
            EXPECT_EQ(Message.substr(0, 10), "LargeData:");
            EchoReceived = true;
        });

    auto ConnectResult = ClientSocket->Connect(ServerAddr);
    ASSERT_EQ(ConnectResult, NetworkError::NONE);

    // 发送大消息
    std::string LargeMessage = "LargeData:" + std::string(4990, 'X'); // 5000字节消息
    auto MessageData = MessageProtocol::EncodeMessage(LargeMessage);
    ClientSocket->AsyncSend(MessageData.data(),
                            MessageData.size(),
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

TEST_F(TcpAsyncEchoTest, ConcurrentConnections)
{
    // 测试并发连接处理
    const int NumConnections = 5;
    std::atomic<int> ConnectedClients = 0;
    std::atomic<int> MessagesReceived = 0;
    std::vector<std::string> ReceivedMessages;
    std::mutex MessagesMutex;

    // 创建服务器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort + 6);

    auto BindResult = Acceptor->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    // 递归接受连接的函数
    std::function<void()> AcceptNextConnection = [&]() {
        Acceptor->AsyncAccept(
            [&ConnectedClients, &MessagesReceived, &ReceivedMessages, &MessagesMutex, &AcceptNextConnection](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
            {
                EXPECT_EQ(Err, NetworkError::NONE);
                EXPECT_NE(ServerSocket, nullptr);
                ConnectedClients++;

                auto Protocol = std::make_shared<MessageProtocol>();

                Protocol->SetMessageHandler(
                    [&MessagesReceived, &ReceivedMessages, &MessagesMutex, ServerSocket, Protocol](const std::string& Message)
                    {
                        {
                            std::lock_guard<std::mutex> Lock(MessagesMutex);
                            ReceivedMessages.push_back(Message);
                        }
                        MessagesReceived++;

                        // 回显消息
                        auto EchoData = MessageProtocol::EncodeMessage(Message);
                        ServerSocket->AsyncSend(EchoData.data(),
                                                EchoData.size(),
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

                // 继续接受下一个连接
                AcceptNextConnection();
            });
    };

    // 开始接受连接
    AcceptNextConnection();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 创建多个客户端连接
    std::vector<std::shared_ptr<AsyncTcpSocket>> ClientSockets;
    std::vector<std::shared_ptr<MessageProtocol>> ClientProtocols;
    std::atomic<int> EchoReceived = 0;

    for (int i = 0; i < NumConnections; ++i)
    {
        auto ClientSocket = std::make_shared<AsyncTcpSocket>(ClientContext);
        auto ClientProtocol = std::make_shared<MessageProtocol>();

        ClientProtocol->SetMessageHandler(
            [&EchoReceived, i](const std::string& Message)
            {
                EXPECT_EQ(Message, "ConcurrentTest:" + std::to_string(i));
                EchoReceived++;
            });

        auto ConnectResult = ClientSocket->Connect(ServerAddr);
        ASSERT_EQ(ConnectResult, NetworkError::NONE);

        ClientSockets.push_back(ClientSocket);
        ClientProtocols.push_back(ClientProtocol);

        // 发送测试消息
        std::string TestMessage = "ConcurrentTest:" + std::to_string(i);
        auto MessageData = MessageProtocol::EncodeMessage(TestMessage);
        ClientSocket->AsyncSend(MessageData.data(),
                                MessageData.size(),
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
    }

    // 等待测试完成
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    EXPECT_EQ(ConnectedClients, NumConnections);
    EXPECT_EQ(MessagesReceived, NumConnections);
    EXPECT_EQ(EchoReceived, NumConnections);
    {
        std::lock_guard<std::mutex> Lock(MessagesMutex);
        EXPECT_EQ(ReceivedMessages.size(), NumConnections);
    }
}

TEST_F(TcpAsyncEchoTest, StressTest)
{
    // 压力测试：快速发送大量小消息
    const int NumMessages = 100;
    std::atomic<int> MessagesReceived = 0;
    std::atomic<int> EchoReceived = 0;
    std::vector<std::string> ReceivedMessages;
    std::mutex MessagesMutex;

    // 创建服务器
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ServerContext);
    NetworkAddress ServerAddr("127.0.0.1", TestPort + 8);

    auto BindResult = Acceptor->Bind(ServerAddr);
    ASSERT_EQ(BindResult, NetworkError::NONE);

    // 服务器接受连接
    Acceptor->AsyncAccept(
        [&MessagesReceived, &ReceivedMessages, &MessagesMutex](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
        {
            EXPECT_EQ(Err, NetworkError::NONE);
            EXPECT_NE(ServerSocket, nullptr);

            auto Protocol = std::make_shared<MessageProtocol>();

            Protocol->SetMessageHandler(
                [&MessagesReceived, &ReceivedMessages, &MessagesMutex, ServerSocket, Protocol](const std::string& Message)
                {
                    {
                        std::lock_guard<std::mutex> Lock(MessagesMutex);
                        ReceivedMessages.push_back(Message);
                    }
                    MessagesReceived++;

                    // 回显消息
                    auto EchoData = MessageProtocol::EncodeMessage(Message);
                    ServerSocket->AsyncSend(EchoData.data(),
                                            EchoData.size(),
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
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 创建客户端连接
    auto ClientSocket = std::make_shared<AsyncTcpSocket>(ClientContext);
    auto ClientProtocol = std::make_shared<MessageProtocol>();

    ClientProtocol->SetMessageHandler(
        [&EchoReceived](const std::string& Message)
        {
            // 验证消息格式
            EXPECT_TRUE(Message.find("StressTest:") == 0);
            EchoReceived++;
        });

    auto ConnectResult = ClientSocket->Connect(ServerAddr);
    ASSERT_EQ(ConnectResult, NetworkError::NONE);

    // 快速发送大量消息
    for (int i = 0; i < NumMessages; ++i)
    {
        std::string TestMessage = "StressTest:" + std::to_string(i);
        auto MessageData = MessageProtocol::EncodeMessage(TestMessage);
        ClientSocket->AsyncSend(MessageData.data(),
                                MessageData.size(),
                                [](NetworkError Err, size_t) { EXPECT_EQ(Err, NetworkError::NONE); });
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
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    EXPECT_EQ(MessagesReceived, NumMessages);
    EXPECT_EQ(EchoReceived, NumMessages);
    {
        std::lock_guard<std::mutex> Lock(MessagesMutex);
        EXPECT_EQ(ReceivedMessages.size(), NumMessages);
    }
}
