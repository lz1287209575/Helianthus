#include "Shared/Network/Asio/ProactorReactorAdapter.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define ssize_t int
using NativeSocketType = SOCKET;
#else
    #include <sys/socket.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
using NativeSocketType = int;
#endif

namespace Helianthus::Network::Asio
{
ProactorReactorAdapter::ProactorReactorAdapter(std::shared_ptr<Reactor> ReactorIn)
    : ReactorPtr(std::move(ReactorIn))
{
}

void ProactorReactorAdapter::AsyncRead(Fd Handle,
                                       char* Buffer,
                                       size_t BufferSize,
                                       CompletionHandler Handler)
{
    if (!ReactorPtr)
    {
        if (Handler)
        {
            Handler(Network::NetworkError::NOT_INITIALIZED, 0);
        }
        return;
    }
    ReactorPtr->Add(
        Handle,
        EventMask::Read,
        [this, Handle, Buffer, BufferSize, Handler](EventMask Event)
        {
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Read)) == 0)
            {
                return;
            }
            ssize_t Received = ::recv(
                static_cast<NativeSocketType>(Handle), Buffer, static_cast<int>(BufferSize), 0);
            Network::NetworkError Err = (Received >= 0) ? Network::NetworkError::NONE
                                                        : Network::NetworkError::RECEIVE_FAILED;
            if (Handler)
            {
                Handler(Err, Received > 0 ? static_cast<size_t>(Received) : 0);
            }
            ReactorPtr->Del(Handle);
        });
}

void ProactorReactorAdapter::AsyncWrite(Fd Handle,
                                        const char* Data,
                                        size_t Size,
                                        CompletionHandler Handler)
{
    if (!ReactorPtr)
    {
        if (Handler)
        {
            Handler(Network::NetworkError::NOT_INITIALIZED, 0);
        }
        return;
    }
    // 简化：直接在可写时一次性写入
    ReactorPtr->Add(
        Handle,
        EventMask::Write,
        [this, Handle, Data, Size, Handler](EventMask Event)
        {
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Write)) == 0)
            {
                return;
            }
            ssize_t Sent =
                ::send(static_cast<NativeSocketType>(Handle), Data, static_cast<int>(Size), 0);
            Network::NetworkError Err =
                (Sent >= 0) ? Network::NetworkError::NONE : Network::NetworkError::SEND_FAILED;
            if (Handler)
            {
                Handler(Err, Sent > 0 ? static_cast<size_t>(Sent) : 0);
            }
            ReactorPtr->Del(Handle);
        });
}

void ProactorReactorAdapter::AsyncReceiveFrom(Fd Handle,
                                              char* Buffer,
                                              size_t BufferSize,
                                              UdpReceiveHandler Handler)
{
    if (!ReactorPtr)
    {
        if (Handler)
        {
            Handler(Network::NetworkError::NOT_INITIALIZED, 0, Network::NetworkAddress{});
        }
        return;
    }
    
    ReactorPtr->Add(
        Handle,
        EventMask::Read,
        [this, Handle, Buffer, BufferSize, Handler](EventMask Event)
        {
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Read)) == 0)
            {
                return;
            }
            
            // 使用 recvfrom 获取发送方地址
            sockaddr_in SockAddr{};
            socklen_t SockAddrLen = sizeof(SockAddr);
            
            ssize_t Received = ::recvfrom(
                static_cast<NativeSocketType>(Handle), 
                Buffer, 
                static_cast<int>(BufferSize), 
                0,
                reinterpret_cast<sockaddr*>(&SockAddr),
                &SockAddrLen);
                
            Network::NetworkError Err = (Received >= 0) ? Network::NetworkError::NONE
                                                        : Network::NetworkError::RECEIVE_FAILED;
            
            Network::NetworkAddress FromAddress;
            if (Received > 0 && SockAddrLen >= sizeof(sockaddr_in))
            {
                char IpStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &SockAddr.sin_addr, IpStr, INET_ADDRSTRLEN);
                FromAddress = Network::NetworkAddress(IpStr, ntohs(SockAddr.sin_port));
            }
            
            if (Handler)
            {
                Handler(Err, Received > 0 ? static_cast<size_t>(Received) : 0, FromAddress);
            }
            ReactorPtr->Del(Handle);
        });
}

void ProactorReactorAdapter::AsyncSendTo(Fd Handle,
                                         const char* Data,
                                         size_t Size,
                                         const Network::NetworkAddress& Address,
                                         UdpSendHandler Handler)
{
    if (!ReactorPtr)
    {
        if (Handler)
        {
            Handler(Network::NetworkError::NOT_INITIALIZED, 0);
        }
        return;
    }
    
    // 准备目标地址
    sockaddr_in SockAddr{};
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(Address.Port);
    inet_pton(AF_INET, Address.Ip.c_str(), &SockAddr.sin_addr);
    
    ReactorPtr->Add(
        Handle,
        EventMask::Write,
        [this, Handle, Data, Size, SockAddr, Handler](EventMask Event)
        {
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Write)) == 0)
            {
                return;
            }
            
            ssize_t Sent = ::sendto(
                static_cast<NativeSocketType>(Handle), 
                Data, 
                static_cast<int>(Size), 
                0,
                reinterpret_cast<const sockaddr*>(&SockAddr),
                sizeof(SockAddr));
                
            Network::NetworkError Err =
                (Sent >= 0) ? Network::NetworkError::NONE : Network::NetworkError::SEND_FAILED;
            if (Handler)
            {
                Handler(Err, Sent > 0 ? static_cast<size_t>(Sent) : 0);
            }
            ReactorPtr->Del(Handle);
        });
}
}  // namespace Helianthus::Network::Asio
