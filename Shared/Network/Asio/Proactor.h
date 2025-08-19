#pragma once

#include <functional>
#include <cstddef>
#include <cstdint>

#include "Shared/Network/NetworkTypes.h"

namespace Helianthus::Network::Asio
{
    using CompletionHandler = std::function<void(Network::NetworkError, size_t)>;
    using Fd = uintptr_t; // 与 Udp/Tcp 的 NativeHandle 对齐，跨平台安全

    // Proactor 抽象：提交 IO 操作，完成后回调
    class Proactor
    {
    public:
        virtual ~Proactor() = default;

        virtual void AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler) = 0;
        virtual void AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler) = 0;
        // 处理完成队列（Windows IOCP 使用；Reactor 适配为 no-op）
        virtual void ProcessCompletions(int TimeoutMs) {}
    };
}


