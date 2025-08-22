#pragma once

#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/Reactor.h"
#include "Shared/Network/Asio/IAsyncSocket.h"
#include "Shared/Network/Sockets/TcpSocket.h"

#include <functional>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>

namespace Helianthus::Network::Asio
{
class IoContext;

class AsyncTcpSocket : public IAsyncSocket
{
public:
    // 兼容性回调类型（保持向后兼容）
    using ReceiveHandler = std::function<void(Network::NetworkError, size_t)>;
    using SendHandler = std::function<void(Network::NetworkError, size_t)>;

    explicit AsyncTcpSocket(std::shared_ptr<IoContext> Ctx);

    // 实现统一接口
    Network::NetworkError Connect(const Network::NetworkAddress& Address) override;
    Network::NetworkError Bind(const Network::NetworkAddress& Address) override;
    void Close() override;
    
    // 统一的异步操作接口
    void AsyncReceive(char* Buffer, size_t BufferSize, AsyncReceiveHandler Handler,
                     CancelToken Token = nullptr, uint32_t TimeoutMs = 0) override;
    void AsyncSend(const char* Data, size_t Size, const Network::NetworkAddress& Address,
                  AsyncSendHandler Handler, CancelToken Token = nullptr, uint32_t TimeoutMs = 0) override;
    void AsyncConnect(const Network::NetworkAddress& Address, AsyncConnectHandler Handler,
                     CancelToken Token = nullptr, uint32_t TimeoutMs = 0) override;
    
    // 统一的取消和超时支持
    void CancelOperation(CancelToken Token) override;
    void SetDefaultTimeout(uint32_t TimeoutMs) override;
    uint32_t GetDefaultTimeout() const override;
    
    // 状态查询
    bool IsConnected() const override;
    bool IsClosed() const override;
    Network::NetworkAddress GetLocalAddress() const override;
    Network::NetworkAddress GetRemoteAddress() const override;

    // 兼容性接口（保持向后兼容）
    void AsyncConnectLegacy(const Network::NetworkAddress& Address, std::function<void(Network::NetworkError)> Handler);
    void AsyncReceiveLegacy(char* Buffer, size_t BufferSize, ReceiveHandler Handler);
    void AsyncSendLegacy(const char* Data, size_t Size, SendHandler Handler);

    Network::Sockets::TcpSocket& Native();

private:
    std::shared_ptr<IoContext> Ctx;
    std::shared_ptr<Reactor> ReactorPtr;
    std::shared_ptr<Proactor> ProactorPtr;
    Network::Sockets::TcpSocket Socket;
    
    // 兼容性回调
    ReceiveHandler PendingRecv;
    char* PendingRecvBuf = nullptr;
    size_t PendingRecvSize = 0;
    const char* PendingSendPtr = nullptr;
    size_t PendingSendRemaining = 0;
    size_t PendingSendTotalSent = 0;
    SendHandler PendingSendHandler;
    bool SendRegistered = false;
    bool ClosedFlag = false;
    bool IsRegistered = false;
    
    // 统一接口支持
    uint32_t DefaultTimeoutMs = 30000; // 默认30秒超时
    std::unordered_map<CancelToken, bool> ActiveOperations;
    mutable std::mutex OperationsMutex;
};
}  // namespace Helianthus::Network::Asio
