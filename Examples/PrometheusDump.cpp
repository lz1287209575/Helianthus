#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

#include "Shared/MessageQueue/MessageQueue.h"

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

        QueueMetrics M{};
        if (MQ.GetQueueMetrics(Q, M) == QueueResult::SUCCESS)
        {
            Os << "helianthus_queue_latency_p50_ms{queue=\"" << Q << "\"} " << M.P50LatencyMs << "\n";
            Os << "helianthus_queue_latency_p95_ms{queue=\"" << Q << "\"} " << M.P95LatencyMs << "\n";
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
        Os << "# HELP helianthus_zero_copy_duration_ms Average zero-copy duration in ms\n";
        Os << "# TYPE helianthus_zero_copy_duration_ms gauge\n";
        Os << "helianthus_zero_copy_duration_ms " << PS.AverageZeroCopyTimeMs << "\n";
        Os << "# HELP helianthus_batch_duration_ms Average batch duration in ms\n";
        Os << "# TYPE helianthus_batch_duration_ms gauge\n";
        Os << "helianthus_batch_duration_ms " << PS.AverageBatchTimeMs << "\n";
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
    }
    return Os.str();
}

int main()
{
    MessageQueue MQ;
    MQ.Initialize();
    QueueConfig C; C.Name = "dump_metrics"; C.Persistence = PersistenceMode::MEMORY_ONLY;
    MQ.CreateQueue(C);

    // 触发零拷贝与批处理一次，以便有非零指标
    const char* Data = "hello";
    ZeroCopyBuffer ZB{};
    MQ.CreateZeroCopyBuffer(Data, 5, ZB);
    MQ.SendMessageZeroCopy(C.Name, ZB);
    MQ.ReleaseZeroCopyBuffer(ZB);

    uint32_t BatchId = 0;
    MQ.CreateBatchForQueue(C.Name, BatchId);
    auto Msg = std::make_shared<Message>();
    Msg->Header.Type = MessageType::TEXT;
    std::string Payload = "world";
    Msg->Payload.Data.assign(Payload.begin(), Payload.end());
    MQ.AddToBatch(BatchId, Msg);
    MQ.CommitBatch(BatchId);

    std::cout << CollectMetrics(MQ) << std::endl;
    MQ.Shutdown();
    return 0;
}


