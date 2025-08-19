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
        if (!ReactorPtr) 
        {
            if (PendingAccept) 
            {
                PendingAccept(Network::NetworkError::NOT_INITIALIZED);
            }
            return;
        }
        const int FdValue = static_cast<int>(Socket.GetNativeHandle());
        ReactorPtr->Add(FdValue, EventMask::Read, [this](EventMask Event){
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

    Network::Sockets::TcpSocket& AsyncTcpAcceptor::Native()
    {
        return Socket;
    }
}
