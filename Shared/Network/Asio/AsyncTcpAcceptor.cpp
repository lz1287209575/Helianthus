#include "Shared/Network/Asio/AsyncTcpAcceptor.h"

#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/Reactor.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace Helianthus::Network::Asio
{
AsyncTcpAcceptor::AsyncTcpAcceptor(std::shared_ptr<IoContext> CtxIn)
    : Ctx(std::move(CtxIn)),
      ReactorPtr(Ctx ? Ctx->GetReactor() : nullptr),
      ProactorPtr(Ctx ? Ctx->GetProactor() : nullptr),
      Socket(),
      PendingAccept()
{
}

Network::NetworkError AsyncTcpAcceptor::Bind(const Network::NetworkAddress& Address,
                                             uint32_t Backlog)
{
    auto Err = Socket.Bind(Address);
    if (Err != Network::NetworkError::NONE)
    {
        return Err;
    }
    return Socket.Listen(Backlog);
}

void AsyncTcpAcceptor::AsyncAccept(AcceptHandler Handler)
{
    PendingAccept = std::move(Handler);

    // 检查 Socket 是否已经绑定和监听
    Fd ListenFd;
    {
        std::lock_guard<std::mutex> lock(SocketMutex);
        // 暂时跳过 IsListening 检查，直接获取文件描述符
        ListenFd = static_cast<Fd>(Socket.GetNativeHandle());
    }

    // 纯 Reactor 路径，依赖事件驱动

    // 暂时跳过 Proactor 路径，直接使用 Reactor
    if (!ReactorPtr)
    {
        if (PendingAccept)
        {
            PendingAccept(Network::NetworkError::NOT_INITIALIZED, nullptr);
        }
        return;
    }

    // 如果已经注册，先移除
    if (IsRegistered)
    {
        ReactorPtr->Del(ListenFd);
        IsRegistered = false;
    }

    // 注册到 Reactor
    bool AddResult = ReactorPtr->Add(
        ListenFd,
        EventMask::Read,
        [this](EventMask Event)
        {
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Read)) == 0)
            {
                return;
            }

            // 接受连接
            sockaddr_in ClientAddr{};
            socklen_t Len = sizeof(ClientAddr);
            int ClientFd =
                ::accept(Socket.GetNativeHandle(), reinterpret_cast<sockaddr*>(&ClientAddr), &Len);

            if (ClientFd >= 0)
            {
                // 创建新的 AsyncTcpSocket 并采用接受的连接
                auto NewSocket = std::make_shared<AsyncTcpSocket>(Ctx);
                NetworkAddress ClientAddrObj(inet_ntoa(ClientAddr.sin_addr),
                                             ntohs(ClientAddr.sin_port));
                NewSocket->Native().Adopt(ClientFd, Socket.GetLocalAddress(), ClientAddrObj, true);

                // 调用回调
                if (PendingAccept)
                {
                    PendingAccept(Network::NetworkError::NONE, NewSocket);
                }
            }
            else
            {
                // 接受失败，调用回调
                if (PendingAccept)
                {
                    PendingAccept(Network::NetworkError::ACCEPT_FAILED, nullptr);
                }
            }

            // 保持注册，继续接受后续连接
            // 注意：不要移除注册，这样服务器可以持续接受连接
        });
    if (AddResult)
    {
        IsRegistered = true;
    }
}

void AsyncTcpAcceptor::AsyncAcceptEx(AcceptExHandler Handler)
{
#ifdef _WIN32
    if (ProactorPtr)
    {
        const auto ListenFd = static_cast<Fd>(Socket.GetNativeHandle());
        ProactorPtr->AsyncAccept(ListenFd,
                                 [Handler](Network::NetworkError Err, Fd Accepted)
                                 {
                                     if (Handler)
                                         Handler(Err, Accepted);
                                 });
        return;
    }
#endif
    // Fallback: use Reactor path and return only status; caller may call Accept() to retrieve
    // connection
    AsyncAccept(
        [Handler](Network::NetworkError Err, std::shared_ptr<AsyncTcpSocket> Socket)
        {
            if (Handler)
                Handler(Err, Socket ? Socket->Native().GetNativeHandle() : 0);
        });
}

Network::Sockets::TcpSocket& AsyncTcpAcceptor::Native()
{
    return Socket;
}
}  // namespace Helianthus::Network::Asio
