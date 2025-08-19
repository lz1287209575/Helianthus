#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"
#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/ProactorReactorAdapter.h"
#ifndef _WIN32
#include "Shared/Network/Asio/ReactorEpoll.h"
#else
#include "Shared/Network/Asio/ReactorIocp.h"
#include "Shared/Network/Asio/ProactorIocp.h"
#endif
#include <queue>
#include <mutex>
#include <algorithm>

namespace Helianthus::Network::Asio
{

    IoContext::IoContext()
        : Running(false)
    {
#ifndef _WIN32
        ReactorPtr = std::make_shared<ReactorEpoll>();
        ProactorPtr = std::make_shared<ProactorReactorAdapter>(ReactorPtr);
#else
        ReactorPtr = std::make_shared<ReactorIocp>();
        ProactorPtr = std::make_shared<ProactorIocp>();
#endif
    }

    IoContext::~IoContext() = default;

    void IoContext::Run()
    {
        Running = true;
        while (Running)
        {
            // 先执行队列任务
            {
                std::queue<std::function<void()>> Local;
                {
                    std::lock_guard<std::mutex> L(QueueMutex);
                    std::swap(Local, TaskQueue);
                }
                while (!Local.empty()) 
                {
                    auto Task = std::move(Local.front()); 
                    Local.pop(); 
                    if (Task) 
                    {
                        Task();
                    }
                }
            }

            // 执行到期的定时任务
            {
                std::vector<ScheduledTask> DueList;
                auto Now = std::chrono::steady_clock::now();
                {
                    std::lock_guard<std::mutex> L(TimerMutex);
                    auto It = std::remove_if(Timers.begin(), Timers.end(), [&](const ScheduledTask& T){
                        if (T.Due <= Now) 
                        {
                            DueList.push_back(T); 
                            return true; 
                        } 
                        return false; 
                    });
                    if (It != Timers.end()) 
                    {
                        Timers.erase(It, Timers.end());
                    }
                }
                for (auto& T : DueList) 
                {
                    if (T.Task) 
                    {
                        T.Task();
                    }
                }
            }

            // 再轮询 IO
            if (ProactorPtr) 
            {
                ProactorPtr->ProcessCompletions(10);
            }

            if (ReactorPtr) 
            {
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
        std::lock_guard<std::mutex> L(QueueMutex);
        TaskQueue.push(std::move(Task));
    }

    std::shared_ptr<Reactor> IoContext::GetReactor() const
    {
        return ReactorPtr;
    }

    std::shared_ptr<Proactor> IoContext::GetProactor() const
    {
        return ProactorPtr;
    }

    void IoContext::PostDelayed(std::function<void()> Task, int DelayMs)
    {
        ScheduledTask T;
        T.Due = std::chrono::steady_clock::now() + std::chrono::milliseconds(DelayMs);
        T.Task = std::move(Task);
        std::lock_guard<std::mutex> L(TimerMutex);
        Timers.push_back(T);
    }
} // namespace Helianthus::Network::Asio

