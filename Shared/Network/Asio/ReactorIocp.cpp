#ifdef _WIN32

#include "Shared/Network/Asio/ReactorIocp.h"
#include "Shared/Network/Asio/ErrorMapping.h"

namespace Helianthus::Network::Asio
{
    ReactorIocp::ReactorIocp()
        : IocpHandle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0))
        , Callbacks()
        , Masks()
    {
        if (!IocpHandle)
        {
            auto Error = ErrorMapping::FromWsaError(static_cast<int>(GetLastError()));
            (void)Error; // TODO: 日志
        }
    }

    ReactorIocp::~ReactorIocp()
    {
        if (IocpHandle) CloseHandle(IocpHandle);
    }

    bool ReactorIocp::Add(Fd Handle, EventMask Mask, IoCallback Callback)
    {
        HANDLE NativeHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(Handle));
        if (!CreateIoCompletionPort(NativeHandle, IocpHandle, static_cast<ULONG_PTR>(Handle), 0))
        {
            auto Error = ErrorMapping::FromWsaError(static_cast<int>(GetLastError()));
            (void)Error; // TODO: 日志
            return false;
        }
        Callbacks[Handle] = std::move(Callback);
        Masks[Handle] = Mask;
        return true;
    }

    bool ReactorIocp::Mod(Fd Handle, EventMask Mask)
    {
        Masks[Handle] = Mask;
        return true;
    }

    bool ReactorIocp::Del(Fd Handle)
    {
        Callbacks.erase(Handle);
        Masks.erase(Handle);
        return true;
    }

    int ReactorIocp::PollOnce(int TimeoutMs)
    {
        DWORD Bytes = 0; 
        ULONG_PTR Key = 0; 
        LPOVERLAPPED OverlappedPtr = nullptr;
        bool Ok = GetQueuedCompletionStatus(IocpHandle, &Bytes, &Key, &OverlappedPtr, TimeoutMs >= 0 ? static_cast<DWORD>(TimeoutMs) : INFINITE);
        if (!Ok && OverlappedPtr == nullptr) 
        {
            DWORD Err = GetLastError();
            if (Err == WAIT_TIMEOUT)
            {
                return 0; // 超时
            }
            auto Error = ErrorMapping::FromWsaError(static_cast<int>(Err));
            (void)Error; // TODO: 日志
            return -1; // 真正的失败
        }
        auto It = Callbacks.find(static_cast<Fd>(Key));
        if (It != Callbacks.end())
        {
            // 简化：IOCP 模型下具体事件类型由上层确定，这里统一回调 Read|Write
            It->second(EventMask::Read | EventMask::Write);
            return 1;
        }
        return 0;
    }
} // namespace Helianthus::Network::Asio

#endif // _WIN32


