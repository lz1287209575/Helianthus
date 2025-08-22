#include "Shared/Network/Asio/AsyncUdpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"
#include "Shared/Network/Asio/ErrorMapping.h"
#include "Common/LogCategory.h"
#include "Common/LogCategories.h"

namespace Helianthus::Network::Asio
{
    AsyncUdpSocket::AsyncUdpSocket(std::shared_ptr<IoContext> CtxIn)
        : Ctx(std::move(CtxIn))
        , ReactorPtr(Ctx ? Ctx->GetReactor() : nullptr)
        , Socket()
        , PendingRecv()
        , PendingSend()
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
            if (Handler) Handler(Network::NetworkError::NOT_INITIALIZED, 0);
            return;
        }

        PendingRecv = std::move(Handler);
        const auto SocketFd = static_cast<Fd>(Socket.GetNativeHandle());
        
        ReactorPtr->Add(SocketFd, EventMask::Read, [this, Buffer, BufferSize](EventMask Ev)
        {
            if ((static_cast<uint32_t>(Ev) & static_cast<uint32_t>(EventMask::Read)) == 0) 
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
            
            if (Cb) Cb(Err, Received);
        });
    }

    void AsyncUdpSocket::AsyncSendTo(const char* Data, size_t Size, const Network::NetworkAddress& Address, SendHandler Handler)
    {
        if (!ReactorPtr)
        {
            if (Handler) Handler(Network::NetworkError::NOT_INITIALIZED, 0);
            return;
        }

        // 尝试直接发送
        Network::NetworkError Err = Socket.SendTo(Data, Size, Address);
        
        if (Err == Network::NetworkError::NONE)
        {
            // 发送成功，直接回调
            if (Handler) Handler(Err, Size);
            return;
        }
        
        if (Err == Network::NetworkError::SEND_FAILED)
        {
            // 发送失败，可能是缓冲区满，注册写事件
            PendingSend = std::move(Handler);
            const auto SocketFd = static_cast<Fd>(Socket.GetNativeHandle());
            
            ReactorPtr->Add(SocketFd, EventMask::Write, [this, Data, Size, Address](EventMask Ev)
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
                
                if (Cb) Cb(Err, Err == Network::NetworkError::NONE ? Size : 0);
            });
        }
        else
        {
            // 其他错误，直接回调
            if (Handler) Handler(Err, 0);
        }
    }

    Network::Sockets::UdpSocket& AsyncUdpSocket::Native()
    {
        return Socket;
    }
}


