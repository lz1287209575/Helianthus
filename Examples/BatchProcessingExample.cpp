#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace Helianthus::Network::Asio;

// 演示批处理功能
void DemoBatchProcessing()
{
    std::cout << "=== 批处理功能演示 ===" << std::endl;
    
    auto Context = std::make_shared<IoContext>();
    
    // 配置任务批处理
    TaskBatchConfig TaskConfig;
    TaskConfig.MaxTaskBatchSize = 32;
    TaskConfig.MinTaskBatchSize = 8;
    TaskConfig.EnableTaskBatching = true;
    Context->SetTaskBatchConfig(TaskConfig);
    
    // 配置 Reactor 批处理
    auto Reactor = Context->GetReactor();
    BatchConfig ReactorConfig;
    ReactorConfig.MaxBatchSize = 64;
    ReactorConfig.MinBatchSize = 16;
    ReactorConfig.EnableAdaptiveBatching = true;
    ReactorConfig.AdaptiveThreshold = 32;
    Reactor->SetBatchConfig(ReactorConfig);
    
    std::cout << "批处理配置已设置" << std::endl;
    
    // 启动批处理事件循环
    std::thread RunThread([Context]() {
        Context->RunBatch();
    });
    
    // 提交大量任务
    std::atomic<int> TaskCounter = 0;
    const int NumTasks = 10000;
    
    std::cout << "提交 " << NumTasks << " 个任务..." << std::endl;
    
    auto StartTime = std::chrono::high_resolution_clock::now();
    
    // 多线程提交任务
    std::vector<std::thread> SubmitThreads;
    for (int i = 0; i < 8; ++i)
    {
        SubmitThreads.emplace_back([Context, &TaskCounter, NumTasks, i]() {
            for (int j = 0; j < NumTasks / 8; ++j)
            {
                Context->Post([&TaskCounter]() {
                    TaskCounter.fetch_add(1);
                    // 模拟一些工作
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                });
            }
        });
    }
    
    // 等待所有提交线程完成
    for (auto& Thread : SubmitThreads)
    {
        Thread.join();
    }
    
    // 等待任务处理完成
    while (TaskCounter.load() < NumTasks)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
    
    std::cout << "所有任务处理完成，耗时: " << Duration.count() << " 微秒" << std::endl;
    
    // 显示任务批处理统计
    auto TaskStats = Context->GetTaskBatchStats();
    std::cout << "任务批处理统计:" << std::endl;
    std::cout << "  总任务: " << TaskStats.TotalTasks << std::endl;
    std::cout << "  总批处理: " << TaskStats.TotalBatches << std::endl;
    std::cout << "  平均批处理大小: " << TaskStats.AverageBatchSize << std::endl;
    std::cout << "  最大批处理大小: " << TaskStats.MaxBatchSize << std::endl;
    std::cout << "  最小批处理大小: " << TaskStats.MinBatchSize << std::endl;
    std::cout << "  平均处理时间: " << TaskStats.AverageProcessingTimeMs << " ms" << std::endl;
    
    // 显示 Reactor 批处理统计
    auto ReactorStats = Reactor->GetPerformanceStats();
    std::cout << "Reactor 批处理统计:" << std::endl;
    std::cout << "  总批处理: " << ReactorStats.TotalBatches << std::endl;
    std::cout << "  平均批处理大小: " << ReactorStats.AverageBatchSize << std::endl;
    std::cout << "  最大批处理大小: " << ReactorStats.MaxBatchSize << std::endl;
    std::cout << "  最小批处理大小: " << ReactorStats.MinBatchSize << std::endl;
    std::cout << "  自适应批处理: " << ReactorStats.AdaptiveBatchCount << std::endl;
    std::cout << "  平均处理时间: " << ReactorStats.AverageProcessingTimeMs << " ms" << std::endl;
    
    // 停止事件循环
    Context->Stop();
    RunThread.join();
}

// 演示性能对比
void DemoPerformanceComparison()
{
    std::cout << "\n=== 性能对比演示 ===" << std::endl;
    
    const int NumTasks = 5000;
    std::atomic<int> TaskCounter = 0;
    
    // 测试1：禁用批处理
    {
        std::cout << "测试1：禁用批处理" << std::endl;
        auto Context = std::make_shared<IoContext>();
        
        TaskBatchConfig Config;
        Config.EnableTaskBatching = false;
        Context->SetTaskBatchConfig(Config);
        Context->ResetTaskBatchStats();
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 提交任务
        for (int i = 0; i < NumTasks; ++i)
        {
            Context->Post([&TaskCounter]() {
                TaskCounter.fetch_add(1);
            });
        }
        
        // 启动事件循环
        std::thread RunThread([Context]() {
            Context->Run();
        });
        
        // 等待任务完成
        while (TaskCounter.load() < NumTasks)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        
        Context->Stop();
        RunThread.join();
        
        auto Stats = Context->GetTaskBatchStats();
        std::cout << "  耗时: " << Duration.count() << " 微秒" << std::endl;
        std::cout << "  总任务: " << Stats.TotalTasks << std::endl;
        std::cout << "  总批处理: " << Stats.TotalBatches << std::endl;
        std::cout << "  平均批处理大小: " << Stats.AverageBatchSize << std::endl;
    }
    
    // 重置计数器
    TaskCounter.store(0);
    
    // 测试2：启用批处理
    {
        std::cout << "测试2：启用批处理" << std::endl;
        auto Context = std::make_shared<IoContext>();
        
        TaskBatchConfig Config;
        Config.EnableTaskBatching = true;
        Config.MaxTaskBatchSize = 32;
        Config.MinTaskBatchSize = 8;
        Context->SetTaskBatchConfig(Config);
        Context->ResetTaskBatchStats();
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 提交任务
        for (int i = 0; i < NumTasks; ++i)
        {
            Context->Post([&TaskCounter]() {
                TaskCounter.fetch_add(1);
            });
        }
        
        // 启动批处理事件循环
        std::thread RunThread([Context]() {
            Context->RunBatch();
        });
        
        // 等待任务完成
        while (TaskCounter.load() < NumTasks)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
        
        Context->Stop();
        RunThread.join();
        
        auto Stats = Context->GetTaskBatchStats();
        std::cout << "  耗时: " << Duration.count() << " 微秒" << std::endl;
        std::cout << "  总任务: " << Stats.TotalTasks << std::endl;
        std::cout << "  总批处理: " << Stats.TotalBatches << std::endl;
        std::cout << "  平均批处理大小: " << Stats.AverageBatchSize << std::endl;
        std::cout << "  最大批处理大小: " << Stats.MaxBatchSize << std::endl;
        std::cout << "  最小批处理大小: " << Stats.MinBatchSize << std::endl;
    }
}

// 演示自适应批处理
void DemoAdaptiveBatching()
{
    std::cout << "\n=== 自适应批处理演示 ===" << std::endl;
    
    auto Context = std::make_shared<IoContext>();
    auto Reactor = Context->GetReactor();
    
    // 配置自适应批处理
    BatchConfig Config;
    Config.MaxBatchSize = 128;
    Config.MinBatchSize = 16;
    Config.EnableAdaptiveBatching = true;
    Config.AdaptiveThreshold = 32;
    Reactor->SetBatchConfig(Config);
    
    // 重置统计
    Reactor->ResetPerformanceStats();
    
    std::cout << "执行多次批处理轮询以触发自适应调整..." << std::endl;
    
    // 执行多次批处理轮询
    for (int i = 0; i < 100; ++i)
    {
        Reactor->PollBatch(1, 64);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    
    // 显示自适应批处理统计
    auto Stats = Reactor->GetPerformanceStats();
    std::cout << "自适应批处理统计:" << std::endl;
    std::cout << "  总批处理: " << Stats.TotalBatches << std::endl;
    std::cout << "  自适应批处理: " << Stats.AdaptiveBatchCount << std::endl;
    std::cout << "  平均批处理大小: " << Stats.AverageBatchSize << std::endl;
    std::cout << "  最大批处理大小: " << Stats.MaxBatchSize << std::endl;
    std::cout << "  最小批处理大小: " << Stats.MinBatchSize << std::endl;
    std::cout << "  平均处理时间: " << Stats.AverageProcessingTimeMs << " ms" << std::endl;
    
    // 显示当前配置
    auto CurrentConfig = Reactor->GetBatchConfig();
    std::cout << "当前批处理配置:" << std::endl;
    std::cout << "  最大批处理大小: " << CurrentConfig.MaxBatchSize << std::endl;
    std::cout << "  最小批处理大小: " << CurrentConfig.MinBatchSize << std::endl;
    std::cout << "  启用自适应批处理: " << (CurrentConfig.EnableAdaptiveBatching ? "是" : "否") << std::endl;
    std::cout << "  自适应阈值: " << CurrentConfig.AdaptiveThreshold << std::endl;
}

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
        
        // 自适应批处理演示
        DemoAdaptiveBatching();
        
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
