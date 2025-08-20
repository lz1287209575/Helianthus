#pragma once

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

    #include "Shared/Network/Asio/Reactor.h"

    #include <unordered_map>

    #include <sys/event.h>

namespace Helianthus::Network::Asio
{
class ReactorKqueue : public Reactor
{
public:
    ReactorKqueue();
    ~ReactorKqueue() override;

    bool Add(Fd Handle, EventMask Mask, IoCallback Callback) override;
    bool Mod(Fd Handle, EventMask Mask) override;
    bool Del(Fd Handle) override;
    int PollOnce(int TimeoutMs) override;

private:
    int KqFd;
    std::unordered_map<Fd, IoCallback> Callbacks;
    std::unordered_map<Fd, EventMask> Masks;
};
}  // namespace Helianthus::Network::Asio

#endif
