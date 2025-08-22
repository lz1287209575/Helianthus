#pragma once

#include <functional>
#include <atomic>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <cstdint>
#include <unordered_set>

#ifdef _WIN32
    // Forward declaration for Windows types
    typedef void* HANDLE;
#endif

namespace Helianthus::Network::Asio
{
    class Reactor;
    class Proactor;

    // 批处理配置
    struct TaskBatchConfig
    {
        size_t MaxTaskBatchSize = 32;     // 最大任务批处理大小
        size_t MinTaskBatchSize = 4;      // 最小任务批处理大小
        int MaxTaskBatchTimeoutMs = 1;    // 最大任务批处理超时
        bool EnableTaskBatching = true;   // 启用任务批处理
    };
    
    // 任务批处理统计
    struct TaskBatchStats
    {
        size_t TotalTasks = 0;
        size_t TotalBatches = 0;
        size_t AverageBatchSize = 0;
        double AverageProcessingTimeMs = 0.0;
        size_t MaxBatchSize = 0;
        size_t MinBatchSize = 0;
    };
    
    // 跨平台唤醒机制
    enum class WakeupType
    {
        EventFd,        // Linux: eventfd
        Pipe,           // BSD/macOS: pipe
        IOCP,           // Windows: IOCP completion
        WakeByAddress   // Windows: WakeByAddressSingle
    };
    
    // 唤醒统计
    struct WakeupStats
    {
        size_t TotalWakeups = 0;
        size_t CrossThreadWakeups = 0;
        size_t SameThreadWakeups = 0;
        double AverageWakeupLatencyMs = 0.0;
        size_t MaxWakeupLatencyMs = 0;
    };

    // Minimal io_context-like executor
    class IoContext
    {
    public:
        IoContext();
        ~IoContext();

        // Run event loop until stopped
        void Run();
        // Stop loop
        void Stop();
        // Post a task to be executed in loop
        void Post(std::function<void()> Task);
        // Post a task with delay (milliseconds)
        void PostDelayed(std::function<void()> Task, int DelayMs);
        
        // 取消和超时语义支持
        using TaskId = uint64_t;
        using CancelToken = std::shared_ptr<std::atomic<bool>>;
        
        // 带取消 token 的任务提交
        TaskId PostWithCancel(std::function<void()> Task, CancelToken Token = nullptr);
        TaskId PostDelayedWithCancel(std::function<void()> Task, int DelayMs, CancelToken Token = nullptr);
        
        // 取消任务
        bool CancelTask(TaskId TaskId);
        
        // 创建取消 token
        CancelToken CreateCancelToken();
        
        // 带超时的异步操作
        template<typename T>
        struct AsyncResult {
            T Result;
            bool IsTimeout = false;
            bool IsCancelled = false;
        };

        std::shared_ptr<Reactor> GetReactor() const;
        std::shared_ptr<Proactor> GetProactor() const;
        
        // 批处理配置方法
        void SetTaskBatchConfig(const TaskBatchConfig& Config);
        TaskBatchConfig GetTaskBatchConfig() const;
        
        // 批处理统计方法
        TaskBatchStats GetTaskBatchStats() const;
        void ResetTaskBatchStats();
        
        // 批处理运行方法
        void RunBatch();
        
        // 跨线程唤醒方法
        void Wakeup();
        void WakeupFromOtherThread();
        
        // 唤醒统计方法
        WakeupStats GetWakeupStats() const;
        void ResetWakeupStats();
        
        // 唤醒配置方法
        void SetWakeupType(WakeupType Type);
        WakeupType GetWakeupType() const;

    private:
        void InitializeWakeupFd();
        void CleanupWakeupFd();
        void ProcessTasks();
        void ProcessDelayedTasks();

        std::atomic<bool> Running;
        std::shared_ptr<Reactor> ReactorPtr;
        std::shared_ptr<Proactor> ProactorPtr;
        
        // Identify the event loop thread
        std::thread::id RunningThreadId;
        
        // 任务队列
        struct QueuedTask
        {
            TaskId Id = 0; // 0 表示不支持取消的普通任务
            std::function<void()> Task;
            int64_t EnqueueTimeNs;
            std::thread::id PostingThreadId;
            CancelToken Token; // 可选取消令牌
        };
        std::queue<QueuedTask> TaskQueue;
        mutable std::mutex TaskQueueMutex;
        
        // 延迟任务队列
        struct DelayedTask {
            TaskId Id;
            std::function<void()> Task;
            int64_t ExecuteTime; // 毫秒时间戳
            CancelToken Token;
            
            DelayedTask(TaskId IdIn, std::function<void()> TaskIn, int64_t ExecuteTimeIn, CancelToken TokenIn = nullptr)
                : Id(IdIn), Task(std::move(TaskIn)), ExecuteTime(ExecuteTimeIn), Token(std::move(TokenIn)) {}
        };
        std::vector<DelayedTask> DelayedTaskQueue;
        mutable std::mutex DelayedTaskQueueMutex;
        
        // 任务 ID 生成器
        std::atomic<TaskId> NextTaskId{1};
        // 普通队列任务的取消集合
        std::unordered_set<TaskId> CancelledTaskIds;
        mutable std::mutex CancelledTaskIdsMutex;
        // 队列中待执行的可取消任务集合
        std::unordered_set<TaskId> PendingTaskIds;
        mutable std::mutex PendingTaskIdsMutex;

        // 下一次 PostWithCancel 默认使用的 Token（兼容旧用法：先 CreateCancelToken，再 PostWithCancel）
        CancelToken NextPostCancelToken;
        mutable std::mutex NextPostCancelTokenMutex;
        
        // 跨线程唤醒机制
        int WakeupFd = -1;
        
        WakeupType CurrentWakeupType = WakeupType::EventFd;
        
        // 管道唤醒（用于 BSD/macOS）
        int WakeupPipe[2] = {-1, -1};
        
        // Windows 唤醒相关
#ifdef _WIN32
        HANDLE WakeupEvent;
        HANDLE WakeupIOCP;
#endif
        
        // Stats
        mutable std::mutex StatsMutex;
        int TotalWakeupsInternal = 0;
        int CrossThreadWakeupsInternal = 0;
        int SameThreadWakeupsInternal = 0;
        double SumWakeupLatencyMsInternal = 0.0;
        int MaxWakeupLatencyMsInternal = 0;
        
        mutable std::mutex WakeupStatsMutex;
        WakeupStats WakeupStatsData;
        
        TaskBatchConfig TaskBatchConfigData;
        mutable std::mutex TaskBatchConfigMutex;
        
        mutable std::mutex TaskBatchStatsMutex;
        TaskBatchStats TaskBatchStatsData;
        
        // 批处理辅助方法
        void ProcessTaskBatch();
        void UpdateTaskBatchStats(size_t BatchSize, double ProcessingTimeMs);
        size_t CalculateTaskBatchSize() const;
        
        // 唤醒辅助方法
        void UpdateWakeupStats(double LatencyMs, bool IsSameThread);
        
        // 超时计算辅助方法
        int CalculateOptimalTimeout(int64_t NextDelay) const;
    };
}


