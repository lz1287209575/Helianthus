#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace Helianthus::Network::Asio;

// 演示基础任务提交与处理
void DemoBatchProcessing()
{
    std::cout << "=== 任务提交与处理演示 ===" << std::endl;

    auto Context = std::make_shared<IoContext>();

    // 启动事件循环
    std::thread RunThread([Context]() {
        Context->Run();
    });

    // 提交大量任务
    std::atomic<int> TaskCounter = 0;
    const int NumTasks = 10000;

    std::cout << "提交 " << NumTasks << " 个任务..." << std::endl;

    auto StartTime = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> SubmitThreads;
    for (int i = 0; i < 8; ++i)
    {
        SubmitThreads.emplace_back([Context, &TaskCounter, NumTasks]() {
            for (int j = 0; j < NumTasks / 8; ++j)
            {
                Context->Post([&TaskCounter]() {
                    TaskCounter.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                });
            }
        });
    }

    for (auto& Thread : SubmitThreads)
    {
        Thread.join();
    }

    while (TaskCounter.load() < NumTasks)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);

    std::cout << "所有任务处理完成，耗时: " << Duration.count() << " 微秒" << std::endl;

    Context->Stop();
    RunThread.join();
}

// 演示简单性能测量
void DemoPerformanceComparison()
{
    std::cout << "\n=== 简单性能演示 ===" << std::endl;

    const int NumTasks = 5000;
    std::atomic<int> TaskCounter = 0;

    auto Context = std::make_shared<IoContext>();

    auto StartTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NumTasks; ++i)
    {
        Context->Post([&TaskCounter]() {
            TaskCounter.fetch_add(1);
        });
    }

    std::thread RunThread([Context]() {
        Context->Run();
    });

    while (TaskCounter.load() < NumTasks)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);

    Context->Stop();
    RunThread.join();

    std::cout << "  处理 " << NumTasks << " 个任务耗时: " << Duration.count() << " 微秒" << std::endl;
}

// 自适应批处理示例已移除（当前接口不支持）

int main()
{
    std::cout << "批处理功能示例程序" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try
    {
        // 批处理功能演示
        DemoBatchProcessing();
        
        // 性能对比演示
        DemoPerformanceComparison();
        
        // 自适应批处理示例已省略
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "所有演示完成！" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
