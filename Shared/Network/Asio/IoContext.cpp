#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"
#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/ProactorReactorAdapter.h"
#if defined(_WIN32)
#include "Shared/Network/Asio/ReactorIocp.h"
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include "Shared/Network/Asio/ReactorKqueue.h"
#else
#include "Shared/Network/Asio/ReactorEpoll.h"
#endif

#include <chrono>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <synchapi.h>
#else
#include <sys/select.h>
#endif

namespace Helianthus::Network::Asio
{
    namespace {
        class TaskQueue {
        public:
            void Push(std::function<void()> fn) {
                std::lock_guard<std::mutex> l(m);
                q.push(std::move(fn));
            }
            bool Pop(std::function<void()>& fn) {
                std::lock_guard<std::mutex> l(m);
                if (q.empty()) return false;
                fn = std::move(q.front());
                q.pop();
                return true;
            }
        private:
            std::queue<std::function<void()>> q;
            std::mutex m;
        };
    }

    IoContext::IoContext()
        : Running(false)
        , WakeupFd(-1)
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
    }

    IoContext::~IoContext() 
    {
        CleanupWakeupFd();
    }

    void IoContext::InitializeWakeupFd()
    {
        // 根据平台选择合适的唤醒机制
#if defined(_WIN32)
        // Windows: 使用 IOCP 或事件
        if (CurrentWakeupType == WakeupType::IOCP)
        {
            WakeupIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
            if (WakeupIOCP != INVALID_HANDLE_VALUE && ProactorPtr)
            {
                // 将 IOCP 添加到 Proactor
                // 这里需要根据具体的 Proactor 实现来添加
            }
        }
        else
        {
            // 使用事件对象
            WakeupEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        }
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        // BSD/macOS: 使用管道
        if (CurrentWakeupType == WakeupType::Pipe)
        {
            if (pipe(WakeupPipe) == 0)
            {
                // 设置非阻塞
                fcntl(WakeupPipe[0], F_SETFL, O_NONBLOCK);
                fcntl(WakeupPipe[1], F_SETFL, O_NONBLOCK);
                
                if (ReactorPtr)
                {
                    ReactorPtr->Add(static_cast<Fd>(WakeupPipe[0]), EventMask::Read,
                        [this](EventMask) {
                            // 读取管道来清除事件
                            char buffer[64];
                            while (read(WakeupPipe[0], buffer, sizeof(buffer)) > 0) {
                                // 继续读取直到没有更多数据
                            }
                        });
                }
            }
        }
        else
        {
            // 回退到 eventfd（如果可用）
            WakeupFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (WakeupFd >= 0 && ReactorPtr)
            {
                ReactorPtr->Add(static_cast<Fd>(WakeupFd), EventMask::Read,
                    [this](EventMask) {
                        uint64_t value;
                        while (read(WakeupFd, &value, sizeof(value)) > 0) {
                            // 继续读取直到没有更多数据
                        }
                    });
            }
        }
#else
        // Linux: 使用 eventfd
        if (CurrentWakeupType == WakeupType::EventFd)
        {
            WakeupFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (WakeupFd >= 0 && ReactorPtr)
            {
                ReactorPtr->Add(static_cast<Fd>(WakeupFd), EventMask::Read,
                    [this](EventMask) {
                        uint64_t value;
                        while (read(WakeupFd, &value, sizeof(value)) > 0) {
                            // 继续读取直到没有更多数据
                        }
                    });
            }
        }
        else
        {
            // 回退到管道
            if (pipe(WakeupPipe) == 0)
            {
                fcntl(WakeupPipe[0], F_SETFL, O_NONBLOCK);
                fcntl(WakeupPipe[1], F_SETFL, O_NONBLOCK);
                
                if (ReactorPtr)
                {
                    ReactorPtr->Add(static_cast<Fd>(WakeupPipe[0]), EventMask::Read,
                        [this](EventMask) {
                            char buffer[64];
                            while (read(WakeupPipe[0], buffer, sizeof(buffer)) > 0) {
                                // 继续读取直到没有更多数据
                            }
                        });
                }
            }
        }
#endif
    }

    void IoContext::CleanupWakeupFd()
    {
        // 清理 eventfd
        if (WakeupFd >= 0) {
            if (ReactorPtr) {
                ReactorPtr->Del(static_cast<Fd>(WakeupFd));
            }
            close(WakeupFd);
            WakeupFd = -1;
        }
        
        // 清理管道
        if (WakeupPipe[0] >= 0) {
            if (ReactorPtr) {
                ReactorPtr->Del(static_cast<Fd>(WakeupPipe[0]));
            }
            close(WakeupPipe[0]);
            close(WakeupPipe[1]);
            WakeupPipe[0] = -1;
            WakeupPipe[1] = -1;
        }
        
#ifdef _WIN32
        // 清理 Windows 事件
        if (WakeupEvent != INVALID_HANDLE_VALUE) {
            CloseHandle(WakeupEvent);
            WakeupEvent = INVALID_HANDLE_VALUE;
        }
        
        // 清理 IOCP
        if (WakeupIOCP != INVALID_HANDLE_VALUE) {
            CloseHandle(WakeupIOCP);
            WakeupIOCP = INVALID_HANDLE_VALUE;
        }
#endif
    }

    void IoContext::ProcessTasks()
    {
        if (TaskBatchConfigData.EnableTaskBatching)
        {
            ProcessTaskBatch();
        }
        else
        {
            // 原有的单任务处理逻辑
            std::vector<std::function<void()>> tasks;
            {
                std::lock_guard<std::mutex> lock(TaskQueueMutex);
                while (!TaskQueue.empty()) {
                    tasks.push_back(std::move(TaskQueue.front()));
                    TaskQueue.pop();
                }
            }
            
            for (auto& task : tasks) {
                task();
            }
        }
    }

    void IoContext::ProcessDelayedTasks()
    {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        std::vector<std::function<void()>> readyTasks;
        {
            std::lock_guard<std::mutex> lock(DelayedTaskQueueMutex);
            
            // 移除已到期的任务
            DelayedTaskQueue.erase(
                std::remove_if(DelayedTaskQueue.begin(), DelayedTaskQueue.end(),
                    [&](const DelayedTask& task) {
                        if (task.ExecuteTime <= now) {
                            readyTasks.push_back(task.Task);
                            return true;
                        }
                        return false;
                    }),
                DelayedTaskQueue.end()
            );
        }
        
        // 执行到期的任务
        for (auto& task : readyTasks) {
            task();
        }
    }

    void IoContext::Run()
    {
        Running = true;
        while (Running)
        {
            // 处理普通任务
            ProcessTasks();
            
            // 处理延迟任务
            ProcessDelayedTasks();
            
            // 计算下次延迟任务的时间
            int64_t nextDelay = -1;
            {
                std::lock_guard<std::mutex> lock(DelayedTaskQueueMutex);
                if (!DelayedTaskQueue.empty()) {
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    nextDelay = std::max<int64_t>(0, DelayedTaskQueue.front().ExecuteTime - now);
                }
            }
            
            // 轮询 IO/完成队列
            int timeout = (nextDelay >= 0) ? static_cast<int>(std::min<int64_t>(nextDelay, 10)) : 10;
            
            if (ProactorPtr) {
                ProactorPtr->ProcessCompletions(timeout);
            }
            if (ReactorPtr) {
                ReactorPtr->PollOnce(timeout);
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
            std::lock_guard<std::mutex> lock(TaskQueueMutex);
            TaskQueue.push(std::move(Task));
        }
        
        // 唤醒事件循环
        Wakeup();
    }

    void IoContext::PostDelayed(std::function<void()> Task, int DelayMs)
    {
        auto executeTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() + DelayMs;
        
        {
            std::lock_guard<std::mutex> lock(DelayedTaskQueueMutex);
            DelayedTaskQueue.emplace_back(std::move(Task), executeTime);
            
            // 按执行时间排序
            std::sort(DelayedTaskQueue.begin(), DelayedTaskQueue.end(),
                [](const DelayedTask& a, const DelayedTask& b) {
                    return a.ExecuteTime < b.ExecuteTime;
                });
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

    // 批处理相关方法实现
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
            ProcessTasks();
            
            // 处理延迟任务
            ProcessDelayedTasks();
            
            // 计算下次延迟任务的时间
            int64_t nextDelay = -1;
            {
                std::lock_guard<std::mutex> lock(DelayedTaskQueueMutex);
                if (!DelayedTaskQueue.empty()) {
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    nextDelay = std::max<int64_t>(0, DelayedTaskQueue.front().ExecuteTime - now);
                }
            }
            
            // 使用批处理轮询
            int timeout = (nextDelay >= 0) ? static_cast<int>(std::min<int64_t>(nextDelay, 10)) : 10;
            
            if (ProactorPtr) {
                ProactorPtr->ProcessCompletions(timeout);
            }
            if (ReactorPtr) {
                // 使用批处理轮询
                ReactorPtr->PollBatch(timeout);
            }
        }
    }

    void IoContext::ProcessTaskBatch()
    {
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 计算批处理大小
        size_t BatchSize = CalculateTaskBatchSize();
        
        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard<std::mutex> lock(TaskQueueMutex);
            
            // 收集任务到批处理
            size_t CollectedTasks = 0;
            while (!TaskQueue.empty() && CollectedTasks < BatchSize) {
                tasks.push_back(std::move(TaskQueue.front()));
                TaskQueue.pop();
                CollectedTasks++;
            }
        }
        
        // 批量执行任务
        for (auto& task : tasks) {
            task();
        }
        
        // 更新统计
        if (!tasks.empty()) {
            auto EndTime = std::chrono::high_resolution_clock::now();
            auto ProcessingTime = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
            UpdateTaskBatchStats(tasks.size(), ProcessingTime.count() / 1000.0);
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
        std::lock_guard<std::mutex> lock(TaskBatchConfigMutex);
        
        if (!TaskBatchConfigData.EnableTaskBatching)
        {
            return 1;  // 禁用批处理时返回1
        }
        
        // 基于队列大小和配置计算批处理大小
        std::lock_guard<std::mutex> queueLock(TaskQueueMutex);
        
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

    // 跨线程唤醒方法实现
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
        bool WakeupSuccess = false;
        
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
                WakeupSuccess = true;
            }
        }
        else if (CurrentWakeupType == WakeupType::WakeByAddress)
        {
            // 使用 WakeByAddressSingle
            WakeByAddressSingle(&WakeupStatsData.TotalWakeups);
            WakeupSuccess = true;
        }
        else if (WakeupEvent != INVALID_HANDLE_VALUE)
        {
            // 使用事件对象
            if (SetEvent(WakeupEvent))
            {
                WakeupSuccess = true;
            }
        }
#else
        if (CurrentWakeupType == WakeupType::EventFd && WakeupFd >= 0)
        {
            // 使用 eventfd
            uint64_t value = 1;
            if (write(WakeupFd, &value, sizeof(value)) == sizeof(value))
            {
                WakeupSuccess = true;
            }
        }
        else if (CurrentWakeupType == WakeupType::Pipe && WakeupPipe[1] >= 0)
        {
            // 使用管道
            char value = 1;
            if (write(WakeupPipe[1], &value, sizeof(value)) == sizeof(value))
            {
                WakeupSuccess = true;
            }
        }
#endif
        
        // 计算唤醒延迟
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Latency = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        double LatencyMs = Latency.count() / 1000.0;
        
        UpdateWakeupStats(LatencyMs, false);
    }

    void IoContext::WakeupFromOtherThread()
    {
        Wakeup();
    }

    WakeupStats IoContext::GetWakeupStats() const
    {
        std::lock_guard<std::mutex> lock(WakeupStatsMutex);
        return WakeupStatsData;
    }

    void IoContext::ResetWakeupStats()
    {
        std::lock_guard<std::mutex> lock(WakeupStatsMutex);
        WakeupStatsData = WakeupStats{};
    }

    void IoContext::SetWakeupType(WakeupType Type)
    {
        CurrentWakeupType = Type;
        
        // 重新初始化唤醒机制
        CleanupWakeupFd();
        InitializeWakeupFd();
    }

    WakeupType IoContext::GetWakeupType() const
    {
        return CurrentWakeupType;
    }

    void IoContext::UpdateWakeupStats(double LatencyMs, bool IsSameThread)
    {
        std::lock_guard<std::mutex> lock(WakeupStatsMutex);
        
        WakeupStatsData.TotalWakeups++;
        
        if (IsSameThread)
        {
            WakeupStatsData.SameThreadWakeups++;
        }
        else
        {
            WakeupStatsData.CrossThreadWakeups++;
        }
        
        // 更新平均延迟
        if (WakeupStatsData.TotalWakeups > 0)
        {
            double TotalLatency = WakeupStatsData.AverageWakeupLatencyMs * 
                                 (WakeupStatsData.TotalWakeups - 1);
            WakeupStatsData.AverageWakeupLatencyMs = 
                (TotalLatency + LatencyMs) / WakeupStatsData.TotalWakeups;
        }
        
        // 更新最大延迟
        if (LatencyMs > WakeupStatsData.MaxWakeupLatencyMs)
        {
            WakeupStatsData.MaxWakeupLatencyMs = static_cast<size_t>(LatencyMs);
        }
    }
}


