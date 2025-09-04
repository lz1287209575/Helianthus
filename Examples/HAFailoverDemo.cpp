#include "Shared/Common/LogCategories.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/StructuredLogger.h"
#include "Shared/MessageQueue/IMessageQueue.h"
#include "Shared/MessageQueue/MessageQueue.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

using namespace Helianthus::MessageQueue;

int main()
{
    // 初始化结构化日志
    Helianthus::Common::StructuredLoggerConfig StructuredLogCfg;
    StructuredLogCfg.EnableConsole = true;  // 启用控制台输出
    StructuredLogCfg.EnableFile = false;    // 禁用文件输出（避免沙盒问题）
    StructuredLogCfg.FilePath = "logs/ha_failover_demo.log";
    Helianthus::Common::StructuredLogger::Initialize(StructuredLogCfg);

    // 设置线程本地上下文
    Helianthus::Common::StructuredLogger::SetThreadField("request_id", "ha_demo_001");
    Helianthus::Common::StructuredLogger::SetThreadField("session_id", "session_ha_001");
    Helianthus::Common::StructuredLogger::SetThreadField("user_id", "admin");

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus HA 故障转移演示开始 ===");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus HA 故障转移演示 ===");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "日志系统初始化完成");

    // 创建消息队列实例
    auto Queue = std::make_unique<MessageQueue>();
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建消息队列实例");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建消息队列实例");

    // 初始化消息队列
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化消息队列...");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化消息队列...");
    auto InitResult = Queue->Initialize();
    if (InitResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "消息队列初始化失败: {}",
              static_cast<int>(InitResult));
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "消息队列初始化失败: {}",
              static_cast<int>(InitResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息队列初始化成功");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "消息队列初始化成功");

    // 配置集群
    ClusterConfig Cluster;

    // 创建2个分片，每个分片2个副本
    Cluster.Shards.resize(2);

    // 分片0：node-a (Leader), node-b (Follower)
    Cluster.Shards[0].Id = 0;
    Cluster.Shards[0].Replicas.push_back({"node-a", ReplicaRole::LEADER, true});
    Cluster.Shards[0].Replicas.push_back({"node-b", ReplicaRole::FOLLOWER, true});

    // 分片1：node-b (Leader), node-a (Follower)
    Cluster.Shards[1].Id = 1;
    Cluster.Shards[1].Replicas.push_back({"node-b", ReplicaRole::LEADER, true});
    Cluster.Shards[1].Replicas.push_back({"node-a", ReplicaRole::FOLLOWER, true});

    // 设置集群配置
    Queue->SetClusterConfig(Cluster);
    H_LOG(MQ,
          Helianthus::Common::LogVerbosity::Display,
          "集群配置设置完成: 2个分片，每个分片2个副本");

    // 配置心跳波动概率（增加故障模拟频率）
    Queue->SetGlobalConfig("cluster.heartbeat.flap.prob", "0.1");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "心跳波动概率设置为 0.1");

    // 设置故障转移回调
    Queue->SetLeaderChangeHandler(
        [](ShardId Shard, const std::string& OldLeader, const std::string& NewLeader)
        {
            H_LOG(MQ,
                  Helianthus::Common::LogVerbosity::Warning,
                  "Leader变更: shard={}, old={}, new={}",
                  Shard,
                  OldLeader,
                  NewLeader);

            Helianthus::Common::LogFields F;
            F.AddField("shard", static_cast<uint32_t>(Shard));
            F.AddField("old_leader", OldLeader);
            F.AddField("new_leader", NewLeader);
            F.AddField("event_type", "leader_change");
            Helianthus::Common::StructuredLogger::Log(Helianthus::Common::StructuredLogLevel::WARN,
                                                      "MQ",
                                                      "Leader change detected",
                                                      F,
                                                      __FILE__,
                                                      __LINE__,
                                                      SPDLOG_FUNCTION);
        });

    Queue->SetFailoverHandler(
        [](ShardId Shard, const std::string& FailedLeader, const std::string& TakeoverNode)
        {
            H_LOG(MQ,
                  Helianthus::Common::LogVerbosity::Error,
                  "Failover发生: shard={}, failed_leader={}, takeover={}",
                  Shard,
                  FailedLeader,
                  TakeoverNode);

            Helianthus::Common::LogFields F;
            F.AddField("shard", static_cast<uint32_t>(Shard));
            F.AddField("failed_leader", FailedLeader);
            F.AddField("takeover_node", TakeoverNode);
            F.AddField("event_type", "failover");
            Helianthus::Common::StructuredLogger::Log(Helianthus::Common::StructuredLogLevel::ERROR,
                                                      "MQ",
                                                      "Failover occurred",
                                                      F,
                                                      __FILE__,
                                                      __LINE__,
                                                      SPDLOG_FUNCTION);
        });

    // 创建测试队列
    QueueConfig Config;
    Config.Name = "ha_test_queue";
    Config.MaxSize = 1000;
    Config.MaxSizeBytes = 1024 * 1024 * 100;  // 100MB
    Config.MessageTtlMs = 30000;              // 30秒
    Config.EnableDeadLetter = true;
    Config.EnablePriority = false;
    Config.EnableBatching = false;
    Config.MaxRetries = 3;
    Config.RetryDelayMs = 1000;
    Config.EnableRetryBackoff = true;
    Config.RetryBackoffMultiplier = 2.0;
    Config.MaxRetryDelayMs = 10000;
    Config.DeadLetterTtlMs = 86400000;  // 24小时

    auto CreateResult = Queue->CreateQueue(Config);
    if (CreateResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Error,
              "创建队列失败: {}",
              static_cast<int>(CreateResult));
        return 1;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建队列成功: {}", Config.Name);

    // 演示1：正常消息发送
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 演示1：正常消息发送 ===");

    for (int i = 1; i <= 5; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = MessageType::TEXT;
        Message->Header.Priority = MessagePriority::NORMAL;
        Message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        Message->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count() +
                                     60000;  // 1分钟后过期

        // 设置分区键以触发路由
        Message->Header.Properties["partition_key"] = "user_" + std::to_string(i % 2);

        std::string Payload = "HA测试消息 #" + std::to_string(i);
        Message->Payload.Data = std::vector<char>(Payload.begin(), Payload.end());
        Message->Payload.Size = Message->Payload.Data.size();

        auto SendResult = Queue->SendMessage(Config.Name, Message);
        if (SendResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ,
                  Helianthus::Common::LogVerbosity::Display,
                  "发送消息成功: id={}, partition_key={}",
                  Message->Header.Id,
                  Message->Header.Properties["partition_key"]);
        }
        else
        {
            H_LOG(MQ,
                  Helianthus::Common::LogVerbosity::Error,
                  "发送消息失败: id={}, error={}",
                  Message->Header.Id,
                  static_cast<int>(SendResult));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 演示2：模拟节点故障
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 演示2：模拟节点故障 ===");

    // 设置 node-b 为不健康状态
    Queue->SetNodeHealth("node-b", false);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "设置 node-b 为不健康状态");

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 继续发送消息，观察路由变化
    for (int i = 6; i <= 10; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = MessageType::TEXT;
        Message->Header.Priority = MessagePriority::NORMAL;
        Message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        Message->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count() +
                                     60000;

        Message->Header.Properties["partition_key"] = "user_" + std::to_string(i % 2);

        std::string Payload = "故障转移测试消息 #" + std::to_string(i);
        Message->Payload.Data = std::vector<char>(Payload.begin(), Payload.end());
        Message->Payload.Size = Message->Payload.Data.size();

        auto SendResult = Queue->SendMessage(Config.Name, Message);
        if (SendResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ,
                  Helianthus::Common::LogVerbosity::Display,
                  "发送消息成功: id={}, partition_key={}",
                  Message->Header.Id,
                  Message->Header.Properties["partition_key"]);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 演示3：恢复节点健康
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 演示3：恢复节点健康 ===");

    Queue->SetNodeHealth("node-b", true);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "恢复 node-b 为健康状态");

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 继续发送消息
    for (int i = 11; i <= 15; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = MessageType::TEXT;
        Message->Header.Priority = MessagePriority::NORMAL;
        Message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        Message->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count() +
                                     60000;

        Message->Header.Properties["partition_key"] = "user_" + std::to_string(i % 2);

        std::string Payload = "恢复测试消息 #" + std::to_string(i);
        Message->Payload.Data = std::vector<char>(Payload.begin(), Payload.end());
        Message->Payload.Size = Message->Payload.Data.size();

        auto SendResult = Queue->SendMessage(Config.Name, Message);
        if (SendResult == QueueResult::SUCCESS)
        {
            H_LOG(MQ,
                  Helianthus::Common::LogVerbosity::Display,
                  "发送消息成功: id={}, partition_key={}",
                  Message->Header.Id,
                  Message->Header.Properties["partition_key"]);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 演示4：查看分片状态
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 演示4：查看分片状态 ===");

    std::vector<ShardInfo> ShardStatuses;
    auto StatusResult = Queue->GetClusterShardStatuses(ShardStatuses);
    if (StatusResult == QueueResult::SUCCESS)
    {
        for (const auto& Shard : ShardStatuses)
        {
            std::string LeaderNode = "无";
            uint32_t HealthyFollowers = 0;

            for (const auto& Replica : Shard.Replicas)
            {
                if (Replica.Role == ReplicaRole::LEADER)
                {
                    LeaderNode = Replica.NodeId + (Replica.Healthy ? "(健康)" : "(不健康)");
                }
                else if (Replica.Healthy)
                {
                    HealthyFollowers++;
                }
            }

            H_LOG(MQ,
                  Helianthus::Common::LogVerbosity::Display,
                  "分片状态: shard={}, leader={}, healthy_followers={}",
                  Shard.Id,
                  LeaderNode,
                  HealthyFollowers);
        }
    }

    // 演示5：查看队列指标
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 演示5：查看队列指标 ===");

    QueueMetrics Metrics;
    auto MetricsResult = Queue->GetQueueMetrics(Config.Name, Metrics);
    if (MetricsResult == QueueResult::SUCCESS)
    {
        H_LOG(MQ,
              Helianthus::Common::LogVerbosity::Display,
              "队列指标: queue={}, pending={}, total={}, processed={}, dlq={}, retried={}, "
              "enq_rate={:.2f}/s, deq_rate={:.2f}/s, p50={:.2f}ms, p95={:.2f}ms",
              Metrics.QueueName,
              Metrics.PendingMessages,
              Metrics.TotalMessages,
              Metrics.ProcessedMessages,
              Metrics.DeadLetterMessages,
              Metrics.RetriedMessages,
              Metrics.EnqueueRate,
              Metrics.DequeueRate,
              Metrics.P50LatencyMs,
              Metrics.P95LatencyMs);
    }

    // 等待一段时间观察心跳和故障转移
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待10秒观察心跳和故障转移...");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待10秒观察心跳和故障转移...");
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 清理
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== HA 故障转移演示完成 ===");
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== HA 故障转移演示完成 ===");

    // 清除线程本地上下文
    Helianthus::Common::StructuredLogger::ClearAllThreadFields();

    return 0;
}
