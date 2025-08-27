#include "Shared/MessageQueue/IMessageQueue.h"
#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace Helianthus::MessageQueue;

int main()
{
    std::cout << "=== 简单 HA 测试开始 ===" << std::endl;
    
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
    std::cout << "集群配置设置完成: 2个分片，每个分片2个副本" << std::endl;

    // 配置心跳波动概率
    Queue->SetGlobalConfig("cluster.heartbeat.flap.prob", "0.1");
    std::cout << "心跳波动概率设置为 0.1" << std::endl;

    // 设置故障转移回调
    Queue->SetLeaderChangeHandler([](ShardId Shard, const std::string& OldLeader, const std::string& NewLeader) {
        std::cout << "Leader变更: shard=" << Shard << ", old=" << OldLeader << ", new=" << NewLeader << std::endl;
    });

    Queue->SetFailoverHandler([](ShardId Shard, const std::string& FailedLeader, const std::string& TakeoverNode) {
        std::cout << "Failover发生: shard=" << Shard << ", failed_leader=" << FailedLeader << ", takeover=" << TakeoverNode << std::endl;
    });

    // 创建测试队列
    QueueConfig Config;
    Config.Name = "ha_test_queue";
    Config.MaxSize = 1000;
    Config.MaxSizeBytes = 1024 * 1024 * 100; // 100MB
    Config.MessageTtlMs = 30000; // 30秒
    Config.EnableDeadLetter = true;
    Config.EnablePriority = false;
    Config.EnableBatching = false;
    Config.MaxRetries = 3;
    Config.RetryDelayMs = 1000;
    Config.EnableRetryBackoff = true;
    Config.RetryBackoffMultiplier = 2.0;
    Config.MaxRetryDelayMs = 10000;
    Config.DeadLetterTtlMs = 86400000; // 24小时

    auto CreateResult = Queue->CreateQueue(Config);
    if (CreateResult != QueueResult::SUCCESS)
    {
        std::cout << "创建队列失败: " << static_cast<int>(CreateResult) << std::endl;
        return 1;
    }
    std::cout << "创建队列成功: " << Config.Name << std::endl;

    // 演示1：正常消息发送
    std::cout << "=== 演示1：正常消息发送 ===" << std::endl;
    
    for (int i = 1; i <= 3; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = MessageType::TEXT;
        Message->Header.Priority = MessagePriority::NORMAL;
        Message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        Message->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + 60000; // 1分钟后过期
        
        // 设置分区键以触发路由
        Message->Header.Properties["partition_key"] = "user_" + std::to_string(i % 2);
        
        std::string Payload = "HA测试消息 #" + std::to_string(i);
        Message->Payload.Data = std::vector<char>(Payload.begin(), Payload.end());
        Message->Payload.Size = Message->Payload.Data.size();

        auto SendResult = Queue->SendMessage(Config.Name, Message);
        if (SendResult == QueueResult::SUCCESS)
        {
            std::cout << "发送消息成功: id=" << Message->Header.Id 
                      << ", partition_key=" << Message->Header.Properties["partition_key"] << std::endl;
        }
        else
        {
            std::cout << "发送消息失败: id=" << Message->Header.Id 
                      << ", error=" << static_cast<int>(SendResult) << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 演示2：模拟节点故障
    std::cout << "=== 演示2：模拟节点故障 ===" << std::endl;
    
    // 设置 node-b 为不健康状态
    Queue->SetNodeHealth("node-b", false);
    std::cout << "设置 node-b 为不健康状态" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 继续发送消息，观察路由变化
    for (int i = 4; i <= 6; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = MessageType::TEXT;
        Message->Header.Priority = MessagePriority::NORMAL;
        Message->Header.Delivery = DeliveryMode::AT_LEAST_ONCE;
        Message->Header.ExpireTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
        
        Message->Header.Properties["partition_key"] = "user_" + std::to_string(i % 2);
        
        std::string Payload = "故障转移测试消息 #" + std::to_string(i);
        Message->Payload.Data = std::vector<char>(Payload.begin(), Payload.end());
        Message->Payload.Size = Message->Payload.Data.size();

        auto SendResult = Queue->SendMessage(Config.Name, Message);
        if (SendResult == QueueResult::SUCCESS)
        {
            std::cout << "发送消息成功: id=" << Message->Header.Id 
                      << ", partition_key=" << Message->Header.Properties["partition_key"] << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 演示3：查看分片状态
    std::cout << "=== 演示3：查看分片状态 ===" << std::endl;
    
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
            
            std::cout << "分片状态: shard=" << Shard.Id 
                      << ", leader=" << LeaderNode 
                      << ", healthy_followers=" << HealthyFollowers << std::endl;
        }
    }

    // 等待一段时间观察心跳
    std::cout << "等待5秒观察心跳..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 清理
    std::cout << "=== 简单 HA 测试完成 ===" << std::endl;

    return 0;
}
