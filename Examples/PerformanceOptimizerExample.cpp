#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>

#include "Shared/MessageQueue/PerformanceOptimizer.h"
#include "Shared/Common/Logger.h"

using namespace Helianthus::MessageQueue;
using namespace Helianthus::Common;

// 性能测试回调函数
void OnPerformanceUpdate(const PerformanceStats& Stats)
{
    std::cout << "=== 性能统计更新 ===" << std::endl;
    std::cout << "内存池命中率: " << Stats.MemoryPoolHitRate << "%" << std::endl;
    std::cout << "零拷贝操作: " << Stats.ZeroCopyOperations << std::endl;
    std::cout << "批处理操作: " << Stats.BatchOperations << std::endl;
    std::cout << "平均分配时间: " << Stats.AverageAllocationTimeMs << " ms" << std::endl;
    std::cout << "平均零拷贝时间: " << Stats.AverageZeroCopyTimeMs << " ms" << std::endl;
    std::cout << "平均批处理时间: " << Stats.AverageBatchTimeMs << " ms" << std::endl;
    std::cout << "总分配次数: " << Stats.TotalAllocations << std::endl;
    std::cout << "总分配字节数: " << Stats.TotalBytesAllocated << std::endl;
    std::cout << "=========================" << std::endl;
}

int main()
{
    // 初始化日志
    Logger::Initialize();
    
    std::cout << "=== 性能优化器示例 ===" << std::endl;
    
    // 获取性能优化器实例
    auto& Optimizer = GetPerformanceOptimizer();
    
    // 配置性能优化器
    PerformanceConfig Config;
    Config.EnableMemoryPool = true;
    Config.MemoryPoolSize = 32 * 1024 * 1024;  // 32MB
    Config.BlockSize = 4096;                    // 4KB块
    Config.EnableMessagePool = true;
    Config.MessagePoolSize = 5000;              // 5000个消息对象
    Config.EnableBatching = true;
    Config.BatchSize = 50;                      // 批处理大小
    Config.BatchTimeoutMs = 500;                // 批处理超时
    Config.EnableZeroCopy = true;
    Config.EnablePreallocation = true;
    Config.PreallocatedMessages = 1000;         // 预分配1000个消息
    Config.EnablePerformanceMonitoring = true;
    Config.MonitoringIntervalMs = 2000;         // 监控间隔2秒
    
    Optimizer.UpdateConfig(Config);
    
    // 初始化性能优化器
    Optimizer.Initialize(Config);
    
    std::cout << "性能优化器已初始化" << std::endl;
    
    // 测试1: 内存池性能
    std::cout << "\n=== 测试1: 内存池性能 ===" << std::endl;
    {
        const size_t TestSize = 10000;
        std::vector<void*> Allocations;
        Allocations.reserve(TestSize);
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 分配内存
        for (size_t i = 0; i < TestSize; ++i)
        {
            size_t Size = 64 + (i % 1024);  // 64字节到1088字节
            void* Ptr = Optimizer.AllocateFromPool(Size);
            Allocations.push_back(Ptr);
        }
        
        // 释放内存
        for (size_t i = 0; i < Allocations.size(); ++i)
        {
            size_t Size = 64 + (i % 1024);
            Optimizer.DeallocateToPool(Allocations[i], Size);
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
        
        std::cout << "内存池测试完成: " << TestSize << " 次分配/释放, 耗时: " 
                  << Duration.count() << " ms" << std::endl;
    }
    
    // 测试2: 消息对象池性能
    std::cout << "\n=== 测试2: 消息对象池性能 ===" << std::endl;
    {
        const size_t TestSize = 5000;
        std::vector<MessagePtr> Messages;
        Messages.reserve(TestSize);
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 创建消息
        for (size_t i = 0; i < TestSize; ++i)
        {
            MessageType Type = static_cast<MessageType>(i % 10);
            MessagePtr Msg = Optimizer.CreateMessage(Type);
            Messages.push_back(Msg);
        }
        
        // 回收消息
        for (MessagePtr& Msg : Messages)
        {
            Optimizer.RecycleMessage(Msg);
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
        
        std::cout << "消息对象池测试完成: " << TestSize << " 次创建/回收, 耗时: " 
                  << Duration.count() << " ms" << std::endl;
    }
    
    // 测试3: 零拷贝性能
    std::cout << "\n=== 测试3: 零拷贝性能 ===" << std::endl;
    {
        const size_t TestSize = 1000;
        std::vector<ZeroCopyBuffer> Buffers;
        Buffers.reserve(TestSize);
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 创建零拷贝缓冲区
        for (size_t i = 0; i < TestSize; ++i)
        {
            std::string Data = "测试数据 " + std::to_string(i) + " 这是一个零拷贝测试";
            ZeroCopyBuffer Buffer = Optimizer.CreateZeroCopyBuffer(Data);
            Buffers.push_back(Buffer);
        }
        
        // 从零拷贝缓冲区创建消息
        for (size_t i = 0; i < TestSize; ++i)
        {
            MessageType Type = static_cast<MessageType>(i % 5);
            MessagePtr Msg = Optimizer.CreateMessageFromZeroCopy(Buffers[i], Type);
            if (Msg)
            {
                Optimizer.RecycleMessage(Msg);
            }
        }
        
        // 释放零拷贝缓冲区
        for (ZeroCopyBuffer& Buffer : Buffers)
        {
            Optimizer.ReleaseZeroCopyBuffer(Buffer);
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
        
        std::cout << "零拷贝测试完成: " << TestSize << " 次操作, 耗时: " 
                  << Duration.count() << " ms" << std::endl;
    }
    
    // 测试4: 批处理性能
    std::cout << "\n=== 测试4: 批处理性能 ===" << std::endl;
    {
        const size_t TestSize = 1000;
        const size_t BatchCount = 20;
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 创建多个批处理
        for (size_t batch = 0; batch < BatchCount; ++batch)
        {
            uint32_t BatchId = Optimizer.CreateBatch("test_queue_" + std::to_string(batch));
            
            // 向批处理添加消息
            for (size_t i = 0; i < TestSize / BatchCount; ++i)
            {
                MessageType Type = static_cast<MessageType>(i % 8);
                MessagePtr Msg = Optimizer.CreateMessage(Type);
                Optimizer.AddToBatch(BatchId, Msg);
            }
            
            // 提交批处理
            Optimizer.CommitBatch(BatchId);
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
        
        std::cout << "批处理测试完成: " << TestSize << " 个消息, " << BatchCount 
                  << " 个批处理, 耗时: " << Duration.count() << " ms" << std::endl;
    }
    
    // 测试5: 混合性能测试
    std::cout << "\n=== 测试5: 混合性能测试 ===" << std::endl;
    {
        const size_t TestSize = 2000;
        std::random_device Rd;
        std::mt19937 Gen(Rd());
        std::uniform_int_distribution<> SizeDist(64, 2048);
        std::uniform_int_distribution<> TypeDist(0, 9);
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < TestSize; ++i)
        {
            // 随机选择操作类型
            int OpType = i % 4;
            
            switch (OpType)
            {
                case 0: // 内存分配
                {
                    size_t Size = SizeDist(Gen);
                    void* Ptr = Optimizer.AllocateFromPool(Size);
                    Optimizer.DeallocateToPool(Ptr, Size);
                    break;
                }
                case 1: // 消息创建
                {
                    MessageType Type = static_cast<MessageType>(TypeDist(Gen));
                    MessagePtr Msg = Optimizer.CreateMessage(Type);
                    Optimizer.RecycleMessage(Msg);
                    break;
                }
                case 2: // 零拷贝
                {
                    std::string Data = "混合测试数据 " + std::to_string(i);
                    ZeroCopyBuffer Buffer = Optimizer.CreateZeroCopyBuffer(Data);
                    MessagePtr Msg = Optimizer.CreateMessageFromZeroCopy(Buffer, MessageType::TEXT);
                    Optimizer.ReleaseZeroCopyBuffer(Buffer);
                    if (Msg) Optimizer.RecycleMessage(Msg);
                    break;
                }
                case 3: // 批处理
                {
                    uint32_t BatchId = Optimizer.CreateBatch("mixed_test");
                    for (int j = 0; j < 5; ++j)
                    {
                        MessagePtr Msg = Optimizer.CreateMessage(MessageType::TEXT);
                        Optimizer.AddToBatch(BatchId, Msg);
                    }
                    Optimizer.CommitBatch(BatchId);
                    break;
                }
            }
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
        
        std::cout << "混合性能测试完成: " << TestSize << " 次操作, 耗时: " 
                  << Duration.count() << " ms" << std::endl;
    }
    
    // 获取并显示最终性能统计
    std::cout << "\n=== 最终性能统计 ===" << std::endl;
    PerformanceStats FinalStats = Optimizer.GetPerformanceStats();
    OnPerformanceUpdate(FinalStats);
    
    // 等待一段时间观察监控
    std::cout << "\n等待10秒观察性能监控..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // 重置统计
    std::cout << "\n重置性能统计..." << std::endl;
    Optimizer.ResetPerformanceStats();
    
    // 关闭性能优化器
    Optimizer.Shutdown();
    
    std::cout << "\n=== 性能优化器示例完成 ===" << std::endl;
    
    return 0;
}
