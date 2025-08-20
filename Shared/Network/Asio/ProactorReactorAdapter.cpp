#include "Shared/Network/Asio/ProactorReactorAdapter.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define ssize_t int
using NativeSocketType = SOCKET;
#else
#include <sys/socket.h>
#include <unistd.h>
using NativeSocketType = int;
#endif

namespace Helianthus::Network::Asio
{
    ProactorReactorAdapter::ProactorReactorAdapter(std::shared_ptr<Reactor> ReactorIn)
        : ReactorPtr(std::move(ReactorIn))
    {
    }

    void ProactorReactorAdapter::AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler)
    {
        if (!ReactorPtr) 
        {
            if (Handler) 
            {
                Handler(Network::NetworkError::NOT_INITIALIZED, 0);
            }
            return;
        }
        ReactorPtr->Add(Handle, EventMask::Read, [this, Handle, Buffer, BufferSize, Handler](EventMask Event){
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Read)) == 0) 
            {
                return;
            }
            ssize_t Received = ::recv(static_cast<NativeSocketType>(Handle), Buffer, static_cast<int>(BufferSize), 0);
            Network::NetworkError Err = (Received >= 0) ? Network::NetworkError::NONE : Network::NetworkError::RECEIVE_FAILED;
            if (Handler) 
            {
                Handler(Err, Received > 0 ? static_cast<size_t>(Received) : 0);
            }
            ReactorPtr->Del(Handle);
        });
    }

    void ProactorReactorAdapter::AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler)
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
        ReactorPtr->Add(Handle, EventMask::Write, [this, Handle, Data, Size, Handler](EventMask Event){
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Write)) == 0) 
            {
                return;
            }
            ssize_t Sent = ::send(static_cast<NativeSocketType>(Handle), Data, static_cast<int>(Size), 0);
            Network::NetworkError Err = (Sent >= 0) ? Network::NetworkError::NONE : Network::NetworkError::SEND_FAILED;
            if (Handler) 
            {
                Handler(Err, Sent > 0 ? static_cast<size_t>(Sent) : 0);
            }
            ReactorPtr->Del(Handle);
        });
    }
} // namespace Helianthus::Network::Asio


