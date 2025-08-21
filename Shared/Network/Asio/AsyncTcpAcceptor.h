#pragma once

#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/Reactor.h"
#include "Shared/Network/Sockets/TcpSocket.h"

#include <functional>
#include <memory>

namespace Helianthus::Network::Asio
{
class IoContext;
class AsyncTcpSocket;  // 前向声明

class AsyncTcpAcceptor
{
public:
    using AcceptHandler = std::function<void(Network::NetworkError, std::shared_ptr<AsyncTcpSocket>)>;  // 返回新连接对象
    using AcceptExHandler = std::function<void(Network::NetworkError, Fd)>;

    explicit AsyncTcpAcceptor(std::shared_ptr<IoContext> Ctx);

    Network::NetworkError Bind(const Network::NetworkAddress& Address, uint32_t Backlog = 128);
    void AsyncAccept(AcceptHandler Handler);
    void AsyncAcceptEx(AcceptExHandler Handler);

    Network::Sockets::TcpSocket& Native();

private:
    std::shared_ptr<IoContext> Ctx;
    std::shared_ptr<Reactor> ReactorPtr;
    std::shared_ptr<Proactor> ProactorPtr;
    Network::Sockets::TcpSocket Socket;
    AcceptHandler PendingAccept;
    bool IsRegistered = false;  // 是否已注册到 Reactor
    mutable std::mutex SocketMutex;  // 保护 Socket 的访问
};
}  // namespace Helianthus::Network::Asio
