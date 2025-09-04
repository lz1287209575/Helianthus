#include "Shared/Network/Asio/AsyncUdpSocket.h"

#include "Shared/Network/Asio/ErrorMapping.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/Reactor.h"

#include <chrono>

#include "Common/LogCategories.h"
#include "Common/LogCategory.h"

namespace Helianthus::Network::Asio
{
AsyncUdpSocket::AsyncUdpSocket(std::shared_ptr<IoContext> CtxIn)
    : Ctx(std::move(CtxIn)),
      ReactorPtr(Ctx ? Ctx->GetReactor() : nullptr),
      ProactorPtr(Ctx ? Ctx->GetProactor() : nullptr),
      Socket(),
      PendingRecv(),
      PendingSend(),
      PendingUdpRecv(),
      PendingUdpSend()
{
    // 设置非阻塞模式
    if (Socket.GetNativeHandle() != 0)
    {
        Socket.SetBlocking(false);
    }
}

Network::NetworkError AsyncUdpSocket::Bind(const Network::NetworkAddress& Address)
{
    auto Result = Socket.Bind(Address);
    if (Result == Network::NetworkError::NONE)
    {
        // 绑定成功后设置非阻塞模式
        Socket.SetBlocking(false);
    }
    return Result;
}

void AsyncUdpSocket::AsyncReceive(char* Buffer, size_t BufferSize, ReceiveHandler Handler)
{
    if (!ReactorPtr)
    {
        if (Handler)
            Handler(Network::NetworkError::NOT_INITIALIZED, 0);
        return;
    }

    PendingRecv = std::move(Handler);
    const auto SocketFd = static_cast<Fd>(Socket.GetNativeHandle());

    ReactorPtr->Add(SocketFd,
                    EventMask::Read,
                    [this, Buffer, BufferSize](EventMask Ev)
                    {
                        if ((static_cast<uint32_t>(Ev) & static_cast<uint32_t>(EventMask::Read)) ==
                            0)
                            return;

                        size_t Received = 0;
                        Network::NetworkError Err = Socket.Receive(Buffer, BufferSize, Received);

                        // 处理非阻塞情况
                        if (Err == Network::NetworkError::NONE && Received == 0)
                        {
                            // 非阻塞，没有数据可读，保持 PendingRecv，等待下次事件
                            return;
                        }

                        auto Cb = PendingRecv;
                        PendingRecv = nullptr;

                        if (Cb)
                            Cb(Err, Received);
                    });
}

void AsyncUdpSocket::AsyncSendTo(const char* Data,
                                 size_t Size,
                                 const Network::NetworkAddress& Address,
                                 SendHandler Handler)
{
    if (!ReactorPtr)
    {
        if (Handler)
            Handler(Network::NetworkError::NOT_INITIALIZED, 0);
        return;
    }

    // 尝试直接发送
    Network::NetworkError Err = Socket.SendTo(Data, Size, Address);

    if (Err == Network::NetworkError::NONE)
    {
        // 发送成功，直接回调
        if (Handler)
            Handler(Err, Size);
        return;
    }

    if (Err == Network::NetworkError::SEND_FAILED)
    {
        // 发送失败，可能是缓冲区满，注册写事件
        PendingSend = std::move(Handler);
        const auto SocketFd = static_cast<Fd>(Socket.GetNativeHandle());

        ReactorPtr->Add(
            SocketFd,
            EventMask::Write,
            [this, Data, Size, Address](EventMask Ev)
            {
                if ((static_cast<uint32_t>(Ev) & static_cast<uint32_t>(EventMask::Write)) == 0)
                    return;

                Network::NetworkError Err = Socket.SendTo(Data, Size, Address);

                if (Err == Network::NetworkError::SEND_FAILED)
                {
                    // 继续等待写事件
                    return;
                }

                auto Cb = PendingSend;
                PendingSend = nullptr;

                if (Cb)
                    Cb(Err, Err == Network::NetworkError::NONE ? Size : 0);
            });
    }
    else
    {
        // 其他错误，直接回调
        if (Handler)
            Handler(Err, 0);
    }
}

Network::Sockets::UdpSocket& Helianthus::Network::Asio::AsyncUdpSocket::Native()
{
    return Socket;
}

// 新的 UDP 特定异步操作方法
void Helianthus::Network::Asio::AsyncUdpSocket::AsyncReceiveFrom(char* Buffer,
                                                                 size_t BufferSize,
                                                                 UdpReceiveHandler Handler)
{
    // Windows 下优先使用 Proactor（IOCP）
#ifdef _WIN32
    if (ProactorPtr)
    {
        const auto SocketFd = static_cast<Fd>(Socket.GetNativeHandle());
        ProactorPtr->AsyncReceiveFrom(SocketFd, Buffer, BufferSize, std::move(Handler));
        return;
    }
#endif

    // 非 Windows 或没有 Proactor 时，使用 Reactor 模式
    if (!ReactorPtr)
    {
        if (Handler)
            Handler(Network::NetworkError::NOT_INITIALIZED, 0, Network::NetworkAddress{});
        return;
    }

    PendingUdpRecv = std::move(Handler);
    const auto SocketFd = static_cast<Fd>(Socket.GetNativeHandle());

    ReactorPtr->Add(SocketFd,
                    EventMask::Read,
                    [this, Buffer, BufferSize](EventMask Ev)
                    {
                        if ((static_cast<uint32_t>(Ev) & static_cast<uint32_t>(EventMask::Read)) ==
                            0)
                            return;

                        size_t Received = 0;
                        Network::NetworkAddress FromAddress;
                        Network::NetworkError Err =
                            Socket.ReceiveFrom(Buffer, BufferSize, Received, FromAddress);

                        // 处理非阻塞情况
                        if (Err == Network::NetworkError::NONE && Received == 0)
                        {
                            // 非阻塞，没有数据可读，保持 PendingUdpRecv，等待下次事件
                            return;
                        }

                        auto Cb = PendingUdpRecv;
                        PendingUdpRecv = nullptr;

                        if (Cb)
                            Cb(Err, Received, FromAddress);
                    });
}

void Helianthus::Network::Asio::AsyncUdpSocket::AsyncSendToProactor(
    const char* Data, size_t Size, const Network::NetworkAddress& Address, UdpSendHandler Handler)
{
    // Windows 下优先使用 Proactor（IOCP）
#ifdef _WIN32
    if (ProactorPtr)
    {
        const auto SocketFd = static_cast<Fd>(Socket.GetNativeHandle());
        ProactorPtr->AsyncSendTo(SocketFd, Data, Size, Address, std::move(Handler));
        return;
    }
#endif

    // 非 Windows 或没有 Proactor 时，使用 Reactor 模式
    if (!ReactorPtr)
    {
        if (Handler)
            Handler(Network::NetworkError::NOT_INITIALIZED, 0);
        return;
    }

    // 尝试直接发送
    Network::NetworkError Err = Socket.SendTo(Data, Size, Address);

    if (Err == Network::NetworkError::NONE)
    {
        // 发送成功，直接回调
        if (Handler)
            Handler(Err, Size);
        return;
    }

    if (Err == Network::NetworkError::SEND_FAILED)
    {
        // 发送失败，可能是缓冲区满，注册写事件
        PendingUdpSend = std::move(Handler);
        const auto SocketFd = static_cast<Fd>(Socket.GetNativeHandle());

        ReactorPtr->Add(
            SocketFd,
            EventMask::Write,
            [this, Data, Size, Address](EventMask Ev)
            {
                if ((static_cast<uint32_t>(Ev) & static_cast<uint32_t>(EventMask::Write)) == 0)
                    return;

                Network::NetworkError Err = Socket.SendTo(Data, Size, Address);

                if (Err == Network::NetworkError::SEND_FAILED)
                {
                    // 继续等待写事件
                    return;
                }

                auto Cb = PendingUdpSend;
                PendingUdpSend = nullptr;

                if (Cb)
                    Cb(Err, Err == Network::NetworkError::NONE ? Size : 0);
            });
    }
    else
    {
        // 其他错误，直接回调
        if (Handler)
            Handler(Err, 0);
    }
}
}  // namespace Helianthus::Network::Asio

// 统一接口实现
Helianthus::Network::NetworkError Helianthus::Network::Asio::AsyncUdpSocket::Connect(
    const Helianthus::Network::NetworkAddress& Address)
{
    // UDP通常不需要连接，但可以设置默认目标地址
    return Helianthus::Network::NetworkError::NONE;
}

void Helianthus::Network::Asio::AsyncUdpSocket::Close()
{
    Socket.Disconnect();
}

void Helianthus::Network::Asio::AsyncUdpSocket::AsyncReceive(char* Buffer,
                                                             size_t BufferSize,
                                                             AsyncReceiveHandler Handler,
                                                             CancelToken Token,
                                                             uint32_t TimeoutMs)
{
    if (!ReactorPtr)
    {
        if (Handler)
            Handler(Helianthus::Network::NetworkError::NOT_INITIALIZED,
                    0,
                    Helianthus::Network::NetworkAddress{});
        return;
    }

    // 注册操作到取消跟踪
    if (Token)
    {
        std::lock_guard<std::mutex> Lock(OperationsMutex);
        ActiveOperations[Token] = true;
    }

    // 使用AsyncReceiveFrom接口，然后转换回调
    this->AsyncReceiveFrom(Buffer,
                           BufferSize,
                           [this, Handler, Token](Helianthus::Network::NetworkError Error,
                                                  size_t Bytes,
                                                  Helianthus::Network::NetworkAddress FromAddr)
                           {
                               // 检查是否被取消
                               if (Token && IsCancelled(Token))
                               {
                                   if (Handler)
                                       Handler(
                                           Helianthus::Network::NetworkError::OPERATION_CANCELLED,
                                           0,
                                           Helianthus::Network::NetworkAddress{});
                               }
                               else
                               {
                                   if (Handler)
                                       Handler(Error, Bytes, FromAddr);
                               }

                               // 清理操作跟踪
                               if (Token)
                               {
                                   std::lock_guard<std::mutex> Lock(OperationsMutex);
                                   ActiveOperations.erase(Token);
                               }
                           });
}

void Helianthus::Network::Asio::AsyncUdpSocket::AsyncSend(
    const char* Data,
    size_t Size,
    const Helianthus::Network::NetworkAddress& Address,
    AsyncSendHandler Handler,
    CancelToken Token,
    uint32_t TimeoutMs)
{
    if (!ReactorPtr)
    {
        if (Handler)
            Handler(Helianthus::Network::NetworkError::NOT_INITIALIZED, 0);
        return;
    }

    // 注册操作到取消跟踪
    if (Token)
    {
        std::lock_guard<std::mutex> Lock(OperationsMutex);
        ActiveOperations[Token] = true;
    }

    // 使用AsyncSendToProactor接口，然后转换回调
    this->AsyncSendToProactor(
        Data,
        Size,
        Address,
        [this, Handler, Token](Helianthus::Network::NetworkError Error, size_t Bytes)
        {
            // 检查是否被取消
            if (Token && IsCancelled(Token))
            {
                if (Handler)
                    Handler(Helianthus::Network::NetworkError::OPERATION_CANCELLED, 0);
            }
            else
            {
                if (Handler)
                    Handler(Error, Bytes);
            }

            // 清理操作跟踪
            if (Token)
            {
                std::lock_guard<std::mutex> Lock(OperationsMutex);
                ActiveOperations.erase(Token);
            }
        });
}

void Helianthus::Network::Asio::AsyncUdpSocket::AsyncConnect(
    const Helianthus::Network::NetworkAddress& Address,
    AsyncConnectHandler Handler,
    CancelToken Token,
    uint32_t TimeoutMs)
{
    // UDP不需要连接，直接返回成功
    if (Handler)
        Handler(Helianthus::Network::NetworkError::NONE);
}

void Helianthus::Network::Asio::AsyncUdpSocket::CancelOperation(CancelToken Token)
{
    if (Token)
    {
        CancelOperation(Token);

        // 从活动操作中移除
        std::lock_guard<std::mutex> Lock(OperationsMutex);
        ActiveOperations.erase(Token);
    }
}

void Helianthus::Network::Asio::AsyncUdpSocket::SetDefaultTimeout(uint32_t TimeoutMs)
{
    DefaultTimeoutMs = TimeoutMs;
}

uint32_t Helianthus::Network::Asio::AsyncUdpSocket::GetDefaultTimeout() const
{
    return DefaultTimeoutMs;
}

bool Helianthus::Network::Asio::AsyncUdpSocket::IsConnected() const
{
    // UDP通常是无连接的
    return true;
}

bool Helianthus::Network::Asio::AsyncUdpSocket::IsClosed() const
{
    return Socket.GetNativeHandle() == 0;
}

Helianthus::Network::NetworkAddress
Helianthus::Network::Asio::AsyncUdpSocket::GetLocalAddress() const
{
    return Socket.GetLocalAddress();
}

Helianthus::Network::NetworkAddress
Helianthus::Network::Asio::AsyncUdpSocket::GetRemoteAddress() const
{
    // UDP没有固定的远程地址
    return Helianthus::Network::NetworkAddress{};
}
