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
#include <queue>
#include <mutex>

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
    }

    IoContext::~IoContext() = default;

    void IoContext::Run()
    {
        Running = true;
        static TaskQueue queue; // 简化：示例级别，可后续放成员
        while (Running)
        {
            // 先执行队列任务
            std::function<void()> task;
            while (queue.Pop(task)) {
                task();
            }

            // 再轮询 IO/完成队列
            if (ProactorPtr) {
                ProactorPtr->ProcessCompletions(10);
            }
            if (ReactorPtr) {
                ReactorPtr->PollOnce(10);
            }
        }
    }

    void IoContext::Stop()
    {
        Running = false;
    }

    void IoContext::Post(std::function<void()> Task)
    {
        // 简化：静态队列占位实现，后续可以替换成成员队列
        static TaskQueue queue;
        queue.Push(std::move(Task));
    }

    std::shared_ptr<Reactor> IoContext::GetReactor() const
    {
        return ReactorPtr;
    }

    std::shared_ptr<Proactor> IoContext::GetProactor() const
    {
        return ProactorPtr;
    }
}


