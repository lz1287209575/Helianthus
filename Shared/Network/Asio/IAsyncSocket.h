#pragma once

#include "Shared/Network/NetworkTypes.h"
#include <functional>
#include <memory>

namespace Helianthus::Network::Asio
{
    // 统一的取消令牌类型
    using CancelToken = std::shared_ptr<std::atomic<bool>>;
    
    // 统一的异步操作回调类型
    using AsyncReceiveHandler = std::function<void(Helianthus::Network::NetworkError, size_t, Helianthus::Network::NetworkAddress)>;
    using AsyncSendHandler = std::function<void(Helianthus::Network::NetworkError, size_t)>;
    using AsyncConnectHandler = std::function<void(Helianthus::Network::NetworkError)>;
    using AsyncAcceptHandler = std::function<void(Helianthus::Network::NetworkError, uintptr_t)>;
    
    // 统一的异步Socket接口
    class IAsyncSocket
    {
    public:
        virtual ~IAsyncSocket() = default;
        
        // 基础连接操作
        virtual Helianthus::Network::NetworkError Connect(const Helianthus::Network::NetworkAddress& Address) = 0;
        virtual Helianthus::Network::NetworkError Bind(const Helianthus::Network::NetworkAddress& Address) = 0;
        virtual void Close() = 0;
        
        // 统一的异步操作接口
        virtual void AsyncReceive(char* Buffer, size_t BufferSize, AsyncReceiveHandler Handler,
                                CancelToken Token = nullptr, uint32_t TimeoutMs = 0) = 0;
        virtual void AsyncSend(const char* Data, size_t Size, const Helianthus::Network::NetworkAddress& Address,
                              AsyncSendHandler Handler, CancelToken Token = nullptr, uint32_t TimeoutMs = 0) = 0;
        virtual void AsyncConnect(const Helianthus::Network::NetworkAddress& Address, AsyncConnectHandler Handler,
                                CancelToken Token = nullptr, uint32_t TimeoutMs = 0) = 0;
        
        // 统一的取消和超时支持
        virtual void CancelOperation(CancelToken Token) = 0;
        virtual void SetDefaultTimeout(uint32_t TimeoutMs) = 0;
        virtual uint32_t GetDefaultTimeout() const = 0;
        
        // 状态查询
        virtual bool IsConnected() const = 0;
        virtual bool IsClosed() const = 0;
        virtual Helianthus::Network::NetworkAddress GetLocalAddress() const = 0;
        virtual Helianthus::Network::NetworkAddress GetRemoteAddress() const = 0;
    };
    
    // 创建取消令牌的便利函数
    inline CancelToken CreateCancelToken()
    {
        return std::make_shared<std::atomic<bool>>(false);
    }
    
    // 检查取消令牌的便利函数
    inline bool IsCancelled(const CancelToken& Token)
    {
        return Token && Token->load();
    }
    
    // 取消操作的便利函数
    inline void CancelOperation(const CancelToken& Token)
    {
        if (Token)
        {
            Token->store(true);
        }
    }
}
