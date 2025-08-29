#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>

#include "Shared/Monitoring/PrometheusExporter.h"
#include "Shared/MessageQueue/MessageQueue.h"

using namespace Helianthus::Monitoring;
using namespace Helianthus::MessageQueue;

static std::string CollectMetrics(MessageQueue& MQ)
{
    std::ostringstream Os;
    std::vector<std::string> Queues = MQ.ListQueues();
    // 批处理指标帮助与类型
    Os << "# HELP helianthus_batch_commits_total Total number of batch commits per queue\n";
    Os << "# TYPE helianthus_batch_commits_total counter\n";
    Os << "# HELP helianthus_batch_messages_total Total number of messages committed via batches per queue\n";
    Os << "# TYPE helianthus_batch_messages_total counter\n";

    for (const auto& Q : Queues)
    {
        QueueStats S{};
        if (MQ.GetQueueStats(Q, S) == QueueResult::SUCCESS)
        {
            Os << "helianthus_queue_pending{queue=\"" << Q << "\"} " << S.PendingMessages << "\n";
            Os << "helianthus_queue_total{queue=\"" << Q << "\"} " << S.TotalMessages << "\n";
            Os << "helianthus_queue_processed{queue=\"" << Q << "\"} " << S.ProcessedMessages << "\n";
            Os << "helianthus_queue_deadletter{queue=\"" << Q << "\"} " << S.DeadLetterMessages << "\n";
            Os << "helianthus_queue_throughput{queue=\"" << Q << "\"} " << S.ThroughputPerSecond << "\n";
        }

        // 队列延迟分位与速率（QueueMetrics）
        QueueMetrics M{};
        if (MQ.GetQueueMetrics(Q, M) == QueueResult::SUCCESS)
        {
            Os << "helianthus_queue_latency_p50_ms{queue=\"" << Q << "\"} " << M.P50LatencyMs << "\n";
            Os << "helianthus_queue_latency_p95_ms{queue=\"" << Q << "\"} " << M.P95LatencyMs << "\n";
            Os << "helianthus_queue_enqueue_rate{queue=\"" << Q << "\"} " << M.EnqueueRate << "\n";
            Os << "helianthus_queue_dequeue_rate{queue=\"" << Q << "\"} " << M.DequeueRate << "\n";
        }

        // 压缩/加密统计（如已启用）
        CompressionStats CS{};
        if (MQ.GetCompressionStats(Q, CS) == QueueResult::SUCCESS)
        {
            Os << "helianthus_compress_total{queue=\"" << Q << "\"} " << CS.TotalMessages << "\n";
            Os << "helianthus_compress_compressed{queue=\"" << Q << "\"} " << CS.CompressedMessages << "\n";
            Os << "helianthus_compress_ratio{queue=\"" << Q << "\"} " << CS.CompressionRatio << "\n";
            Os << "helianthus_compress_time_avg_ms{queue=\"" << Q << "\"} " << CS.AverageCompressionTimeMs << "\n";
            Os << "helianthus_decompress_time_avg_ms{queue=\"" << Q << "\"} " << CS.AverageDecompressionTimeMs << "\n";
        }
        EncryptionStats ES{};
        if (MQ.GetEncryptionStats(Q, ES) == QueueResult::SUCCESS)
        {
            Os << "helianthus_encrypt_total{queue=\"" << Q << "\"} " << ES.TotalMessages << "\n";
            Os << "helianthus_encrypt_encrypted{queue=\"" << Q << "\"} " << ES.EncryptedMessages << "\n";
            Os << "helianthus_encrypt_time_avg_ms{queue=\"" << Q << "\"} " << ES.AverageEncryptionTimeMs << "\n";
            Os << "helianthus_decrypt_time_avg_ms{queue=\"" << Q << "\"} " << ES.AverageDecryptionTimeMs << "\n";
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
        Os << "# HELP helianthus_perf_total_allocations Total allocation operations\n";
        Os << "# TYPE helianthus_perf_total_allocations counter\n";
        Os << "helianthus_perf_total_allocations " << PS.TotalAllocations << "\n";
        Os << "# HELP helianthus_perf_total_deallocations Total deallocation operations\n";
        Os << "# TYPE helianthus_perf_total_deallocations counter\n";
        Os << "helianthus_perf_total_deallocations " << PS.TotalDeallocations << "\n";
        Os << "# HELP helianthus_perf_bytes_current Currently allocated bytes\n";
        Os << "# TYPE helianthus_perf_bytes_current gauge\n";
        Os << "helianthus_perf_bytes_current " << PS.CurrentBytesAllocated << "\n";
        Os << "# HELP helianthus_perf_bytes_peak Peak allocated bytes\n";
        Os << "# TYPE helianthus_perf_bytes_peak gauge\n";
        Os << "helianthus_perf_bytes_peak " << PS.PeakBytesAllocated << "\n";
        Os << "# HELP helianthus_perf_mem_hit_rate Memory pool hit rate\n";
        Os << "# TYPE helianthus_perf_mem_hit_rate gauge\n";
        Os << "helianthus_perf_mem_hit_rate " << PS.MemoryPoolHitRate << "\n";
        Os << "# HELP helianthus_perf_zero_copy_ops Zero-copy operations count\n";
        Os << "# TYPE helianthus_perf_zero_copy_ops counter\n";
        Os << "helianthus_perf_zero_copy_ops " << PS.ZeroCopyOperations << "\n";
        Os << "# HELP helianthus_perf_batch_ops Batch operations count\n";
        Os << "# TYPE helianthus_perf_batch_ops counter\n";
        Os << "helianthus_perf_batch_ops " << PS.BatchOperations << "\n";
        Os << "# HELP helianthus_perf_alloc_time_avg_ms Average allocation time in ms\n";
        Os << "# TYPE helianthus_perf_alloc_time_avg_ms gauge\n";
        Os << "helianthus_perf_alloc_time_avg_ms " << PS.AverageAllocationTimeMs << "\n";
        Os << "# HELP helianthus_perf_free_time_avg_ms Average deallocation time in ms\n";
        Os << "# TYPE helianthus_perf_free_time_avg_ms gauge\n";
        Os << "helianthus_perf_free_time_avg_ms " << PS.AverageDeallocationTimeMs << "\n";
        // 新增：零拷贝/批处理耗时（与 TODO 对齐）
        Os << "# HELP helianthus_zero_copy_duration_ms Average zero-copy duration in ms\n";
        Os << "# TYPE helianthus_zero_copy_duration_ms gauge\n";
        Os << "helianthus_zero_copy_duration_ms " << PS.AverageZeroCopyTimeMs << "\n";
        Os << "# HELP helianthus_batch_duration_ms Average batch duration in ms\n";
        Os << "# TYPE helianthus_batch_duration_ms gauge\n";
        Os << "helianthus_batch_duration_ms " << PS.AverageBatchTimeMs << "\n";
        Os << "# HELP helianthus_perf_zero_time_avg_ms Average zero-copy processing time in ms\n";
        Os << "# TYPE helianthus_perf_zero_time_avg_ms gauge\n";
        Os << "helianthus_perf_zero_time_avg_ms " << PS.AverageZeroCopyTimeMs << "\n";
        Os << "# HELP helianthus_perf_batch_time_avg_ms Average batch processing time in ms\n";
        Os << "# TYPE helianthus_perf_batch_time_avg_ms gauge\n";
        Os << "helianthus_perf_batch_time_avg_ms " << PS.AverageBatchTimeMs << "\n";
    }

    // 事务统计
    TransactionStats TS{};
    if (MQ.GetTransactionStats(TS) == QueueResult::SUCCESS)
    {
        Os << "# HELP helianthus_tx_total Total number of transactions\n";
        Os << "# TYPE helianthus_tx_total counter\n";
        Os << "helianthus_tx_total " << TS.TotalTransactions << "\n";
        Os << "# HELP helianthus_tx_committed Total number of committed transactions\n";
        Os << "# TYPE helianthus_tx_committed counter\n";
        Os << "helianthus_tx_committed " << TS.CommittedTransactions << "\n";
        Os << "# HELP helianthus_tx_rolled_back Total number of rolled back transactions\n";
        Os << "# TYPE helianthus_tx_rolled_back counter\n";
        Os << "helianthus_tx_rolled_back " << TS.RolledBackTransactions << "\n";
        Os << "# HELP helianthus_tx_timeout Total number of timed-out transactions\n";
        Os << "# TYPE helianthus_tx_timeout counter\n";
        Os << "helianthus_tx_timeout " << TS.TimeoutTransactions << "\n";
        Os << "# HELP helianthus_tx_failed Total number of failed transactions\n";
        Os << "# TYPE helianthus_tx_failed counter\n";
        Os << "helianthus_tx_failed " << TS.FailedTransactions << "\n";
        Os << "# HELP helianthus_tx_success_rate Success rate of transactions\n";
        Os << "# TYPE helianthus_tx_success_rate gauge\n";
        Os << "helianthus_tx_success_rate " << TS.SuccessRate << "\n";
        Os << "# HELP helianthus_tx_avg_commit_ms Average commit time in ms\n";
        Os << "# TYPE helianthus_tx_avg_commit_ms gauge\n";
        Os << "helianthus_tx_avg_commit_ms " << TS.AverageCommitTimeMs << "\n";
        Os << "# HELP helianthus_tx_avg_rollback_ms Average rollback time in ms\n";
        Os << "# TYPE helianthus_tx_avg_rollback_ms gauge\n";
        Os << "helianthus_tx_avg_rollback_ms " << TS.AverageRollbackTimeMs << "\n";
    }
    return Os.str();
}

int main()
{
    MessageQueue MQ;
    MQ.Initialize();
    QueueConfig C; C.Name = "metrics_demo"; C.Persistence = PersistenceMode::MEMORY_ONLY;
    MQ.CreateQueue(C);

    PrometheusExporter Exporter;
    Exporter.Start(9108, [&](){ return CollectMetrics(MQ); });
    std::cout << "Exporter started on :9108 /metrics" << std::endl;
    // 启动一个简单的批处理演示线程，周期性提交批次以便观察 batch 统计增长
    std::thread([&MQ]() {
        const std::string Q = "metrics_demo";
        for (;;) {
            uint32_t BatchId = 0;
            if (MQ.CreateBatchForQueue(Q, BatchId) != QueueResult::SUCCESS) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            for (int i = 0; i < 10; ++i) {
                auto Msg = std::make_shared<Message>();
                std::string Payload = std::string("demo-") + std::to_string(i);
                Msg->Header.Type = MessageType::TEXT;
                Msg->Payload.Data.assign(Payload.begin(), Payload.end());
                MQ.AddToBatch(BatchId, Msg);
            }
            MQ.CommitBatch(BatchId);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }).detach();
    // 简单阻塞
    while (true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }
    return 0;
}


