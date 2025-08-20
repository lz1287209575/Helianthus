#ifndef _WIN32

#include "Shared/Network/Asio/ReactorEpoll.h"
#include "Shared/Network/Asio/ErrorMapping.h"
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <vector>

namespace Helianthus::Network::Asio
{
    uint32_t ReactorEpoll::ToNative(EventMask Mask, bool EdgeTriggered)
    {
        uint32_t ev = 0;
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Read)) != 0) 
        {
            ev |= EPOLLIN;
        }
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Write)) != 0) 
        {
            ev |= EPOLLOUT;
        }
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Error)) != 0) 
        {
            ev |= EPOLLERR;
        }
        
        // 边沿触发模式
        if (EdgeTriggered) 
        {
            ev |= EPOLLET;
        }
        
        return ev;
    }

    EventMask ReactorEpoll::FromNative(uint32_t Events)
    {
        EventMask mask = EventMask::None;
        if (Events & (EPOLLIN | EPOLLPRI)) 
        {
            mask = mask | EventMask::Read;
        }
        if (Events & EPOLLOUT) 
        {
            mask = mask | EventMask::Write;
        }
        if (Events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) 
        {
            mask = mask | EventMask::Error;
        }
        return mask;
    }

    ReactorEpoll::ReactorEpoll()
        : EpollFd(epoll_create1(EPOLL_CLOEXEC))
        , Callbacks()
        , EdgeTriggered(false)
        , MaxEvents(64)
    {
        if (EpollFd < 0) {
            auto Error = ErrorMapping::FromErrno(errno);
            // TODO: 使用项目的日志系统记录 ErrorMapping::GetErrorString(Error)
        }
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
        epoll_event ev{};
        ev.events = ToNative(Mask, EdgeTriggered);
        ev.data.fd = static_cast<int>(Handle);
        
        if (epoll_ctl(EpollFd, EPOLL_CTL_ADD, static_cast<int>(Handle), &ev) != 0) 
        {
            auto Error = ErrorMapping::FromErrno(errno);
            // TODO: 使用项目的日志系统记录 ErrorMapping::GetErrorString(Error)
            return false;
        }
        
        Callbacks[Handle] = std::move(Callback);
        return true;
    }

    bool ReactorEpoll::Mod(Fd Handle, EventMask Mask)
    {
        epoll_event ev{};
        ev.events = ToNative(Mask, EdgeTriggered);
        ev.data.fd = static_cast<int>(Handle);
        
        if (epoll_ctl(EpollFd, EPOLL_CTL_MOD, static_cast<int>(Handle), &ev) != 0) 
        {
            auto Error = ErrorMapping::FromErrno(errno);
            // TODO: 使用项目的日志系统记录 ErrorMapping::GetErrorString(Error)
            return false;
        }
        
        return true;
    }

    bool ReactorEpoll::Del(Fd Handle)
    {
        Callbacks.erase(Handle);
        
        if (epoll_ctl(EpollFd, EPOLL_CTL_DEL, static_cast<int>(Handle), nullptr) != 0) 
        {
            auto Error = ErrorMapping::FromErrno(errno);
            // TODO: 使用项目的日志系统记录 ErrorMapping::GetErrorString(Error)
            return false;
        }
        
        return true;
    }

    int ReactorEpoll::PollOnce(int TimeoutMs)
    {
        std::vector<epoll_event> events(MaxEvents);
        int n = epoll_wait(EpollFd, events.data(), MaxEvents, TimeoutMs);
        
        if (n < 0) 
        {
            if (errno == EINTR) 
            {
                // 被信号中断，正常情况
                return 0;
            }
            auto Error = ErrorMapping::FromErrno(errno);
            // TODO: 使用项目的日志系统记录 ErrorMapping::GetErrorString(Error)
            return -1;
        }
        
        if (n == 0) 
        {
            // 超时，没有事件
            return 0;
        }
        
        // 处理事件
        for (int i = 0; i < n; ++i)
        {
            auto fd = static_cast<Fd>(events[i].data.fd);
            auto it = Callbacks.find(fd);
            if (it == Callbacks.end()) 
            {
                continue;
            }
            
            EventMask mask = FromNative(events[i].events);
            if (mask != EventMask::None) 
            {
                it->second(mask);
            }
        }
        
        return n;
    }
}

#endif


