#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Monitoring/EnhancedPrometheusExporter.h"

#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

using namespace Helianthus::Monitoring;
using namespace Helianthus::MessageQueue;

static std::string CollectEnhancedMetrics(MessageQueue& MQ,
                                          EnhancedPrometheusExporter& EnhancedExporter)
{
    std::ostringstream Os;
    std::vector<std::string> Queues = MQ.ListQueues();

    // 基础队列指标
    for (const auto& Q : Queues)
    {
        QueueStats S{};
        if (MQ.GetQueueStats(Q, S) == QueueResult::SUCCESS)
        {
            Os << "helianthus_queue_pending{queue=\"" << Q << "\"} " << S.PendingMessages << "\n";
            Os << "helianthus_queue_total{queue=\"" << Q << "\"} " << S.TotalMessages << "\n";
            Os << "helianthus_queue_processed{queue=\"" << Q << "\"} " << S.ProcessedMessages
               << "\n";
            Os << "helianthus_queue_deadletter{queue=\"" << Q << "\"} " << S.DeadLetterMessages
               << "\n";
            Os << "helianthus_queue_throughput{queue=\"" << Q << "\"} " << S.ThroughputPerSecond
               << "\n";
        }

        QueueMetrics M{};
        if (MQ.GetQueueMetrics(Q, M) == QueueResult::SUCCESS)
        {
            Os << "helianthus_queue_latency_p50_ms{queue=\"" << Q << "\"} " << M.P50LatencyMs
               << "\n";
            Os << "helianthus_queue_latency_p95_ms{queue=\"" << Q << "\"} " << M.P95LatencyMs
               << "\n";
            Os << "helianthus_queue_enqueue_rate{queue=\"" << Q << "\"} " << M.EnqueueRate << "\n";
            Os << "helianthus_queue_dequeue_rate{queue=\"" << Q << "\"} " << M.DequeueRate << "\n";
        }

        // 批处理统计导出
        uint64_t CommitCount = 0, MessageCount = 0;
        if (MQ.GetBatchCounters(Q, CommitCount, MessageCount) == QueueResult::SUCCESS)
        {
            Os << "helianthus_batch_commits_total{queue=\"" << Q << "\"} " << CommitCount << "\n";
            Os << "helianthus_batch_messages_total{queue=\"" << Q << "\"} " << MessageCount << "\n";
        }
    }

    // 全局性能统计
    PerformanceStats PS{};
    if (MQ.GetPerformanceStats(PS) == QueueResult::SUCCESS)
    {
        Os << "helianthus_zero_copy_duration_ms " << PS.AverageZeroCopyTimeMs << "\n";
        Os << "helianthus_batch_duration_ms " << PS.AverageBatchTimeMs << "\n";
    }

    // 事务统计
    TransactionStats TS{};
    if (MQ.GetTransactionStats(TS) == QueueResult::SUCCESS)
    {
        Os << "helianthus_tx_total " << TS.TotalTransactions << "\n";
        Os << "helianthus_tx_committed " << TS.CommittedTransactions << "\n";
        Os << "helianthus_tx_rolled_back " << TS.RolledBackTransactions << "\n";
    }

    return Os.str();
}

int main()
{
    MessageQueue MQ;
    MQ.Initialize();
    QueueConfig C;
    C.Name = "enhanced_metrics_demo";
    C.Persistence = PersistenceMode::MEMORY_ONLY;
    MQ.CreateQueue(C);

    // 创建增强的 Prometheus 导出器
    EnhancedPrometheusExporter EnhancedExporter;
    EnhancedExporter.Start(9109, [&]() { return CollectEnhancedMetrics(MQ, EnhancedExporter); });
    std::cout << "Enhanced Prometheus Exporter started on :9109 /metrics" << std::endl;

    // 启动批处理演示线程
    std::thread(
        [&MQ, &EnhancedExporter]()
        {
            const std::string Q = "enhanced_metrics_demo";
            std::random_device Rd;
            std::mt19937 Gen(Rd());
            std::normal_distribution<> DurationDist(5000000, 1000000);  // 5ms ± 1ms

            for (;;)
            {
                auto StartTime = std::chrono::high_resolution_clock::now();

                uint32_t BatchId = 0;
                if (MQ.CreateBatchForQueue(Q, BatchId) != QueueResult::SUCCESS)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                // 添加随机数量的消息到批次
                std::uniform_int_distribution<> MessageCountDist(5, 20);
                int MessageCount = MessageCountDist(Gen);

                for (int i = 0; i < MessageCount; ++i)
                {
                    auto Msg = std::make_shared<Message>();
                    std::string Payload = std::string("enhanced-demo-") + std::to_string(i);
                    Msg->Header.Type = MessageType::TEXT;
                    Msg->Payload.Data.assign(Payload.begin(), Payload.end());
                    MQ.AddToBatch(BatchId, Msg);
                }

                MQ.CommitBatch(BatchId);

                auto EndTime = std::chrono::high_resolution_clock::now();
                auto Duration =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);

                // 更新增强的批处理性能统计
                EnhancedExporter.UpdateBatchPerformance(Q, Duration.count(), MessageCount);

                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        })
        .detach();

    // 启动零拷贝演示线程
    std::thread(
        [&MQ, &EnhancedExporter]()
        {
            const std::string Q = "enhanced_metrics_demo";
            std::random_device Rd;
            std::mt19937 Gen(Rd());
            std::normal_distribution<> DurationDist(1000000, 200000);  // 1ms ± 0.2ms

            for (;;)
            {
                auto StartTime = std::chrono::high_resolution_clock::now();

                const char* Data = "zero-copy-demo-data";
                ZeroCopyBuffer ZB{};
                MQ.CreateZeroCopyBuffer(Data, strlen(Data), ZB);
                MQ.SendMessageZeroCopy(Q, ZB);
                MQ.ReleaseZeroCopyBuffer(ZB);

                auto EndTime = std::chrono::high_resolution_clock::now();
                auto Duration =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(EndTime - StartTime);

                // 更新增强的零拷贝性能统计
                EnhancedExporter.UpdateZeroCopyPerformance(Duration.count());

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        })
        .detach();

    // 启动事务演示线程
    std::thread(
        [&MQ, &EnhancedExporter]()
        {
            const std::string Q = "enhanced_metrics_demo";
            std::random_device Rd;
            std::mt19937 Gen(Rd());
            std::normal_distribution<> DurationDist(10000000, 2000000);  // 10ms ± 2ms

            for (;;)
            {
                auto StartTime = std::chrono::high_resolution_clock::now();

                TransactionId TxId = 0;
                TxId = MQ.BeginTransaction("", 30000);
                if (TxId != 0)
                {
                    // 添加事务操作
                    auto Msg = std::make_shared<Message>();
                    std::string Payload = "transaction-demo-message";
                    Msg->Header.Type = MessageType::TEXT;
                    Msg->Payload.Data.assign(Payload.begin(), Payload.end());

                    if (MQ.SendMessageInTransaction(TxId, Q, Msg) == QueueResult::SUCCESS)
                    {
                        // 模拟一些处理时间
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));

                        auto CommitStartTime = std::chrono::high_resolution_clock::now();
                        bool Success = MQ.CommitTransaction(TxId) == QueueResult::SUCCESS;
                        auto CommitEndTime = std::chrono::high_resolution_clock::now();
                        auto CommitDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            CommitEndTime - CommitStartTime);

                        // 更新增强的事务性能统计
                        EnhancedExporter.UpdateTransactionPerformance(
                            Success,
                            !Success,
                            false,
                            false,
                            Success ? CommitDuration.count() : 0,
                            0);
                    }
                    else
                    {
                        MQ.RollbackTransaction(TxId);
                        EnhancedExporter.UpdateTransactionPerformance(
                            false, true, false, false, 0, 0);
                    }
                }

                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        })
        .detach();

    // 定期打印统计信息
    std::thread(
        [&EnhancedExporter]()
        {
            for (;;)
            {
                std::this_thread::sleep_for(std::chrono::seconds(10));

                std::cout << "\n=== Enhanced Metrics Summary ===" << std::endl;

                // 批处理统计
                const auto& BatchStats = EnhancedExporter.GetBatchStats("enhanced_metrics_demo");
                std::cout << "Batch Stats:" << std::endl;
                std::cout << "  Total Batches: " << BatchStats.TotalBatches.load() << std::endl;
                std::cout << "  Total Messages: " << BatchStats.TotalMessages.load() << std::endl;
                std::cout << "  Avg Duration: " << BatchStats.GetAverageDurationMs() << " ms"
                          << std::endl;
                std::cout << "  P50 Duration: " << BatchStats.GetP50DurationMs() << " ms"
                          << std::endl;
                std::cout << "  P95 Duration: " << BatchStats.GetP95DurationMs() << " ms"
                          << std::endl;
                std::cout << "  P99 Duration: " << BatchStats.GetP99DurationMs() << " ms"
                          << std::endl;

                // 零拷贝统计
                const auto& ZeroCopyStats = EnhancedExporter.GetZeroCopyStats();
                std::cout << "Zero-Copy Stats:" << std::endl;
                std::cout << "  Total Operations: " << ZeroCopyStats.TotalOperations.load()
                          << std::endl;
                std::cout << "  Avg Duration: " << ZeroCopyStats.GetAverageDurationMs() << " ms"
                          << std::endl;
                std::cout << "  P50 Duration: " << ZeroCopyStats.GetP50DurationMs() << " ms"
                          << std::endl;
                std::cout << "  P95 Duration: " << ZeroCopyStats.GetP95DurationMs() << " ms"
                          << std::endl;
                std::cout << "  P99 Duration: " << ZeroCopyStats.GetP99DurationMs() << " ms"
                          << std::endl;

                // 事务统计
                const auto& TxStats = EnhancedExporter.GetTransactionStats();
                std::cout << "Transaction Stats:" << std::endl;
                std::cout << "  Total Transactions: " << TxStats.TotalTransactions.load()
                          << std::endl;
                std::cout << "  Committed: " << TxStats.CommittedTransactions.load() << std::endl;
                std::cout << "  Rolled Back: " << TxStats.RolledBackTransactions.load()
                          << std::endl;
                std::cout << "  Success Rate: " << (TxStats.GetSuccessRate() * 100) << "%"
                          << std::endl;
                std::cout << "  Avg Commit Time: " << TxStats.GetAverageCommitTimeMs() << " ms"
                          << std::endl;
                std::cout << "  P95 Commit Time: " << TxStats.GetP95CommitTimeMs() << " ms"
                          << std::endl;

                std::cout << "================================" << std::endl;
            }
        })
        .detach();

    // 简单阻塞
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
