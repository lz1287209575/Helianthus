#pragma once

#include <functional>
#include <atomic>
#include <memory>
#include <vector>

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

        std::shared_ptr<Reactor> GetReactor() const;

    private:
        std::atomic<bool> Running;
        std::shared_ptr<Reactor> ReactorPtr;
        // simple pending tasks queue (single-producer scenario for now)
        // 为简化，此处实现放在 cpp 内部
    };
}


