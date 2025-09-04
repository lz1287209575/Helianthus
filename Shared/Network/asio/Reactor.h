#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace Helianthus::Network::Asio
{
using Fd = uintptr_t;

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

// 批处理配置
struct BatchConfig
{
    size_t MaxBatchSize = 64;            // 最大批处理大小
    size_t MinBatchSize = 4;             // 最小批处理大小
    int MaxBatchTimeoutMs = 1;           // 最大批处理超时（毫秒）
    bool EnableAdaptiveBatching = true;  // 启用自适应批处理
    size_t AdaptiveThreshold = 16;       // 自适应阈值
};

class Reactor
{
public:
    virtual ~Reactor() = default;
    virtual bool Add(Fd Handle, EventMask Mask, IoCallback Callback) = 0;
    virtual bool Mod(Fd Handle, EventMask Mask) = 0;
    virtual bool Del(Fd Handle) = 0;
    virtual int PollOnce(int TimeoutMs) = 0;

    // 批处理相关方法
    virtual int PollBatch(int TimeoutMs, size_t MaxEvents = 64) = 0;
    virtual void SetBatchConfig(const BatchConfig& Config) = 0;
    virtual BatchConfig GetBatchConfig() const = 0;

    // 性能统计
    struct PerformanceStats
    {
        size_t TotalEvents = 0;
        size_t TotalBatches = 0;
        size_t AverageBatchSize = 0;
        double AverageProcessingTimeMs = 0.0;
        size_t MaxBatchSize = 0;
        size_t MinBatchSize = 0;
        size_t AdaptiveBatchCount = 0;
    };

    virtual PerformanceStats GetPerformanceStats() const = 0;
    virtual void ResetPerformanceStats() = 0;
};
}  // namespace Helianthus::Network::Asio
