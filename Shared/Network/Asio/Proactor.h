#pragma once

#include "Shared/Network/NetworkTypes.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace Helianthus::Network::Asio
{
using CompletionHandler = std::function<void(Network::NetworkError, size_t)>;
using Fd = uintptr_t;  // 与 Udp/Tcp 的 NativeHandle 对齐，跨平台安全
using AcceptResultHandler = std::function<void(Network::NetworkError, Fd)>;
using ConnectHandler = std::function<void(Network::NetworkError)>;

// Proactor 抽象：提交 IO 操作，完成后回调
class Proactor
{
public:
    virtual ~Proactor() = default;

    virtual void
    AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler) = 0;
    virtual void
    AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler) = 0;
    // 异步连接（Windows IOCP 使用 ConnectEx；非 Windows 可不实现）
    virtual void AsyncConnect(Fd Handle, const Network::NetworkAddress& Address, ConnectHandler Handler) {}
    // 处理完成队列（Windows IOCP 使用；Reactor 适配为 no-op）
    virtual void ProcessCompletions(int TimeoutMs) {}
    // 取消指定句柄上的所有挂起 I/O（Windows IOCP 使用）
    virtual void Cancel(Fd /*Handle*/) {}
    // 异步接受连接（Windows IOCP 使用；非 Windows 可不实现）
    virtual void AsyncAccept(Fd /*ListenHandle*/, AcceptResultHandler /*Handler*/) {}
    
    // IOCP 唤醒机制（Windows IOCP 使用；其他平台可为空实现）
    virtual void Wakeup() {}
    virtual void Stop() {}
};
}  // namespace Helianthus::Network::Asio
