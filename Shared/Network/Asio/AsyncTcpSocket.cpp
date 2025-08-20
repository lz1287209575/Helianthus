#include "Shared/Network/Asio/AsyncTcpSocket.h"

#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"

namespace Helianthus::Network::Asio
{
AsyncTcpSocket::AsyncTcpSocket(std::shared_ptr<IoContext> CtxIn)
    : Ctx(std::move(CtxIn)),
      ReactorPtr(Ctx ? Ctx->GetReactor() : nullptr),
      ProactorPtr(Ctx ? Ctx->GetProactor() : nullptr),
      Socket(),
      PendingRecv()
{
}

Network::NetworkError AsyncTcpSocket::Connect(const Network::NetworkAddress& Address)
{
    return Socket.Connect(Address);
}

void AsyncTcpSocket::AsyncReceive(char* Buffer, size_t BufferSize, ReceiveHandler Handler)
{
    PendingRecv = std::move(Handler);
    const auto FdValue = static_cast<Fd>(Socket.GetNativeHandle());
    if (ProactorPtr)
    {
        ProactorPtr->AsyncRead(FdValue,
                               Buffer,
                               BufferSize,
                               [this](Network::NetworkError Err, size_t Bytes)
                               {
                                   auto Callback = PendingRecv;
                                   PendingRecv = nullptr;
                                   if (Callback)
                                   {
                                       Callback(Err, Bytes);
                                   }
                               });
        return;
    }
    if (!ReactorPtr)
    {
        if (PendingRecv)
        {
            PendingRecv(Network::NetworkError::NOT_INITIALIZED, 0);
        }
        return;
    }
    ReactorPtr->Add(
        FdValue,
        EventMask::Read,
        [this, Buffer, BufferSize](EventMask Event)
        {
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Read)) == 0)
            {
                return;
            }
            size_t Received = 0;
            Network::NetworkError Err = Socket.Receive(Buffer, BufferSize, Received);
            auto Callback = PendingRecv;
            PendingRecv = nullptr;
            if (Callback)
            {
                Callback(Err, Received);
            }
        });
}

void AsyncTcpSocket::AsyncSend(const char* Data, size_t Size, SendHandler Handler)
{
    const auto FdValue = static_cast<Fd>(Socket.GetNativeHandle());
    if (ProactorPtr)
    {
        ProactorPtr->AsyncWrite(FdValue,
                                Data,
                                Size,
                                [Handler](Network::NetworkError Err, size_t Bytes)
                                {
                                    if (Handler)
                                    {
                                        Handler(Err, Bytes);
                                    }
                                });
        return;
    }
    size_t Sent = 0;
    Network::NetworkError Err = Socket.Send(Data, Size, Sent);
    if (Handler)
        Handler(Err, Sent);
}

Network::Sockets::TcpSocket& AsyncTcpSocket::Native()
{
    return Socket;
}

void AsyncTcpSocket::Close()
{
    if (ClosedFlag)
    {
        return;
    }
    ClosedFlag = true;
    const auto Handle = static_cast<Fd>(Socket.GetNativeHandle());
    if (ProactorPtr)
    {
        ProactorPtr->Cancel(Handle);
    }
    Socket.Disconnect();
}
}  // namespace Helianthus::Network::Asio
