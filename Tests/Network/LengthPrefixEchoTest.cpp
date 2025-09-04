#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/NetworkTypes.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

using namespace Helianthus::Network::Asio;
using namespace Helianthus::Network;

namespace
{
struct ReadState
{
    std::shared_ptr<AsyncTcpSocket> Socket;
    std::shared_ptr<std::vector<char>> Buffer;
    size_t TargetSize = 0;
    size_t BytesRead = 0;
};

void ReadExact(std::shared_ptr<ReadState> State, std::function<void(NetworkError)> OnDone)
{
    const size_t Remaining = State->TargetSize - State->BytesRead;
    State->Socket->AsyncReceive(State->Buffer->data() + State->BytesRead,
                                Remaining,
                                [State, OnDone](NetworkError Err, size_t Bytes, NetworkAddress)
                                {
                                    if (Err != NetworkError::NONE)
                                    {
                                        OnDone(Err);
                                        return;
                                    }
                                    State->BytesRead += Bytes;
                                    if (State->BytesRead < State->TargetSize)
                                    {
                                        ReadExact(State, OnDone);
                                    }
                                    else
                                    {
                                        OnDone(NetworkError::NONE);
                                    }
                                });
}
}  // namespace

class LengthPrefixEchoTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ServerContext = std::make_shared<IoContext>();
        ClientContext = std::make_shared<IoContext>();
        ServerThread = std::thread([this] { ServerContext->Run(); });
        ClientThread = std::thread([this] { ClientContext->Run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    void TearDown() override
    {
        if (ServerContext)
            ServerContext->Stop();
        if (ClientContext)
            ClientContext->Stop();
        if (ServerThread.joinable())
            ServerThread.join();
        if (ClientThread.joinable())
            ClientThread.join();
    }

    std::shared_ptr<IoContext> ServerContext;
    std::shared_ptr<IoContext> ClientContext;
    std::thread ServerThread;
    std::thread ClientThread;
    std::shared_ptr<AsyncTcpSocket> AcceptedServerSocket;
};

TEST_F(LengthPrefixEchoTest, SingleMessage)
{
    static constexpr uint16_t TestPort = 12360;
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ServerContext);
    NetworkAddress Addr("127.0.0.1", TestPort);
    ASSERT_EQ(Acceptor->Bind(Addr), NetworkError::NONE);

    std::atomic<bool> Done = false;
    std::string Message = "HelloLengthPrefix";
    std::string Echoed;

    // Server accept and echo back
    Acceptor->AsyncAccept(
        [this, &Done](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
        {
            ASSERT_EQ(Err, NetworkError::NONE);
            ASSERT_NE(ServerSocket, nullptr);
            AcceptedServerSocket = ServerSocket;

            // Read 4-byte length (network order)
            auto HeaderState = std::make_shared<ReadState>();
            HeaderState->Socket = ServerSocket;
            HeaderState->Buffer = std::make_shared<std::vector<char>>(4);
            HeaderState->TargetSize = 4;
            HeaderState->BytesRead = 0;

            ReadExact(HeaderState,
                      [this, ServerSocket, HeaderState, &Done](NetworkError Err2)
                      {
                          ASSERT_EQ(Err2, NetworkError::NONE);
                          uint32_t NetLen = 0;
                          std::memcpy(&NetLen, HeaderState->Buffer->data(), 4);
                          uint32_t BodyLen = ntohl(NetLen);

                          auto BodyState = std::make_shared<ReadState>();
                          BodyState->Socket = ServerSocket;
                          BodyState->Buffer = std::make_shared<std::vector<char>>(BodyLen);
                          BodyState->TargetSize = BodyLen;
                          BodyState->BytesRead = 0;

                          ReadExact(
                              BodyState,
                              [ServerSocket, BodyState, &Done](NetworkError Err3)
                              {
                                  ASSERT_EQ(Err3, NetworkError::NONE);
                                  // Echo back (header + body)
                                  uint32_t NetLenOut =
                                      htonl(static_cast<uint32_t>(BodyState->TargetSize));
                                  char HeaderOut[4];
                                  std::memcpy(HeaderOut, &NetLenOut, 4);
                                  ServerSocket->AsyncSend(
                                      HeaderOut,
                                      4,
                                      NetworkAddress{},
                                      [ServerSocket, BodyState, &Done](NetworkError Err4, size_t)
                                      {
                                          ASSERT_EQ(Err4, NetworkError::NONE);
                                          ServerSocket->AsyncSend(BodyState->Buffer->data(),
                                                                  BodyState->TargetSize,
                                                                  NetworkAddress{},
                                                                  [&Done](NetworkError Err5, size_t)
                                                                  {
                                                                      ASSERT_EQ(Err5,
                                                                                NetworkError::NONE);
                                                                      Done = true;
                                                                  });
                                      });
                              });
                      });
        });

    // Client connect and send
    auto Client = std::make_shared<AsyncTcpSocket>(ClientContext);

    // 使用异步连接
    std::atomic<bool> ConnectCompleted = false;
    NetworkError ConnectResult = NetworkError::NONE;

    Client->AsyncConnect(Addr,
                         [&ConnectCompleted, &ConnectResult](NetworkError Err)
                         {
                             ConnectResult = Err;
                             ConnectCompleted = true;
                         });

    // 等待连接完成
    for (int i = 0; i < 50 && !ConnectCompleted.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(ConnectCompleted.load());
    ASSERT_EQ(ConnectResult, NetworkError::NONE);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // build header
    uint32_t NetLen = htonl(static_cast<uint32_t>(Message.size()));
    char Header[4];
    std::memcpy(Header, &NetLen, 4);

    // Send header + body
    std::atomic<bool> ClientDone = false;
    Client->AsyncSend(Header,
                      4,
                      NetworkAddress{},
                      [Client, &Message, &ClientDone](NetworkError Err, size_t)
                      {
                          ASSERT_EQ(Err, NetworkError::NONE);
                          Client->AsyncSend(Message.data(),
                                            Message.size(),
                                            NetworkAddress{},
                                            [Client, &ClientDone](NetworkError Err2, size_t)
                                            {
                                                ASSERT_EQ(Err2, NetworkError::NONE);
                                                ClientDone = true;
                                            });
                      });

    // Read echo back
    auto EchoHeader = std::make_shared<ReadState>();
    EchoHeader->Socket = Client;
    EchoHeader->Buffer = std::make_shared<std::vector<char>>(4);
    EchoHeader->TargetSize = 4;
    EchoHeader->BytesRead = 0;

    std::atomic<bool> EchoDone = false;
    ReadExact(EchoHeader,
              [Client, &EchoDone, &Echoed, EchoHeader](NetworkError ErrH)
              {
                  ASSERT_EQ(ErrH, NetworkError::NONE);
                  uint32_t NetLen2 = 0;
                  std::memcpy(&NetLen2, EchoHeader->Buffer->data(), 4);
                  uint32_t BodyLen2 = ntohl(NetLen2);
                  auto EchoBody = std::make_shared<ReadState>();
                  EchoBody->Socket = Client;
                  EchoBody->Buffer = std::make_shared<std::vector<char>>(BodyLen2);
                  EchoBody->TargetSize = BodyLen2;
                  EchoBody->BytesRead = 0;
                  ReadExact(EchoBody,
                            [&EchoDone, &Echoed, EchoBody](NetworkError ErrB)
                            {
                                ASSERT_EQ(ErrB, NetworkError::NONE);
                                Echoed.assign(EchoBody->Buffer->data(),
                                              EchoBody->Buffer->data() + EchoBody->TargetSize);
                                EchoDone = true;
                            });
              });

    // Wait for completion
    for (int i = 0; i < 300 && !(Done.load() && ClientDone.load() && EchoDone.load()); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(Done.load());
    ASSERT_TRUE(ClientDone.load());
    ASSERT_TRUE(EchoDone.load());
    EXPECT_EQ(Echoed, Message);
}

TEST_F(LengthPrefixEchoTest, FragmentedMessage)
{
    static constexpr uint16_t TestPort = 12361;
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(ServerContext);
    NetworkAddress Addr("127.0.0.1", TestPort);
    ASSERT_EQ(Acceptor->Bind(Addr), NetworkError::NONE);

    std::atomic<bool> Done = false;
    std::string Message = "Fragmented_Payload_Message_Test";
    std::string Echoed;

    Acceptor->AsyncAccept(
        [this, &Done](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
        {
            ASSERT_EQ(Err, NetworkError::NONE);
            ASSERT_NE(ServerSocket, nullptr);
            AcceptedServerSocket = ServerSocket;

            auto HeaderState = std::make_shared<ReadState>();
            HeaderState->Socket = ServerSocket;
            HeaderState->Buffer = std::make_shared<std::vector<char>>(4);
            HeaderState->TargetSize = 4;
            HeaderState->BytesRead = 0;

            ReadExact(HeaderState,
                      [this, ServerSocket, HeaderState, &Done](NetworkError Err2)
                      {
                          ASSERT_EQ(Err2, NetworkError::NONE);
                          uint32_t NetLen = 0;
                          std::memcpy(&NetLen, HeaderState->Buffer->data(), 4);
                          uint32_t BodyLen = ntohl(NetLen);
                          auto BodyState = std::make_shared<ReadState>();
                          BodyState->Socket = ServerSocket;
                          BodyState->Buffer = std::make_shared<std::vector<char>>(BodyLen);
                          BodyState->TargetSize = BodyLen;
                          BodyState->BytesRead = 0;
                          ReadExact(
                              BodyState,
                              [ServerSocket, BodyState, &Done](NetworkError Err3)
                              {
                                  ASSERT_EQ(Err3, NetworkError::NONE);
                                  uint32_t NetLenOut =
                                      htonl(static_cast<uint32_t>(BodyState->TargetSize));
                                  char HeaderOut[4];
                                  std::memcpy(HeaderOut, &NetLenOut, 4);
                                  ServerSocket->AsyncSend(
                                      HeaderOut,
                                      4,
                                      NetworkAddress{},
                                      [ServerSocket, BodyState, &Done](NetworkError Err4, size_t)
                                      {
                                          ASSERT_EQ(Err4, NetworkError::NONE);
                                          ServerSocket->AsyncSend(BodyState->Buffer->data(),
                                                                  BodyState->TargetSize,
                                                                  NetworkAddress{},
                                                                  [&Done](NetworkError Err5, size_t)
                                                                  {
                                                                      ASSERT_EQ(Err5,
                                                                                NetworkError::NONE);
                                                                      Done = true;
                                                                  });
                                      });
                              });
                      });
        });

    // Client connect and send fragmented header/body
    auto Client = std::make_shared<AsyncTcpSocket>(ClientContext);

    // 使用异步连接
    std::atomic<bool> ConnectCompleted = false;
    NetworkError ConnectResult = NetworkError::NONE;

    Client->AsyncConnect(Addr,
                         [&ConnectCompleted, &ConnectResult](NetworkError Err)
                         {
                             ConnectResult = Err;
                             ConnectCompleted = true;
                         });

    // 等待连接完成
    for (int i = 0; i < 50 && !ConnectCompleted.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(ConnectCompleted.load());
    ASSERT_EQ(ConnectResult, NetworkError::NONE);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint32_t NetLen = htonl(static_cast<uint32_t>(Message.size()));
    char Header[4];
    std::memcpy(Header, &NetLen, 4);

    std::atomic<bool> ClientDone = false;
    // Send header in two fragments
    Client->AsyncSend(
        Header,
        2,
        NetworkAddress{},
        [Client, Header, &Message, &ClientDone](NetworkError Err, size_t)
        {
            ASSERT_EQ(Err, NetworkError::NONE);
            Client->AsyncSend(
                Header + 2,
                2,
                NetworkAddress{},
                [Client, &Message, &ClientDone](NetworkError Err2, size_t)
                {
                    ASSERT_EQ(Err2, NetworkError::NONE);
                    // Send body in two fragments as well
                    size_t Half = Message.size() / 2;
                    Client->AsyncSend(
                        Message.data(),
                        Half,
                        NetworkAddress{},
                        [Client, &Message, Half, &ClientDone](NetworkError Err3, size_t)
                        {
                            ASSERT_EQ(Err3, NetworkError::NONE);
                            Client->AsyncSend(Message.data() + Half,
                                              Message.size() - Half,
                                              NetworkAddress{},
                                              [&ClientDone](NetworkError Err4, size_t)
                                              {
                                                  ASSERT_EQ(Err4, NetworkError::NONE);
                                                  ClientDone = true;
                                              });
                        });
                });
        });

    // Read echo back
    auto EchoHeader = std::make_shared<ReadState>();
    EchoHeader->Socket = Client;
    EchoHeader->Buffer = std::make_shared<std::vector<char>>(4);
    EchoHeader->TargetSize = 4;
    EchoHeader->BytesRead = 0;

    std::atomic<bool> EchoDone = false;
    ReadExact(EchoHeader,
              [Client, &EchoDone, &Echoed, EchoHeader](NetworkError ErrH)
              {
                  ASSERT_EQ(ErrH, NetworkError::NONE);
                  uint32_t NetLen2 = 0;
                  std::memcpy(&NetLen2, EchoHeader->Buffer->data(), 4);
                  uint32_t BodyLen2 = ntohl(NetLen2);
                  auto EchoBody = std::make_shared<ReadState>();
                  EchoBody->Socket = Client;
                  EchoBody->Buffer = std::make_shared<std::vector<char>>(BodyLen2);
                  EchoBody->TargetSize = BodyLen2;
                  EchoBody->BytesRead = 0;
                  ReadExact(EchoBody,
                            [&EchoDone, &Echoed, EchoBody](NetworkError ErrB)
                            {
                                ASSERT_EQ(ErrB, NetworkError::NONE);
                                Echoed.assign(EchoBody->Buffer->data(),
                                              EchoBody->Buffer->data() + EchoBody->TargetSize);
                                EchoDone = true;
                            });
              });

    for (int i = 0; i < 500 && !(Done.load() && ClientDone.load() && EchoDone.load()); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(Done.load());
    ASSERT_TRUE(ClientDone.load());
    ASSERT_TRUE(EchoDone.load());
    EXPECT_EQ(Echoed, Message);
}
