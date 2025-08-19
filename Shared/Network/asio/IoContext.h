#pragma once

#include <functional>
#include <atomic>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <chrono>
#include "Shared/Network/Asio/Proactor.h"

namespace Helianthus::Network::Asio
{
    class Reactor;

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
        // Post a delayed task executed after DelayMs
        void PostDelayed(std::function<void()> Task, int DelayMs);

        std::shared_ptr<Reactor> GetReactor() const;
        std::shared_ptr<Proactor> GetProactor() const;

    private:
        std::atomic<bool> Running;
        std::shared_ptr<Reactor> ReactorPtr;
        std::shared_ptr<Proactor> ProactorPtr;
        // Task queue and timers
        std::mutex QueueMutex;
        std::queue<std::function<void()>> TaskQueue;
        struct ScheduledTask
        {
            std::chrono::steady_clock::time_point Due;
            std::function<void()> Task;
        };
        std::mutex TimerMutex;
        std::vector<ScheduledTask> Timers;
    };
} // namespace Helianthus::Network::Asio
