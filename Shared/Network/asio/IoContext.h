#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <thread>
#include <cstdint>

namespace Helianthus::Network::Asio
{
class Reactor;
class Proactor;

// Wakeup types (mainly for non-Windows). Tests only verify getter/setter roundtrip.
enum class WakeupType
{
    EventFd,
    Pipe,
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

    // Statistics for wakeups observed when tasks are processed in the loop
    struct WakeupStats
    {
        int TotalWakeups;
        int CrossThreadWakeups;
        int SameThreadWakeups;
        double AverageWakeupLatencyMs;
        int MaxWakeupLatencyMs;
    };

    WakeupType GetWakeupType() const;
    void SetWakeupType(WakeupType Type);

    WakeupStats GetWakeupStats() const;
    void ResetWakeupStats();

    // Explicit cross-thread wakeup trigger (no-op on Windows). Used by tests.
    void WakeupFromOtherThread();

    std::shared_ptr<Reactor> GetReactor() const;
    std::shared_ptr<Proactor> GetProactor() const;

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
        std::function<void()> Task;
        int64_t EnqueueTimeNs;
        std::thread::id PostingThreadId;
    };
    std::queue<QueuedTask> TaskQueue;
    mutable std::mutex TaskQueueMutex;

    // 延迟任务队列
    struct DelayedTask
    {
        std::function<void()> Task;
        int64_t ExecuteTime;  // 毫秒时间戳

        DelayedTask(std::function<void()> TaskIn, int64_t ExecuteTimeIn)
            : Task(std::move(TaskIn)), ExecuteTime(ExecuteTimeIn)
        {
        }
    };
    std::vector<DelayedTask> DelayedTaskQueue;
    mutable std::mutex DelayedTaskQueueMutex;

    // 跨线程唤醒机制
    int WakeupFd = -1;

    // Stats
    mutable std::mutex StatsMutex;
    int TotalWakeupsInternal = 0;
    int CrossThreadWakeupsInternal = 0;
    int SameThreadWakeupsInternal = 0;
    double SumWakeupLatencyMsInternal = 0.0;
    int MaxWakeupLatencyMsInternal = 0;

    WakeupType CurrentWakeupType;
};
}  // namespace Helianthus::Network::Asio
