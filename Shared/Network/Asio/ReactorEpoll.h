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

        // 配置选项
        void SetEdgeTriggered(bool Enable) { EdgeTriggered = Enable; }
        void SetMaxEvents(int MaxEvents) { MaxEvents = MaxEvents; }

    private:
        static uint32_t ToNative(EventMask Mask, bool EdgeTriggered);
        static EventMask FromNative(uint32_t Events);

        int EpollFd;
        std::unordered_map<Fd, IoCallback> Callbacks;
        bool EdgeTriggered = false;  // 默认电平触发
        int MaxEvents = 64;          // 默认最大事件数
    };
}

#endif


