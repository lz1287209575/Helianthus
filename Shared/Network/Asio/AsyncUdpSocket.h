#pragma once

#include "Shared/Network/Asio/IAsyncSocket.h"
#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/Reactor.h"
#include "Shared/Network/Sockets/UdpSocket.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Helianthus::Network::Asio
{
class IoContext;

class AsyncUdpSocket : public IAsyncSocket
{
public:
    // 兼容性回调类型（保持向后兼容）
    using ReceiveHandler = std::function<void(Network::NetworkError, size_t)>;
    using SendHandler = std::function<void(Network::NetworkError, size_t)>;
    using UdpReceiveHandler =
        std::function<void(Network::NetworkError, size_t, Network::NetworkAddress)>;
    using UdpSendHandler = std::function<void(Network::NetworkError, size_t)>;

    explicit AsyncUdpSocket(std::shared_ptr<IoContext> Ctx);

    // 实现统一接口
    Network::NetworkError Connect(const Network::NetworkAddress& Address) override;
    Network::NetworkError Bind(const Network::NetworkAddress& Address) override;
    void Close() override;

    // 统一的异步操作接口
    void AsyncReceive(char* Buffer,
                      size_t BufferSize,
                      AsyncReceiveHandler Handler,
                      CancelToken Token = nullptr,
                      uint32_t TimeoutMs = 0) override;
    void AsyncSend(const char* Data,
                   size_t Size,
                   const Network::NetworkAddress& Address,
                   AsyncSendHandler Handler,
                   CancelToken Token = nullptr,
                   uint32_t TimeoutMs = 0) override;
    void AsyncConnect(const Network::NetworkAddress& Address,
                      AsyncConnectHandler Handler,
                      CancelToken Token = nullptr,
                      uint32_t TimeoutMs = 0) override;

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
    void AsyncReceive(char* Buffer, size_t BufferSize, ReceiveHandler Handler);
    void AsyncSendTo(const char* Data,
                     size_t Size,
                     const Network::NetworkAddress& Address,
                     SendHandler Handler);
    void AsyncReceiveFrom(char* Buffer, size_t BufferSize, UdpReceiveHandler Handler);
    void AsyncSendToProactor(const char* Data,
                             size_t Size,
                             const Network::NetworkAddress& Address,
                             UdpSendHandler Handler);

    Network::Sockets::UdpSocket& Native();

private:
    std::shared_ptr<IoContext> Ctx;
    std::shared_ptr<Reactor> ReactorPtr;
    std::shared_ptr<Proactor> ProactorPtr;
    Network::Sockets::UdpSocket Socket;

    // 兼容性回调
    ReceiveHandler PendingRecv;
    SendHandler PendingSend;
    UdpReceiveHandler PendingUdpRecv;
    UdpSendHandler PendingUdpSend;

    // 统一接口支持
    uint32_t DefaultTimeoutMs = 30000;  // 默认30秒超时
    std::unordered_map<CancelToken, bool> ActiveOperations;
    mutable std::mutex OperationsMutex;
};
}  // namespace Helianthus::Network::Asio
