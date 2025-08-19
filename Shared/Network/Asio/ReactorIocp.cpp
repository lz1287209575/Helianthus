#ifdef _WIN32

#include "Shared/Network/Asio/ReactorIocp.h"

namespace Helianthus::Network::Asio
{
    ReactorIocp::ReactorIocp()
        : IocpHandle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0))
        , Callbacks()
        , Masks()
    {
    }

    ReactorIocp::~ReactorIocp()
    {
        if (IocpHandle) CloseHandle(IocpHandle);
    }

    bool ReactorIocp::Add(Fd Handle, EventMask Mask, IoCallback Callback)
    {
        HANDLE h = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(Handle));
        if (!CreateIoCompletionPort(h, IocpHandle, static_cast<ULONG_PTR>(Handle), 0))
            return false;
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
        DWORD bytes = 0; ULONG_PTR key = 0; LPOVERLAPPED ov = nullptr;
        BOOL ok = GetQueuedCompletionStatus(IocpHandle, &bytes, &key, &ov, TimeoutMs >= 0 ? static_cast<DWORD>(TimeoutMs) : INFINITE);
        if (!ok && ov == nullptr) return 0; // timeout or failure without event
        auto it = Callbacks.find(static_cast<int>(key));
        if (it != Callbacks.end())
        {
            // 简化：IOCP 模型下具体事件类型由上层确定，这里统一回调 Read|Write
            it->second(EventMask::Read | EventMask::Write);
            return 1;
        }
        return 0;
    }
}

#endif // _WIN32


