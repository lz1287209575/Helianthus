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

    // 批处理相关
    int PollBatch(int TimeoutMs, size_t MaxEvents = 64) override;
    void SetBatchConfig(const BatchConfig& Config) override;
    BatchConfig GetBatchConfig() const override;

    // 性能统计
    Reactor::PerformanceStats GetPerformanceStats() const override;
    void ResetPerformanceStats() override;

private:
    int KqFd;
    std::unordered_map<Fd, IoCallback> Callbacks;
    std::unordered_map<Fd, EventMask> Masks;

    BatchConfig BatchCfg;
    Reactor::PerformanceStats PerfStats;
};
}  // namespace Helianthus::Network::Asio

#endif
