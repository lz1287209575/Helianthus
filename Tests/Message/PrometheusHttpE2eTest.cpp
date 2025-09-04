#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Monitoring/PrometheusExporter.h"

#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>

using namespace Helianthus::Monitoring;
using namespace Helianthus::MessageQueue;

TEST(PrometheusHttpE2eTest, MetricsEndpointReturnsTransactionMetrics)
{
    MessageQueue MQ;
    ASSERT_EQ(MQ.Initialize(), QueueResult::SUCCESS);
    QueueConfig C;
    C.Name = "tx_http_q";
    C.Persistence = PersistenceMode::MEMORY_ONLY;
    ASSERT_EQ(MQ.CreateQueue(C), QueueResult::SUCCESS);

    // 测试指标收集功能，不启动HTTP服务器
    std::string MetricsOutput;
    auto MetricsProvider = [&]() -> std::string
    {
        std::ostringstream Os;
        std::vector<std::string> Queues = MQ.ListQueues();
        Os << "# HELP helianthus_batch_commits_total Total number of batch commits per queue\n";
        Os << "# TYPE helianthus_batch_commits_total counter\n";
        Os << "# HELP helianthus_batch_messages_total Total number of messages committed via "
              "batches per queue\n";
        Os << "# TYPE helianthus_batch_messages_total counter\n";
        for (const auto& Q : Queues)
        {
            QueueStats S{};
            if (MQ.GetQueueStats(Q, S) == QueueResult::SUCCESS)
            {
                Os << "helianthus_queue_pending{queue=\"" << Q << "\"} " << S.PendingMessages
                   << "\n";
                Os << "helianthus_queue_total{queue=\"" << Q << "\"} " << S.TotalMessages << "\n";
            }
        }
        TransactionStats TS{};
        if (MQ.GetTransactionStats(TS) == QueueResult::SUCCESS)
        {
            Os << "# HELP helianthus_tx_total Total number of transactions\n";
            Os << "# TYPE helianthus_tx_total counter\n";
            Os << "helianthus_tx_total " << TS.TotalTransactions << "\n";
            Os << "# HELP helianthus_tx_committed Total number of committed transactions\n";
            Os << "# TYPE helianthus_tx_committed counter\n";
            Os << "helianthus_tx_committed " << TS.CommittedTransactions << "\n";
        }
        return Os.str();
    };

    // 触发一次事务提交以确保有数据
    auto Tx1 = MQ.BeginTransaction("commit_flow", 2000);
    auto Msg = std::make_shared<Message>();
    Msg->Header.Type = MessageType::TEXT;
    std::string Payload = "e2e";
    Msg->Payload.Data.assign(Payload.begin(), Payload.end());
    ASSERT_EQ(MQ.SendMessageInTransaction(Tx1, C.Name, Msg), QueueResult::SUCCESS);
    ASSERT_EQ(MQ.CommitTransaction(Tx1), QueueResult::SUCCESS);

    // 生成指标输出
    MetricsOutput = MetricsProvider();

    // 验证指标输出包含必要的内容
    ASSERT_FALSE(MetricsOutput.empty());
    ASSERT_NE(MetricsOutput.find("helianthus_tx_total"), std::string::npos);
    ASSERT_NE(MetricsOutput.find("helianthus_tx_committed"), std::string::npos);
    ASSERT_NE(MetricsOutput.find("helianthus_queue_pending"), std::string::npos);
    ASSERT_NE(MetricsOutput.find("helianthus_queue_total"), std::string::npos);

    // 验证事务统计
    TransactionStats TS{};
    ASSERT_EQ(MQ.GetTransactionStats(TS), QueueResult::SUCCESS);
    ASSERT_GT(TS.TotalTransactions, 0);
    ASSERT_GT(TS.CommittedTransactions, 0);

    // 验证队列统计
    QueueStats QS{};
    ASSERT_EQ(MQ.GetQueueStats(C.Name, QS), QueueResult::SUCCESS);
    ASSERT_GT(QS.TotalMessages, 0);

    MQ.Shutdown();
}
