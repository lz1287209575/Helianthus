#include "Shared/Network/Asio/IoContext.h"

#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/ProactorReactorAdapter.h"
#include "Shared/Network/Asio/Reactor.h"
#if defined(_WIN32)
    #include "Shared/Network/Asio/ProactorIocp.h"
    #include "Shared/Network/Asio/ReactorIocp.h"
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #include "Shared/Network/Asio/ReactorKqueue.h"
#else
    #include "Shared/Network/Asio/ReactorEpoll.h"
#endif

#include <algorithm>
#include <chrono>
#include <vector>
#if !defined(_WIN32)
    #include <errno.h>
    #include <fcntl.h>
    #include <sys/eventfd.h>
    #include <unistd.h>
#endif

namespace Helianthus::Network::Asio
{
IoContext::IoContext() : Running(false), WakeupFd(-1)
{
#if defined(_WIN32)
    ReactorPtr = std::make_shared<ReactorIocp>();
    ProactorPtr = std::make_shared<ProactorIocp>();
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    ReactorPtr = std::make_shared<ReactorKqueue>();
    ProactorPtr = std::make_shared<ProactorReactorAdapter>(ReactorPtr);
#else
    ReactorPtr = std::make_shared<ReactorEpoll>();
    ProactorPtr = std::make_shared<ProactorReactorAdapter>(ReactorPtr);
#endif

    InitializeWakeupFd();

    // Default
    CurrentWakeupType =
#if defined(_WIN32)
        WakeupType::Pipe;
#else
        WakeupType::EventFd;
#endif
}

IoContext::~IoContext()
{
    CleanupWakeupFd();
}

void IoContext::InitializeWakeupFd()
{
#ifndef _WIN32
    // 使用 eventfd 创建唤醒文件描述符
    WakeupFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (WakeupFd >= 0 && ReactorPtr)
    {
        // 将唤醒文件描述符添加到 Reactor
        ReactorPtr->Add(static_cast<Fd>(WakeupFd),
                        EventMask::Read,
                        [this](EventMask)
                        {
                            // 读取 eventfd 来清除事件
                            uint64_t Value;
                            while (read(WakeupFd, &Value, sizeof(Value)) > 0)
                            {
                                // 继续读取直到没有更多数据
                            }
                        });
    }
#endif
}

void IoContext::CleanupWakeupFd()
{
#if !defined(_WIN32)
    if (WakeupFd >= 0)
    {
        if (ReactorPtr)
        {
            ReactorPtr->Del(static_cast<Fd>(WakeupFd));
        }
        close(WakeupFd);
        WakeupFd = -1;
    }
#endif
}

void IoContext::ProcessTasks()
{
    std::vector<QueuedTask> Tasks;
    {
        std::lock_guard<std::mutex> Lock(TaskQueueMutex);
        while (!TaskQueue.empty())
        {
            Tasks.push_back(std::move(TaskQueue.front()));
            TaskQueue.pop();
        }
    }

    for (auto& Node : Tasks)
    {
        // Stats before executing task
        int64_t NowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
        int64_t LatencyNs = NowNs - Node.EnqueueTimeNs;
        int LatencyMs = static_cast<int>(LatencyNs / 1'000'000);
        {
            std::lock_guard<std::mutex> Lock(StatsMutex);
            ++TotalWakeupsInternal;
            if (Node.PostingThreadId == RunningThreadId)
            {
                ++SameThreadWakeupsInternal;
            }
            else
            {
                ++CrossThreadWakeupsInternal;
            }
            SumWakeupLatencyMsInternal += static_cast<double>(LatencyMs);
            if (LatencyMs > MaxWakeupLatencyMsInternal)
            {
                MaxWakeupLatencyMsInternal = LatencyMs;
            }
        }

        Node.Task();
    }
}

void IoContext::ProcessDelayedTasks()
{
    auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();

    std::vector<std::function<void()>> ReadyTasks;
    {
        std::lock_guard<std::mutex> Lock(DelayedTaskQueueMutex);

        // 移除已到期的任务
        DelayedTaskQueue.erase(std::remove_if(DelayedTaskQueue.begin(),
                                              DelayedTaskQueue.end(),
                                              [&](const DelayedTask& task)
                                              {
                                                  if (task.ExecuteTime <= Now)
                                                  {
                                                      ReadyTasks.push_back(task.Task);
                                                      return true;
                                                  }
                                                  return false;
                                              }),
                               DelayedTaskQueue.end());
    }

    // 执行到期的任务
    for (auto& Task : ReadyTasks)
    {
        Task();
    }
}

void IoContext::Run()
{
    Running = true;
    RunningThreadId = std::this_thread::get_id();
    while (Running)
    {
        // 处理普通任务
        ProcessTasks();

        // 处理延迟任务
        ProcessDelayedTasks();

        // 计算下次延迟任务的时间
        int64_t NextDelay = -1;
        {
            std::lock_guard<std::mutex> Lock(DelayedTaskQueueMutex);
            if (!DelayedTaskQueue.empty())
            {
                auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
                NextDelay = std::max<int64_t>(0, DelayedTaskQueue.front().ExecuteTime - Now);
            }
        }

        // 轮询 IO/完成队列
        int Timeout = (NextDelay >= 0) ? static_cast<int>(std::min<int64_t>(NextDelay, 10)) : 10;

        if (ProactorPtr)
        {
            ProactorPtr->ProcessCompletions(Timeout);
        }
        if (ReactorPtr)
        {
            ReactorPtr->PollOnce(Timeout);
        }
    }
}

void IoContext::Stop()
{
    Running = false;
}

void IoContext::Post(std::function<void()> Task)
{
    {
        std::lock_guard<std::mutex> Lock(TaskQueueMutex);
        QueuedTask Node;
        Node.Task = std::move(Task);
        Node.EnqueueTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now().time_since_epoch())
                                 .count();
        Node.PostingThreadId = std::this_thread::get_id();
        TaskQueue.push(std::move(Node));
    }

    // 唤醒事件循环（非 Windows 使用 eventfd）
#if !defined(_WIN32)
    if (WakeupFd >= 0)
    {
        uint64_t Value = 1;
        write(WakeupFd, &Value, sizeof(Value));
    }
#endif
}

void IoContext::PostDelayed(std::function<void()> Task, int DelayMs)
{
    auto ExecuteTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count() +
                       DelayMs;

    {
        std::lock_guard<std::mutex> Lock(DelayedTaskQueueMutex);
        DelayedTaskQueue.emplace_back(std::move(Task), ExecuteTime);

        // 按执行时间排序
        std::sort(DelayedTaskQueue.begin(),
                  DelayedTaskQueue.end(),
                  [](const DelayedTask& A, const DelayedTask& B)
                  { return A.ExecuteTime < B.ExecuteTime; });
    }
}

std::shared_ptr<Reactor> IoContext::GetReactor() const
{
    return ReactorPtr;
}

std::shared_ptr<Proactor> IoContext::GetProactor() const
{
    return ProactorPtr;
}

WakeupType IoContext::GetWakeupType() const
{
    return CurrentWakeupType;
}

void IoContext::SetWakeupType(WakeupType Type)
{
    CurrentWakeupType = Type;
}

IoContext::WakeupStats IoContext::GetWakeupStats() const
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    WakeupStats Out{};
    Out.TotalWakeups = TotalWakeupsInternal;
    Out.CrossThreadWakeups = CrossThreadWakeupsInternal;
    Out.SameThreadWakeups = SameThreadWakeupsInternal;
    Out.AverageWakeupLatencyMs = (TotalWakeupsInternal > 0)
                                     ? (SumWakeupLatencyMsInternal / TotalWakeupsInternal)
                                     : 0.0;
    Out.MaxWakeupLatencyMs = MaxWakeupLatencyMsInternal;
    return Out;
}

void IoContext::ResetWakeupStats()
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    TotalWakeupsInternal = 0;
    CrossThreadWakeupsInternal = 0;
    SameThreadWakeupsInternal = 0;
    SumWakeupLatencyMsInternal = 0.0;
    MaxWakeupLatencyMsInternal = 0;
}

void IoContext::WakeupFromOtherThread()
{
#if !defined(_WIN32)
    if (WakeupFd >= 0)
    {
        uint64_t Value = 1;
        write(WakeupFd, &Value, sizeof(Value));
    }
#endif
}
}  // namespace Helianthus::Network::Asio
