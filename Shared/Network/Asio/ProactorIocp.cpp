#ifdef _WIN32

#include "Shared/Network/Asio/ProactorIocp.h"

namespace Helianthus::Network::Asio
{
    static Network::NetworkError ConvertWinSockError(int ErrorCode, bool IsWrite)
    {
        switch (ErrorCode)
        {
            case WSAETIMEDOUT: return Network::NetworkError::TIMEOUT;
            case WSAECONNRESET: return Network::NetworkError::CONNECTION_CLOSED;
            case WSAENETUNREACH: return Network::NetworkError::NETWORK_UNREACHABLE;
            case WSAEACCES: return Network::NetworkError::PERMISSION_DENIED;
            default: return IsWrite ? Network::NetworkError::SEND_FAILED : Network::NetworkError::RECEIVE_FAILED;
        }
    }
    ProactorIocp::ProactorIocp()
        : IocpHandle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0))
        , Associated()
    {
    }

    ProactorIocp::~ProactorIocp()
    {
        if (IocpHandle) CloseHandle(IocpHandle);
    }

    void ProactorIocp::AssociateSocketIfNeeded(SOCKET Socket)
    {
        Fd Key = static_cast<Fd>(Socket);
        if (Associated.find(Key) != Associated.end()) 
        {
            return;
        }
        HANDLE Handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(Socket));
        CreateIoCompletionPort(Handle, IocpHandle, static_cast<ULONG_PTR>(Key), 0);
        Associated.insert(Key);
    }

    void ProactorIocp::AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler)
    {
        SOCKET Socket = static_cast<SOCKET>(Handle);
        AssociateSocketIfNeeded(Socket);
        auto* op = new Op{};
        memset(&op->Ov, 0, sizeof(op->Ov));
        op->Socket = Socket; 
        op->Buffer = Buffer;
        op->BufferSize = BufferSize;
        op->ConstData = nullptr; 
        op->DataSize = 0; 
        op->Handler = std::move(Handler); 
        op->IsWrite = false; 
        op->Transferred = 0;
        // 使用 WSARecv 提交异步读
        WSABUF BufferDescriptor; 
        BufferDescriptor.buf = op->Buffer; 
        BufferDescriptor.len = static_cast<ULONG>(op->BufferSize);
        DWORD Flags = 0; 
        DWORD Bytes = 0;
        int RecvResult = WSARecv(Socket, &BufferDescriptor, 1, &Bytes, &Flags, &op->Ov, nullptr);
        if (RecvResult == SOCKET_ERROR)
        {
            int ErrorCode = WSAGetLastError();
            if (ErrorCode != WSA_IO_PENDING)
            {
                auto Completion = std::move(op->Handler); delete op;
                if (Completion) 
                {
                    Completion(ConvertWinSockError(ErrorCode, false), 0);
                }
            }
        }
    }

    void ProactorIocp::AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler)
    {
        SOCKET Socket = static_cast<SOCKET>(Handle);
        AssociateSocketIfNeeded(Socket);
        auto* op = new Op{};
        memset(&op->Ov, 0, sizeof(op->Ov));
        op->Socket = Socket; 
        op->Buffer = nullptr;
        op->BufferSize = 0;
        op->ConstData = Data;
        op->DataSize = Size;
        op->Handler = std::move(Handler);
        op->IsWrite = true;
        op->Transferred = 0;
        // 使用 WSASend 提交异步写
        WSABUF BufferDescriptor; BufferDescriptor.buf = const_cast<char*>(op->ConstData); BufferDescriptor.len = static_cast<ULONG>(op->DataSize);
        DWORD Bytes = 0;
        int SendResult = WSASend(Socket, &BufferDescriptor, 1, &Bytes, 0, &op->Ov, nullptr);
        if (SendResult == SOCKET_ERROR)
        {
            int ErrorCode = WSAGetLastError();
            if (ErrorCode != WSA_IO_PENDING)
            {
                auto Completion = std::move(op->Handler); delete op;
                if (Completion) 
                {
                    Completion(ConvertWinSockError(ErrorCode, true), 0);
                }
            }
        }
    }

    void ProactorIocp::ProcessCompletions(int TimeoutMs)
    {
        DWORD Bytes = 0; 
        ULONG_PTR Key = 0; 
        LPOVERLAPPED OverlappedPtr = nullptr;
        BOOL Ok = GetQueuedCompletionStatus(IocpHandle, &Bytes, &Key, &OverlappedPtr, TimeoutMs >= 0 ? static_cast<DWORD>(TimeoutMs) : INFINITE);
        if (!Ok && OverlappedPtr == nullptr) 
        {
            return; // timeout
        }
        if (OverlappedPtr == nullptr) 
        {
            return; // spurious
        }
        Op* Operation = reinterpret_cast<Op*>(OverlappedPtr);
        Operation->Transferred += Bytes;
        Network::NetworkError err = Network::NetworkError::NONE;
        size_t ToReport = Operation->Transferred;
        bool done = true;
        if (!Ok)
        {
            int ErrorCode = GetLastError();
            err = ConvertWinSockError(ErrorCode, Operation->IsWrite);
        }
        else if (!Operation->IsWrite && Bytes == 0)
        {
            err = Network::NetworkError::CONNECTION_CLOSED;
        }
        else
        {
            // 对写：未写满则继续提交
            if (Operation->IsWrite && Operation->Transferred < Operation->DataSize)
            {
                memset(&Operation->Ov, 0, sizeof(Operation->Ov));
                WSABUF BufferDescriptor; 
                BufferDescriptor.buf = const_cast<char*>(Operation->ConstData) + Operation->Transferred; 
                BufferDescriptor.len = static_cast<ULONG>(Operation->DataSize - Operation->Transferred);
                DWORD More = 0; int R = WSASend(Operation->Socket, &BufferDescriptor, 1, &More, 0, &Operation->Ov, nullptr);
                if (R == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING) {
                    done = false;
                } else if (R == 0) {
                    done = false; // 将在下次完成回调汇总
                } else if (R == SOCKET_ERROR) {
                    err = ConvertWinSockError(WSAGetLastError(), true); done = true;
                }
            }
            // 对读：未读满则继续提交，直至缓冲区填满或出错/对端关闭
            if (!Operation->IsWrite && Operation->Transferred < Operation->BufferSize)
            {
                memset(&Operation->Ov, 0, sizeof(Operation->Ov));
                WSABUF BufferDescriptor; 
                BufferDescriptor.buf = Operation->Buffer + Operation->Transferred; 
                BufferDescriptor.len = static_cast<ULONG>(Operation->BufferSize - Operation->Transferred);
                DWORD Flags = 0; DWORD More = 0;
                int R = WSARecv(Operation->Socket, &BufferDescriptor, 1, &More, &Flags, &Operation->Ov, nullptr);
                if (R == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING) {
                    done = false;
                } else if (R == 0) {
                    done = false;
                } else if (R == SOCKET_ERROR) {
                    err = ConvertWinSockError(WSAGetLastError(), false); done = true;
                }
                // 若继续提交，则本次不回调，等待下一次完成
            }
        }
        if (done)
        {
            auto Handler = std::move(Operation->Handler);
            delete Operation;
            if (Handler) 
            {
                Handler(err, ToReport);
            }
        }
    }

    void ProactorIocp::Cancel(Fd Handle)
    {
        SOCKET s = static_cast<SOCKET>(Handle);
        HANDLE h = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(s));
        // Cancel all pending IO on this handle
        CancelIoEx(h, nullptr);
    }
}

#endif // _WIN32


