#include "Shared/Network/Asio/IoContext.h"
#include "Common/LogCategories.h"
#include "Common/Logger.h"
#include "Common/LogCategory.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

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

using namespace Helianthus::Network::Asio;
using Helianthus::Common::LogVerbosity;

namespace Helianthus::Network::Asio
{
IoContext::IoContext() : Running(false), WakeupFd(-1)
#if defined(_WIN32)
    , WakeupEvent(INVALID_HANDLE_VALUE), WakeupIOCP(INVALID_HANDLE_VALUE)
#endif
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
    // 保留原有实现作为兼容性接口
    ProcessTaskBatch();
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
                                              [&](const DelayedTask& Task)
                                              {
                                                  if (Task.ExecuteTime <= Now)
                                                  {
                                                      // 检查是否被取消
                                                      if (Task.Token && Task.Token->load()) {
                                                          return true; // 被取消，移除任务
                                                      }
                                                      ReadyTasks.push_back(Task.Task);
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
    
    // 配置 Reactor 批处理
    if (ReactorPtr)
    {
        BatchConfig Config;
        Config.MaxBatchSize = 64;
        Config.MinBatchSize = 4;
        Config.MaxBatchTimeoutMs = 1;
        Config.EnableAdaptiveBatching = true;
        Config.AdaptiveThreshold = 16;
        ReactorPtr->SetBatchConfig(Config);
    }
    
        while (Running)
    {
        // 处理普通任务（批处理）
        ProcessTaskBatch();

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

        // 智能超时计算：根据任务队列状态和延迟任务调整超时
        int Timeout = CalculateOptimalTimeout(NextDelay);
        /* run loop timeout trace suppressed */
        
        // 使用批处理轮询，减少上下文切换
        if (ProactorPtr)
        {
            // 限制单次处理时间，避免长时间阻塞
            int ProactorTimeout = std::min(Timeout, 100); // 最多100ms
            ProactorPtr->ProcessCompletions(ProactorTimeout);
            // 检查是否在 ProcessCompletions 中收到了停止信号
            if (!Running)
            {
                break;
            }
        }
        if (ReactorPtr)
        {
            // 使用批处理轮询，提高吞吐量
            int ReactorTimeout = std::min(Timeout, 100); // 最多100ms
            ReactorPtr->PollBatch(ReactorTimeout, 64);
        }
        
        // 再次检查运行状态，确保能及时响应停止请求
        if (!Running)
        {
            break;
        }
    }
    }

    void IoContext::Stop()
{
    Running = false;
    
    // 在 Windows 上使用 IOCP 唤醒机制确保立即停止
#if defined(_WIN32)
    if (ProactorPtr)
    {
        ProactorPtr->Stop();
    }
    else
    {
        // 如果没有 Proactor，使用其他唤醒机制
        Wakeup();
    }
#else
    // 非 Windows 平台使用文件描述符唤醒
    if (WakeupFd >= 0)
    {
        uint64_t Value = 1;
        write(WakeupFd, &Value, sizeof(Value));
    }
#endif
}

    void IoContext::Post(std::function<void()> Task)
    {
    auto PostTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    auto PostingThreadId = std::this_thread::get_id();
    
    {
        std::lock_guard<std::mutex> Lock(TaskQueueMutex);
        QueuedTask Node;
        Node.Id = 0; // 不可取消的普通任务
        Node.Task = std::move(Task);
        Node.EnqueueTimeNs = PostTimeNs;
        Node.PostingThreadId = PostingThreadId;
        TaskQueue.push(std::move(Node));
    }

    // 更新唤醒统计
    // 注意：唤醒统计在 ProcessTaskBatch 中计算，避免重复统计
    // bool IsCrossThread = (PostingThreadId != RunningThreadId) || (RunningThreadId == std::thread::id{});
    // UpdateWakeupStats(0.0, !IsCrossThread); // 延迟将在任务执行时计算

    // 唤醒事件循环
#if defined(_WIN32)
    if (ProactorPtr)
    {
        ProactorPtr->Wakeup();
    }
#else
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

    bool ShouldWakeup = false;
    {
        std::lock_guard<std::mutex> Lock(DelayedTaskQueueMutex);
        DelayedTaskQueue.emplace_back(NextTaskId.fetch_add(1), std::move(Task), ExecuteTime);

        // 按执行时间排序
        std::sort(DelayedTaskQueue.begin(),
                  DelayedTaskQueue.end(),
                  [](const DelayedTask& A, const DelayedTask& B)
                  { return A.ExecuteTime < B.ExecuteTime; });
        
        // 如果这个任务是新的最早任务，需要唤醒事件循环
        if (!DelayedTaskQueue.empty() && DelayedTaskQueue.front().ExecuteTime == ExecuteTime)
        {
            ShouldWakeup = true;
        }
    }
    
    // 如果需要唤醒，则唤醒事件循环
    if (ShouldWakeup)
    {
#if defined(_WIN32)
        if (ProactorPtr)
        {
            ProactorPtr->Wakeup();
        }
#else
        if (WakeupFd >= 0)
        {
            uint64_t Value = 1;
            write(WakeupFd, &Value, sizeof(Value));
        }
#endif
    }
}

IoContext::TaskId IoContext::PostWithCancel(std::function<void()> Task, CancelToken Token)
{
    TaskId Id = NextTaskId.fetch_add(1);
    
    auto PostTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    auto PostingThreadId = std::this_thread::get_id();
    
    {
        std::lock_guard<std::mutex> Lock(TaskQueueMutex);
        QueuedTask Node;
        Node.Id = Id;
        // 若未显式传入 Token，则使用最近创建的 Token（兼容旧用法）
        if (!Token) {
            std::lock_guard<std::mutex> TokLock(NextPostCancelTokenMutex);
            Node.Token = NextPostCancelToken;
        } else {
            Node.Token = Token;
        }
        Node.Task = std::move(Task);
        Node.EnqueueTimeNs = PostTimeNs;
        Node.PostingThreadId = PostingThreadId;
        TaskQueue.push(std::move(Node));
    }

    // 记录待执行任务Id
    if (Id != 0) {
        std::lock_guard<std::mutex> PendingLock(PendingTaskIdsMutex);
        PendingTaskIds.insert(Id);
    }

    // 更新唤醒统计
    bool IsCrossThread = (PostingThreadId != RunningThreadId) || (RunningThreadId == std::thread::id{});
    UpdateWakeupStats(0.0, !IsCrossThread);

    // 唤醒事件循环
#if !defined(_WIN32)
    if (WakeupFd >= 0)
    {
        uint64_t Value = 1;
        write(WakeupFd, &Value, sizeof(Value));
    }
#endif

    return Id;
}

IoContext::TaskId IoContext::PostDelayedWithCancel(std::function<void()> Task, int DelayMs, CancelToken Token)
{
    TaskId Id = NextTaskId.fetch_add(1);
    auto ExecuteTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count() +
                       DelayMs;

    bool ShouldWakeup = false;
    {
        std::lock_guard<std::mutex> Lock(DelayedTaskQueueMutex);
        DelayedTaskQueue.emplace_back(Id, std::move(Task), ExecuteTime, Token);

        // 按执行时间排序
        std::sort(DelayedTaskQueue.begin(),
                  DelayedTaskQueue.end(),
                  [](const DelayedTask& A, const DelayedTask& B)
                  { return A.ExecuteTime < B.ExecuteTime; });
        
        // 如果这个任务是新的最早任务，需要唤醒事件循环
        if (!DelayedTaskQueue.empty() && DelayedTaskQueue.front().ExecuteTime == ExecuteTime)
        {
            ShouldWakeup = true;
        }
    }
    
    // 如果需要唤醒，则唤醒事件循环
    if (ShouldWakeup)
    {
#if defined(_WIN32)
        if (ProactorPtr)
        {
            ProactorPtr->Wakeup();
        }
#else
        if (WakeupFd >= 0)
        {
            uint64_t Value = 1;
            write(WakeupFd, &Value, sizeof(Value));
        }
#endif
    }
    
    return Id;
}

bool IoContext::CancelTask(TaskId TaskId)
{
    // 标记普通队列任务为取消
    {
        std::lock_guard<std::mutex> CancelLock(CancelledTaskIdsMutex);
        CancelledTaskIds.insert(TaskId);
    }

    // 检查延迟任务队列
    {
        std::lock_guard<std::mutex> Lock(DelayedTaskQueueMutex);
        auto It = std::find_if(DelayedTaskQueue.begin(), DelayedTaskQueue.end(),
                              [TaskId](const DelayedTask& Task) { return Task.Id == TaskId; });
        if (It != DelayedTaskQueue.end()) {
            DelayedTaskQueue.erase(It);
            // 同时从待执行集合移除
            std::lock_guard<std::mutex> PendingLock(PendingTaskIdsMutex);
            PendingTaskIds.erase(TaskId);
            return true;
        }
    }

    // 如果既不在延迟队列也未记录为待执行，则返回 false
    {
        std::lock_guard<std::mutex> PendingLock(PendingTaskIdsMutex);
        return PendingTaskIds.find(TaskId) != PendingTaskIds.end();
    }
}

IoContext::CancelToken IoContext::CreateCancelToken()
{
    auto Token = std::make_shared<std::atomic<bool>>(false);
    // 作为下一次 PostWithCancel 的默认 Token 供兼容测试用例
    {
        std::lock_guard<std::mutex> TokLock(NextPostCancelTokenMutex);
        NextPostCancelToken = Token;
    }
    return Token;
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

WakeupStats IoContext::GetWakeupStats() const
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

// 批处理配置方法实现
void IoContext::SetTaskBatchConfig(const TaskBatchConfig& Config)
{
    std::lock_guard<std::mutex> lock(TaskBatchConfigMutex);
    TaskBatchConfigData = Config;
}

TaskBatchConfig IoContext::GetTaskBatchConfig() const
{
    std::lock_guard<std::mutex> lock(TaskBatchConfigMutex);
    return TaskBatchConfigData;
}

TaskBatchStats IoContext::GetTaskBatchStats() const
{
    std::lock_guard<std::mutex> lock(TaskBatchStatsMutex);
    return TaskBatchStatsData;
}

void IoContext::ResetTaskBatchStats()
{
    std::lock_guard<std::mutex> lock(TaskBatchStatsMutex);
    TaskBatchStatsData = TaskBatchStats{};
}

void IoContext::RunBatch()
{
    Running = true;
    while (Running)
    {
        // 处理任务批处理
        ProcessTaskBatch();
        
        // 处理延迟任务
        ProcessDelayedTasks();
        
        // 计算下次延迟任务的时间
        int64_t NextDelay = -1;
        {
            std::lock_guard<std::mutex> Lock(DelayedTaskQueueMutex);
            if (!DelayedTaskQueue.empty()) {
                auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                NextDelay = std::max<int64_t>(0, DelayedTaskQueue.front().ExecuteTime - Now);
            }
        }
        
        // 使用批处理轮询
        int Timeout = (NextDelay >= 0) ? static_cast<int>(std::min<int64_t>(NextDelay, 10)) : 10;
        
        if (ProactorPtr) {
            ProactorPtr->ProcessCompletions(Timeout);
        }
        if (ReactorPtr) {
            // 使用批处理轮询
            ReactorPtr->PollBatch(Timeout);
        }
    }
}

void IoContext::ProcessTaskBatch()
{
    auto StartTime = std::chrono::high_resolution_clock::now();
    
    // 计算批处理大小
    size_t BatchSize = CalculateTaskBatchSize();
    
    std::vector<std::function<void()>> Tasks;
    {
        std::lock_guard<std::mutex> Lock(TaskQueueMutex);
        
        // 收集任务到批处理，同时记录延迟信息
        size_t CollectedTasks = 0;
        while (!TaskQueue.empty() && CollectedTasks < BatchSize) {
            auto& Node = TaskQueue.front();
            
            // 计算延迟
            auto NowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
            int64_t LatencyNs = NowNs - Node.EnqueueTimeNs;
            int LatencyMs = static_cast<int>(LatencyNs / 1'000'000);
            
            // 更新唤醒延迟统计
            bool IsSameThread = (Node.PostingThreadId == RunningThreadId);
            UpdateWakeupStats(static_cast<double>(LatencyMs), IsSameThread);

            bool ShouldSkip = false;
            if (Node.Token && Node.Token->load()) {
                ShouldSkip = true;
            }
            if (Node.Id != 0) {
                std::lock_guard<std::mutex> CancelLock(CancelledTaskIdsMutex);
                if (CancelledTaskIds.find(Node.Id) != CancelledTaskIds.end()) {
                    ShouldSkip = true;
                }
            }

            if (!ShouldSkip) {
                Tasks.push_back(std::move(Node.Task));
                CollectedTasks++;
            }
            // 出队后从待执行集合移除
            if (Node.Id != 0) {
                std::lock_guard<std::mutex> PendingLock(PendingTaskIdsMutex);
                PendingTaskIds.erase(Node.Id);
            }
            TaskQueue.pop();
        }
    }
    
    // 批量执行任务
    for (auto& Task : Tasks) {
        Task();
    }
    
    // 更新批处理统计
    if (!Tasks.empty()) {
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto ProcessingTime = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        UpdateTaskBatchStats(Tasks.size(), ProcessingTime.count() / 1000.0);
    }
}

void IoContext::UpdateTaskBatchStats(size_t BatchSize, double ProcessingTimeMs)
{
    std::lock_guard<std::mutex> lock(TaskBatchStatsMutex);
    
    TaskBatchStatsData.TotalTasks += BatchSize;
    TaskBatchStatsData.TotalBatches++;
    
    // 更新平均批处理大小
    if (TaskBatchStatsData.TotalBatches > 0)
    {
        TaskBatchStatsData.AverageBatchSize = 
            TaskBatchStatsData.TotalTasks / TaskBatchStatsData.TotalBatches;
    }
    
    // 更新平均处理时间
    if (TaskBatchStatsData.TotalBatches > 0)
    {
        double TotalTime = TaskBatchStatsData.AverageProcessingTimeMs * 
                          (TaskBatchStatsData.TotalBatches - 1);
        TaskBatchStatsData.AverageProcessingTimeMs = 
            (TotalTime + ProcessingTimeMs) / TaskBatchStatsData.TotalBatches;
    }
    
    // 更新最大/最小批处理大小
    if (BatchSize > TaskBatchStatsData.MaxBatchSize)
    {
        TaskBatchStatsData.MaxBatchSize = BatchSize;
    }
    if (TaskBatchStatsData.MinBatchSize == 0 || BatchSize < TaskBatchStatsData.MinBatchSize)
    {
        TaskBatchStatsData.MinBatchSize = BatchSize;
    }
}

size_t IoContext::CalculateTaskBatchSize() const
{
    std::lock_guard<std::mutex> Lock(TaskBatchConfigMutex);
    
    if (!TaskBatchConfigData.EnableTaskBatching)
    {
        return 1;  // 禁用批处理时返回1
    }
    
    // 基于队列大小和配置计算批处理大小
    std::lock_guard<std::mutex> QueueLock(TaskQueueMutex);
    
    size_t QueueSize = TaskQueue.size();
    
    // 如果队列很大，增加批处理大小
    if (QueueSize > TaskBatchConfigData.MaxTaskBatchSize * 2)
    {
        return std::min(TaskBatchConfigData.MaxTaskBatchSize * 2, 
                       static_cast<size_t>(128));
    }
    
    // 如果队列较小，使用较小的批处理大小
    if (QueueSize < TaskBatchConfigData.MinTaskBatchSize)
    {
        return std::min(QueueSize, TaskBatchConfigData.MinTaskBatchSize);
    }
    
    return std::min(QueueSize, TaskBatchConfigData.MaxTaskBatchSize);
}

void IoContext::Wakeup()
{
    auto StartTime = std::chrono::high_resolution_clock::now();
    
    // 检查是否在同一线程
    bool IsSameThread = false;
    // TODO: 实现线程ID检查
    
    if (IsSameThread)
    {
        // 同线程唤醒，无需实际唤醒
        UpdateWakeupStats(0.0, true);
        return;
    }
    
    // 跨线程唤醒
    /* wakeup processing begin */
    
#if defined(_WIN32)
    if (CurrentWakeupType == WakeupType::IOCP && WakeupIOCP != INVALID_HANDLE_VALUE)
    {
        // 使用 IOCP 唤醒
        DWORD BytesTransferred = 0;
        ULONG_PTR CompletionKey = 0;
        LPOVERLAPPED Overlapped = nullptr;
        
        // 投递一个空完成包
        if (PostQueuedCompletionStatus(WakeupIOCP, 0, 0, nullptr))
        {
            /* Wakeup event consumed */
        }
    }
    else if (CurrentWakeupType == WakeupType::WakeByAddress)
    {
        // 使用 WakeByAddressSingle
        WakeByAddressSingle(&WakeupStatsData.TotalWakeups);
        /* Wakeup event consumed */
    }
    else if (WakeupEvent != INVALID_HANDLE_VALUE)
    {
        // 使用事件对象
        if (SetEvent(WakeupEvent))
        {
            /* Wakeup event consumed */
        }
    }
#else
    if (CurrentWakeupType == WakeupType::EventFd && WakeupFd >= 0)
    {
        // 使用 eventfd
        uint64_t Value = 1;
        if (write(WakeupFd, &Value, sizeof(Value)) == sizeof(Value))
        {
            /* Wakeup event consumed */
        }
    }
    else if (CurrentWakeupType == WakeupType::Pipe && WakeupPipe[1] >= 0)
    {
        // 使用管道
        char Value = 1;
        if (write(WakeupPipe[1], &Value, sizeof(Value)) == sizeof(Value))
        {
            /* Wakeup event consumed */
        }
    }
#endif
    
    // 计算唤醒延迟
    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Latency = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
    double LatencyMs = Latency.count() / 1000.0;
    
    UpdateWakeupStats(LatencyMs, false);
}

void IoContext::UpdateWakeupStats(double LatencyMs, bool IsSameThread)
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    
    TotalWakeupsInternal++;
    
    if (IsSameThread)
    {
        SameThreadWakeupsInternal++;
    }
    else
    {
        CrossThreadWakeupsInternal++;
    }
    
    // 更新延迟统计
    SumWakeupLatencyMsInternal += LatencyMs;
    if (static_cast<int>(LatencyMs) > MaxWakeupLatencyMsInternal)
    {
        MaxWakeupLatencyMsInternal = static_cast<int>(LatencyMs);
    }
}

int IoContext::CalculateOptimalTimeout(int64_t NextDelay) const
{
    // 基础超时：10ms
    int BaseTimeout = 10;
    
    // 如果有延迟任务，使用较小的超时
    if (NextDelay >= 0) {
        return static_cast<int>(std::min<int64_t>(NextDelay, BaseTimeout));
    }
    
    // 检查任务队列状态，如果队列为空，可以增加超时以减少 CPU 使用
    {
        std::lock_guard<std::mutex> Lock(TaskQueueMutex);
        if (TaskQueue.empty()) {
            // 队列为空时，使用较长的超时以减少空转
            return std::min(BaseTimeout * 2, 50); // 最大 50ms
        }
    }
    
    return BaseTimeout;
}
}  // namespace Helianthus::Network::Asio
