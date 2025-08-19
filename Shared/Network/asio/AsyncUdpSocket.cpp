#include "Shared/Network/Asio/AsyncUdpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"

namespace Helianthus::Network::Asio
{
    AsyncUdpSocket::AsyncUdpSocket(std::shared_ptr<IoContext> CtxIn)
        : Ctx(std::move(CtxIn))
        , ReactorPtr(Ctx ? Ctx->GetReactor() : nullptr)
        , Socket()
        , PendingRecv()
    {
    }

    Network::NetworkError AsyncUdpSocket::Bind(const Network::NetworkAddress& Address)
    {
        return Socket.Bind(Address);
    }

    void AsyncUdpSocket::AsyncReceive(char* Buffer, size_t BufferSize, ReceiveHandler Handler)
    {
        PendingRecv = std::move(Handler);
        if (!ReactorPtr) 
        {
            if (PendingRecv) 
            {
                PendingRecv(Network::NetworkError::NOT_INITIALIZED, 0);
            }
            return;
        }
        const int fd = Socket.GetNativeHandle();
        ReactorPtr->Add(fd, EventMask::Read, [this, Buffer, BufferSize](EventMask ev){
            if ((static_cast<uint32_t>(ev) & static_cast<uint32_t>(EventMask::Read)) == 0) 
            {
                return;
            }
            size_t received = 0; Network::NetworkError err = Socket.Receive(Buffer, BufferSize, received);
            auto Callback = PendingRecv; 
            PendingRecv = nullptr;
            if (Callback) 
            {
                Callback(err, received);
            }
        });
    }

    void AsyncUdpSocket::AsyncSendTo(const char* Data, size_t Size, const Network::NetworkAddress& Address, SendHandler Handler)
    {
        (void)ReactorPtr; // 暂不使用写事件
        Network::NetworkError Err = Socket.SendTo(Data, Size, Address);
        if (Handler) 
        {
            Handler(Err, Err == Network::NetworkError::SUCCESS ? Size : 0);
        }
    }

    Network::Sockets::UdpSocket& AsyncUdpSocket::Native()
    {
        return Socket;
    }
}


