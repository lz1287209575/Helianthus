#pragma once

#include "Shared/Network/Sockets/TcpSocket.h"
#include "Shared/Network/Asio/Reactor.h"
#include <functional>
#include <memory>

namespace Helianthus::Network::Asio
{
    class IoContext;

    class AsyncTcpAcceptor
    {
    public:
        using AcceptHandler = std::function<void(Network::NetworkError)>; // 简化：后续可返回新连接对象

        explicit AsyncTcpAcceptor(std::shared_ptr<IoContext> Ctx);

        Network::NetworkError Bind(const Network::NetworkAddress& Address, uint32_t Backlog = 128);
        void AsyncAccept(AcceptHandler Handler);

        Network::Sockets::TcpSocket& Native();

    private:
        std::shared_ptr<IoContext> Ctx;
        std::shared_ptr<Reactor> ReactorPtr;
        Network::Sockets::TcpSocket Socket;
        AcceptHandler PendingAccept;
    };
}


