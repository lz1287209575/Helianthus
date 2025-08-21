#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/WinSockInit.h"

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

using namespace Helianthus::Network;
using namespace Helianthus::Network::Asio;

static void WriteUint32LE(uint32_t v, char out[4])
{
    out[0] = static_cast<char>(v & 0xFF);
    out[1] = static_cast<char>((v >> 8) & 0xFF);
    out[2] = static_cast<char>((v >> 16) & 0xFF);
    out[3] = static_cast<char>((v >> 24) & 0xFF);
}

TEST(TcpEchoAsyncTest, LengthPrefixedEcho)
{
    auto Ctx = std::make_shared<IoContext>();
    Helianthus::Network::EnsureWinSockInitialized();

    AsyncTcpAcceptor Acceptor(Ctx);
    ASSERT_EQ(Acceptor.Native().Bind({"127.0.0.1", 50002}), NetworkError::NONE);
    ASSERT_EQ(Acceptor.Native().Listen(16), NetworkError::NONE);
    auto Bound = Acceptor.Native().GetLocalAddress();
    ASSERT_GT(Bound.Port, 0);

    AsyncTcpSocket Client(Ctx);
    ASSERT_EQ(Client.Connect({"127.0.0.1", Bound.Port}), NetworkError::NONE);

    // Server accept and echo on callback（改为异步收发，避免阻塞/非阻塞差异）
    std::atomic<bool> Done{false};
    std::atomic<bool> ServerOk{true};
    std::string Payload = "HelloEcho_LengthPrefix";
    char Header[4];
    WriteUint32LE(static_cast<uint32_t>(Payload.size()), Header);

    Acceptor.AsyncAccept(
        [&](NetworkError Err, std::shared_ptr<AsyncTcpSocket> ServerSocket)
        {
            if (Err != NetworkError::NONE || !ServerSocket)
            {
                ServerOk = false;
                return;
            }
            // 读 4 字节长度
            auto HeaderBuf = std::make_shared<std::vector<char>>(4);
            ServerSocket->AsyncReceive(HeaderBuf->data(), 4,
                [&, HeaderBuf, ServerSocket](NetworkError e1, size_t b1)
                {
                    if (e1 != NetworkError::NONE || b1 != 4) { ServerOk = false; Done = true; return; }
                    uint32_t Len = static_cast<uint8_t>((*HeaderBuf)[0]) |
                                   (static_cast<uint8_t>((*HeaderBuf)[1]) << 8) |
                                   (static_cast<uint8_t>((*HeaderBuf)[2]) << 16) |
                                   (static_cast<uint8_t>((*HeaderBuf)[3]) << 24);
                    auto BodyBuf = std::make_shared<std::vector<char>>(Len);
                    ServerSocket->AsyncReceive(BodyBuf->data(), Len,
                        [&, BodyBuf, ServerSocket](NetworkError e2, size_t b2)
                        {
                            if (e2 != NetworkError::NONE || b2 != BodyBuf->size()) { ServerOk = false; Done = true; return; }
                            ServerSocket->AsyncSend(Header, 4,
                                [&, BodyBuf, ServerSocket](NetworkError e3, size_t)
                                {
                                    if (e3 != NetworkError::NONE) { ServerOk = false; Done = true; return; }
                                    ServerSocket->AsyncSend(BodyBuf->data(), BodyBuf->size(),
                                        [&, BodyBuf](NetworkError e4, size_t)
                                        {
                                            if (e4 != NetworkError::NONE) ServerOk = false;
                                            Done = true;
                                        });
                                });
                        });
                });
        });

    // Client send in fragments to simulate 半包/粘包
    std::thread ClientSendThread(
        [&]()
        {
            size_t s = 0;
            ASSERT_EQ(Client.Native().Send(Header, 2, s), NetworkError::NONE);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            s = 0;
            ASSERT_EQ(Client.Native().Send(Header + 2, 2, s), NetworkError::NONE);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            s = 0;
            ASSERT_EQ(Client.Native().Send(Payload.data(), 3, s), NetworkError::NONE);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            s = 0;
            ASSERT_EQ(
                Client.Native().Send(Payload.data() + 3, Payload.size() - 3, s),
                NetworkError::NONE);
        });

    // Client read echo fully (off the event loop thread)
    std::vector<char> Echo(Payload.size());
    std::thread ClientRecvThread(
        [&]()
        {
            size_t GotHdr = 0;
            char EH[4];
            while (GotHdr < 4)
            {
                size_t n = 0;
                auto e = Client.Native().Receive(EH + GotHdr, 4 - GotHdr, n);
                if (e != NetworkError::NONE)
                    return;
                GotHdr += n;
            }
            uint32_t Len = static_cast<uint8_t>(EH[0]) | (static_cast<uint8_t>(EH[1]) << 8) |
                           (static_cast<uint8_t>(EH[2]) << 16) |
                           (static_cast<uint8_t>(EH[3]) << 24);
            size_t RecvTot = 0;
            while (RecvTot < Len)
            {
                size_t n = 0;
                auto e = Client.Native().Receive(Echo.data() + RecvTot, Len - RecvTot, n);
                if (e != NetworkError::NONE)
                    return;
                RecvTot += n;
            }
            Ctx->Post([&]() { Ctx->Stop(); });
        });

    // Safety guard: force stop if not finished within 2 seconds
    Ctx->PostDelayed(
        [&]()
        {
            if (!Done.load())
            {
                Client.Close();
                Ctx->Stop();
            }
        },
        2000);

    Ctx->Run();

    if (ClientSendThread.joinable()) ClientSendThread.join();
    if (ClientRecvThread.joinable()) ClientRecvThread.join();
    ASSERT_TRUE(ServerOk);
    ASSERT_TRUE(Done.load());
    ASSERT_EQ(std::string(Echo.begin(), Echo.end()), Payload);
}

TEST(TcpEchoAsyncTest, TimeoutAndCancel)
{
    auto Ctx = std::make_shared<IoContext>();
    Helianthus::Network::EnsureWinSockInitialized();

    AsyncTcpAcceptor Acceptor(Ctx);
    ASSERT_EQ(Acceptor.Native().Bind({"127.0.0.1", 50003}), NetworkError::NONE);
    ASSERT_EQ(Acceptor.Native().Listen(16), NetworkError::NONE);
    auto Bound = Acceptor.Native().GetLocalAddress();

    AsyncTcpSocket Client(Ctx);
    ASSERT_EQ(Client.Connect({"127.0.0.1", Bound.Port}), NetworkError::NONE);

    // Accept and do nothing (no data)
    bool Accepted = false;
    Acceptor.AsyncAccept([&](NetworkError e, std::shared_ptr<AsyncTcpSocket> s) {
        (void)s;
        Accepted = (e == NetworkError::NONE);
    });

    // Set a pending read on client, then cancel after 20ms
    std::vector<char> BigBuf(4096);
    bool CanceledCalled = false;
    NetworkError CancelErr = NetworkError::NONE;
    Client.AsyncReceive(BigBuf.data(),
                        BigBuf.size(),
                        [&](NetworkError e, size_t)
                        {
                            CanceledCalled = true;
                            CancelErr = e;
                        });
    Ctx->PostDelayed(
        [&]()
        {
            auto p = Ctx->GetProactor();
            if (p)
                p->Cancel(static_cast<Fd>(Client.Native().GetNativeHandle()));
        },
        20);
    Ctx->PostDelayed([&]() { Ctx->Stop(); }, 100);
    Ctx->Run();

    ASSERT_TRUE(Accepted);
    ASSERT_TRUE(CanceledCalled);
    ASSERT_NE(CancelErr, NetworkError::NONE);
}
