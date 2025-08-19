#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include "Shared/Network/Asio/Proactor.h"

namespace Helianthus::Network::Asio
{
    enum class EventMask : uint32_t
    {
        None = 0,
        Read = 1 << 0,
        Write = 1 << 1,
        Error = 1 << 2,
    };

    inline EventMask operator|(EventMask A, EventMask B)
    {
        return static_cast<EventMask>(static_cast<uint32_t>(A) | static_cast<uint32_t>(B));
    }

    using IoCallback = std::function<void(EventMask)>;

    class Reactor
    {
    public:
        virtual ~Reactor() = default;
        virtual bool Add(Fd Handle, EventMask Mask, IoCallback Callback) = 0;
        virtual bool Mod(Fd Handle, EventMask Mask) = 0;
        virtual bool Del(Fd Handle) = 0;
        virtual int  PollOnce(int TimeoutMs) = 0;
    };
} // namespace Helianthus::Network::Asio

