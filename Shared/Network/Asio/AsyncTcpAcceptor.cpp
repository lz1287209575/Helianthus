#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"

namespace Helianthus::Network::Asio
{
    AsyncTcpAcceptor::AsyncTcpAcceptor(std::shared_ptr<IoContext> CtxIn)
        : Ctx(std::move(CtxIn))
        , ReactorPtr(Ctx ? Ctx->GetReactor() : nullptr)
        , Socket()
        , PendingAccept()
    {
    }

    Network::NetworkError AsyncTcpAcceptor::Bind(const Network::NetworkAddress& Address, uint32_t Backlog)
    {
        auto err = Socket.Bind(Address);
        if (err != Network::NetworkError::NONE) return err;
        return Socket.Listen(Backlog);
    }

    void AsyncTcpAcceptor::AsyncAccept(AcceptHandler Handler)
    {
        PendingAccept = std::move(Handler);
        if (!ReactorPtr) { if (PendingAccept) PendingAccept(Network::NetworkError::NOT_INITIALIZED); return; }
        const int fd = static_cast<int>(Socket.GetNativeHandle());
        ReactorPtr->Add(fd, EventMask::Read, [this](EventMask ev){
            if ((static_cast<uint32_t>(ev) & static_cast<uint32_t>(EventMask::Read)) == 0) return;
            auto res = Socket.Accept();
            auto cb = PendingAccept; PendingAccept = nullptr;
            if (cb) cb(res);
        });
    }

    Network::Sockets::TcpSocket& AsyncTcpAcceptor::Native()
    {
        return Socket;
    }
}


