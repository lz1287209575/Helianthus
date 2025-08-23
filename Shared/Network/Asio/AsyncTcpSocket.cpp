#include "Shared/Network/Asio/AsyncTcpSocket.h"

#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace Helianthus::Network::Asio
{
AsyncTcpSocket::AsyncTcpSocket(std::shared_ptr<IoContext> CtxIn)
    : Ctx(std::move(CtxIn)),
      ReactorPtr(Ctx ? Ctx->GetReactor() : nullptr),
      ProactorPtr(Ctx ? Ctx->GetProactor() : nullptr),
      Socket(),
      PendingRecv()
{
    // 确保Socket正确初始化
    // Socket的默认构造函数会创建一个有效的TcpSocket实例
    // 但是我们需要确保套接字被正确创建
    if (Socket.GetNativeHandle() == -1)
    {
        // 如果套接字没有创建，我们需要手动创建一个
        // 这里我们暂时不创建，让Connect方法来处理
    }
}

Network::NetworkError AsyncTcpSocket::Connect(const Network::NetworkAddress& Address)
{
    auto Err = Socket.Connect(Address);
    if (Err == Network::NetworkError::NONE)
    {
        // 确保用于 Reactor 的套接字为非阻塞，避免在回调中阻塞导致事件循环卡死
        Socket.SetBlocking(false);
    }
    return Err;
}

Network::NetworkError AsyncTcpSocket::Bind(const Network::NetworkAddress& Address)
{
    auto Err = Socket.Bind(Address);
    if (Err == Network::NetworkError::NONE)
    {
        Socket.SetBlocking(false);
    }
    return Err;
}

void AsyncTcpSocket::AsyncConnectLegacy(const Network::NetworkAddress& Address, std::function<void(Network::NetworkError)> Handler)
{
    if (ClosedFlag)
    {
        if (Handler)
        {
            Handler(Network::NetworkError::CONNECTION_CLOSED);
        }
        return;
    }

    // Windows 下暂时禁用 Proactor（IOCP），直接使用 Reactor
    // 这样可以避免 ConnectEx 的复杂性
    /*
#ifdef _WIN32
    if (ProactorPtr)
    {
        // 确保套接字已创建
        if (Socket.GetNativeHandle() == -1)
        {
            // 先尝试连接以创建套接字
            auto Err = Connect(Address);
            if (Err == Network::NetworkError::NONE)
            {
                // 连接立即成功
                if (Handler)
                {
                    Handler(Err);
                }
                return;
            }
            else if (Err != Network::NetworkError::CONNECTION_FAILED)
            {
                // 其他错误，直接返回
                if (Handler)
                {
                    Handler(Err);
                }
                return;
            }
            // 如果返回CONNECTION_FAILED，说明套接字已创建但连接正在进行中
        }
        else
        {
            // 套接字已存在，检查连接状态
            if (Socket.IsConnected())
            {
                // 已经连接
                if (Handler)
                {
                    Handler(Network::NetworkError::NONE);
                }
                return;
            }
            else if (Socket.GetConnectionState() == ConnectionState::CONNECTING)
            {
                // 正在连接中，需要等待
                // 继续执行下面的异步等待逻辑
            }
            else
            {
                // 套接字存在但状态异常，重新连接
                Socket.Disconnect();
                auto Err = Connect(Address);
                if (Err == Network::NetworkError::NONE)
                {
                    if (Handler)
                    {
                        Handler(Err);
                    }
                    return;
                }
                else if (Err != Network::NetworkError::CONNECTION_FAILED)
                {
                    if (Handler)
                    {
                        Handler(Err);
                    }
                    return;
                }
            }
        }
        
        const auto FdValue = static_cast<Fd>(Socket.GetNativeHandle());
        if (FdValue != -1)
        {
            // 对于ConnectEx，需要先绑定套接字到本地地址
            // 绑定到任意可用端口
            Network::NetworkAddress LocalAddr("127.0.0.1", 0);
            auto BindErr = Socket.Bind(LocalAddr);
            if (BindErr != Network::NetworkError::NONE)
            {
                if (Handler)
                {
                    Handler(BindErr);
                }
                return;
            }
            
            ProactorPtr->AsyncConnect(FdValue, Address, std::move(Handler));
            return;
        }
    }
#endif
    */

    // 非 Windows 或没有 Proactor 时，使用 Reactor 实现异步连接
    if (ReactorPtr)
    {
        // 确保套接字已创建
        if (Socket.GetNativeHandle() == -1)
        {
            // 先尝试连接以创建套接字
            auto Err = Connect(Address);
            if (Err == Network::NetworkError::NONE)
            {
                // 连接立即成功
                if (Handler)
                {
                    Handler(Err);
                }
                return;
            }
            else if (Err != Network::NetworkError::CONNECTION_FAILED)
            {
                // 其他错误，直接返回
                if (Handler)
                {
                    Handler(Err);
                }
                return;
            }
            // 如果返回CONNECTION_FAILED，说明套接字已创建但连接正在进行中
        }
        else
        {
            // 套接字已存在，检查连接状态
            if (Socket.IsConnected())
            {
                // 已经连接
                if (Handler)
                {
                    Handler(Network::NetworkError::NONE);
                }
                return;
            }
            else if (Socket.GetConnectionState() == ConnectionState::CONNECTING)
            {
                // 正在连接中，需要等待
                // 继续执行下面的异步等待逻辑
            }
            else
            {
                // 套接字存在但状态异常，重新连接
                Socket.Disconnect();
                auto Err = Connect(Address);
                if (Err == Network::NetworkError::NONE)
                {
                    if (Handler)
                    {
                        Handler(Err);
                    }
                    return;
                }
                else if (Err != Network::NetworkError::CONNECTION_FAILED)
                {
                    if (Handler)
                    {
                        Handler(Err);
                    }
                    return;
                }
            }
        }
        
        // 连接正在进行中，需要异步等待
        const auto FdValue = static_cast<Fd>(Socket.GetNativeHandle());
        if (FdValue == -1)
        {
            // 套接字创建失败
            if (Handler)
            {
                Handler(Network::NetworkError::SOCKET_CREATE_FAILED);
            }
            return;
        }
            
            // 保存连接回调
            PendingConnectCb = std::move(Handler);
            PendingConnectAddr = Address;
            
            // 注册写事件，等待连接完成
            if (ReactorPtr->Add(FdValue, EventMask::Write,
                [this](EventMask Event) {
                    if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Write)) == 0)
                    {
                        return;
                    }
                    
                    // 检查连接状态
                    auto ConnectErr = Socket.CheckConnectionStatus();
                    
                    // 移除注册
                    const auto FdValue = static_cast<Fd>(Socket.GetNativeHandle());
                    ReactorPtr->Del(FdValue);
                    ConnectRegistered = false;
                    
                    // 调用回调
                    auto Cb = std::move(PendingConnectCb);
                    PendingConnectCb = nullptr;
                    if (Cb)
                    {
                        Cb(ConnectErr);
                    }
                }))
            {
                ConnectRegistered = true;
                return;
            }
        }
        else
        {
            // 其他错误，直接返回
            if (Handler)
            {
                Handler(Err);
            }
            return;
        }
    }
    
    // 如果所有异步方法都失败，返回错误
    if (Handler)
    {
        Handler(Network::NetworkError::CONNECTION_FAILED);
    }
}

void AsyncTcpSocket::AsyncReceiveLegacy(char* Buffer, size_t BufferSize, ReceiveHandler Handler)
{
    if (ClosedFlag)
    {
        if (Handler)
        {
            Handler(Network::NetworkError::CONNECTION_CLOSED, 0);
        }
        return;
    }

    PendingRecv = std::move(Handler);
    const auto FdValue = static_cast<Fd>(Socket.GetNativeHandle());
    PendingRecvBuf = Buffer;
    PendingRecvSize = BufferSize;

    // Windows 下优先使用 Proactor（IOCP）。非 Windows 统一走 Reactor。
#ifdef _WIN32
    if (ProactorPtr)
    {
        ProactorPtr->AsyncRead(FdValue,
                               Buffer,
                               BufferSize,
                               [this](Network::NetworkError Err, size_t Bytes)
                               {
                                   auto cb = PendingRecv;
                                   PendingRecv = nullptr;
                                   if (cb) cb(Err, Bytes);
                               });
        return;
    }
#endif

    // 快路径：尝试一次非阻塞接收，如果有数据则直接完成回调
    {
        size_t immediateReceived = 0;
        Network::NetworkError immediateErr = Socket.Receive(Buffer, BufferSize, immediateReceived);
        if (immediateErr == Network::NetworkError::NONE && immediateReceived > 0)
        {
            auto cb = PendingRecv;
            PendingRecv = nullptr;
            if (cb)
            {
                cb(immediateErr, immediateReceived);
            }
            return;
        }
    }

    if (!ReactorPtr)
    {
        if (PendingRecv)
        {
            PendingRecv(Network::NetworkError::NOT_INITIALIZED, 0);
        }
        return;
    }
    
    // 如果尚未注册，则注册一次（保持注册，避免竞态）
    
    // 注册/更新到 Reactor（每次调用都更新回调）
    if (ReactorPtr->Add(FdValue, EventMask::Read,
        [this](EventMask Event)
        {
            if (ClosedFlag)
            {
                return;
            }
            
            if ((static_cast<uint32_t>(Event) & static_cast<uint32_t>(EventMask::Read)) == 0)
            {
                return;
            }
            
            // 如果当前没有挂起的接收回调，避免错误消费字节
            if (!PendingRecv)
            {
                return;
            }

            size_t Received = 0;
            bool PeerClosed = false;
            // 读尽直到 EAGAIN/无数据 或 缓冲满
            while (true)
            {
                size_t chunk = 0;
                Network::NetworkError errNow = Socket.Receive(
                    PendingRecvBuf + Received,
                    PendingRecvSize - Received,
                    chunk);
                if (errNow == Network::NetworkError::NONE && chunk > 0)
                {
                    Received += chunk;
                    if (Received >= PendingRecvSize)
                    {
                        break;
                    }
                    continue;
                }
                if (errNow == Network::NetworkError::CONNECTION_CLOSED)
                {
                    PeerClosed = true;
                }
                // 对于非致命情况（如 EAGAIN 映射为 RECEIVE_FAILED），跳出等待下一次事件
                break;
            }

            // 仅在读到数据或对端关闭时回调并清理状态；否则保留 PendingRecv 以等待下一次事件
            if (Received > 0 || PeerClosed)
            {
                auto Callback = PendingRecv;
                PendingRecv = nullptr;
                PendingRecvBuf = nullptr;
                PendingRecvSize = 0;

                // 保持注册，不在此处删除，避免与后续再次注册产生竞态
                if (Callback)
                {
                    Callback(PeerClosed ? Network::NetworkError::CONNECTION_CLOSED
                                        : Network::NetworkError::NONE,
                             Received);
                }
                if (PeerClosed)
                {
                    Close();
                }
            }
        }))
    {
        IsRegistered = true;
    }
}

void AsyncTcpSocket::AsyncSendLegacy(const char* Data, size_t Size, SendHandler Handler)
{
    const auto FdValue = static_cast<Fd>(Socket.GetNativeHandle());
    // Windows 下优先使用 Proactor（IOCP）。非 Windows 统一走 Reactor。
#ifdef _WIN32
    if (ProactorPtr)
    {
        ProactorPtr->AsyncWrite(FdValue,
                                Data,
                                Size,
                                [Handler](Network::NetworkError Err, size_t Bytes)
                                {
                                    if (Handler) Handler(Err, Bytes);
                                });
        return;
    }
#endif

    PendingSendPtr = Data;
    PendingSendRemaining = Size;
    PendingSendTotalSent = 0;
    PendingSendHandler = std::move(Handler);

    // 先尝试一次立即发送
    size_t Sent = 0;
    Network::NetworkError Err = Socket.Send(PendingSendPtr, PendingSendRemaining, Sent);
    if (Err == Network::NetworkError::NONE)
    {
        PendingSendPtr += Sent;
        PendingSendRemaining -= Sent;
        PendingSendTotalSent += Sent;
        if (PendingSendRemaining == 0)
        {
            auto cb = PendingSendHandler;
            PendingSendHandler = nullptr;
            if (cb) cb(Network::NetworkError::NONE, PendingSendTotalSent);
            return;
        }
    }

    // 未完全发送，注册写事件
    if (!ReactorPtr) {
        auto cb = PendingSendHandler;
        PendingSendHandler = nullptr;
        if (cb) cb(Network::NetworkError::NOT_INITIALIZED, PendingSendTotalSent);
        return;
    }

    if (ReactorPtr->Add(FdValue, EventMask::Write,
        [this](EventMask ev){
            if ((static_cast<uint32_t>(ev) & static_cast<uint32_t>(EventMask::Write)) == 0) return;
            if (PendingSendRemaining == 0) return;
            size_t SentNow = 0;
            Network::NetworkError ErrNow = Socket.Send(PendingSendPtr, PendingSendRemaining, SentNow);
            if (ErrNow != Network::NetworkError::NONE && SentNow == 0) return; // 等待下一次可写
            PendingSendPtr += SentNow;
            PendingSendRemaining -= SentNow;
            PendingSendTotalSent += SentNow;
            if (PendingSendRemaining == 0)
            {
                auto cb = PendingSendHandler;
                PendingSendHandler = nullptr;
                if (cb) cb(Network::NetworkError::NONE, PendingSendTotalSent);
            }
        }))
    {
        SendRegistered = true;
    }
}

Network::Sockets::TcpSocket& AsyncTcpSocket::Native()
{
    return Socket;
}

void AsyncTcpSocket::Close()
{
    if (ClosedFlag)
    {
        return;
    }
    ClosedFlag = true;
    
    const auto Handle = static_cast<Fd>(Socket.GetNativeHandle());
    
    // 取消 Proactor 操作
    if (ProactorPtr)
    {
        ProactorPtr->Cancel(Handle);
    }
    
    // 从 Reactor 移除
    if (ReactorPtr)
    {
        if (IsRegistered)
        {
            ReactorPtr->Del(Handle);
            IsRegistered = false;
        }
        if (SendRegistered)
        {
            ReactorPtr->Del(Handle);
            SendRegistered = false;
        }
        if (ConnectRegistered)
        {
            ReactorPtr->Del(Handle);
            ConnectRegistered = false;
        }
    }
    
    // 清理回调
    if (PendingRecv)
    {
        PendingRecv(Network::NetworkError::CONNECTION_CLOSED, 0);
        PendingRecv = nullptr;
    }
    
    if (PendingSendHandler)
    {
        PendingSendHandler(Network::NetworkError::CONNECTION_CLOSED, 0);
        PendingSendHandler = nullptr;
    }
    
    if (PendingConnectCb)
    {
        PendingConnectCb(Network::NetworkError::CONNECTION_CLOSED);
        PendingConnectCb = nullptr;
    }
    
    Socket.Disconnect();
}

// 统一接口实现
void AsyncTcpSocket::AsyncReceive(char* Buffer, size_t BufferSize, AsyncReceiveHandler Handler,
                                 CancelToken Token, uint32_t TimeoutMs)
{
    if (ClosedFlag)
    {
        if (Handler) Handler(Network::NetworkError::CONNECTION_CLOSED, 0, Network::NetworkAddress{});
        return;
    }

    // 注册操作到取消跟踪
    if (Token)
    {
        std::lock_guard<std::mutex> Lock(OperationsMutex);
        ActiveOperations[Token] = true;
    }

    // 使用兼容性接口，然后转换回调
    AsyncReceiveLegacy(Buffer, BufferSize, [this, Handler, Token](Network::NetworkError Error, size_t Bytes) {
        // 检查是否被取消
        if (Token && IsCancelled(Token))
        {
            if (Handler) Handler(Network::NetworkError::OPERATION_CANCELLED, 0, Network::NetworkAddress{});
        }
        else
        {
            if (Handler) Handler(Error, Bytes, Network::NetworkAddress{}); // TCP没有地址信息
        }
        
        // 清理操作跟踪
        if (Token)
        {
            std::lock_guard<std::mutex> Lock(OperationsMutex);
            ActiveOperations.erase(Token);
        }
    });
}

void AsyncTcpSocket::AsyncSend(const char* Data, size_t Size, const Network::NetworkAddress& Address,
                              AsyncSendHandler Handler, CancelToken Token, uint32_t TimeoutMs)
{
    if (ClosedFlag)
    {
        if (Handler) Handler(Network::NetworkError::CONNECTION_CLOSED, 0);
        return;
    }

    // 注册操作到取消跟踪
    if (Token)
    {
        std::lock_guard<std::mutex> Lock(OperationsMutex);
        ActiveOperations[Token] = true;
    }

    // 使用兼容性接口，然后转换回调
    AsyncSendLegacy(Data, Size, [this, Handler, Token](Network::NetworkError Error, size_t Bytes) {
        // 检查是否被取消
        if (Token && IsCancelled(Token))
        {
            if (Handler) Handler(Network::NetworkError::OPERATION_CANCELLED, 0);
        }
        else
        {
            if (Handler) Handler(Error, Bytes);
        }
        
        // 清理操作跟踪
        if (Token)
        {
            std::lock_guard<std::mutex> Lock(OperationsMutex);
            ActiveOperations.erase(Token);
        }
    });
}

void AsyncTcpSocket::AsyncConnect(const Network::NetworkAddress& Address, AsyncConnectHandler Handler,
                                 CancelToken Token, uint32_t TimeoutMs)
{
    if (ClosedFlag)
    {
        if (Handler) Handler(Network::NetworkError::CONNECTION_CLOSED);
        return;
    }

    // 注册操作到取消跟踪
    if (Token)
    {
        std::lock_guard<std::mutex> Lock(OperationsMutex);
        ActiveOperations[Token] = true;
    }

    // 使用兼容性接口，然后转换回调
    AsyncConnectLegacy(Address, [this, Handler, Token](Network::NetworkError Error) {
        // 检查是否被取消
        if (Token && IsCancelled(Token))
        {
            if (Handler) Handler(Network::NetworkError::OPERATION_CANCELLED);
        }
        else
        {
            if (Handler) Handler(Error);
        }
        
        // 清理操作跟踪
        if (Token)
        {
            std::lock_guard<std::mutex> Lock(OperationsMutex);
            ActiveOperations.erase(Token);
        }
    });
}

void AsyncTcpSocket::CancelOperation(CancelToken Token)
{
    if (Token)
    {
        CancelOperation(Token);
        
        // 从活动操作中移除
        std::lock_guard<std::mutex> Lock(OperationsMutex);
        ActiveOperations.erase(Token);
    }
}

void AsyncTcpSocket::SetDefaultTimeout(uint32_t TimeoutMs)
{
    DefaultTimeoutMs = TimeoutMs;
}

uint32_t AsyncTcpSocket::GetDefaultTimeout() const
{
    return DefaultTimeoutMs;
}

bool AsyncTcpSocket::IsConnected() const
{
    return !ClosedFlag && Socket.IsConnected();
}

bool AsyncTcpSocket::IsClosed() const
{
    return ClosedFlag;
}

Network::NetworkAddress AsyncTcpSocket::GetLocalAddress() const
{
    return Socket.GetLocalAddress();
}

Network::NetworkAddress AsyncTcpSocket::GetRemoteAddress() const
{
    return Socket.GetRemoteAddress();
}
}  // namespace Helianthus::Network::Asio
