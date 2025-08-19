#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"

namespace Helianthus::Network::Asio
{
    AsyncTcpSocket::AsyncTcpSocket(std::shared_ptr<IoContext> CtxIn)
        : Ctx(std::move(CtxIn))
        , ReactorPtr(Ctx ? Ctx->GetReactor() : nullptr)
        , ProactorPtr(Ctx ? Ctx->GetProactor() : nullptr)
        , Socket()
        , PendingRecv()
    {
    }

    Network::NetworkError AsyncTcpSocket::Connect(const Network::NetworkAddress& Address)
    {
        return Socket.Connect(Address);
    }

    void AsyncTcpSocket::AsyncReceive(char* Buffer, size_t BufferSize, ReceiveHandler Handler)
    {
        PendingRecv = std::move(Handler);
        const auto fd = static_cast<Fd>(Socket.GetNativeHandle());
        if (ProactorPtr)
        {
            ProactorPtr->AsyncRead(fd, Buffer, BufferSize, [this](Network::NetworkError err, size_t n){
                auto cb = PendingRecv; PendingRecv = nullptr; if (cb) cb(err, n);
            });
            return;
        }
        if (!ReactorPtr) { if (PendingRecv) PendingRecv(Network::NetworkError::NOT_INITIALIZED, 0); return; }
        ReactorPtr->Add(fd, EventMask::Read, [this, Buffer, BufferSize](EventMask ev){
            if ((static_cast<uint32_t>(ev) & static_cast<uint32_t>(EventMask::Read)) == 0) return;
            size_t received = 0; Network::NetworkError err = Socket.Receive(Buffer, BufferSize, received);
            auto cb = PendingRecv; PendingRecv = nullptr;
            if (cb) cb(err, received);
        });
    }

    void AsyncTcpSocket::AsyncSend(const char* Data, size_t Size, SendHandler Handler)
    {
        const auto fd = static_cast<Fd>(Socket.GetNativeHandle());
        if (ProactorPtr)
        {
            ProactorPtr->AsyncWrite(fd, Data, Size, [Handler](Network::NetworkError err, size_t n){ if (Handler) Handler(err, n); });
            return;
        }
        size_t sent = 0; Network::NetworkError err = Socket.Send(Data, Size, sent);
        if (Handler) Handler(err, sent);
    }

    Network::Sockets::TcpSocket& AsyncTcpSocket::Native()
    {
        return Socket;
    }
}


