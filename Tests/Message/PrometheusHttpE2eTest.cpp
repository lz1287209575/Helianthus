#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string>

#include "Shared/Monitoring/PrometheusExporter.h"
#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Network/Sockets/TcpSocket.h"
#include "Shared/Network/NetworkTypes.h"

using namespace Helianthus::Monitoring;
using namespace Helianthus::MessageQueue;
using namespace Helianthus::Network;
using namespace Helianthus::Network::Sockets;

TEST(PrometheusHttpE2eTest, MetricsEndpointReturnsTransactionMetrics)
{
    MessageQueue MQ;
    ASSERT_EQ(MQ.Initialize(), QueueResult::SUCCESS);
    QueueConfig C; C.Name = "tx_http_q"; C.Persistence = PersistenceMode::MEMORY_ONLY;
    ASSERT_EQ(MQ.CreateQueue(C), QueueResult::SUCCESS);

    PrometheusExporter Exporter;
    ASSERT_TRUE(Exporter.Start(9133, [&](){
        // 复用示例的收集逻辑
        std::ostringstream Os;
        std::vector<std::string> Queues = MQ.ListQueues();
        Os << "# HELP helianthus_batch_commits_total Total number of batch commits per queue\n";
        Os << "# TYPE helianthus_batch_commits_total counter\n";
        Os << "# HELP helianthus_batch_messages_total Total number of messages committed via batches per queue\n";
        Os << "# TYPE helianthus_batch_messages_total counter\n";
        for (const auto& Q : Queues) {
            QueueStats S{}; if (MQ.GetQueueStats(Q, S) == QueueResult::SUCCESS) {
                Os << "helianthus_queue_pending{queue=\"" << Q << "\"} " << S.PendingMessages << "\n";
                Os << "helianthus_queue_total{queue=\"" << Q << "\"} " << S.TotalMessages << "\n";
            }
        }
        TransactionStats TS{};
        if (MQ.GetTransactionStats(TS) == QueueResult::SUCCESS) {
            Os << "# HELP helianthus_tx_total Total number of transactions\n";
            Os << "# TYPE helianthus_tx_total counter\n";
            Os << "helianthus_tx_total " << TS.TotalTransactions << "\n";
            Os << "# HELP helianthus_tx_committed Total number of committed transactions\n";
            Os << "# TYPE helianthus_tx_committed counter\n";
            Os << "helianthus_tx_committed " << TS.CommittedTransactions << "\n";
        }
        return Os.str();
    }));

    // 触发一次事务提交以确保有数据
    auto Tx1 = MQ.BeginTransaction("commit_flow", 2000);
    auto Msg = std::make_shared<Message>();
    Msg->Header.Type = MessageType::TEXT;
    std::string Payload = "e2e";
    Msg->Payload.Data.assign(Payload.begin(), Payload.end());
    ASSERT_EQ(MQ.SendMessageInTransaction(Tx1, C.Name, Msg), QueueResult::SUCCESS);
    ASSERT_EQ(MQ.CommitTransaction(Tx1), QueueResult::SUCCESS);

    // 连接 HTTP 并请求 /metrics
    TcpSocket Client;
    NetworkAddress Addr{"127.0.0.1", 9133};
    ASSERT_EQ(Client.Connect(Addr), NetworkError::NONE);
    const std::string HttpReq = "GET /metrics HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    size_t Sent = 0; ASSERT_EQ(Client.Send(HttpReq.data(), HttpReq.size(), Sent), NetworkError::NONE);
    ASSERT_EQ(Sent, HttpReq.size());

    // 接收响应
    std::string Resp;
    char Buf[4096]; size_t Recv = 0;
    while (true) {
        auto Err = Client.Receive(Buf, sizeof(Buf), Recv);
        if (Err == NetworkError::NONE && Recv > 0) {
            Resp.append(Buf, Buf + Recv);
            if (Recv < sizeof(Buf)) break;
        } else {
            break;
        }
    }
    Client.Disconnect();

    // 断言包含事务指标关键字
    ASSERT_NE(Resp.find("200 OK"), std::string::npos);
    ASSERT_NE(Resp.find("helianthus_tx_total"), std::string::npos);
    ASSERT_NE(Resp.find("helianthus_tx_committed"), std::string::npos);

    Exporter.Stop();
    MQ.Shutdown();
}


