#ifndef _WIN32

#include "Shared/Network/Asio/ReactorEpoll.h"
#include <unistd.h>

namespace Helianthus::Network::Asio
{
    static uint32_t ToNative(EventMask m)
    {
        uint32_t ev = 0;
        if ((static_cast<uint32_t>(m) & static_cast<uint32_t>(EventMask::Read)) != 0) ev |= EPOLLIN;
        if ((static_cast<uint32_t>(m) & static_cast<uint32_t>(EventMask::Write)) != 0) ev |= EPOLLOUT;
        if ((static_cast<uint32_t>(m) & static_cast<uint32_t>(EventMask::Error)) != 0) ev |= EPOLLERR;
        return ev;
    }

    ReactorEpoll::ReactorEpoll()
        : EpollFd(epoll_create1(EPOLL_CLOEXEC))
        , Callbacks()
    {
    }

    ReactorEpoll::~ReactorEpoll()
    {
        if (EpollFd >= 0) close(EpollFd);
    }

    bool ReactorEpoll::Add(Fd Handle, EventMask Mask, IoCallback Callback)
    {
        epoll_event ev{};
        ev.events = ToNative(Mask);
        ev.data.fd = Handle;
        if (epoll_ctl(EpollFd, EPOLL_CTL_ADD, Handle, &ev) != 0) return false;
        Callbacks[Handle] = std::move(Callback);
        return true;
    }

    bool ReactorEpoll::Mod(Fd Handle, EventMask Mask)
    {
        epoll_event ev{};
        ev.events = ToNative(Mask);
        ev.data.fd = Handle;
        return epoll_ctl(EpollFd, EPOLL_CTL_MOD, Handle, &ev) == 0;
    }

    bool ReactorEpoll::Del(Fd Handle)
    {
        Callbacks.erase(Handle);
        return epoll_ctl(EpollFd, EPOLL_CTL_DEL, Handle, nullptr) == 0;
    }

    int ReactorEpoll::PollOnce(int TimeoutMs)
    {
        constexpr int MaxEvents = 64;
        epoll_event events[MaxEvents];
        int n = epoll_wait(EpollFd, events, MaxEvents, TimeoutMs);
        if (n <= 0) return n;
        for (int i = 0; i < n; ++i)
        {
            auto fd = static_cast<Fd>(events[i].data.fd);
            auto it = Callbacks.find(fd);
            if (it == Callbacks.end()) continue;
            EventMask mask = EventMask::None;
            if (events[i].events & (EPOLLIN | EPOLLPRI)) mask = mask | EventMask::Read;
            if (events[i].events & EPOLLOUT) mask = mask | EventMask::Write;
            if (events[i].events & (EPOLLERR | EPOLLHUP)) mask = mask | EventMask::Error;
            it->second(mask);
        }
        return n;
    }
}

#endif


