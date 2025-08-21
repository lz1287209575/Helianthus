#ifndef _WIN32

#include "Shared/Network/Asio/ReactorEpoll.h"
#include "Shared/Network/Asio/ErrorMapping.h"
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <vector>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <iostream>

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
            int addErrno = errno;
            if (addErrno == EEXIST)
            {
                // 已存在则改为修改，合并已有掩码与新掩码，避免覆盖
                uint32_t existingMaskBits = 0;
                {
                    std::lock_guard<std::mutex> gm(RegisteredMasksMutex);
                    auto it = RegisteredMasks.find(Handle);
                    if (it != RegisteredMasks.end()) existingMaskBits = it->second;
                }
                uint32_t newMaskBits = static_cast<uint32_t>(Mask) | existingMaskBits;
                epoll_event ev2{};
                ev2.events = ToNative(static_cast<EventMask>(newMaskBits), EdgeTriggered);
                ev2.data.fd = static_cast<int>(Handle);
                if (epoll_ctl(EpollFd, EPOLL_CTL_MOD, static_cast<int>(Handle), &ev2) != 0)
                {
                    auto Error = ErrorMapping::FromErrno(errno);
                    // TODO: 使用项目的日志系统记录 ErrorMapping::GetErrorString(Error)
                    return false;
                }
                {
                    std::lock_guard<std::mutex> gm(RegisteredMasksMutex);
                    RegisteredMasks[Handle] = newMaskBits;
                }
            }
            else
            {
                auto Error = ErrorMapping::FromErrno(addErrno);
                // TODO: 使用项目的日志系统记录 ErrorMapping::GetErrorString(Error)
                return false;
            }
        }
        
        {
            std::lock_guard<std::mutex> g(CallbacksMutex);
            Callbacks[Handle] = std::move(Callback);
        }
        {
            std::lock_guard<std::mutex> gm(RegisteredMasksMutex);
            auto it = RegisteredMasks.find(Handle);
            uint32_t prev = (it != RegisteredMasks.end()) ? it->second : 0u;
            RegisteredMasks[Handle] = prev | static_cast<uint32_t>(Mask);
        }
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
        {
            std::lock_guard<std::mutex> g(CallbacksMutex);
            Callbacks.erase(Handle);
        }
        {
            std::lock_guard<std::mutex> gm(RegisteredMasksMutex);
            RegisteredMasks.erase(Handle);
        }
        
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
            IoCallback cb;
            EventMask mask = FromNative(events[i].events);
            if (mask == EventMask::None) { continue; }
            {
                std::lock_guard<std::mutex> g(CallbacksMutex);
                auto it = Callbacks.find(static_cast<Fd>(events[i].data.fd));
                if (it == Callbacks.end()) { continue; }
                cb = it->second;
            }
            if (cb) { cb(mask); }
        }
        
        return n;
    }

    // 批处理相关方法实现
    int ReactorEpoll::PollBatch(int TimeoutMs, size_t MaxEvents)
    {
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 计算自适应批处理大小
        size_t BatchSize = CalculateAdaptiveBatchSize();
        BatchSize = std::min(BatchSize, MaxEvents);
        
        std::vector<epoll_event> events(BatchSize);
        
        // 计算实际超时时间
        int ActualTimeout = TimeoutMs;
        if (BatchConfigData.EnableAdaptiveBatching && BatchConfigData.MaxBatchTimeoutMs > 0)
        {
            ActualTimeout = std::min(TimeoutMs, BatchConfigData.MaxBatchTimeoutMs);
        }
        
        int n = epoll_wait(EpollFd, events.data(), static_cast<int>(BatchSize), ActualTimeout);
        
        if (n < 0) 
        {
            if (errno == EINTR) 
            {
                return 0;
            }
            auto Error = ErrorMapping::FromErrno(errno);
            return -1;
        }
        
        if (n == 0) 
        {
            return 0;
        }
        
        // 处理批处理事件
        int ProcessedEvents = ProcessBatchEvents(events, static_cast<size_t>(n));
        
        // 更新性能统计
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto ProcessingTime = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        UpdatePerformanceStats(static_cast<size_t>(n), ProcessingTime.count() / 1000.0);
        
        return ProcessedEvents;
    }

    void ReactorEpoll::SetBatchConfig(const BatchConfig& Config)
    {
        std::lock_guard<std::mutex> lock(BatchConfigMutex);
        BatchConfigData = Config;
    }

    BatchConfig ReactorEpoll::GetBatchConfig() const
    {
        std::lock_guard<std::mutex> lock(BatchConfigMutex);
        return BatchConfigData;
    }

    ReactorEpoll::PerformanceStats ReactorEpoll::GetPerformanceStats() const
    {
        std::lock_guard<std::mutex> lock(StatsMutex);
        return PerformanceStatsData;
    }

    void ReactorEpoll::ResetPerformanceStats()
    {
        std::lock_guard<std::mutex> lock(StatsMutex);
        PerformanceStatsData = PerformanceStats{};
    }

    int ReactorEpoll::ProcessBatchEvents(const std::vector<epoll_event>& Events, size_t Count)
    {
        int ProcessedCount = 0;
        
        // 批量处理事件，减少锁竞争
        std::vector<std::pair<Fd, EventMask>> EventBatch;
        EventBatch.reserve(Count);
        
        // 收集所有事件
        for (size_t i = 0; i < Count; ++i)
        {
            auto fd = static_cast<Fd>(Events[i].data.fd);
            EventMask mask = FromNative(Events[i].events);
            if (mask != EventMask::None)
            {
                EventBatch.emplace_back(fd, mask);
            }
        }
        
        // 批量执行回调
        for (const auto& [fd, mask] : EventBatch)
        {
            auto it = Callbacks.find(fd);
            if (it != Callbacks.end())
            {
                it->second(mask);
                ProcessedCount++;
            }
        }
        
        return ProcessedCount;
    }

    void ReactorEpoll::UpdatePerformanceStats(size_t BatchSize, double ProcessingTimeMs)
    {
        std::lock_guard<std::mutex> lock(StatsMutex);
        
        PerformanceStatsData.TotalEvents += BatchSize;
        PerformanceStatsData.TotalBatches++;
        
        // 更新平均批处理大小
        if (PerformanceStatsData.TotalBatches > 0)
        {
            PerformanceStatsData.AverageBatchSize = 
                PerformanceStatsData.TotalEvents / PerformanceStatsData.TotalBatches;
        }
        
        // 更新平均处理时间
        if (PerformanceStatsData.TotalBatches > 0)
        {
            double TotalTime = PerformanceStatsData.AverageProcessingTimeMs * 
                              (PerformanceStatsData.TotalBatches - 1);
            PerformanceStatsData.AverageProcessingTimeMs = 
                (TotalTime + ProcessingTimeMs) / PerformanceStatsData.TotalBatches;
        }
        
        // 更新最大/最小批处理大小
        if (BatchSize > PerformanceStatsData.MaxBatchSize)
        {
            PerformanceStatsData.MaxBatchSize = BatchSize;
        }
        if (PerformanceStatsData.MinBatchSize == 0 || BatchSize < PerformanceStatsData.MinBatchSize)
        {
            PerformanceStatsData.MinBatchSize = BatchSize;
        }
        
        // 自适应批处理计数
        if (BatchSize >= BatchConfigData.AdaptiveThreshold)
        {
            PerformanceStatsData.AdaptiveBatchCount++;
        }
    }

    size_t ReactorEpoll::CalculateAdaptiveBatchSize() const
    {
        std::lock_guard<std::mutex> lock(BatchConfigMutex);
        
        if (!BatchConfigData.EnableAdaptiveBatching)
        {
            return BatchConfigData.MaxBatchSize;
        }
        
        // 基于性能统计计算自适应批处理大小
        std::lock_guard<std::mutex> statsLock(StatsMutex);
        
        if (PerformanceStatsData.TotalBatches == 0)
        {
            return BatchConfigData.MaxBatchSize;
        }
        
        // 如果平均处理时间较低且批处理大小较大，增加批处理大小
        if (PerformanceStatsData.AverageProcessingTimeMs < 0.1 && 
            PerformanceStatsData.AverageBatchSize > BatchConfigData.AdaptiveThreshold)
        {
            return std::min(BatchConfigData.MaxBatchSize * 2, 
                           static_cast<size_t>(256));
        }
        
        // 如果处理时间较高，减少批处理大小
        if (PerformanceStatsData.AverageProcessingTimeMs > 1.0)
        {
            return std::max(BatchConfigData.MinBatchSize, 
                           BatchConfigData.MaxBatchSize / 2);
        }
        
        return BatchConfigData.MaxBatchSize;
    }
}

#endif


