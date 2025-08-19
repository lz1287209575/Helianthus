#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

#include "Shared/Network/Asio/ReactorKqueue.h"
#include <unistd.h>

namespace Helianthus::Network::Asio
{
    static int ToFilter(EventMask m)
    {
        // 简化：只注册读/写两类
        return 0;
    }

    ReactorKqueue::ReactorKqueue()
        : KqFd(kqueue())
        , Callbacks()
        , Masks()
    {
    }

    ReactorKqueue::~ReactorKqueue()
    {
        if (KqFd >= 0) close(KqFd);
    }

    bool ReactorKqueue::Add(Fd Handle, EventMask Mask, IoCallback Callback)
    {
        Masks[Handle] = Mask;
        Callbacks[Handle] = std::move(Callback);
        struct kevent changes[2]; int n = 0;
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Read)) != 0)
            EV_SET(&changes[n++], Handle, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Write)) != 0)
            EV_SET(&changes[n++], Handle, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);
        return kevent(KqFd, changes, n, nullptr, 0, nullptr) == 0;
    }

    bool ReactorKqueue::Mod(Fd Handle, EventMask Mask)
    {
        Masks[Handle] = Mask;
        struct timespec ts{0,0};
        struct kevent changes[2]; int n = 0;
        EV_SET(&changes[n++], Handle, EVFILT_READ,  ( (static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Read))  ? EV_ADD : EV_DELETE ), 0, 0, nullptr);
        EV_SET(&changes[n++], Handle, EVFILT_WRITE, ( (static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Write)) ? EV_ADD : EV_DELETE ), 0, 0, nullptr);
        return kevent(KqFd, changes, n, nullptr, 0, &ts) == 0;
    }

    bool ReactorKqueue::Del(Fd Handle)
    {
        Masks.erase(Handle);
        Callbacks.erase(Handle);
        struct timespec ts{0,0};
        struct kevent changes[2]; int n = 0;
        EV_SET(&changes[n++], Handle, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        EV_SET(&changes[n++], Handle, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        return kevent(KqFd, changes, n, nullptr, 0, &ts) == 0;
    }

    int ReactorKqueue::PollOnce(int TimeoutMs)
    {
        struct timespec ts;
        if (TimeoutMs < 0) { ts.tv_sec = 0; ts.tv_nsec = 0; }
        else { ts.tv_sec = TimeoutMs / 1000; ts.tv_nsec = (TimeoutMs % 1000) * 1000000; }
        struct kevent evlist[64];
        int n = kevent(KqFd, nullptr, 0, evlist, 64, &ts);
        if (n <= 0) return n;
        for (int i = 0; i < n; ++i)
        {
            Fd fd = static_cast<Fd>(evlist[i].ident);
            auto it = Callbacks.find(fd);
            if (it == Callbacks.end()) continue;
            EventMask mask = EventMask::None;
            if (evlist[i].filter == EVFILT_READ)  mask = mask | EventMask::Read;
            if (evlist[i].filter == EVFILT_WRITE) mask = mask | EventMask::Write;
            if (evlist[i].flags & EV_ERROR)       mask = mask | EventMask::Error;
            it->second(mask);
        }
        return n;
    }
}

#endif


