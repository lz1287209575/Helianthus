#include <gtest/gtest.h>
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Sockets/UdpSocket.h"

using namespace Helianthus::Network;
using namespace Helianthus::Network::Asio;

TEST(NetworkEchoTest, BasicSendReceive)
{
    auto Ctx = std::make_shared<IoContext>();

    // 使用 UDP 进行最小可运行回环测试
    Sockets::UdpSocket Server;
    Sockets::UdpSocket Client;
    NetworkAddress Addr{"127.0.0.1", 50001};
    ASSERT_EQ(Server.Bind(Addr), NetworkError::SUCCESS);
    auto Bound = Server.GetLocalAddress();
    ASSERT_GT(Bound.Port, 0);

    const char* Msg = "hello";
    ASSERT_EQ(Client.Connect({"127.0.0.1", Bound.Port}), NetworkError::SUCCESS);

    size_t BytesSent = 0;
    ASSERT_EQ(Client.Send(Msg, 5, BytesSent), NetworkError::SUCCESS);
    ASSERT_EQ(BytesSent, 5u);

    char Buffer[16] = {0}; 
    size_t BytesRecv = 0;
    ASSERT_EQ(Server.Receive(Buffer, sizeof(Buffer), BytesRecv), NetworkError::SUCCESS);
    ASSERT_EQ(BytesRecv, 5u);
    Buffer[5] = '\0';
    ASSERT_STREQ(Buffer, "hello");
}


