#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/WinSockInit.h"

#include <atomic>
#include <thread>
#include <array>

#include <gtest/gtest.h>

using namespace Helianthus::Network;
using namespace Helianthus::Network::Asio;

namespace {
struct ReadState {
    std::shared_ptr<AsyncTcpSocket> Socket;
    char* Buf = nullptr;
    size_t Target = 0;
    size_t Read = 0;
};

static void ReadExact(std::shared_ptr<ReadState> st,
                      std::function<void(NetworkError)> onDone)
{
    const size_t remain = st->Target - st->Read;
    st->Socket->AsyncReceive(st->Buf + st->Read, remain,
        [st, onDone](NetworkError e, size_t n, NetworkAddress){
            if (e != NetworkError::NONE) { onDone(e); return; }
            st->Read += n;
            if (st->Read < st->Target) { ReadExact(st, onDone); }
            else { onDone(NetworkError::NONE); }
        });
}
}

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

    // 提前启动事件循环线程，确保异步事件不丢失
    std::thread LoopThread([&]() { Ctx->Run(); });

    AsyncTcpAcceptor Acceptor(Ctx);
    ASSERT_EQ(Acceptor.Native().Bind({"127.0.0.1", 50002}), NetworkError::NONE);
    ASSERT_EQ(Acceptor.Native().Listen(16), NetworkError::NONE);
    auto Bound = Acceptor.Native().GetLocalAddress();
    ASSERT_GT(Bound.Port, 0);

    AsyncTcpSocket Client(Ctx);

    // Server accept and echo on callback（全异步）
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
            // 读 4 字节长度（精确读）
            auto Hdr = std::make_shared<std::array<char,4>>();
            auto hs = std::make_shared<ReadState>();
            hs->Socket = ServerSocket;
            hs->Buf = Hdr->data();
            hs->Target = 4;
            hs->Read = 0;
            ReadExact(hs, [&, Hdr, ServerSocket](NetworkError e1){
                if (e1 != NetworkError::NONE) { ServerOk = false; Done = true; return; }
                uint32_t Len = static_cast<uint8_t>((*Hdr)[0]) |
                               (static_cast<uint8_t>((*Hdr)[1]) << 8) |
                               (static_cast<uint8_t>((*Hdr)[2]) << 16) |
                               (static_cast<uint8_t>((*Hdr)[3]) << 24);
                auto Body = std::make_shared<std::vector<char>>(Len);
                auto bs = std::make_shared<ReadState>();
                bs->Socket = ServerSocket;
                bs->Buf = Body->data();
                bs->Target = Len;
                bs->Read = 0;
                ReadExact(bs, [&, Body, ServerSocket](NetworkError e2){
                    if (e2 != NetworkError::NONE) { ServerOk = false; Done = true; return; }
                    ServerSocket->AsyncSend(Header, 4, NetworkAddress{},
                        [&, Body, ServerSocket](NetworkError e3, size_t){
                            if (e3 != NetworkError::NONE) { ServerOk = false; Done = true; Ctx->Post([&](){ Ctx->Stop(); }); return; }
                            ServerSocket->AsyncSend(Body->data(), Body->size(), NetworkAddress{},
                                [&, Body](NetworkError e4, size_t){ if (e4 != NetworkError::NONE) ServerOk = false; Done = true; Ctx->Post([&](){ Ctx->Stop(); }); });
                        });
                });
            });
        });

    // Client async send (header fragmented) + async receive echo back
    std::vector<char> Echo(Payload.size());
    std::atomic<bool> EchoDone{false};
    // 先发起连接，确保服务端已注册 accept
    ASSERT_EQ(Client.Connect({"127.0.0.1", Bound.Port}), NetworkError::NONE);
    // send header in two parts
    Client.AsyncSend(Header, 2, NetworkAddress{},
        [&](NetworkError e, size_t){
            ASSERT_EQ(e, NetworkError::NONE);
            Client.AsyncSend(Header + 2, 2, NetworkAddress{},
                [&](NetworkError e2, size_t){
                    ASSERT_EQ(e2, NetworkError::NONE);
                    // send body in two parts
                    Client.AsyncSend(Payload.data(), 3, NetworkAddress{},
                        [&](NetworkError e3, size_t){
                            ASSERT_EQ(e3, NetworkError::NONE);
                            Client.AsyncSend(Payload.data() + 3, Payload.size() - 3, NetworkAddress{},
                                [&](NetworkError e4, size_t){ ASSERT_EQ(e4, NetworkError::NONE); });
                        });
                });
        });
    // read echo header then body
    auto CEHdr = std::make_shared<std::array<char,4>>();
    auto chs = std::make_shared<ReadState>();
    chs->Socket = std::shared_ptr<AsyncTcpSocket>(&Client, [](AsyncTcpSocket*){}); // 非拥有包装
    chs->Buf = CEHdr->data();
    chs->Target = 4;
    chs->Read = 0;
    ReadExact(chs, [&](NetworkError er){
        if (er != NetworkError::NONE) return;
        uint32_t Len = static_cast<uint8_t>((*CEHdr)[0]) | (static_cast<uint8_t>((*CEHdr)[1]) << 8) |
                       (static_cast<uint8_t>((*CEHdr)[2]) << 16) | (static_cast<uint8_t>((*CEHdr)[3]) << 24);
        auto cbs = std::make_shared<ReadState>();
        cbs->Socket = std::shared_ptr<AsyncTcpSocket>(&Client, [](AsyncTcpSocket*){}); // 非拥有包装
        cbs->Buf = Echo.data();
        cbs->Target = Len;
        cbs->Read = 0;
        ReadExact(cbs, [&](NetworkError er2){ if (er2 == NetworkError::NONE) { EchoDone = true; Ctx->Post([&](){ Ctx->Stop(); }); } });
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

    // 等待事件循环退出
    if (LoopThread.joinable()) LoopThread.join();

    ASSERT_TRUE(ServerOk);
    ASSERT_TRUE(Done.load());
    ASSERT_TRUE(EchoDone.load());
    ASSERT_EQ(std::string(Echo.begin(), Echo.end()), Payload);
}

TEST(TcpEchoAsyncTest, TimeoutAndCancel)
{
    auto Ctx = std::make_shared<IoContext>();
    Helianthus::Network::EnsureWinSockInitialized();

    // 提前启动事件循环线程
    std::thread LoopThread([&]() { Ctx->Run(); });

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
                        [&](NetworkError e, size_t, NetworkAddress)
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
    if (LoopThread.joinable()) LoopThread.join();

    ASSERT_TRUE(Accepted);
    ASSERT_TRUE(CanceledCalled);
    ASSERT_NE(CancelErr, NetworkError::NONE);
}
