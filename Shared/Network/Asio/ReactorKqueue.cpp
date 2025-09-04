#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

    #include "Shared/Network/Asio/ReactorKqueue.h"

    #include "Shared/Network/Asio/ErrorMapping.h"

    #include <unistd.h>

namespace Helianthus::Network::Asio
{
static int ToFilter(EventMask m)
{
    // 简化：只注册读/写两类
    return 0;
}

ReactorKqueue::ReactorKqueue() : KqFd(kqueue()), Callbacks(), Masks()
{
    if (KqFd < 0)
    {
        auto Error = ErrorMapping::FromErrno(errno);
        (void)Error;  // TODO: 接入日志系统输出 ErrorMapping::GetErrorString(Error)
    }
}

ReactorKqueue::~ReactorKqueue()
{
    if (KqFd >= 0)
    {
        close(KqFd);
    }
}

bool ReactorKqueue::Add(Fd Handle, EventMask Mask, IoCallback Callback)
{
    Masks[Handle] = Mask;
    Callbacks[Handle] = std::move(Callback);
    struct kevent Changes[2];
    int n = 0;
    if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Read)) != 0)
    {
        EV_SET(&Changes[n++], Handle, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    }
    if ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Write)) != 0)
    {
        EV_SET(&Changes[n++], Handle, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);
    }
    int rc = kevent(KqFd, Changes, n, nullptr, 0, nullptr);
    if (rc != 0)
    {
        auto Error = ErrorMapping::FromErrno(errno);
        (void)Error;  // TODO: 日志
        return false;
    }
    return true;
}

bool ReactorKqueue::Mod(Fd Handle, EventMask Mask)
{
    Masks[Handle] = Mask;
    struct timespec ts
    {
        0, 0
    };
    struct kevent Changes[2];
    int n = 0;
    EV_SET(&Changes[n++],
           Handle,
           EVFILT_READ,
           ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Read)) ? EV_ADD
                                                                                   : EV_DELETE),
           0,
           0,
           nullptr);
    EV_SET(&Changes[n++],
           Handle,
           EVFILT_WRITE,
           ((static_cast<uint32_t>(Mask) & static_cast<uint32_t>(EventMask::Write)) ? EV_ADD
                                                                                    : EV_DELETE),
           0,
           0,
           nullptr);
    int rc = kevent(KqFd, Changes, n, nullptr, 0, &ts);
    if (rc != 0)
    {
        auto Error = ErrorMapping::FromErrno(errno);
        (void)Error;  // TODO: 日志
        return false;
    }
    return true;
}

bool ReactorKqueue::Del(Fd Handle)
{
    Masks.erase(Handle);
    Callbacks.erase(Handle);
    struct timespec ts
    {
        0, 0
    };
    struct kevent Changes[2];
    int n = 0;
    EV_SET(&Changes[n++], Handle, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&Changes[n++], Handle, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    int rc = kevent(KqFd, Changes, n, nullptr, 0, &ts);
    if (rc != 0)
    {
        auto Error = ErrorMapping::FromErrno(errno);
        (void)Error;  // TODO: 日志
        return false;
    }
    return true;
}

int ReactorKqueue::PollOnce(int TimeoutMs)
{
    struct timespec ts;
    if (TimeoutMs < 0)
    {
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
    }
    else
    {
        ts.tv_sec = TimeoutMs / 1000;
        ts.tv_nsec = (TimeoutMs % 1000) * 1000000;
    }
    struct kevent EvList[64];
    int n = kevent(KqFd, nullptr, 0, EvList, 64, &ts);
    if (n <= 0)
    {
        if (n == 0)
        {
            return 0;  // 超时
        }
        // n < 0
        if (errno == EINTR)
        {
            return 0;  // 被信号中断
        }
        auto Error = ErrorMapping::FromErrno(errno);
        (void)Error;  // TODO: 日志
        return -1;
    }
    for (int i = 0; i < n; ++i)
    {
        Fd fd = static_cast<Fd>(EvList[i].ident);
        auto it = Callbacks.find(fd);
        if (it == Callbacks.end())
        {
            continue;
        }
        EventMask mask = EventMask::None;
        if (EvList[i].filter == EVFILT_READ)
        {
            mask = mask | EventMask::Read;
        }
        if (EvList[i].filter == EVFILT_WRITE)
        {
            mask = mask | EventMask::Write;
        }
        if (EvList[i].flags & EV_ERROR)
        {
            mask = mask | EventMask::Error;
        }
        it->second(mask);
    }
    return n;
}

int ReactorKqueue::PollBatch(int TimeoutMs, size_t MaxEvents)
{
    if (MaxEvents == 0)
        MaxEvents = 64;
    struct timespec ts;
    if (TimeoutMs < 0)
    {
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
    }
    else
    {
        ts.tv_sec = TimeoutMs / 1000;
        ts.tv_nsec = (TimeoutMs % 1000) * 1000000;
    }
    std::vector<struct kevent> EvList(MaxEvents);
    auto Start = std::chrono::high_resolution_clock::now();
    int n = kevent(KqFd, nullptr, 0, EvList.data(), static_cast<int>(EvList.size()), &ts);
    auto End = std::chrono::high_resolution_clock::now();
    if (n <= 0)
    {
        if (n == 0 || errno == EINTR)
            return 0;
        return -1;
    }
    size_t BatchCount = 0;
    for (int i = 0; i < n; ++i)
    {
        Fd fd = static_cast<Fd>(EvList[i].ident);
        auto it = Callbacks.find(fd);
        if (it == Callbacks.end())
            continue;
        EventMask mask = EventMask::None;
        if (EvList[i].filter == EVFILT_READ)
            mask = mask | EventMask::Read;
        if (EvList[i].filter == EVFILT_WRITE)
            mask = mask | EventMask::Write;
        if (EvList[i].flags & EV_ERROR)
            mask = mask | EventMask::Error;
        it->second(mask);
        BatchCount++;
    }
    PerfStats.TotalEvents += static_cast<size_t>(n);
    PerfStats.TotalBatches++;
    PerfStats.MaxBatchSize = std::max(PerfStats.MaxBatchSize, static_cast<size_t>(n));
    if (PerfStats.MinBatchSize == 0)
        PerfStats.MinBatchSize = static_cast<size_t>(n);
    else
        PerfStats.MinBatchSize = std::min(PerfStats.MinBatchSize, static_cast<size_t>(n));
    double Ms = std::chrono::duration<double, std::milli>(End - Start).count();
    // 简单EMA
    PerfStats.AverageProcessingTimeMs = PerfStats.AverageProcessingTimeMs * 0.9 + Ms * 0.1;
    PerfStats.AverageBatchSize =
        static_cast<size_t>((PerfStats.AverageBatchSize * 0.9) + (static_cast<double>(n) * 0.1));
    return n;
}

void ReactorKqueue::SetBatchConfig(const BatchConfig& Config)
{
    BatchCfg = Config;
}

BatchConfig ReactorKqueue::GetBatchConfig() const
{
    return BatchCfg;
}

Reactor::PerformanceStats ReactorKqueue::GetPerformanceStats() const
{
    return PerfStats;
}

void ReactorKqueue::ResetPerformanceStats()
{
    PerfStats = PerformanceStats{};
}
}  // namespace Helianthus::Network::Asio

#endif
