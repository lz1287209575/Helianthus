#include "Shared/Network/Asio/AsyncTcpAcceptor.h"

#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/Reactor.h"

#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
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
    std::cout << "AsyncAccept called" << std::endl;
    std::cout << "Moving handler..." << std::endl;
    PendingAccept = std::move(Handler);
    std::cout << "Handler moved successfully" << std::endl;
    
    std::cout << "About to check socket..." << std::endl;
    
    // 检查 Socket 是否已经绑定和监听
    Fd ListenFd;
    {
        std::cout << "Acquiring socket mutex..." << std::endl;
        std::lock_guard<std::mutex> lock(SocketMutex);
        std::cout << "Socket mutex acquired" << std::endl;
        std::cout << "About to call Socket.IsListening()..." << std::endl;
        // 暂时跳过 IsListening 检查，直接获取文件描述符
        ListenFd = static_cast<Fd>(Socket.GetNativeHandle());
        std::cout << "Got ListenFd: " << ListenFd << std::endl;
    }
    
    std::cout << "About to check ProactorPtr..." << std::endl;
    // 暂时跳过 Proactor 路径，直接使用 Reactor
    std::cout << "Skipping Proactor path, using Reactor" << std::endl;
    
    std::cout << "About to check ReactorPtr..." << std::endl;
    if (!ReactorPtr)
    {
        std::cout << "ReactorPtr is null, returning" << std::endl;
        if (PendingAccept)
        {
            PendingAccept(Network::NetworkError::NOT_INITIALIZED, nullptr);
        }
        return;
    }
    std::cout << "ReactorPtr is not null" << std::endl;
    
    // 如果已经注册，先移除
    if (IsRegistered)
    {
        ReactorPtr->Del(ListenFd);
        IsRegistered = false;
    }
    
    // 注册到 Reactor
    std::cout << "Attempting to register fd " << ListenFd << " to Reactor" << std::endl;
    std::cout << "ReactorPtr is null: " << (ReactorPtr == nullptr) << std::endl;
    std::cout << "Socket native handle: " << Socket.GetNativeHandle() << std::endl;
    std::cout << "Socket is listening: " << Socket.IsListening() << std::endl;
    bool AddResult = ReactorPtr->Add(ListenFd, EventMask::Read,
        [this](EventMask Event)
        {
            std::cout << "Reactor event received: " << static_cast<int>(Event) << std::endl;
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Read)) == 0)
            {
                return;
            }
            
            // 接受连接
            sockaddr_in ClientAddr{};
            socklen_t Len = sizeof(ClientAddr);
            int ClientFd = ::accept(Socket.GetNativeHandle(), reinterpret_cast<sockaddr*>(&ClientAddr), &Len);
            
            std::cout << "Accept result: " << ClientFd << std::endl;
            
            auto Callback = PendingAccept;
            PendingAccept = nullptr;
            
            // 移除注册
            const auto ListenFd = static_cast<Fd>(Socket.GetNativeHandle());
            if (ReactorPtr && IsRegistered)
            {
                ReactorPtr->Del(ListenFd);
                IsRegistered = false;
            }
            
            if (Callback)
            {
                if (ClientFd >= 0)
                {
                    // 创建新的 AsyncTcpSocket 并采用接受的连接
                    auto NewSocket = std::make_shared<AsyncTcpSocket>(Ctx);
                    NetworkAddress ClientAddrObj(inet_ntoa(ClientAddr.sin_addr), ntohs(ClientAddr.sin_port));
                    NewSocket->Native().Adopt(ClientFd, Socket.GetLocalAddress(), ClientAddrObj, true);
                    Callback(Network::NetworkError::NONE, NewSocket);
                }
                else
                {
                    Callback(Network::NetworkError::ACCEPT_FAILED, nullptr);
                }
            }
        });
    
    std::cout << "Reactor Add result: " << AddResult << std::endl;
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
