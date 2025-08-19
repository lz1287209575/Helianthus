#pragma once

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <unordered_map>
#include "Shared/Network/Asio/Proactor.h"

namespace Helianthus::Network::Asio
{
    class ProactorIocp : public Proactor
    {
    public:
        ProactorIocp();
        ~ProactorIocp() override;

        void AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler) override;
        void AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler) override;
        void ProcessCompletions(int TimeoutMs) override;

    private:
        HANDLE IocpHandle;
        struct Op
        {
            OVERLAPPED Ov;
            char* Buffer;
            size_t BufferSize;
            const char* ConstData;
            size_t DataSize;
            CompletionHandler Handler;
            bool IsWrite;
        };
        std::unordered_map<Fd, Op*> Pending;
    };
}

#endif // _WIN32


