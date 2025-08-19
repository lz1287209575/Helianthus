#ifndef _WIN32

#include "Shared/Network/Asio/ReactorEpoll.h"
#include <unistd.h>

namespace Helianthus::Network::Asio
{
    static uint32_t ToNative(EventMask Mask)
    {
        uint32_t Native = 0;
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Read)) != 0) 
        {   
            Native |= EPOLLIN;
        }
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Write)) != 0) 
        {
            Native |= EPOLLOUT;
        }
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Error)) != 0) 
        {
            Native |= EPOLLERR;
        }
        return Native;
    }

    ReactorEpoll::ReactorEpoll()
        : EpollFd(epoll_create1(EPOLL_CLOEXEC))
        , Callbacks()
    {
    }

    ReactorEpoll::~ReactorEpoll()
    {
        if (EpollFd >= 0) 
        {
            close(EpollFd);
        }
    }

    bool ReactorEpoll::Add(Fd Handle, EventMask Mask, IoCallback Callback)
    {
        epoll_event Event{};
        Event.events = ToNative(Mask);
        Event.data.fd = Handle;
        if (epoll_ctl(EpollFd, EPOLL_CTL_ADD, Handle, &Event) != 0) 
        {
            return false;
        }
        Callbacks[Handle] = std::move(Callback);
        return true;
    }

    bool ReactorEpoll::Mod(Fd Handle, EventMask Mask)
    {
        epoll_event Event{};
        Event.events = ToNative(Mask);
        Event.data.fd = Handle;
        return epoll_ctl(EpollFd, EPOLL_CTL_MOD, Handle, &Event) == 0;
    }

    bool ReactorEpoll::Del(Fd Handle)
    {
        Callbacks.erase(Handle);
        return epoll_ctl(EpollFd, EPOLL_CTL_DEL, Handle, nullptr) == 0;
    }

    int ReactorEpoll::PollOnce(int TimeoutMs)
    {
        constexpr int MaxEvents = 64;
        epoll_event Events[MaxEvents];
        int Count = epoll_wait(EpollFd, Events, MaxEvents, TimeoutMs);
        if (Count <= 0) return Count;
        for (int I = 0; I < Count; ++I)
        {
            auto Fd = Events[I].data.fd;
            auto It = Callbacks.find(Fd);
            if (It == Callbacks.end()) 
            {
                continue;
            }
            EventMask mask = EventMask::None;
            if (Events[I].events & (EPOLLIN | EPOLLPRI))
            {
                mask = mask | EventMask::Read;
            }
            if (Events[I].events & EPOLLOUT)
            {
                mask = mask | EventMask::Write;
            }
            if (Events[I].events & (EPOLLERR | EPOLLHUP))
            { 
                mask = mask | EventMask::Error;
            }
            It->second(mask);
        }
        return Count;
    }

} // namespace Helianthus::Network::Asio

#endif // _WIN32
