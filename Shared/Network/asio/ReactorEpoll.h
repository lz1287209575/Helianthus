#pragma once

#ifndef _WIN32

#include "Shared/Network/Asio/Reactor.h"
#include <sys/epoll.h>
#include <unordered_map>

namespace Helianthus::Network::Asio
{
    class ReactorEpoll : public Reactor
    {
    public:
        ReactorEpoll();
        ~ReactorEpoll() override;

        bool Add(Fd Handle, EventMask Mask, IoCallback Callback) override;
        bool Mod(Fd Handle, EventMask Mask) override;
        bool Del(Fd Handle) override;
        int  PollOnce(int TimeoutMs) override;

    private:
        int EpollFd;
        std::unordered_map<Fd, IoCallback> Callbacks;
    };
}

#endif


