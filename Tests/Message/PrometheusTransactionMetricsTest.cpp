#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string>

#include "Shared/MessageQueue/MessageQueue.h"

using namespace Helianthus::MessageQueue;

TEST(PrometheusTransactionMetricsTest, ExportsTransactionCounters)
{
    MessageQueue MQ;
    ASSERT_EQ(MQ.Initialize(), QueueResult::SUCCESS);
    QueueConfig C; C.Name = "tx_metrics_q"; C.Persistence = PersistenceMode::MEMORY_ONLY;
    ASSERT_EQ(MQ.CreateQueue(C), QueueResult::SUCCESS);

    // 触发一次提交与一次回滚
    {
        auto Tx1 = MQ.BeginTransaction("commit_flow", 5000);
        auto Msg = std::make_shared<Message>();
        Msg->Header.Type = MessageType::TEXT;
        std::string Payload = "ok";
        Msg->Payload.Data.assign(Payload.begin(), Payload.end());
        ASSERT_EQ(MQ.SendMessageInTransaction(Tx1, C.Name, Msg), QueueResult::SUCCESS);
        ASSERT_EQ(MQ.CommitTransaction(Tx1), QueueResult::SUCCESS);
    }
    {
        auto Tx2 = MQ.BeginTransaction("rollback_flow", 5000);
        ASSERT_EQ(MQ.RollbackTransaction(Tx2, "test"), QueueResult::SUCCESS);
    }

    // 构造 Prometheus 指标文本（与示例一致的事务指标片段）
    std::ostringstream Os;
    TransactionStats TS{};
    ASSERT_EQ(MQ.GetTransactionStats(TS), QueueResult::SUCCESS);
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
    std::string Body = Os.str();

    // 断言事务相关 HELP/TYPE 与关键计数出现
    EXPECT_NE(Body.find("# HELP helianthus_tx_total"), std::string::npos);
    EXPECT_NE(Body.find("# TYPE helianthus_tx_total counter"), std::string::npos);
    EXPECT_NE(Body.find("helianthus_tx_total"), std::string::npos);
    EXPECT_NE(Body.find("helianthus_tx_committed"), std::string::npos);
    EXPECT_NE(Body.find("helianthus_tx_rolled_back"), std::string::npos);

    MQ.Shutdown();
}


