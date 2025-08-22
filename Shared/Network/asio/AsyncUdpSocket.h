#pragma once

#include "Shared/Network/Sockets/UdpSocket.h"
#include "Shared/Network/Asio/Reactor.h"
#include <functional>
#include <memory>

namespace Helianthus::Network::Asio
{
    class IoContext;

    class AsyncUdpSocket
    {
    public:
        using ReceiveHandler = std::function<void(Network::NetworkError, size_t)>;
        using SendHandler    = std::function<void(Network::NetworkError, size_t)>;

        explicit AsyncUdpSocket(std::shared_ptr<IoContext> Ctx);

        Network::NetworkError Bind(const Network::NetworkAddress& Address);
        void AsyncReceive(char* Buffer, size_t BufferSize, ReceiveHandler Handler);
        void AsyncSendTo(const char* Data, size_t Size, const Network::NetworkAddress& Address, SendHandler Handler);

        Network::Sockets::UdpSocket& Native();

    private:
        std::shared_ptr<IoContext> Ctx;
        std::shared_ptr<Reactor> ReactorPtr;
        Network::Sockets::UdpSocket Socket;
        ReceiveHandler PendingRecv;
        SendHandler PendingSend;
    };
}


