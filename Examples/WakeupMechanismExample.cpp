#include "Shared/Network/Asio/IoContext.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace Helianthus::Network::Asio;

// 演示跨线程唤醒机制
void DemoWakeupMechanism()
{
    std::cout << "=== 跨线程唤醒机制演示 ===" << std::endl;
    
    auto Context = std::make_shared<IoContext>();
    
    // 显示当前唤醒类型
    auto WakeupType = Context->GetWakeupType();
    std::cout << "当前唤醒类型: ";
    switch (WakeupType)
    {
        case WakeupType::EventFd:
            std::cout << "EventFd (Linux)";
            break;
        case WakeupType::Pipe:
            std::cout << "Pipe (BSD/macOS)";
            break;
        case WakeupType::IOCP:
            std::cout << "IOCP (Windows)";
            break;
        case WakeupType::WakeByAddress:
            std::cout << "WakeByAddress (Windows)";
            break;
    }
    std::cout << std::endl;
    
    // 重置唤醒统计
    Context->ResetWakeupStats();
    
    // 启动事件循环线程
    std::thread RunThread([Context]() {
        Context->Run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 从多个线程提交任务
    std::atomic<int> TaskCounter = 0;
    const int NumThreads = 4;
    const int TasksPerThread = 100;
    
    std::cout << "启动 " << NumThreads << " 个线程，每个线程提交 " << TasksPerThread << " 个任务..." << std::endl;
    
    auto StartTime = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> PostThreads;
    for (int i = 0; i < NumThreads; ++i)
    {
        PostThreads.emplace_back([Context, &TaskCounter, TasksPerThread, i]() {
            for (int j = 0; j < TasksPerThread; ++j)
            {
                Context->Post([&TaskCounter]() {
                    TaskCounter.fetch_add(1);
                    // 模拟一些工作
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                });
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& Thread : PostThreads)
    {
        Thread.join();
    }
    
    // 等待任务处理完成
    while (TaskCounter.load() < NumThreads * TasksPerThread)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
    
    std::cout << "所有任务处理完成，耗时: " << Duration.count() << " 微秒" << std::endl;
    
    // 停止事件循环
    Context->Stop();
    RunThread.join();
    
    // 显示唤醒统计
    auto Stats = Context->GetWakeupStats();
    std::cout << "唤醒统计:" << std::endl;
    std::cout << "  总唤醒次数: " << Stats.TotalWakeups << std::endl;
    std::cout << "  跨线程唤醒: " << Stats.CrossThreadWakeups << std::endl;
    std::cout << "  同线程唤醒: " << Stats.SameThreadWakeups << std::endl;
    std::cout << "  平均延迟: " << Stats.AverageWakeupLatencyMs << " ms" << std::endl;
    std::cout << "  最大延迟: " << Stats.MaxWakeupLatencyMs << " ms" << std::endl;
}

// 演示不同唤醒类型的性能对比
void DemoWakeupTypeComparison()
{
    std::cout << "\n=== 唤醒类型性能对比 ===" << std::endl;
    
    const int NumTasks = 1000;
    std::atomic<int> TaskCounter = 0;
    
    // 测试 EventFd
    {
        std::cout << "测试 EventFd 唤醒类型..." << std::endl;
        auto Context = std::make_shared<IoContext>();
        Context->SetWakeupType(WakeupType::EventFd);
        Context->ResetWakeupStats();
        
        std::thread RunThread([Context]() {
            Context->Run();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < NumTasks; ++i)
        {
            Context->Post([&TaskCounter]() {
                TaskCounter.fetch_add(1);
            });
        }
        
        while (TaskCounter.load() < NumTasks)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        
        Context->Stop();
        RunThread.join();
        
        auto Stats = Context->GetWakeupStats();
        std::cout << "  EventFd - 耗时: " << Duration.count() << " 微秒" << std::endl;
        std::cout << "  平均延迟: " << Stats.AverageWakeupLatencyMs << " ms" << std::endl;
        std::cout << "  最大延迟: " << Stats.MaxWakeupLatencyMs << " ms" << std::endl;
    }
    
    // 重置计数器
    TaskCounter.store(0);
    
    // 测试 Pipe
    {
        std::cout << "测试 Pipe 唤醒类型..." << std::endl;
        auto Context = std::make_shared<IoContext>();
        Context->SetWakeupType(WakeupType::Pipe);
        Context->ResetWakeupStats();
        
        std::thread RunThread([Context]() {
            Context->Run();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < NumTasks; ++i)
        {
            Context->Post([&TaskCounter]() {
                TaskCounter.fetch_add(1);
            });
        }
        
        while (TaskCounter.load() < NumTasks)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        
        Context->Stop();
        RunThread.join();
        
        auto Stats = Context->GetWakeupStats();
        std::cout << "  Pipe - 耗时: " << Duration.count() << " 微秒" << std::endl;
        std::cout << "  平均延迟: " << Stats.AverageWakeupLatencyMs << " ms" << std::endl;
        std::cout << "  最大延迟: " << Stats.MaxWakeupLatencyMs << " ms" << std::endl;
    }
}

// 演示直接唤醒方法
void DemoDirectWakeup()
{
    std::cout << "\n=== 直接唤醒方法演示 ===" << std::endl;
    
    auto Context = std::make_shared<IoContext>();
    Context->ResetWakeupStats();
    
    // 启动事件循环线程
    std::thread RunThread([Context]() {
        Context->Run();
    });
    
    // 等待事件循环启动
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    std::atomic<int> TaskCounter = 0;
    const int NumWakeups = 50;
    
    std::cout << "执行 " << NumWakeups << " 次直接唤醒..." << std::endl;
    
    // 从另一个线程直接调用唤醒
    std::thread WakeupThread([Context, &TaskCounter, NumWakeups]() {
        for (int i = 0; i < NumWakeups; ++i)
        {
            // 提交任务
            Context->Post([&TaskCounter]() {
                TaskCounter.fetch_add(1);
            });
            
            // 直接唤醒
            Context->WakeupFromOtherThread();
            
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // 等待任务完成
    while (TaskCounter.load() < NumWakeups)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 停止事件循环
    Context->Stop();
    RunThread.join();
    WakeupThread.join();
    
    // 显示统计
    auto Stats = Context->GetWakeupStats();
    std::cout << "直接唤醒统计:" << std::endl;
    std::cout << "  总唤醒次数: " << Stats.TotalWakeups << std::endl;
    std::cout << "  跨线程唤醒: " << Stats.CrossThreadWakeups << std::endl;
    std::cout << "  平均延迟: " << Stats.AverageWakeupLatencyMs << " ms" << std::endl;
}

// 演示唤醒统计重置
void DemoWakeupStatsReset()
{
    std::cout << "\n=== 唤醒统计重置演示 ===" << std::endl;
    
    auto Context = std::make_shared<IoContext>();
    
    // 提交一些任务
    std::atomic<int> TaskCounter = 0;
    for (int i = 0; i < 10; ++i)
    {
        Context->Post([&TaskCounter]() {
            TaskCounter.fetch_add(1);
        });
    }
    
    // 启动事件循环处理任务
    std::thread RunThread([Context]() {
        Context->Run();
    });
    
    // 等待任务完成
    while (TaskCounter.load() < 10)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Context->Stop();
    RunThread.join();
    
    // 显示重置前的统计
    auto StatsBefore = Context->GetWakeupStats();
    std::cout << "重置前统计:" << std::endl;
    std::cout << "  总唤醒次数: " << StatsBefore.TotalWakeups << std::endl;
    std::cout << "  跨线程唤醒: " << StatsBefore.CrossThreadWakeups << std::endl;
    std::cout << "  平均延迟: " << StatsBefore.AverageWakeupLatencyMs << " ms" << std::endl;
    
    // 重置统计
    Context->ResetWakeupStats();
    
    // 显示重置后的统计
    auto StatsAfter = Context->GetWakeupStats();
    std::cout << "重置后统计:" << std::endl;
    std::cout << "  总唤醒次数: " << StatsAfter.TotalWakeups << std::endl;
    std::cout << "  跨线程唤醒: " << StatsAfter.CrossThreadWakeups << std::endl;
    std::cout << "  平均延迟: " << StatsAfter.AverageWakeupLatencyMs << " ms" << std::endl;
}

int main()
{
    std::cout << "跨线程唤醒机制示例程序" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try
    {
        // 跨线程唤醒机制演示
        DemoWakeupMechanism();
        
        // 唤醒类型性能对比
        DemoWakeupTypeComparison();
        
        // 直接唤醒方法演示
        DemoDirectWakeup();
        
        // 唤醒统计重置演示
        DemoWakeupStatsReset();
        
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
