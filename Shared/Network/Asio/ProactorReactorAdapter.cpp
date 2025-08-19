#include "Shared/Network/Asio/ProactorReactorAdapter.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace Helianthus::Network::Asio
{
    ProactorReactorAdapter::ProactorReactorAdapter(std::shared_ptr<Reactor> RIn)
        : R(std::move(RIn))
    {
    }

    void ProactorReactorAdapter::AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler)
    {
        if (!R) { if (Handler) Handler(Network::NetworkError::NOT_INITIALIZED, 0); return; }
        R->Add(Handle, EventMask::Read, [this, Handle, Buffer, BufferSize, Handler](EventMask ev){
            if ((static_cast<uint32_t>(ev) & static_cast<uint32_t>(EventMask::Read)) == 0) return;
            ssize_t n = ::recv(Handle, Buffer, BufferSize, 0);
            Network::NetworkError err = (n >= 0) ? Network::NetworkError::NONE : Network::NetworkError::RECEIVE_FAILED;
            if (Handler) Handler(err, n > 0 ? static_cast<size_t>(n) : 0);
            R->Del(Handle);
        });
    }

    void ProactorReactorAdapter::AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler)
    {
        if (!R) { if (Handler) Handler(Network::NetworkError::NOT_INITIALIZED, 0); return; }
        // 简化：直接在可写时一次性写入
        R->Add(Handle, EventMask::Write, [this, Handle, Data, Size, Handler](EventMask ev){
            if ((static_cast<uint32_t>(ev) & static_cast<uint32_t>(EventMask::Write)) == 0) return;
            ssize_t n = ::send(Handle, Data, Size, 0);
            Network::NetworkError err = (n >= 0) ? Network::NetworkError::NONE : Network::NetworkError::SEND_FAILED;
            if (Handler) Handler(err, n > 0 ? static_cast<size_t>(n) : 0);
            R->Del(Handle);
        });
    }
}


