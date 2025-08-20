#pragma once

#ifdef _WIN32

    #include "Shared/Network/Asio/Proactor.h"

    #include <cstddef>
    #include <unordered_map>
    #include <unordered_set>

    #include <mswsock.h>
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>

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
    void AsyncAccept(Fd ListenHandle, AcceptResultHandler Handler) override;

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
        enum class OpType
        {
            Read,
            Write,
            Accept
        } Type;
        SOCKET ListenSocket;           // for Accept
        AcceptResultHandler AcceptCb;  // valid when Type==Accept
    };

    // 已关联到 IOCP 的句柄集合，避免重复关联
    std::unordered_set<Fd> Associated;

    void AssociateSocketIfNeeded(SOCKET Socket);
    // AcceptEx pointer cache
    LPFN_ACCEPTEX AcceptExPtr = nullptr;
    void EnsureAcceptEx(SOCKET ListenSocket);
};
}  // namespace Helianthus::Network::Asio

#endif  // _WIN32
