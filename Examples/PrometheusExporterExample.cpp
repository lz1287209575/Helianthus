#include <iostream>
#include <sstream>
#include <vector>

#include "Shared/Monitoring/PrometheusExporter.h"
#include "Shared/MessageQueue/MessageQueue.h"

using namespace Helianthus::Monitoring;
using namespace Helianthus::MessageQueue;

static std::string CollectMetrics(MessageQueue& MQ)
{
    std::ostringstream Os;
    std::vector<std::string> Queues = MQ.ListQueues();
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
    }

    // 全局性能统计
    PerformanceStats PS{};
    if (MQ.GetPerformanceStats(PS) == QueueResult::SUCCESS)
    {
        Os << "helianthus_perf_total_allocations " << PS.TotalAllocations << "\n";
        Os << "helianthus_perf_total_deallocations " << PS.TotalDeallocations << "\n";
        Os << "helianthus_perf_bytes_current " << PS.CurrentBytesAllocated << "\n";
        Os << "helianthus_perf_bytes_peak " << PS.PeakBytesAllocated << "\n";
        Os << "helianthus_perf_mem_hit_rate " << PS.MemoryPoolHitRate << "\n";
        Os << "helianthus_perf_zero_copy_ops " << PS.ZeroCopyOperations << "\n";
        Os << "helianthus_perf_batch_ops " << PS.BatchOperations << "\n";
        Os << "helianthus_perf_alloc_time_avg_ms " << PS.AverageAllocationTimeMs << "\n";
        Os << "helianthus_perf_free_time_avg_ms " << PS.AverageDeallocationTimeMs << "\n";
        Os << "helianthus_perf_zero_time_avg_ms " << PS.AverageZeroCopyTimeMs << "\n";
        Os << "helianthus_perf_batch_time_avg_ms " << PS.AverageBatchTimeMs << "\n";
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
    // 简单阻塞
    while (true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }
    return 0;
}


