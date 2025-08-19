#pragma once

#include "Shared/Network/Sockets/TcpSocket.h"
#include "Shared/Network/Asio/Reactor.h"
#include "Shared/Network/Asio/Proactor.h"
#include <functional>
#include <memory>

namespace Helianthus::Network::Asio
{
    class IoContext;

    class AsyncTcpSocket
    {
    public:
        using ReceiveHandler = std::function<void(Network::NetworkError, size_t)>;
        using SendHandler    = std::function<void(Network::NetworkError, size_t)>;

        explicit AsyncTcpSocket(std::shared_ptr<IoContext> Ctx);

        Network::NetworkError Connect(const Network::NetworkAddress& Address);
        void AsyncReceive(char* Buffer, size_t BufferSize, ReceiveHandler Handler);
        void AsyncSend(const char* Data, size_t Size, SendHandler Handler);
        void Close();

        Network::Sockets::TcpSocket& Native();

    private:
        std::shared_ptr<IoContext> Ctx;
        std::shared_ptr<Reactor> ReactorPtr;
        std::shared_ptr<Proactor> ProactorPtr;
        Network::Sockets::TcpSocket Socket;
        ReceiveHandler PendingRecv;
        bool ClosedFlag = false;
    };
}


