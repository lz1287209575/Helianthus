#include "Shared/Common/LogCategories.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/MessageQueue/IMessageQueue.h"
#include "Shared/MessageQueue/MessageQueue.h"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace Helianthus::MessageQueue;

int main()
{
    std::cout << "=== 性能优化功能测试开始 ===" << std::endl;

    // 创建消息队列实例
    auto Queue = std::make_unique<MessageQueue>();
    std::cout << "创建消息队列实例" << std::endl;

    // 初始化消息队列
    std::cout << "开始初始化消息队列..." << std::endl;
    auto InitResult = Queue->Initialize();
    if (InitResult != QueueResult::SUCCESS)
    {
        std::cout << "消息队列初始化失败: " << static_cast<int>(InitResult) << std::endl;
        return 1;
    }
    std::cout << "消息队列初始化成功" << std::endl;

    // 创建测试队列
    QueueConfig Config;
    Config.Name = "performance_test_queue";
    Config.MaxSize = 10000;
    Config.MaxSizeBytes = 1024 * 1024 * 100;  // 100MB
    Config.MessageTtlMs = 30000;              // 30秒
    Config.EnableDeadLetter = true;
    Config.EnablePriority = false;
    Config.EnableBatching = false;

    auto CreateResult = Queue->CreateQueue(Config);
    if (CreateResult != QueueResult::SUCCESS)
    {
        std::cout << "创建队列失败: " << static_cast<int>(CreateResult) << std::endl;
        return 1;
    }
    std::cout << "创建队列成功: " << Config.Name << std::endl;

    // 测试1：设置内存池配置
    std::cout << "=== 测试1：设置内存池配置 ===" << std::endl;

    MemoryPoolConfig MemoryPoolConfig;
    MemoryPoolConfig.InitialSize = 1024 * 1024;    // 1MB
    MemoryPoolConfig.MaxSize = 100 * 1024 * 1024;  // 100MB
    MemoryPoolConfig.BlockSize = 4096;             // 4KB
    MemoryPoolConfig.GrowthFactor = 2;             // 增长因子
    MemoryPoolConfig.EnablePreallocation = true;   // 启用预分配
    MemoryPoolConfig.PreallocationBlocks = 1000;   // 预分配1000个块
    MemoryPoolConfig.EnableCompaction = true;      // 启用内存压缩
    MemoryPoolConfig.CompactionThreshold = 50;     // 50%压缩阈值

    auto SetMemoryPoolResult = Queue->SetMemoryPoolConfig(MemoryPoolConfig);
    if (SetMemoryPoolResult == QueueResult::SUCCESS)
    {
        std::cout << "设置内存池配置成功" << std::endl;
    }
    else
    {
        std::cout << "设置内存池配置失败: " << static_cast<int>(SetMemoryPoolResult) << std::endl;
    }

    // 测试2：设置缓冲区配置
    std::cout << "=== 测试2：设置缓冲区配置 ===" << std::endl;

    BufferConfig BufferConfig;
    BufferConfig.InitialCapacity = 8192;       // 8KB
    BufferConfig.MaxCapacity = 1024 * 1024;    // 1MB
    BufferConfig.GrowthFactor = 2;             // 增长因子
    BufferConfig.EnableZeroCopy = true;        // 启用零拷贝
    BufferConfig.EnableCompression = false;    // 禁用压缩
    BufferConfig.CompressionThreshold = 1024;  // 压缩阈值
    BufferConfig.EnableBatching = true;        // 启用批处理
    BufferConfig.BatchSize = 100;              // 批处理大小
    BufferConfig.BatchTimeoutMs = 100;         // 批处理超时时间

    auto SetBufferResult = Queue->SetBufferConfig(BufferConfig);
    if (SetBufferResult == QueueResult::SUCCESS)
    {
        std::cout << "设置缓冲区配置成功" << std::endl;
    }
    else
    {
        std::cout << "设置缓冲区配置失败: " << static_cast<int>(SetBufferResult) << std::endl;
    }

    // 测试3：内存池分配和释放
    std::cout << "=== 测试3：内存池分配和释放 ===" << std::endl;

    void* Ptr1 = nullptr;
    auto AllocResult1 = Queue->AllocateFromPool(1024, Ptr1);
    if (AllocResult1 == QueueResult::SUCCESS)
    {
        std::cout << "内存池分配成功: ptr=" << Ptr1 << ", size=1024" << std::endl;

        auto DeallocResult = Queue->DeallocateToPool(Ptr1, 1024);
        if (DeallocResult == QueueResult::SUCCESS)
        {
            std::cout << "内存池释放成功" << std::endl;
        }
    }

    void* Ptr2 = nullptr;
    auto AllocResult2 = Queue->AllocateFromPool(4096, Ptr2);
    if (AllocResult2 == QueueResult::SUCCESS)
    {
        std::cout << "内存池分配成功: ptr=" << Ptr2 << ", size=4096" << std::endl;
        Queue->DeallocateToPool(Ptr2, 4096);
    }

    // 测试4：零拷贝操作
    std::cout << "=== 测试4：零拷贝操作 ===" << std::endl;

    std::string TestData = "这是一个零拷贝测试数据，包含中文字符和English characters";
    ZeroCopyBuffer ZeroCopyBuffer;

    auto CreateZeroCopyResult =
        Queue->CreateZeroCopyBuffer(TestData.data(), TestData.size(), ZeroCopyBuffer);
    if (CreateZeroCopyResult == QueueResult::SUCCESS)
    {
        std::cout << "创建零拷贝缓冲区成功: size=" << ZeroCopyBuffer.Size << std::endl;

        auto SendZeroCopyResult = Queue->SendMessageZeroCopy(Config.Name, ZeroCopyBuffer);
        if (SendZeroCopyResult == QueueResult::SUCCESS)
        {
            std::cout << "零拷贝发送消息成功" << std::endl;
        }

        Queue->ReleaseZeroCopyBuffer(ZeroCopyBuffer);
        std::cout << "释放零拷贝缓冲区成功" << std::endl;
    }

    // 测试5：批处理操作
    std::cout << "=== 测试5：批处理操作 ===" << std::endl;

    uint32_t BatchId = 0;
    auto CreateBatchResult = Queue->CreateBatchForQueue(Config.Name, BatchId);
    if (CreateBatchResult == QueueResult::SUCCESS)
    {
        std::cout << "创建批处理成功: id=" << BatchId << std::endl;

        // 添加消息到批处理
        for (int i = 0; i < 5; ++i)
        {
            auto MessagePtr = std::make_shared<Message>();
            MessagePtr->Header.Id = static_cast<MessageId>(i);
            MessagePtr->Header.Priority = MessagePriority::NORMAL;
            MessagePtr->Header.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();
            std::string messageText = "批处理消息 " + std::to_string(i);
            MessagePtr->Payload.Data = std::vector<char>(messageText.begin(), messageText.end());

            auto AddToBatchResult = Queue->AddToBatch(BatchId, MessagePtr);
            if (AddToBatchResult == QueueResult::SUCCESS)
            {
                std::cout << "添加到批处理成功: message_id=" << i << std::endl;
            }
        }

        // 提交批处理
        auto CommitBatchResult = Queue->CommitBatch(BatchId);
        if (CommitBatchResult == QueueResult::SUCCESS)
        {
            std::cout << "提交批处理成功" << std::endl;
        }
    }

    // 测试6：查询性能统计
    std::cout << "=== 测试6：查询性能统计 ===" << std::endl;

    PerformanceStats PerformanceStats;
    auto GetStatsResult = Queue->GetPerformanceStats(PerformanceStats);
    if (GetStatsResult == QueueResult::SUCCESS)
    {
        std::cout << "性能统计:" << std::endl;
        std::cout << "  总分配次数: " << PerformanceStats.TotalAllocations << std::endl;
        std::cout << "  总释放次数: " << PerformanceStats.TotalDeallocations << std::endl;
        std::cout << "  总分配字节数: " << PerformanceStats.TotalBytesAllocated << std::endl;
        std::cout << "  当前分配字节数: " << PerformanceStats.CurrentBytesAllocated << std::endl;
        std::cout << "  峰值分配字节数: " << PerformanceStats.PeakBytesAllocated << std::endl;
        std::cout << "  内存池命中次数: " << PerformanceStats.MemoryPoolHits << std::endl;
        std::cout << "  内存池未命中次数: " << PerformanceStats.MemoryPoolMisses << std::endl;
        std::cout << "  内存池命中率: " << (PerformanceStats.MemoryPoolHitRate * 100) << "%"
                  << std::endl;
        std::cout << "  零拷贝操作次数: " << PerformanceStats.ZeroCopyOperations << std::endl;
        std::cout << "  批处理操作次数: " << PerformanceStats.BatchOperations << std::endl;
        std::cout << "  平均分配时间: " << PerformanceStats.AverageAllocationTimeMs << "ms"
                  << std::endl;
        std::cout << "  平均释放时间: " << PerformanceStats.AverageDeallocationTimeMs << "ms"
                  << std::endl;
        std::cout << "  平均零拷贝时间: " << PerformanceStats.AverageZeroCopyTimeMs << "ms"
                  << std::endl;
        std::cout << "  平均批处理时间: " << PerformanceStats.AverageBatchTimeMs << "ms"
                  << std::endl;
    }

    // 测试7：内存池压缩
    std::cout << "=== 测试7：内存池压缩 ===" << std::endl;

    auto CompactResult = Queue->CompactMemoryPool();
    if (CompactResult == QueueResult::SUCCESS)
    {
        std::cout << "内存池压缩成功" << std::endl;
    }

    // 测试8：重置性能统计
    std::cout << "=== 测试8：重置性能统计 ===" << std::endl;

    auto ResetStatsResult = Queue->ResetPerformanceStats();
    if (ResetStatsResult == QueueResult::SUCCESS)
    {
        std::cout << "重置性能统计成功" << std::endl;
    }

    // 测试9：查询配置
    std::cout << "=== 测试9：查询配置 ===" << std::endl;

    struct MemoryPoolConfig RetrievedMemoryPoolConfig;
    auto GetMemoryPoolResult = Queue->GetMemoryPoolConfig(RetrievedMemoryPoolConfig);
    if (GetMemoryPoolResult == QueueResult::SUCCESS)
    {
        std::cout << "内存池配置:" << std::endl;
        std::cout << "  初始大小: " << RetrievedMemoryPoolConfig.InitialSize << " bytes"
                  << std::endl;
        std::cout << "  最大大小: " << RetrievedMemoryPoolConfig.MaxSize << " bytes" << std::endl;
        std::cout << "  块大小: " << RetrievedMemoryPoolConfig.BlockSize << " bytes" << std::endl;
        std::cout << "  增长因子: " << RetrievedMemoryPoolConfig.GrowthFactor << std::endl;
        std::cout << "  启用预分配: "
                  << (RetrievedMemoryPoolConfig.EnablePreallocation ? "是" : "否") << std::endl;
        std::cout << "  预分配块数: " << RetrievedMemoryPoolConfig.PreallocationBlocks << std::endl;
        std::cout << "  启用压缩: " << (RetrievedMemoryPoolConfig.EnableCompaction ? "是" : "否")
                  << std::endl;
        std::cout << "  压缩阈值: " << RetrievedMemoryPoolConfig.CompactionThreshold << "%"
                  << std::endl;
    }

    struct BufferConfig RetrievedBufferConfig;
    auto GetBufferResult = Queue->GetBufferConfig(RetrievedBufferConfig);
    if (GetBufferResult == QueueResult::SUCCESS)
    {
        std::cout << "缓冲区配置:" << std::endl;
        std::cout << "  初始容量: " << RetrievedBufferConfig.InitialCapacity << " bytes"
                  << std::endl;
        std::cout << "  最大容量: " << RetrievedBufferConfig.MaxCapacity << " bytes" << std::endl;
        std::cout << "  增长因子: " << RetrievedBufferConfig.GrowthFactor << std::endl;
        std::cout << "  启用零拷贝: " << (RetrievedBufferConfig.EnableZeroCopy ? "是" : "否")
                  << std::endl;
        std::cout << "  启用压缩: " << (RetrievedBufferConfig.EnableCompression ? "是" : "否")
                  << std::endl;
        std::cout << "  压缩阈值: " << RetrievedBufferConfig.CompressionThreshold << " bytes"
                  << std::endl;
        std::cout << "  启用批处理: " << (RetrievedBufferConfig.EnableBatching ? "是" : "否")
                  << std::endl;
        std::cout << "  批处理大小: " << RetrievedBufferConfig.BatchSize << std::endl;
        std::cout << "  批处理超时: " << RetrievedBufferConfig.BatchTimeoutMs << "ms" << std::endl;
    }

    // 清理
    std::cout << "=== 性能优化功能测试完成 ===" << std::endl;

    // 正确关闭消息队列
    std::cout << "开始关闭消息队列..." << std::endl;
    Queue->Shutdown();
    std::cout << "消息队列关闭完成" << std::endl;

    return 0;
}
