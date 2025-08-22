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
#include <unordered_map>
#include "Common/Logger.h"
#include "Common/LogCategories.h"

using namespace Helianthus::Network::Asio;
using Helianthus::Common::Logger;
using Helianthus::Common::LogVerbosity;

namespace Helianthus::Network::Asio
{
    uint32_t ReactorEpoll::ToNative(EventMask Mask, bool EdgeTriggered)
    {
        uint32_t Ev = 0;
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Read)) != 0) 
        {
            Ev |= EPOLLIN;
        }
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Write)) != 0) 
        {
            Ev |= EPOLLOUT;
        }
        if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Error)) != 0) 
        {
            Ev |= EPOLLERR;
        }
        
        // 边沿触发模式
        if (EdgeTriggered) 
        {
            Ev |= EPOLLET;
        }
        
        return Ev;
    }

    EventMask ReactorEpoll::FromNative(uint32_t Events)
    {
        EventMask Mask = EventMask::None;
        if (Events & (EPOLLIN | EPOLLPRI)) 
        {
            Mask = Mask | EventMask::Read;
        }
        if (Events & EPOLLOUT) 
        {
            Mask = Mask | EventMask::Write;
        }
        if (Events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) 
        {
            Mask = Mask | EventMask::Error;
        }
        return Mask;
    }

    ReactorEpoll::ReactorEpoll()
        : EpollFd(epoll_create1(EPOLL_CLOEXEC))
        , Callbacks()
        , EdgeTriggered(false)
        , MaxEvents(64)
    {
        if (EpollFd < 0) {
            H_LOG(Net, LogVerbosity::Error, "epoll_create1 failed: {}({})", std::strerror(errno), errno);
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
        epoll_event Ev{};
        Ev.events = ToNative(Mask, EdgeTriggered);
        Ev.data.fd = static_cast<int>(Handle);
        
        if (epoll_ctl(EpollFd, EPOLL_CTL_ADD, static_cast<int>(Handle), &Ev) != 0) 
        {
            int AddErrno = errno;
            if (AddErrno == EEXIST)
            {
                // 已存在则改为修改，合并已有掩码与新掩码，避免覆盖
                uint32_t ExistingMaskBits = 0;
                {
                    std::lock_guard<std::mutex> Gm(RegisteredMasksMutex);
                    auto It = RegisteredMasks.find(Handle);
                    if (It != RegisteredMasks.end()) ExistingMaskBits = It->second;
                }
                uint32_t NewMaskBits = static_cast<uint32_t>(Mask) | ExistingMaskBits;
                epoll_event Ev2{};
                Ev2.events = ToNative(static_cast<EventMask>(NewMaskBits), EdgeTriggered);
                Ev2.data.fd = static_cast<int>(Handle);
                if (epoll_ctl(EpollFd, EPOLL_CTL_MOD, static_cast<int>(Handle), &Ev2) != 0)
                {
                    H_LOG(Net, LogVerbosity::Warning, "epoll_ctl MOD failed fd={} mask={} err={}({})", Handle, static_cast<int>(Ev.events), std::strerror(errno), errno);
                    return false;
                }
                {
                    std::lock_guard<std::mutex> Gm(RegisteredMasksMutex);
                    RegisteredMasks[Handle] = NewMaskBits;
                }
            }
            else
            {
                H_LOG(Net, LogVerbosity::Warning, "epoll_ctl ADD retry failed fd={} err={}({})", Handle, std::strerror(AddErrno), AddErrno);
                return false;
            }
        }
        
        {
            std::lock_guard<std::mutex> G(CallbacksMutex);
            Callbacks[Handle] = std::move(Callback);
        }
        {
            std::lock_guard<std::mutex> Gm(RegisteredMasksMutex);
            auto It = RegisteredMasks.find(Handle);
            uint32_t Prev = (It != RegisteredMasks.end()) ? It->second : 0u;
            RegisteredMasks[Handle] = Prev | static_cast<uint32_t>(Mask);
        }
        return true;
    }

    bool ReactorEpoll::Mod(Fd Handle, EventMask Mask)
    {
        epoll_event Ev{};
        Ev.events = ToNative(Mask, EdgeTriggered);
        Ev.data.fd = static_cast<int>(Handle);
        
        if (epoll_ctl(EpollFd, EPOLL_CTL_MOD, static_cast<int>(Handle), &Ev) != 0) 
        {
            H_LOG(Net, LogVerbosity::Warning, "epoll_ctl MOD failed fd={} mask={} err={}({})", Handle, static_cast<int>(Ev.events), std::strerror(errno), errno);
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
            H_LOG(Net, LogVerbosity::Warning, "epoll_ctl DEL failed fd={} err={}({})", Handle, std::strerror(errno), errno);
            return false;
        }
        
        return true;
    }

    int ReactorEpoll::PollOnce(int TimeoutMs)
    {
        std::vector<epoll_event> Events(MaxEvents);
        int N = epoll_wait(EpollFd, Events.data(), MaxEvents, TimeoutMs);
        
        if (N < 0) 
        {
            if (errno == EINTR) 
            {
                // 被信号中断，正常情况
                return 0;
            }
            (void)ErrorMapping::FromErrno(errno);
            H_LOG(Net, LogVerbosity::Warning, "epoll_wait error: {}({})", std::strerror(errno), errno);
            return -1;
        }
        
        if (N == 0) 
        {
            // 超时，没有事件
            return 0;
        }
        
        // 处理事件
        for (int I = 0; I < N; ++I)
        {
            IoCallback Cb;
            EventMask Mask = FromNative(Events[I].events);
            if (Mask == EventMask::None) { continue; }
            {
                std::lock_guard<std::mutex> G(CallbacksMutex);
                auto It = Callbacks.find(static_cast<Fd>(Events[I].data.fd));
                if (It == Callbacks.end()) { continue; }
                Cb = It->second;
            }
            if (Cb) { Cb(Mask); }
        }
        
        return N;
    }

    // 批处理相关方法实现
    int ReactorEpoll::PollBatch(int TimeoutMs, size_t MaxEvents)
    {
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 计算自适应批处理大小
        size_t BatchSize = CalculateAdaptiveBatchSize();
        BatchSize = std::min(BatchSize, MaxEvents);
        
        // 预分配事件缓冲区，避免重复分配
        static thread_local std::vector<epoll_event> EventBuffer;
        if (EventBuffer.size() < BatchSize) {
            EventBuffer.resize(BatchSize);
        }
        
        // 计算实际超时时间
        int ActualTimeout = TimeoutMs;
        if (BatchConfigData.EnableAdaptiveBatching && BatchConfigData.MaxBatchTimeoutMs > 0)
        {
            ActualTimeout = std::min(TimeoutMs, BatchConfigData.MaxBatchTimeoutMs);
        }
        
        int n = epoll_wait(EpollFd, EventBuffer.data(), static_cast<int>(BatchSize), ActualTimeout);
        
        if (n < 0) 
        {
            if (errno == EINTR) 
            {
                return 0;
            }
            (void)ErrorMapping::FromErrno(errno);
            return -1;
        }
        
        if (n == 0) 
        {
            return 0;
        }
        
        // 处理批处理事件（优化版本）
        int ProcessedEvents = ProcessBatchEventsOptimized(EventBuffer, static_cast<size_t>(n));
        
        // 更新性能统计
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto ProcessingTime = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        UpdatePerformanceStats(static_cast<size_t>(n), ProcessingTime.count() / 1000.0);
        
        return ProcessedEvents;
    }

    void ReactorEpoll::SetBatchConfig(const BatchConfig& Config)
    {
        std::lock_guard<std::mutex> Lock(BatchConfigMutex);
        BatchConfigData = Config;
    }

    BatchConfig ReactorEpoll::GetBatchConfig() const
    {
        std::lock_guard<std::mutex> Lock(BatchConfigMutex);
        return BatchConfigData;
    }

    ReactorEpoll::PerformanceStats ReactorEpoll::GetPerformanceStats() const
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        return PerformanceStatsData;
    }

    void ReactorEpoll::ResetPerformanceStats()
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        PerformanceStatsData = PerformanceStats{};
    }

    int ReactorEpoll::ProcessBatchEventsOptimized(const std::vector<epoll_event>& Events, size_t Count)
    {
        int ProcessedCount = 0;
        
        // 使用线程本地存储减少锁竞争
        static thread_local std::vector<std::pair<Fd, EventMask>> EventBatch;
        EventBatch.clear();
        EventBatch.reserve(Count);
        
        // 收集所有事件，进行去抖处理
        std::unordered_map<Fd, EventMask> EventMap;
        for (size_t I = 0; I < Count; ++I)
        {
            auto SocketFd = static_cast<Fd>(Events[I].data.fd);
            EventMask Mask = FromNative(Events[I].events);
            if (Mask != EventMask::None)
            {
                // 事件去抖：合并同一 fd 的多个事件
                auto It = EventMap.find(SocketFd);
                if (It != EventMap.end()) {
                    It->second = static_cast<EventMask>(static_cast<int>(It->second) | static_cast<int>(Mask));
                } else {
                    EventMap[SocketFd] = Mask;
                }
            }
        }
        
        // 转换为批处理格式
        for (const auto& [Fd, Mask] : EventMap) {
            EventBatch.emplace_back(Fd, Mask);
        }
        
        // 批量获取回调，减少锁持有时间
        std::vector<IoCallback> CallbackBatch;
        CallbackBatch.reserve(EventBatch.size());
        
        {
            std::lock_guard<std::mutex> G(CallbacksMutex);
            for (const auto& [Fd, Mask] : EventBatch) {
                auto It = Callbacks.find(Fd);
                if (It != Callbacks.end()) {
                    CallbackBatch.push_back(It->second);
                } else {
                    CallbackBatch.push_back(nullptr);
                }
            }
        }
        
        // 批量执行回调，无锁执行
        for (size_t I = 0; I < EventBatch.size(); ++I) {
            if (CallbackBatch[I]) {
                CallbackBatch[I](EventBatch[I].second);
                ProcessedCount++;
            }
        }
        
        return ProcessedCount;
    }

    int ReactorEpoll::ProcessBatchEvents(const std::vector<epoll_event>& Events, size_t Count)
    {
        // 使用优化版本
        return ProcessBatchEventsOptimized(Events, Count);
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


