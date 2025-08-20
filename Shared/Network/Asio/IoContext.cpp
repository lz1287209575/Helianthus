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
#ifndef _WIN32
        // 使用 eventfd 创建唤醒文件描述符
        WakeupFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (WakeupFd >= 0 && ReactorPtr) {
            // 将唤醒文件描述符添加到 Reactor
            ReactorPtr->Add(static_cast<Fd>(WakeupFd), EventMask::Read, 
                [this](EventMask) {
                    // 读取 eventfd 来清除事件
                    uint64_t value;
                    while (read(WakeupFd, &value, sizeof(value)) > 0) {
                        // 继续读取直到没有更多数据
                    }
                });
        }
#endif
    }

    void IoContext::CleanupWakeupFd()
    {
        if (WakeupFd >= 0) {
            if (ReactorPtr) {
                ReactorPtr->Del(static_cast<Fd>(WakeupFd));
            }
            close(WakeupFd);
            WakeupFd = -1;
        }
    }

    void IoContext::ProcessTasks()
    {
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
        if (WakeupFd >= 0) {
            uint64_t value = 1;
            write(WakeupFd, &value, sizeof(value));
        }
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
}


