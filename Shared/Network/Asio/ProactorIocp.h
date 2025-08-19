#pragma once

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <unordered_map>
#include <unordered_set>
#include <cstddef>
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
        void Cancel(Fd Handle) override;

    private:
        HANDLE IocpHandle;
        struct Op
        {
            OVERLAPPED Ov;
            SOCKET Socket;
            char* Buffer;
            size_t BufferSize;
            const char* ConstData;
            size_t DataSize;
            CompletionHandler Handler;
            bool IsWrite;
            size_t Transferred;
        };

        // 已关联到 IOCP 的句柄集合，避免重复关联
        std::unordered_set<Fd> Associated;

        void AssociateSocketIfNeeded(SOCKET Socket);
    };
} // namespace Helianthus::Network::Asio

#endif // _WIN32


