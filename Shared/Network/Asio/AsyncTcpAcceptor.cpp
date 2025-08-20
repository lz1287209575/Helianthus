#include "Shared/Network/Asio/AsyncTcpAcceptor.h"

#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/Reactor.h"

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
    const auto ListenFd = static_cast<Fd>(Socket.GetNativeHandle());
    if (ProactorPtr)
    {
        ProactorPtr->AsyncAccept(ListenFd,
                                 [this](Network::NetworkError Err, Fd Accepted)
                                 {
                                     auto Callback = PendingAccept;
                                     PendingAccept = nullptr;
                                     if (Callback)
                                         Callback(Err);
                                 });
        return;
    }
    if (!ReactorPtr)
    {
        if (PendingAccept)
        {
            PendingAccept(Network::NetworkError::NOT_INITIALIZED);
        }
        return;
    }
    ReactorPtr->Add(
        static_cast<int>(ListenFd),
        EventMask::Read,
        [this](EventMask Event)
        {
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Read)) == 0)
            {
                return;
            }
            auto Result = Socket.Accept();
            auto Callback = PendingAccept;
            PendingAccept = nullptr;
            if (Callback)
            {
                Callback(Result);
            }
        });
}

void AsyncTcpAcceptor::AsyncAcceptEx(AcceptExHandler Handler)
{
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
    // Fallback: use Reactor path and return only status; caller may call Accept() to retrieve
    // connection
    AsyncAccept(
        [Handler](Network::NetworkError Err)
        {
            if (Handler)
                Handler(Err, 0);
        });
}

Network::Sockets::TcpSocket& AsyncTcpAcceptor::Native()
{
    return Socket;
}
}  // namespace Helianthus::Network::Asio
