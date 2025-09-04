#include "Shared/Network/Asio/IoContext.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace Helianthus::Network::Asio;

// 简化版演示：跨线程提交任务并由 IoContext 处理
void DemoWakeupBasics()
{
    std::cout << "=== 跨线程任务唤醒演示（简化版） ===" << std::endl;

    auto Context = std::make_shared<IoContext>();

    // 启动事件循环线程
    std::thread RunThread([Context]() { Context->Run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::atomic<int> TaskCounter = 0;
    const int NumThreads = 4;
    const int TasksPerThread = 100;

    std::cout << "启动 " << NumThreads << " 个线程，每个提交 " << TasksPerThread << " 个任务..."
              << std::endl;

    auto StartTime = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> PostThreads;
    for (int i = 0; i < NumThreads; ++i)
    {
        PostThreads.emplace_back(
            [Context, &TaskCounter, TasksPerThread]()
            {
                for (int j = 0; j < TasksPerThread; ++j)
                {
                    Context->Post(
                        [&TaskCounter]()
                        {
                            TaskCounter.fetch_add(1);
                            std::this_thread::sleep_for(std::chrono::microseconds(10));
                        });
                }
            });
    }

    for (auto& Thread : PostThreads)
    {
        Thread.join();
    }

    while (TaskCounter.load() < NumThreads * TasksPerThread)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
    std::cout << "所有任务处理完成，耗时: " << Duration.count() << " 微秒" << std::endl;

    Context->Stop();
    RunThread.join();
}

int main()
{
    std::cout << "跨线程唤醒机制示例程序（简化版）" << std::endl;
    std::cout << "========================================" << std::endl;

    try
    {
        DemoWakeupBasics();
        std::cout << "\n========================================" << std::endl;
        std::cout << "演示完成！" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
