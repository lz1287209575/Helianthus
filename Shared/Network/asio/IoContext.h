#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace Helianthus::Network::Asio
{
class Reactor;
class Proactor;

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

    // 任务队列
    std::queue<std::function<void()>> TaskQueue;
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
};
}  // namespace Helianthus::Network::Asio
