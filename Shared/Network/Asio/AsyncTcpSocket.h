#pragma once

#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/Reactor.h"
#include "Shared/Network/Sockets/TcpSocket.h"

#include <functional>
#include <memory>

namespace Helianthus::Network::Asio
{
class IoContext;

class AsyncTcpSocket
{
public:
    using ReceiveHandler = std::function<void(Network::NetworkError, size_t)>;
    using SendHandler = std::function<void(Network::NetworkError, size_t)>;

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
    char* PendingRecvBuf = nullptr;
    size_t PendingRecvSize = 0;
    // 简易发送状态（单次发送序列）
    const char* PendingSendPtr = nullptr;
    size_t PendingSendRemaining = 0;
    size_t PendingSendTotalSent = 0;
    SendHandler PendingSendHandler;
    bool SendRegistered = false;
    bool ClosedFlag = false;
    bool IsRegistered = false;  // 是否已注册到 Reactor
};
}  // namespace Helianthus::Network::Asio
