#include "Shared/Network/Asio/AsyncTcpSocket.h"

#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"
#include <iostream>
#include <thread>

namespace Helianthus::Network::Asio
{
AsyncTcpSocket::AsyncTcpSocket(std::shared_ptr<IoContext> CtxIn)
    : Ctx(std::move(CtxIn)),
      ReactorPtr(Ctx ? Ctx->GetReactor() : nullptr),
      ProactorPtr(Ctx ? Ctx->GetProactor() : nullptr),
      Socket(),
      PendingRecv()
{
}

Network::NetworkError AsyncTcpSocket::Connect(const Network::NetworkAddress& Address)
{
    return Socket.Connect(Address);
}

void AsyncTcpSocket::AsyncReceive(char* Buffer, size_t BufferSize, ReceiveHandler Handler)
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
        [this, Buffer, BufferSize](EventMask Event)
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
            // 读尽直到 EAGAIN 或缓冲满
            while (true)
            {
                size_t chunk = 0;
                Network::NetworkError errNow = Socket.Receive(Buffer + Received, BufferSize - Received, chunk);
                if (errNow == Network::NetworkError::NONE && chunk > 0)
                {
                    Received += chunk;
                    if (Received >= BufferSize) break;
                    continue;
                }
                break;
            }
            Network::NetworkError Err = (Received > 0) ? Network::NetworkError::NONE : Network::NetworkError::RECEIVE_FAILED;

            auto Callback = PendingRecv;
            PendingRecv = nullptr;

            // 保持注册，不在此处删除，避免与后续再次注册产生竞态
            
            if (Callback)
            {
                Callback(Err, Received);
            }
        }))
    {
        IsRegistered = true;
    }
}

void AsyncTcpSocket::AsyncSend(const char* Data, size_t Size, SendHandler Handler)
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
    if (ReactorPtr && IsRegistered)
    {
        ReactorPtr->Del(Handle);
        IsRegistered = false;
    }
    
    // 清理回调
    if (PendingRecv)
    {
        PendingRecv(Network::NetworkError::CONNECTION_CLOSED, 0);
        PendingRecv = nullptr;
    }
    
    Socket.Disconnect();
}
}  // namespace Helianthus::Network::Asio
