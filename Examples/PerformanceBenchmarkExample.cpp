#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <iomanip>
#include <sstream>

#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/MessageQueue/MessageTypes.h"
#include "Shared/Common/Types.h"
#include "Shared/Monitoring/EnhancedPrometheusExporter.h"

using namespace Helianthus::MessageQueue;
using namespace Helianthus::Common;
using namespace Helianthus::Monitoring;

class PerformanceBenchmark
{
public:
    struct BenchmarkResult
    {
        std::string TestName;
        uint64_t MessageCount;
        uint64_t DurationNs;
        uint64_t MemoryUsageBytes;
        double Throughput; // messages/second
        double LatencyMs;  // average latency
        double P95LatencyMs;
        double P99LatencyMs;
    };

    PerformanceBenchmark() : MQ(), EnhancedExporter()
    {
        // 初始化消息队列
        QueueResult InitResult = MQ.Initialize();
        if (InitResult != QueueResult::SUCCESS)
        {
            std::cerr << "消息队列初始化失败: " << static_cast<int>(InitResult) << std::endl;
            return;
        }
        
        // 创建测试队列
        QueueConfig Config;
        Config.Name = "benchmark_queue";
        Config.MaxSize = 100000;
        Config.EnableBatching = true;
        Config.BatchSize = 100;
        
        QueueResult Result = MQ.CreateQueue(Config);
        if (Result != QueueResult::SUCCESS)
        {
            std::cerr << "创建测试队列失败: " << static_cast<int>(Result) << std::endl;
        }
    }

    void RunAllBenchmarks()
    {
        std::cout << "=== Helianthus 性能基准测试 ===" << std::endl;
        std::cout << "开始时间: " << GetCurrentTimestampString() << std::endl;
        std::cout << std::endl;

        std::vector<BenchmarkResult> Results;

        // 基础性能测试
        Results.push_back(RunBasicPerformanceTest());
        Results.push_back(RunCompressionPerformanceTest());
        Results.push_back(RunEncryptionPerformanceTest());
        Results.push_back(RunBatchProcessingTest());
        Results.push_back(RunZeroCopyTest());
        Results.push_back(RunTransactionTest());
        Results.push_back(RunConcurrentTest());

        // 生成报告
        GenerateReport(Results);
    }

private:
    MessageQueue MQ;
    EnhancedPrometheusExporter EnhancedExporter;

    BenchmarkResult RunBasicPerformanceTest()
    {
        std::cout << "运行基础性能测试..." << std::endl;
        
        const uint64_t MessageCount = 10000;
        const size_t MessageSize = 1024; // 1KB
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        // 发送消息
        for (uint64_t i = 0; i < MessageCount; ++i)
        {
            auto Msg = CreateTestMessage(i, MessageSize);
            MQ.SendMessage("benchmark_queue", Msg);
        }
        
        // 接收消息
        for (uint64_t i = 0; i < MessageCount; ++i)
        {
            MessagePtr ReceivedMsg;
            MQ.ReceiveMessage("benchmark_queue", ReceivedMsg, 1000);
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
        
        BenchmarkResult Result;
        Result.TestName = "基础性能测试";
        Result.MessageCount = MessageCount;
        Result.DurationNs = Duration.count();
        Result.Throughput = static_cast<double>(MessageCount) / (Duration.count() / 1e9);
        Result.LatencyMs = static_cast<double>(Duration.count()) / (MessageCount * 2) / 1e6; // 发送+接收
        
        std::cout << "  完成: " << MessageCount << " 消息, " 
                  << std::fixed << std::setprecision(2) 
                  << Result.Throughput << " msg/s, "
                  << Result.LatencyMs << " ms 平均延迟" << std::endl;
        
        return Result;
    }

    BenchmarkResult RunCompressionPerformanceTest()
    {
        std::cout << "运行压缩性能测试..." << std::endl;
        
        const uint64_t MessageCount = 5000;
        const size_t MessageSize = 4096; // 4KB，更容易压缩
        
        // 测试无批处理
        QueueConfig NoBatchConfig;
        NoBatchConfig.Name = "no_batch_queue";
        NoBatchConfig.MaxSize = 50000;
        NoBatchConfig.EnableBatching = false;
        MQ.CreateQueue(NoBatchConfig);
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        for (uint64_t i = 0; i < MessageCount; ++i)
        {
            auto Msg = CreateTestMessage(i, MessageSize);
            MQ.SendMessage("no_batch_queue", Msg);
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto NoBatchDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
        
        // 测试有批处理
        QueueConfig BatchConfig;
        BatchConfig.Name = "batch_queue";
        BatchConfig.MaxSize = 50000;
        BatchConfig.EnableBatching = true;
        BatchConfig.BatchSize = 50;
        MQ.CreateQueue(BatchConfig);
        
        StartTime = std::chrono::high_resolution_clock::now();
        
        for (uint64_t i = 0; i < MessageCount; ++i)
        {
            auto Msg = CreateTestMessage(i, MessageSize);
            MQ.SendMessage("batch_queue", Msg);
        }
        
        EndTime = std::chrono::high_resolution_clock::now();
        auto BatchDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
        
        BenchmarkResult Result;
        Result.TestName = "批处理性能测试";
        Result.MessageCount = MessageCount;
        Result.DurationNs = BatchDuration.count();
        Result.Throughput = static_cast<double>(MessageCount) / (BatchDuration.count() / 1e9);
        Result.LatencyMs = static_cast<double>(BatchDuration.count()) / MessageCount / 1e6;
        
        double BatchRatio = static_cast<double>(NoBatchDuration.count()) / BatchDuration.count();
        
        std::cout << "  完成: " << MessageCount << " 消息, "
                  << std::fixed << std::setprecision(2) 
                  << Result.Throughput << " msg/s, "
                  << "批处理提升: " << BatchRatio << "x" << std::endl;
        
        return Result;
    }

    BenchmarkResult RunEncryptionPerformanceTest()
    {
        std::cout << "运行优先级性能测试..." << std::endl;
        
        const uint64_t MessageCount = 3000;
        const size_t MessageSize = 2048; // 2KB
        
        // 测试无优先级
        QueueConfig NoPriorityConfig;
        NoPriorityConfig.Name = "no_priority_queue";
        NoPriorityConfig.MaxSize = 30000;
        NoPriorityConfig.EnablePriority = false;
        MQ.CreateQueue(NoPriorityConfig);
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        for (uint64_t i = 0; i < MessageCount; ++i)
        {
            auto Msg = CreateTestMessage(i, MessageSize);
            MQ.SendMessage("no_priority_queue", Msg);
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto NoPriorityDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
        
        // 测试有优先级
        QueueConfig PriorityConfig;
        PriorityConfig.Name = "priority_queue";
        PriorityConfig.MaxSize = 30000;
        PriorityConfig.EnablePriority = true;
        MQ.CreateQueue(PriorityConfig);
        
        StartTime = std::chrono::high_resolution_clock::now();
        
        for (uint64_t i = 0; i < MessageCount; ++i)
        {
            auto Msg = CreateTestMessage(i, MessageSize);
            MQ.SendMessage("priority_queue", Msg);
        }
        
        EndTime = std::chrono::high_resolution_clock::now();
        auto PriorityDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
        
        BenchmarkResult Result;
        Result.TestName = "优先级性能测试";
        Result.MessageCount = MessageCount;
        Result.DurationNs = PriorityDuration.count();
        Result.Throughput = static_cast<double>(MessageCount) / (PriorityDuration.count() / 1e9);
        Result.LatencyMs = static_cast<double>(PriorityDuration.count()) / MessageCount / 1e6;
        
        double PriorityOverhead = static_cast<double>(PriorityDuration.count()) / NoPriorityDuration.count();
        
        std::cout << "  完成: " << MessageCount << " 消息, "
                  << std::fixed << std::setprecision(2) 
                  << Result.Throughput << " msg/s, "
                  << "优先级开销: " << PriorityOverhead << "x" << std::endl;
        
        return Result;
    }

    BenchmarkResult RunBatchProcessingTest()
    {
        std::cout << "运行批处理性能测试..." << std::endl;
        
        const uint64_t TotalMessages = 20000;
        const uint64_t BatchSize = 100;
        const uint64_t BatchCount = TotalMessages / BatchSize;
        const size_t MessageSize = 512; // 512B
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        for (uint64_t Batch = 0; Batch < BatchCount; ++Batch)
        {
            auto BatchStartTime = std::chrono::high_resolution_clock::now();
            
            std::vector<MessagePtr> Messages;
            for (uint64_t i = 0; i < BatchSize; ++i)
            {
                auto Msg = CreateTestMessage(Batch * BatchSize + i, MessageSize);
                Messages.push_back(Msg);
            }
            
            // 逐个发送消息（模拟批处理）
            for (const auto& Msg : Messages)
            {
                MQ.SendMessage("benchmark_queue", Msg);
            }
            
            auto BatchEndTime = std::chrono::high_resolution_clock::now();
            auto BatchDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(BatchEndTime - BatchStartTime);
            
            // 更新增强指标
            EnhancedExporter.UpdateBatchPerformance("benchmark_queue", BatchDuration.count(), BatchSize);
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
        
        BenchmarkResult Result;
        Result.TestName = "批处理性能测试";
        Result.MessageCount = TotalMessages;
        Result.DurationNs = Duration.count();
        Result.Throughput = static_cast<double>(TotalMessages) / (Duration.count() / 1e9);
        Result.LatencyMs = static_cast<double>(Duration.count()) / TotalMessages / 1e6;
        
        // 获取批处理统计
        const auto& BatchStats = EnhancedExporter.GetBatchStats("benchmark_queue");
        Result.P95LatencyMs = BatchStats.GetP95DurationMs();
        Result.P99LatencyMs = BatchStats.GetP99DurationMs();
        
        std::cout << "  完成: " << TotalMessages << " 消息 (" << BatchCount << " 批次), "
                  << std::fixed << std::setprecision(2) 
                  << Result.Throughput << " msg/s, "
                  << Result.LatencyMs << " ms 平均延迟, "
                  << "P95: " << Result.P95LatencyMs << " ms" << std::endl;
        
        return Result;
    }

    BenchmarkResult RunZeroCopyTest()
    {
        std::cout << "运行零拷贝性能测试..." << std::endl;
        
        const uint64_t OperationCount = 5000;
        const size_t DataSize = 8192; // 8KB
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        for (uint64_t i = 0; i < OperationCount; ++i)
        {
            auto OpStartTime = std::chrono::high_resolution_clock::now();
            
            // 模拟零拷贝操作
            auto Msg = CreateTestMessage(i, DataSize);
            MQ.SendMessage("benchmark_queue", Msg);
            
            auto OpEndTime = std::chrono::high_resolution_clock::now();
            auto OpDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(OpEndTime - OpStartTime);
            
            // 更新增强指标
            EnhancedExporter.UpdateZeroCopyPerformance(OpDuration.count());
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
        
        BenchmarkResult Result;
        Result.TestName = "零拷贝性能测试";
        Result.MessageCount = OperationCount;
        Result.DurationNs = Duration.count();
        Result.Throughput = static_cast<double>(OperationCount) / (Duration.count() / 1e9);
        Result.LatencyMs = static_cast<double>(Duration.count()) / OperationCount / 1e6;
        
        // 获取零拷贝统计
        const auto& ZeroCopyStats = EnhancedExporter.GetZeroCopyStats();
        Result.P95LatencyMs = ZeroCopyStats.GetP95DurationMs();
        Result.P99LatencyMs = ZeroCopyStats.GetP99DurationMs();
        
        std::cout << "  完成: " << OperationCount << " 操作, "
                  << std::fixed << std::setprecision(2) 
                  << Result.Throughput << " ops/s, "
                  << Result.LatencyMs << " ms 平均延迟, "
                  << "P95: " << Result.P95LatencyMs << " ms" << std::endl;
        
        return Result;
    }

    BenchmarkResult RunTransactionTest()
    {
        std::cout << "运行事务性能测试..." << std::endl;
        
        const uint64_t TransactionCount = 1000;
        const uint64_t MessagesPerTx = 10;
        const size_t MessageSize = 256; // 256B
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        for (uint64_t i = 0; i < TransactionCount; ++i)
        {
            auto TxStartTime = std::chrono::high_resolution_clock::now();
            
            // 开始事务
            TransactionId TxId = MQ.BeginTransaction("benchmark_transaction", 30000);
            if (TxId == 0) continue;
            
            // 在事务中发送消息
            for (uint64_t j = 0; j < MessagesPerTx; ++j)
            {
                auto Msg = CreateTestMessage(i * MessagesPerTx + j, MessageSize);
                MQ.SendMessageInTransaction(TxId, "benchmark_queue", Msg);
            }
            
            // 提交事务
            auto CommitStartTime = std::chrono::high_resolution_clock::now();
            QueueResult CommitResult = MQ.CommitTransaction(TxId);
            auto CommitEndTime = std::chrono::high_resolution_clock::now();
            auto CommitDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(CommitEndTime - CommitStartTime);
            
            auto TxEndTime = std::chrono::high_resolution_clock::now();
            auto TxDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(TxEndTime - TxStartTime);
            
            // 更新增强指标
            bool Committed = (CommitResult == QueueResult::SUCCESS);
            EnhancedExporter.UpdateTransactionPerformance(
                Committed, !Committed, false, false,
                CommitDuration.count(), 0
            );
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
        
        BenchmarkResult Result;
        Result.TestName = "事务性能测试";
        Result.MessageCount = TransactionCount * MessagesPerTx;
        Result.DurationNs = Duration.count();
        Result.Throughput = static_cast<double>(TransactionCount) / (Duration.count() / 1e9);
        Result.LatencyMs = static_cast<double>(Duration.count()) / TransactionCount / 1e6;
        
        // 获取事务统计
        const auto& TxStats = EnhancedExporter.GetTransactionStats();
        Result.P95LatencyMs = TxStats.GetP95CommitTimeMs();
        Result.P99LatencyMs = TxStats.GetP99CommitTimeMs();
        
        std::cout << "  完成: " << TransactionCount << " 事务, "
                  << std::fixed << std::setprecision(2) 
                  << Result.Throughput << " tx/s, "
                  << Result.LatencyMs << " ms 平均延迟, "
                  << "成功率: " << TxStats.GetSuccessRate() * 100 << "%" << std::endl;
        
        return Result;
    }

    BenchmarkResult RunConcurrentTest()
    {
        std::cout << "运行并发性能测试..." << std::endl;
        
        const uint64_t ThreadCount = 4;
        const uint64_t MessagesPerThread = 2500;
        const size_t MessageSize = 1024; // 1KB
        
        auto StartTime = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> Threads;
        for (uint64_t ThreadId = 0; ThreadId < ThreadCount; ++ThreadId)
        {
            Threads.emplace_back([this, ThreadId, MessagesPerThread, MessageSize]() {
                for (uint64_t i = 0; i < MessagesPerThread; ++i)
                {
                    auto Msg = CreateTestMessage(ThreadId * MessagesPerThread + i, MessageSize);
                    MQ.SendMessage("benchmark_queue", Msg);
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& Thread : Threads)
        {
            Thread.join();
        }
        
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Duration = std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);
        
        BenchmarkResult Result;
        Result.TestName = "并发性能测试";
        Result.MessageCount = ThreadCount * MessagesPerThread;
        Result.DurationNs = Duration.count();
        Result.Throughput = static_cast<double>(Result.MessageCount) / (Duration.count() / 1e9);
        Result.LatencyMs = static_cast<double>(Duration.count()) / Result.MessageCount / 1e6;
        
        std::cout << "  完成: " << ThreadCount << " 线程, " << Result.MessageCount << " 消息, "
                  << std::fixed << std::setprecision(2) 
                  << Result.Throughput << " msg/s, "
                  << Result.LatencyMs << " ms 平均延迟" << std::endl;
        
        return Result;
    }

    MessagePtr CreateTestMessage(uint64_t Id, size_t Size)
    {
        auto Msg = std::make_shared<Message>();
        Msg->Header.Id = Id;
        Msg->Header.Type = MessageType::TEXT;
        Msg->Header.Priority = MessagePriority::NORMAL;
        Msg->Header.Timestamp = GetCurrentTimestamp();
        
        // 生成测试数据
        std::stringstream Ss;
        Ss << "Benchmark message " << Id << " with size " << Size;
        std::string BaseData = Ss.str();
        
        // 填充到指定大小
        std::string Data;
        while (Data.size() < Size)
        {
            Data += BaseData + " | ";
        }
        Data.resize(Size);
        
        Msg->Payload = MessagePayload(Data);
        return Msg;
    }

    void GenerateReport(const std::vector<BenchmarkResult>& Results)
    {
        std::cout << std::endl;
        std::cout << "=== 性能基准测试报告 ===" << std::endl;
        std::cout << "结束时间: " << GetCurrentTimestampString() << std::endl;
        std::cout << std::endl;
        
        std::cout << std::left << std::setw(25) << "测试名称"
                  << std::setw(12) << "消息数"
                  << std::setw(12) << "吞吐量"
                  << std::setw(12) << "平均延迟"
                  << std::setw(12) << "P95延迟"
                  << std::setw(12) << "P99延迟"
                  << std::endl;
        
        std::cout << std::string(85, '-') << std::endl;
        
        for (const auto& Result : Results)
        {
            std::cout << std::left << std::setw(25) << Result.TestName
                      << std::setw(12) << Result.MessageCount
                      << std::setw(12) << std::fixed << std::setprecision(0) << Result.Throughput
                      << std::setw(12) << std::fixed << std::setprecision(2) << Result.LatencyMs
                      << std::setw(12) << std::fixed << std::setprecision(2) << Result.P95LatencyMs
                      << std::setw(12) << std::fixed << std::setprecision(2) << Result.P99LatencyMs
                      << std::endl;
        }
        
        std::cout << std::endl;
        std::cout << "=== 性能总结 ===" << std::endl;
        
        // 找到最佳性能
        auto MaxThroughput = std::max_element(Results.begin(), Results.end(),
            [](const BenchmarkResult& a, const BenchmarkResult& b) {
                return a.Throughput < b.Throughput;
            });
        
        auto MinLatency = std::min_element(Results.begin(), Results.end(),
            [](const BenchmarkResult& a, const BenchmarkResult& b) {
                return a.LatencyMs < b.LatencyMs;
            });
        
        std::cout << "最高吞吐量: " << MaxThroughput->TestName 
                  << " (" << std::fixed << std::setprecision(0) << MaxThroughput->Throughput << " msg/s)" << std::endl;
        std::cout << "最低延迟: " << MinLatency->TestName 
                  << " (" << std::fixed << std::setprecision(2) << MinLatency->LatencyMs << " ms)" << std::endl;
        
        std::cout << std::endl;
        std::cout << "=== 测试完成 ===" << std::endl;
    }

    MessageTimestamp GetCurrentTimestamp()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string GetCurrentTimestampString()
    {
        auto Now = std::chrono::system_clock::now();
        auto TimeT = std::chrono::system_clock::to_time_t(Now);
        auto Tm = *std::localtime(&TimeT);
        
        std::stringstream Ss;
        Ss << std::put_time(&Tm, "%Y-%m-%d %H:%M:%S");
        return Ss.str();
    }
};

int main()
{
    try
    {
        PerformanceBenchmark Benchmark;
        Benchmark.RunAllBenchmarks();
    }
    catch (const std::exception& e)
    {
        std::cerr << "性能测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
