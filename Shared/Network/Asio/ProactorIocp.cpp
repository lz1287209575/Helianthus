#ifdef _WIN32

#include "Shared/Network/Asio/ProactorIocp.h"

namespace Helianthus::Network::Asio
{
    ProactorIocp::ProactorIocp()
        : IocpHandle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0))
        , Pending()
    {
    }

    ProactorIocp::~ProactorIocp()
    {
        if (IocpHandle) CloseHandle(IocpHandle);
    }

    void ProactorIocp::AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler)
    {
        SOCKET s = reinterpret_cast<SOCKET>(Handle);
        CreateIoCompletionPort(reinterpret_cast<HANDLE>(s), IocpHandle, static_cast<ULONG_PTR>(Handle), 0);
        auto* op = new Op{};
        memset(&op->Ov, 0, sizeof(op->Ov));
        op->Buffer = Buffer; op->BufferSize = BufferSize; op->ConstData = nullptr; op->DataSize = 0; op->Handler = std::move(Handler); op->IsWrite = false;
        Pending[static_cast<Fd>(Handle)] = op;
        // 使用 WSARecv 提交异步读
        WSABUF buf; buf.buf = op->Buffer; buf.len = static_cast<ULONG>(op->BufferSize);
        DWORD flags = 0; DWORD bytes = 0;
        int r = WSARecv(s, &buf, 1, &bytes, &flags, &op->Ov, nullptr);
        (void)r;
    }

    void ProactorIocp::AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler)
    {
        SOCKET s = reinterpret_cast<SOCKET>(Handle);
        CreateIoCompletionPort(reinterpret_cast<HANDLE>(s), IocpHandle, static_cast<ULONG_PTR>(Handle), 0);
        auto* op = new Op{};
        memset(&op->Ov, 0, sizeof(op->Ov));
        op->Buffer = nullptr; op->BufferSize = 0; op->ConstData = Data; op->DataSize = Size; op->Handler = std::move(Handler); op->IsWrite = true;
        Pending[static_cast<Fd>(Handle)] = op;
        // 使用 WSASend 提交异步写
        WSABUF buf; buf.buf = const_cast<char*>(op->ConstData); buf.len = static_cast<ULONG>(op->DataSize);
        DWORD bytes = 0;
        int r = WSASend(s, &buf, 1, &bytes, 0, &op->Ov, nullptr);
        (void)r;
    }

    void ProactorIocp::ProcessCompletions(int TimeoutMs)
    {
        DWORD bytes = 0; ULONG_PTR key = 0; LPOVERLAPPED pov = nullptr;
        BOOL ok = GetQueuedCompletionStatus(IocpHandle, &bytes, &key, &pov, TimeoutMs >= 0 ? static_cast<DWORD>(TimeoutMs) : INFINITE);
        if (!ok && pov == nullptr) return;
        auto it = Pending.find(static_cast<Fd>(key));
        if (it == Pending.end()) return;
        Op* op = it->second; Pending.erase(it);
        auto handler = std::move(op->Handler);
        delete op;
        if (handler)
        {
            handler(ok ? Network::NetworkError::NONE : Network::NetworkError::RECEIVE_FAILED, static_cast<size_t>(bytes));
        }
    }
}

#endif // _WIN32


