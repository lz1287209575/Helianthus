#pragma once

#ifndef _WIN32

    #include "Shared/Network/Asio/Reactor.h"

    #include <mutex>
    #include <unordered_map>

    #include <sys/epoll.h>

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
    int PollOnce(int TimeoutMs) override;

    // 批处理相关方法
    int PollBatch(int TimeoutMs, size_t MaxEvents = 64) override;
    void SetBatchConfig(const BatchConfig& Config) override;
    BatchConfig GetBatchConfig() const override;

    // 性能统计
    PerformanceStats GetPerformanceStats() const override;
    void ResetPerformanceStats() override;

    // 配置选项
    void SetEdgeTriggered(bool Enable)
    {
        EdgeTriggered = Enable;
    }
    void SetMaxEvents(int MaxEvents)
    {
        MaxEvents = MaxEvents;
    }

private:
    static uint32_t ToNative(EventMask Mask, bool EdgeTriggered);
    static EventMask FromNative(uint32_t Events);

    int EpollFd;
    std::unordered_map<Fd, IoCallback> Callbacks;
    mutable std::mutex CallbacksMutex;
    // 跟踪每个 fd 的已注册事件掩码（原始 EventMask 按位或）
    std::unordered_map<Fd, uint32_t> RegisteredMasks;
    mutable std::mutex RegisteredMasksMutex;
    bool EdgeTriggered = false;  // 默认电平触发
    int MaxEvents = 64;          // 默认最大事件数

    // 批处理相关
    BatchConfig BatchConfigData;
    mutable std::mutex BatchConfigMutex;

    // 性能统计
    mutable std::mutex StatsMutex;
    PerformanceStats PerformanceStatsData;

    // 批处理辅助方法
    int ProcessBatchEvents(const std::vector<epoll_event>& Events, size_t Count);
    int ProcessBatchEventsOptimized(const std::vector<epoll_event>& Events, size_t Count);
    void UpdatePerformanceStats(size_t BatchSize, double ProcessingTimeMs);
    size_t CalculateAdaptiveBatchSize() const;
};
}  // namespace Helianthus::Network::Asio

#endif
