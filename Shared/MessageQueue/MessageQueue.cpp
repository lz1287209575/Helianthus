#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"
#include "Shared/Common/StructuredLogger.h"

#include <algorithm>
#include <future>
#include <iostream>
#include <sstream>
#include <numeric>
#include <cstring>
#include <zlib.h>  // GZIP 压缩
#include <openssl/evp.h>  // OpenSSL 加密
#include <openssl/rand.h>  // 随机数生成

namespace Helianthus::MessageQueue
{
// QueueData 实现
MessageQueue::QueueData::QueueData(const QueueConfig& Config)
    : Config(Config),
      PriorityMessages(
          [](const MessagePtr& a, const MessagePtr& b)
          { return static_cast<int>(a->Header.Priority) < static_cast<int>(b->Header.Priority); })
{
    Stats.CreatedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
}

bool MessageQueue::QueueData::ShouldUsePriorityQueue() const
{
    return Config.EnablePriority;
}

void MessageQueue::QueueData::AddMessage(MessagePtr Message)
{
    if (ShouldUsePriorityQueue())
    {
        PriorityMessages.push(Message);
    }
    else
    {
        Messages.push(Message);
    }
    Stats.TotalMessages++;
    Stats.PendingMessages++;

    // 记录入队事件时间戳
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    EnqueueTimestamps.push_back(now);
}

MessagePtr MessageQueue::QueueData::GetNextMessage()
{
    MessagePtr Message = nullptr;

    if (ShouldUsePriorityQueue() && !PriorityMessages.empty())
    {
        Message = PriorityMessages.top();
        PriorityMessages.pop();
    }
    else if (!Messages.empty())
    {
        Message = Messages.front();
        Messages.pop();
    }

    if (Message)
    {
        Stats.PendingMessages--;
        // 记录出队事件时间戳
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        DequeueTimestamps.push_back(now);
    }

    return Message;
}

bool MessageQueue::QueueData::IsEmpty() const
{
    return Messages.empty() && PriorityMessages.empty();
}

size_t MessageQueue::QueueData::GetMessageCount() const
{
    return Messages.size() + PriorityMessages.size();
}

// TopicData 实现
MessageQueue::TopicData::TopicData(const TopicConfig& Config) : Config(Config)
{
    Stats.CreatedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
}

// ScheduledMessage 实现
MessageQueue::ScheduledMessage::ScheduledMessage(MessagePtr Msg,
                                                 const std::string& Queue,
                                                 MessageTimestamp ExecTime,
                                                 uint32_t Interval,
                                                 uint32_t Count)
    : Message(Msg),
      QueueName(Queue),
      ExecuteTime(ExecTime),
      IntervalMs(Interval),
      RemainingCount(Count),
      IsRecurring(Interval > 0)
{
}

// MessageQueue 主要实现
MessageQueue::MessageQueue()
    : Initialized(false), ShuttingDown(false), NextMessageId(1), StopConsumerThreads(false)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "创建消息队列实例");
}

MessageQueue::~MessageQueue()
{
    if (Initialized.load())
    {
        Shutdown();
    }
}

QueueResult MessageQueue::Initialize()
{
    if (Initialized.load())
    {
        return QueueResult::SUCCESS;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "开始初始化消息队列系统");

    ShuttingDown = false;
    StopConsumerThreads = false;

    // 从配置读取分片参数（可选）
    {
        std::shared_lock<std::shared_mutex> Lock(ConfigMutex);
        auto ItShards = GlobalConfig.find("cluster.shards");
        if (ItShards != GlobalConfig.end())
        {
            try { ShardCount = static_cast<uint32_t>(std::stoul(ItShards->second)); } catch (...) {}
        }
        auto ItVnodes = GlobalConfig.find("cluster.shard.vnodes");
        if (ItVnodes != GlobalConfig.end())
        {
            try { ShardVirtualNodes = static_cast<uint32_t>(std::stoul(ItVnodes->second)); } catch (...) {}
        }
    }

    // 初始化持久化管理器
    PersistenceMgr = std::make_unique<PersistenceManager>();
    PersistenceSettings.Type = PersistenceType::FILE_BASED;
    PersistenceSettings.DataDirectory = "./message_queue_data";
    
    auto PersistenceResult = PersistenceMgr->Initialize(PersistenceSettings);
    if (PersistenceResult != QueueResult::SUCCESS)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "持久化管理器初始化失败，将使用内存模式, code={}", static_cast<int>(PersistenceResult));
        // 继续初始化，但不使用持久化功能
    }

    // 构建一致性哈希环（单进程：使用本地节点ID）
    RebuildShardRing();

    // 初始化 WAL 每分片容器
    {
        std::unique_lock<std::shared_mutex> Lock(ClusterMutex);
        WalPerShard.clear();
        WalPerShard.resize(std::max(ShardCount, 1u));
        for (uint32_t I = 0; I < WalPerShard.size(); ++I)
        {
            // 初始化Follower位点
            if (I < Cluster.Shards.size())
            {
                for (const auto& R : Cluster.Shards[I].Replicas)
                {
                    if (R.Role == ReplicaRole::FOLLOWER)
                    {
                        WalPerShard[I].FollowerAppliedIndex[R.NodeId] = 0;
                    }
                }
            }
        }
    }

    // 启动调度器线程
    SchedulerThread = std::thread(&MessageQueue::ProcessScheduledMessages, this);

    // 启动DLQ监控线程
    StopDeadLetterMonitor = false;
    DeadLetterMonitorThread = std::thread(&MessageQueue::ProcessDeadLetterMonitoring, this);

    // 启动指标监控线程
    StopMetricsMonitor = false;
    MetricsMonitorThread = std::thread(&MessageQueue::ProcessMetricsMonitoring, this);

    // 启动心跳线程
    StopHeartbeat = false;
    HeartbeatThread = std::thread(&MessageQueue::ProcessHeartbeat, this);

    // 启动事务超时监控线程
    StopTransactionTimeout = false;
    TransactionTimeoutThread = std::thread(&MessageQueue::ProcessTransactionTimeouts, this);

    // 启动告警监控线程
    StopAlertMonitor = false;
    AlertMonitorThread = std::thread(&MessageQueue::ProcessAlertMonitoring, this);

    // 初始化内存池
    InitializeMemoryPool();

    // 初始化全局统计
    {
        std::unique_lock<std::shared_mutex> Lock(StatsMutex);
        GlobalStats = QueueStats{};
        GlobalStats.CreatedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
    }

    Initialized = true;
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "消息队列系统初始化成功");

    return QueueResult::SUCCESS;
}

void MessageQueue::Shutdown()
{
    if (!Initialized.load())
    {
        return;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "开始关闭消息队列系统");

    ShuttingDown = true;
    StopConsumerThreads = true;
    StopDeadLetterMonitor = true;  // 停止DLQ监控线程
    {
        std::lock_guard<std::mutex> lk(MetricsMonitorMutex);
        StopMetricsMonitor = true; // 停止指标监控线程
    }
    MetricsMonitorCondition.notify_all();

    // 停止告警监控线程
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "停止告警监控线程");
    StopAlertMonitor = true;
    AlertMonitorCondition.notify_all();
    if (AlertMonitorThread.joinable())
    {
        AlertMonitorThread.join();
    }

    // 停止事务超时监控线程
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "停止事务超时监控线程");
    StopTransactionTimeout = true;
    TransactionTimeoutCondition.notify_all();
    if (TransactionTimeoutThread.joinable())
    {
        TransactionTimeoutThread.join();
    }

    // 停止DLQ监控线程
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "停止DLQ监控线程");
    DeadLetterMonitorCondition.notify_all();
    if (DeadLetterMonitorThread.joinable())
    {
        // 直接join，如果线程已经停止，join会立即返回
        DeadLetterMonitorThread.join();
    }

    // 停止指标监控线程
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "停止指标监控线程");
    if (MetricsMonitorThread.joinable())
    {
        MetricsMonitorThread.join();
    }

    // 停止心跳线程
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "停止心跳线程");
    {
        std::lock_guard<std::mutex> Lock(HeartbeatMutex);
        StopHeartbeat = true;
    }
    HeartbeatCondition.notify_all();
    if (HeartbeatThread.joinable())
    {
        HeartbeatThread.join();
    }

    // 停止调度器线程
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "停止调度器线程");
    SchedulerCondition.notify_all();
    if (SchedulerThread.joinable())
    {
        // 直接join，如果线程已经停止，join会立即返回
        SchedulerThread.join();
    }

    // 停止所有消费者线程
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "停止消费者线程");
    for (auto& Thread : ConsumerThreads)
    {
        if (Thread.joinable())
        {
            // 直接join，如果线程已经停止，join会立即返回
            Thread.join();
        }
    }
    ConsumerThreads.clear();

    // 清理队列和主题
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "清理队列和主题");
    {
        std::unique_lock<std::shared_mutex> Lock(QueuesMutex);
        for (auto& [Name, Queue] : Queues)
        {
            Queue->NotifyCondition.notify_all();
        }
        Queues.clear();
    }

    {
        std::unique_lock<std::shared_mutex> Lock(TopicsMutex);
        Topics.clear();
    }

    // 清理内存池
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "清理内存池");
    CleanupMemoryPool();

    // 关闭持久化管理器（带超时）
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "关闭持久化管理器");
    if (PersistenceMgr)
    {
        try {
            PersistenceMgr->Shutdown();
        } catch (const std::exception& e) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "持久化管理器关闭异常: {}", e.what());
        }
        PersistenceMgr.reset();
    }

    Initialized = false;
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "消息队列系统关闭完成");
}

bool MessageQueue::IsInitialized() const
{
    return Initialized.load();
}

QueueResult MessageQueue::CreateQueue(const QueueConfig& Config)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    if (!ValidateQueueConfig(Config))
    {
        return QueueResult::INVALID_PARAMETER;
    }

    std::unique_lock<std::shared_mutex> Lock(QueuesMutex);

    if (Queues.find(Config.Name) != Queues.end())
    {
        return QueueResult::QUEUE_NOT_FOUND;  // Queue already exists
    }

    auto QueueData = std::make_unique<MessageQueue::QueueData>(Config);
    Queues[Config.Name] = std::move(QueueData);

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "创建队列: {}", Config.Name);
    NotifyEvent(Config.Name, "QueueCreated", "Queue created successfully");

    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::DeleteQueue(const std::string& QueueName)
{
    if (!Initialized.load())
    {
        return QueueResult::INTERNAL_ERROR;
    }

    std::unique_lock<std::shared_mutex> Lock(QueuesMutex);

    auto It = Queues.find(QueueName);
    if (It == Queues.end())
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    // 通知所有等待的消费者
    It->second->NotifyCondition.notify_all();
    Queues.erase(It);

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "删除队列: {}", QueueName);
    NotifyEvent(QueueName, "QueueDeleted", "Queue deleted successfully");

    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::PurgeQueue(const std::string& QueueName)
{
    auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::unique_lock<std::shared_mutex> Lock(Queue->Mutex);

    // 清空所有消息
    while (!Queue->Messages.empty())
    {
        Queue->Messages.pop();
    }
    while (!Queue->PriorityMessages.empty())
    {
        Queue->PriorityMessages.pop();
    }

    Queue->PendingAcknowledgments.clear();
    Queue->Stats.PendingMessages = 0;

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "清空队列: {}", QueueName);
    NotifyEvent(QueueName, "QueuePurged", "Queue purged successfully");

    return QueueResult::SUCCESS;
}

bool MessageQueue::QueueExists(const std::string& QueueName) const
{
    std::shared_lock<std::shared_mutex> Lock(QueuesMutex);
    return Queues.find(QueueName) != Queues.end();
}

std::vector<std::string> MessageQueue::ListQueues() const
{
    std::shared_lock<std::shared_mutex> Lock(QueuesMutex);
    std::vector<std::string> QueueNames;
    QueueNames.reserve(Queues.size());

    for (const auto& [Name, Queue] : Queues)
    {
        QueueNames.push_back(Name);
    }

    return QueueNames;
}

QueueResult MessageQueue::GetQueueInfo(const std::string& QueueName, QueueConfig& OutConfig) const
{
    const auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::shared_lock<std::shared_mutex> Lock(Queue->Mutex);
    OutConfig = Queue->Config;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::UpdateQueueConfig(const std::string& QueueName, const QueueConfig& Config)
{
    auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    if (!ValidateQueueConfig(Config))
    {
        return QueueResult::INVALID_PARAMETER;
    }

    std::unique_lock<std::shared_mutex> Lock(Queue->Mutex);
    Queue->Config = Config;

    NotifyEvent(QueueName, "QueueConfigUpdated", "Queue configuration updated");
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::SendMessage(const std::string& QueueName, MessagePtr Message)
{
    if (!ValidateMessage(Message))
    {
        return QueueResult::INVALID_PARAMETER;
    }

    auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    // 分配消息ID
    Message->Header.Id = GenerateMessageId();
    Message->Status = MessageStatus::SENT;
    auto enqueueStart = std::chrono::high_resolution_clock::now();
    {
        Helianthus::Common::LogFields f;
        f.AddField("queue", QueueName);
        f.AddField("message_id", static_cast<uint64_t>(Message->Header.Id));
        f.AddField("priority", static_cast<int32_t>(static_cast<int>(Message->Header.Priority)));
        f.AddField("delivery", static_cast<int32_t>(static_cast<int>(Message->Header.Delivery)));
        Helianthus::Common::StructuredLogger::Log(
            Helianthus::Common::StructuredLogLevel::INFO,
            "MQ",
            "Message sent",
            f,
            __FILE__, __LINE__, SPDLOG_FUNCTION);
    }

    std::unique_lock<std::shared_mutex> Lock(Queue->Mutex);

    // 检查队列容量
    if (Queue->GetMessageCount() >= Queue->Config.MaxSize)
    {
        Helianthus::Common::LogFields f; f.AddField("queue", QueueName);
        Helianthus::Common::StructuredLogger::Log(
            Helianthus::Common::StructuredLogLevel::WARN,
            "MQ",
            "Queue full",
            f,
            __FILE__, __LINE__, SPDLOG_FUNCTION);
        return QueueResult::QUEUE_FULL;
    }

    // 检查消息大小
    if (Message->Payload.Size > Queue->Config.MaxSizeBytes / Queue->Config.MaxSize)
    {
        Helianthus::Common::LogFields f; f.AddField("queue", QueueName);
        f.AddField("payload_size", static_cast<uint64_t>(Message->Payload.Size));
        Helianthus::Common::StructuredLogger::Log(
            Helianthus::Common::StructuredLogLevel::WARN,
            "MQ",
            "Message too large",
            f,
            __FILE__, __LINE__, SPDLOG_FUNCTION);
        return QueueResult::MESSAGE_TOO_LARGE;
    }

    // 添加消息到队列
    Queue->AddMessage(Message);
    // 埋点：入队延迟（近似，当前实现为0，保留接口以便后续扩展）
    auto enqueueEnd = std::chrono::high_resolution_clock::now();
    const double enqueueLatencyMs = std::chrono::duration<double, std::milli>(enqueueEnd - enqueueStart).count();
    RecordLatencySample(Queue, enqueueLatencyMs);

    // 如果启用了消息路由，尝试路由消息
    auto RouteResult = RouteMessage(QueueName, Message);

    // 模拟复制：根据路由信息尝试复制到副本（占位）
    {
        int ShardIndex = -1;
        auto ItShard = Message->Header.Properties.find("routed_shard");
        if (ItShard != Message->Header.Properties.end())
        {
            try { ShardIndex = std::stoi(ItShard->second); } catch (...) { ShardIndex = -1; }
        }
        if (ShardIndex >= 0 && MinReplicationAcks > 0)
        {
            // 追加 WAL 占位
            {
                std::unique_lock<std::shared_mutex> Lk(ClusterMutex);
                if (static_cast<size_t>(ShardIndex) < WalPerShard.size())
                {
                    auto& Wal = WalPerShard[static_cast<size_t>(ShardIndex)];
                    WalEntry E;
                    E.Index = Wal.Entries.size() + 1;
                    E.Id = Message->Header.Id;
                    E.Queue = QueueName;
                    E.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    Wal.Entries.push_back(std::move(E));
                }
            }

            uint32_t Acks = SimulateReplication(static_cast<uint32_t>(ShardIndex), Message->Header.Id);
            ReplicationEvents.fetch_add(1, std::memory_order_relaxed);
            ReplicationAcksTotal.fetch_add(Acks, std::memory_order_relaxed);
            Helianthus::Common::LogFields F;
            F.AddField("queue", QueueName);
            F.AddField("message_id", static_cast<uint64_t>(Message->Header.Id));
            F.AddField("shard", ShardIndex);
            F.AddField("replication_min_acks", MinReplicationAcks);
            F.AddField("replication_acks", Acks);
            // 滞后度占位（健康Follower数 - ACK数）
            uint32_t HealthyFollowers = 0;
            {
                std::shared_lock<std::shared_mutex> Lk(ClusterMutex);
                if (static_cast<size_t>(ShardIndex) < Cluster.Shards.size())
                {
                    for (const auto& R : Cluster.Shards[static_cast<size_t>(ShardIndex)].Replicas)
                    {
                        if (R.Role == ReplicaRole::FOLLOWER && R.Healthy) HealthyFollowers++;
                    }
                }
            }
            const int32_t Lag = static_cast<int32_t>(HealthyFollowers) - static_cast<int32_t>(Acks);
            F.AddField("replication_lag", Lag);
            // 导出Follower已应用位点占位（第一位Follower）
            {
                std::shared_lock<std::shared_mutex> Lk(ClusterMutex);
                if (static_cast<size_t>(ShardIndex) < WalPerShard.size())
                {
                    uint64_t AnyFollowerIndex = 0;
                    for (const auto& P : WalPerShard[static_cast<size_t>(ShardIndex)].FollowerAppliedIndex)
                    {
                        AnyFollowerIndex = std::max(AnyFollowerIndex, P.second);
                    }
                    F.AddField("follower_applied_index", static_cast<uint64_t>(AnyFollowerIndex));
                    F.AddField("leader_log_len", static_cast<uint64_t>(WalPerShard[static_cast<size_t>(ShardIndex)].Entries.size()));
                }
            }
            Helianthus::Common::StructuredLogger::Log(
                Helianthus::Common::StructuredLogLevel::INFO,
                "MQ",
                "Replication simulated",
                F,
                __FILE__, __LINE__, SPDLOG_FUNCTION);
        }
    }

    Lock.unlock();

    // 如果启用了持久化，保存消息到磁盘
    if (PersistenceMgr && Queue->Config.Persistence != PersistenceMode::MEMORY_ONLY)
    {
        auto PersistenceResult = PersistenceMgr->SaveMessage(QueueName, Message);
        if (PersistenceResult != QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning,
                 "持久化消息失败，继续处理 queue={} id={} code={}",
                 QueueName, static_cast<uint64_t>(Message->Header.Id), static_cast<int>(PersistenceResult));
            // 继续处理，不因为持久化失败而阻塞消息发送
        }
    }

    // 通知等待的消费者
    Queue->NotifyCondition.notify_one();

    UpdateQueueStats(QueueName, true);
    NotifyEvent(QueueName, "MessageSent", "Message sent to queue");

    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ReceiveMessage(const std::string& QueueName,
                                         MessagePtr& OutMessage,
                                         uint32_t TimeoutMs)
{
    auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::unique_lock<std::shared_mutex> Lock(Queue->Mutex);

    // 检查是否有消息可用
    if (Queue->IsEmpty())
    {
        if (TimeoutMs == 0)
        {
            // 非阻塞模式，立即返回
            return QueueResult::TIMEOUT;
        }
        else if (TimeoutMs > 0)
        {
            // 有超时的等待
            auto TimeoutPoint =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(TimeoutMs);
            if (!Queue->NotifyCondition.wait_until(
                    Lock,
                    TimeoutPoint,
                    [Queue, this] { return !Queue->IsEmpty() || ShuttingDown.load(); }))
            {
                return QueueResult::TIMEOUT;
            }
        }
        else
        {
            // 无限等待
            Queue->NotifyCondition.wait(
                Lock, [Queue, this] { return !Queue->IsEmpty() || ShuttingDown.load(); });
        }
    }

    // 获取下一个消息
    OutMessage = Queue->GetNextMessage();
    if (!OutMessage)
    {
        return QueueResult::TIMEOUT;
    }

    // 检查消息是否过期
    if (IsMessageExpired(OutMessage))
    {
        // 释放当前锁，避免死锁
        Lock.unlock();
        MoveToDeadLetter(QueueName, OutMessage, DeadLetterReason::EXPIRED);
        return QueueResult::TIMEOUT;  // 递归尝试获取下一个消息
    }

    // 检查是否需要重试
    // 检查消息是否过期
    if (IsMessageExpired(OutMessage))
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "消息已过期，移动到死信队列: messageId={}", OutMessage->Header.Id);
        
        // 释放当前锁，避免死锁
        Lock.unlock();
        MoveToDeadLetter(QueueName, OutMessage, DeadLetterReason::EXPIRED);
        UpdateQueueStats(QueueName, false, false);
        return QueueResult::TIMEOUT;
    }
    
    if (OutMessage->Header.RetryCount > 0)
    {
        auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        
        if (OutMessage->Header.NextRetryTime > Now)
        {
            // 消息还没有到重试时间，跳过
            return QueueResult::TIMEOUT;
        }
    }

    OutMessage->Status = MessageStatus::DELIVERED;
    {
        Helianthus::Common::LogFields f;
        f.AddField("queue", QueueName);
        f.AddField("message_id", static_cast<uint64_t>(OutMessage->Header.Id));
        Helianthus::Common::StructuredLogger::Log(
            Helianthus::Common::StructuredLogLevel::INFO,
            "MQ",
            "Message received",
            f,
            __FILE__, __LINE__, SPDLOG_FUNCTION);
    }

    // 如果不是自动确认，添加到待确认列表
    const auto ConsumerIt = Queue->Consumers.begin();
    if (ConsumerIt != Queue->Consumers.end() && !ConsumerIt->second.AutoAcknowledge)
    {
        Queue->PendingAcknowledgments[OutMessage->Header.Id] = OutMessage;
    }

    // 释放当前锁，避免死锁
    Lock.unlock();
    UpdateQueueStats(QueueName, false, true);
    NotifyEvent(QueueName, "MessageReceived", "Message received from queue");

    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::AcknowledgeMessage(const std::string& QueueName, MessageId MessageId)
{
    auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::unique_lock<std::shared_mutex> Lock(Queue->Mutex);

    auto It = Queue->PendingAcknowledgments.find(MessageId);
    if (It == Queue->PendingAcknowledgments.end())
    {
        return QueueResult::INVALID_PARAMETER;
    }

    It->second->Status = MessageStatus::ACKNOWLEDGED;
    Queue->Stats.ProcessedMessages++;
    Queue->PendingAcknowledgments.erase(It);

    // 简化：以(当前时间 - 创建时间)作为处理时延样本
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch()).count();
    const double latencyMs = static_cast<double>(nowMs - It->second->CreatedTime);
    RecordLatencySample(Queue, latencyMs);

    {
        Helianthus::Common::LogFields f;
        f.AddField("queue", QueueName);
        f.AddField("message_id", static_cast<uint64_t>(MessageId));
        Helianthus::Common::StructuredLogger::Log(
            Helianthus::Common::StructuredLogLevel::INFO,
            "MQ",
            "Message acknowledged",
            f,
            __FILE__, __LINE__, SPDLOG_FUNCTION);
    }

    NotifyEvent(QueueName, "MessageAcknowledged", "Message acknowledged");
    return QueueResult::SUCCESS;
}

// 实现其他方法的简化版本
QueueResult MessageQueue::CreateTopic(const TopicConfig& Config)
{
    if (!Initialized.load() || !ValidateTopicConfig(Config))
    {
        return QueueResult::INVALID_PARAMETER;
    }

    std::unique_lock<std::shared_mutex> Lock(TopicsMutex);

    if (Topics.find(Config.Name) != Topics.end())
    {
        return QueueResult::QUEUE_NOT_FOUND;  // Topic already exists
    }

    auto Topic = std::make_unique<TopicData>(Config);
    Topics[Config.Name] = std::move(Topic);

    std::cout << "[MessageQueue] 创建主题: " << Config.Name << std::endl;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::PublishMessage(const std::string& TopicName, MessagePtr Message)
{
    if (!ValidateMessage(Message))
    {
        return QueueResult::INVALID_PARAMETER;
    }

    auto* Topic = GetTopicData(TopicName);
    if (!Topic)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    Message->Header.Id = GenerateMessageId();
    Message->Status = MessageStatus::SENT;

    return DeliverMessageToSubscribers(TopicName, Message);
}

QueueResult MessageQueue::Subscribe(const std::string& TopicName,
                                    const std::string& SubscriberId,
                                    MessageHandler Handler)
{
    auto* Topic = GetTopicData(TopicName);
    if (!Topic)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::unique_lock<std::shared_mutex> Lock(Topic->Mutex);
    Topic->Subscribers[SubscriberId] = Handler;
    Topic->Stats.ActiveSubscribers++;

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "订阅主题: {} 订阅者: {}", TopicName, SubscriberId);
    return QueueResult::SUCCESS;
}

// 内部辅助方法实现
MessageId MessageQueue::GenerateMessageId()
{
    return NextMessageId.fetch_add(1);
}

MessageQueue::QueueData* MessageQueue::GetQueueData(const std::string& QueueName)
{
    std::shared_lock<std::shared_mutex> Lock(QueuesMutex);
    auto It = Queues.find(QueueName);
    return (It != Queues.end()) ? It->second.get() : nullptr;
}

const MessageQueue::QueueData* MessageQueue::GetQueueData(const std::string& QueueName) const
{
    std::shared_lock<std::shared_mutex> Lock(QueuesMutex);
    auto It = Queues.find(QueueName);
    return (It != Queues.end()) ? It->second.get() : nullptr;
}

MessageQueue::TopicData* MessageQueue::GetTopicData(const std::string& TopicName)
{
    std::shared_lock<std::shared_mutex> Lock(TopicsMutex);
    auto It = Topics.find(TopicName);
    return (It != Topics.end()) ? It->second.get() : nullptr;
}

bool MessageQueue::ValidateMessage(MessagePtr Message)
{
    return Message && Message->Header.Type != MessageType::UNKNOWN && !Message->Payload.IsEmpty();
}

bool MessageQueue::ValidateQueueConfig(const QueueConfig& Config)
{
    return !Config.Name.empty() && Config.MaxSize > 0 && Config.MaxSizeBytes > 0;
}

bool MessageQueue::ValidateTopicConfig(const TopicConfig& Config)
{
    return !Config.Name.empty() && Config.MaxSubscribers > 0;
}

bool MessageQueue::IsMessageExpired(MessagePtr Message)
{
    if (Message->Header.ExpireTime == 0)
    {
        return false;
    }

    auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    return Now > Message->Header.ExpireTime;
}

void MessageQueue::MoveToDeadLetter(const std::string& QueueName, MessagePtr Message, DeadLetterReason Reason)
{
    auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "队列不存在，无法移动到死信队列: {}", QueueName);
        return;
    }

    // 设置死信队列信息
    Message->Status = MessageStatus::DEAD_LETTER;
    Message->Header.DeadLetterReasonValue = Reason;
    Message->Header.OriginalQueue = QueueName;
    Message->LastModifiedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();

    // 确保死信队列存在（在获取任何锁之前）
    auto DeadLetterQueueName = GetDeadLetterQueueName(QueueName);
    if (EnsureDeadLetterQueue(QueueName) == QueueResult::SUCCESS)
    {
        auto* DeadLetterQueue = GetQueueData(DeadLetterQueueName);
        if (DeadLetterQueue)
        {
            // 先获取死信队列的锁
            std::unique_lock<std::shared_mutex> DeadLetterLock(DeadLetterQueue->Mutex);
            DeadLetterQueue->DeadLetterMessages.push(Message);
            DeadLetterQueue->Stats.DeadLetterMessages++;
            DeadLetterLock.unlock();  // 立即释放死信队列锁
            
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, 
                  "消息已移动到死信队列: queue={}, messageId={}, reason={}", 
                  QueueName, Message->Header.Id, static_cast<int>(Reason));
            {
                Helianthus::Common::LogFields f;
                f.AddField("queue", QueueName);
                f.AddField("dlq", DeadLetterQueueName);
                f.AddField("message_id", static_cast<uint64_t>(Message->Header.Id));
                f.AddField("reason", static_cast<int32_t>(static_cast<int>(Reason)));
                Helianthus::Common::StructuredLogger::Log(
                    Helianthus::Common::StructuredLogLevel::WARN,
                    "MQ",
                    "Message moved to DLQ",
                    f,
                    __FILE__, __LINE__, SPDLOG_FUNCTION);
            }
        }
    }

    // 更新原队列统计（最后获取原队列锁）
    std::unique_lock<std::shared_mutex> QueueLock(Queue->Mutex);
    Queue->Stats.DeadLetterMessages++;
    
    // 根据原因更新相应统计
    switch (Reason)
    {
        case DeadLetterReason::EXPIRED:
            Queue->Stats.ExpiredMessages++;
            break;
        case DeadLetterReason::MAX_RETRIES_EXCEEDED:
            Queue->Stats.RetriedMessages++;
            break;
        case DeadLetterReason::REJECTED:
            Queue->Stats.RejectedMessages++;
            break;
        default:
            break;
    }
}

QueueResult MessageQueue::DeliverMessageToSubscribers(const std::string& TopicName,
                                                      MessagePtr Message)
{
    auto* Topic = GetTopicData(TopicName);
    if (!Topic)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::shared_lock<std::shared_mutex> Lock(Topic->Mutex);

    // 异步分发给所有订阅者
    for (const auto& [SubscriberId, Handler] : Topic->Subscribers)
    {
        if (Handler)
        {
            std::thread(
                [Handler, Message]()
                {
                    try
                    {
                        Handler(Message);
                    }
                    catch (const std::exception& e)
                    {
                        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "订阅者处理异常: {}", e.what());
                    }
                })
                .detach();
        }
    }

    Topic->Stats.TotalMessages++;
    UpdateTopicStats(TopicName, true);

    return QueueResult::SUCCESS;
}

void MessageQueue::UpdateQueueStats(const std::string& QueueName,
                                    bool MessageSent,
                                    bool MessageReceived)
{
    auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return;
    }

    std::unique_lock<std::shared_mutex> Lock(Queue->Mutex);
    if (MessageSent)
    {
        Queue->Stats.TotalMessages++;
    }
    if (MessageReceived)
    {
        Queue->Stats.ProcessedMessages++;
    }

    Queue->Stats.LastMessageTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();
}

void MessageQueue::UpdateTopicStats(const std::string& TopicName, bool MessagePublished)
{
    auto* Topic = GetTopicData(TopicName);
    if (!Topic)
    {
        return;
    }

    std::unique_lock<std::shared_mutex> Lock(Topic->Mutex);
    if (MessagePublished)
    {
        Topic->Stats.TotalMessages++;
    }

    Topic->Stats.LastMessageTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();
}

void MessageQueue::NotifyEvent(const std::string& QueueName,
                               const std::string& Event,
                               const std::string& Details)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    if (EventHandler)
    {
        EventHandler(QueueName, Event, Details);
    }
}

void MessageQueue::NotifyError(QueueResult Result, const std::string& ErrorMessage)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    if (ErrorHandlerFunc)
    {
        ErrorHandlerFunc(Result, ErrorMessage);
    }
}

void MessageQueue::ProcessScheduledMessages()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "启动消息调度线程");

    while (!ShuttingDown.load())
    {
        std::unique_lock<std::mutex> Lock(ScheduledMessagesMutex);

        // 等待直到有调度消息或需要关闭
        // 等待直到有调度消息或需要关闭
        SchedulerCondition.wait_for(Lock,
                                    std::chrono::milliseconds(100), // 改为100毫秒，提高响应性
                                    [this]
                                    { return ShuttingDown.load() || !ScheduledMessages.empty(); });

        if (ShuttingDown.load())
        {
            break;
        }

        auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();

        // 处理到期的消息
        for (auto It = ScheduledMessages.begin(); It != ScheduledMessages.end();)
        {
            if (It->ExecuteTime <= Now)
            {
                // 发送消息
                auto Result = SendMessage(It->QueueName, It->Message);

                // 处理周期性消息
                if (It->IsRecurring && (It->RemainingCount == 0 || It->RemainingCount > 1))
                {
                    It->ExecuteTime = static_cast<Helianthus::MessageQueue::MessageTimestamp>(
                        Now + It->IntervalMs);
                    if (It->RemainingCount > 1)
                    {
                        It->RemainingCount--;
                    }
                    ++It;
                }
                else
                {
                    It = ScheduledMessages.erase(It);
                }
            }
            else
            {
                ++It;
            }
        }
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "调度线程停止");
}

// 实现剩余的必要方法的简化版本
QueueResult MessageQueue::SendMessageAsync(const std::string& QueueName,
                                           MessagePtr Message,
                                           AcknowledgeHandler Handler)
{
    // 简化实现：直接调用同步版本
    auto Result = SendMessage(QueueName, Message);
    if (Handler)
    {
        Handler(Message->Header.Id, Result == QueueResult::SUCCESS);
    }
    return Result;
}

std::future<QueueResult> MessageQueue::SendMessageFuture(const std::string& QueueName,
                                                         MessagePtr Message)
{
    auto Promise = std::make_shared<std::promise<QueueResult>>();
    auto Future = Promise->get_future();

    std::thread(
        [this, QueueName, Message, Promise]()
        {
            auto Result = SendMessage(QueueName, Message);
            Promise->set_value(Result);
        })
        .detach();

    return Future;
}

QueueResult
MessageQueue::ScheduleMessage(const std::string& QueueName, MessagePtr Message, uint32_t DelayMs)
{
    if (!ValidateMessage(Message))
    {
        return QueueResult::INVALID_PARAMETER;
    }

    auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

    std::lock_guard<std::mutex> Lock(ScheduledMessagesMutex);
    ScheduledMessages.emplace_back(Message, QueueName, Now + DelayMs);
    SchedulerCondition.notify_one();

    return QueueResult::SUCCESS;
}

// 提供其他方法的基础实现
QueueResult MessageQueue::GetQueueStats(const std::string& QueueName, QueueStats& OutStats) const
{
    const auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::shared_lock<std::shared_mutex> Lock(Queue->Mutex);
    OutStats = Queue->Stats;
    return QueueResult::SUCCESS;
}

std::string MessageQueue::GetQueueInfo() const
{
    std::stringstream Info;
    Info << "MessageQueue Status:\n";
    Info << "  Initialized: " << (Initialized.load() ? "Yes" : "No") << "\n";
    Info << "  Total Queues: " << Queues.size() << "\n";
    Info << "  Total Topics: " << Topics.size() << "\n";
    return Info.str();
}

// 为编译提供空实现的方法
QueueResult MessageQueue::DeleteTopic(const std::string& TopicName)
{
    return QueueResult::SUCCESS;
}
bool MessageQueue::TopicExists(const std::string& TopicName) const
{
    return GetTopicData(TopicName) != nullptr;
}
std::vector<std::string> MessageQueue::ListTopics() const
{
    return {};
}
QueueResult MessageQueue::GetTopicInfo(const std::string& TopicName, TopicConfig& OutConfig) const
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::SendBatchMessages(const std::string& QueueName,
                                            const std::vector<MessagePtr>& Messages)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::ReceiveBatchMessages(const std::string& QueueName,
                                               std::vector<MessagePtr>& OutMessages,
                                               uint32_t MaxCount,
                                               uint32_t TimeoutMs)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::PeekMessage(const std::string& QueueName, MessagePtr& OutMessage)
{
    return QueueResult::SUCCESS;
}
QueueResult
MessageQueue::RejectMessage(const std::string& QueueName, MessageId MessageId, bool Requeue)
{
    auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::unique_lock<std::shared_mutex> Lock(Queue->Mutex);

    auto It = Queue->PendingAcknowledgments.find(MessageId);
    if (It == Queue->PendingAcknowledgments.end())
    {
        return QueueResult::MESSAGE_NOT_FOUND;
    }

    auto Message = It->second;
    Queue->PendingAcknowledgments.erase(It);

    if (Requeue && Message->CanRetry())
    {
        // 计算下次重试时间
        auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        
        uint32_t RetryDelay = Queue->Config.RetryDelayMs;
        if (Queue->Config.EnableRetryBackoff)
        {
            // 指数退避：延迟时间 = 基础延迟 * (退避倍数 ^ 重试次数)
            RetryDelay = static_cast<uint32_t>(
                Queue->Config.RetryDelayMs * 
                std::pow(Queue->Config.RetryBackoffMultiplier, Message->Header.RetryCount)
            );
            
            // 限制最大延迟时间
            if (RetryDelay > Queue->Config.MaxRetryDelayMs)
            {
                RetryDelay = Queue->Config.MaxRetryDelayMs;
            }
        }
        
        Message->Header.NextRetryTime = Now + RetryDelay;
        Message->IncrementRetry();
        Message->Status = MessageStatus::PENDING;
        
        // 重新添加到队列
        Queue->AddMessage(Message);
        Queue->Stats.RetriedMessages++;
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, 
              "消息重试: queue={}, messageId={}, retryCount={}, nextRetryTime={}", 
              QueueName, MessageId, Message->Header.RetryCount, Message->Header.NextRetryTime);
        {
            Helianthus::Common::LogFields f;
            f.AddField("queue", QueueName);
            f.AddField("message_id", static_cast<uint64_t>(MessageId));
            f.AddField("retry_count", static_cast<int32_t>(Message->Header.RetryCount));
            f.AddField("next_retry_time", static_cast<uint64_t>(Message->Header.NextRetryTime));
            Helianthus::Common::StructuredLogger::Log(
                Helianthus::Common::StructuredLogLevel::WARN,
                "MQ",
                "Message retry scheduled",
                f,
                __FILE__, __LINE__, SPDLOG_FUNCTION);
        }
        
        NotifyEvent(QueueName, "MessageRetried", "Message requeued for retry");
    }
    else
    {
        // 超过最大重试次数或不重试，移动到死信队列
        DeadLetterReason Reason = Requeue ? DeadLetterReason::MAX_RETRIES_EXCEEDED : DeadLetterReason::REJECTED;
        
        // 释放当前锁，避免死锁
        Lock.unlock();
        MoveToDeadLetter(QueueName, Message, Reason);
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, 
              "消息移动到死信队列: queue={}, messageId={}, reason=\"", 
              QueueName, MessageId, static_cast<int>(Reason));
        {
            Helianthus::Common::LogFields f;
            f.AddField("queue", QueueName);
            f.AddField("message_id", static_cast<uint64_t>(MessageId));
            f.AddField("reason", static_cast<int32_t>(static_cast<int>(Reason)));
            Helianthus::Common::StructuredLogger::Log(
                Helianthus::Common::StructuredLogLevel::WARN,
                "MQ",
                "Message dead-lettered",
                f,
                __FILE__, __LINE__, SPDLOG_FUNCTION);
        }
        
        NotifyEvent(QueueName, "MessageDeadLettered", "Message moved to dead letter queue");
    }

    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::AcknowledgeBatch(const std::string& QueueName,
                                           const std::vector<MessageId>& MessageIds)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::RegisterConsumer(const std::string& QueueName,
                                           const ConsumerConfig& Config,
                                           MessageHandler Handler)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::RegisterBatchConsumer(const std::string& QueueName,
                                                const ConsumerConfig& Config,
                                                BatchMessageHandler Handler)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::UnregisterConsumer(const std::string& QueueName,
                                             const std::string& ConsumerId)
{
    return QueueResult::SUCCESS;
}
std::vector<std::string> MessageQueue::GetActiveConsumers(const std::string& QueueName) const
{
    return {};
}
QueueResult MessageQueue::RegisterProducer(const std::string& QueueName,
                                           const ProducerConfig& Config)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::UnregisterProducer(const std::string& QueueName,
                                             const std::string& ProducerId)
{
    return QueueResult::SUCCESS;
}
std::vector<std::string> MessageQueue::GetActiveProducers(const std::string& QueueName) const
{
    return {};
}
QueueResult MessageQueue::PublishBatchMessages(const std::string& TopicName,
                                               const std::vector<MessagePtr>& Messages)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::Unsubscribe(const std::string& TopicName, const std::string& SubscriberId)
{
    return QueueResult::SUCCESS;
}
std::vector<std::string> MessageQueue::GetActiveSubscribers(const std::string& TopicName) const
{
    return {};
}
QueueResult MessageQueue::BroadcastMessage(MessagePtr Message)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::BroadcastToQueues(const std::vector<std::string>& QueueNames,
                                            MessagePtr Message)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::BroadcastToTopics(const std::vector<std::string>& TopicNames,
                                            MessagePtr Message)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::ScheduleRecurringMessage(const std::string& QueueName,
                                                   MessagePtr Message,
                                                   uint32_t IntervalMs,
                                                   uint32_t Count)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::CancelScheduledMessage(MessageId MessageId)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::SetMessageFilter(const std::string& QueueName,
                                           const std::string& FilterExpression)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::SetMessageRouter(const std::string& SourceQueue,
                                           const std::string& TargetQueue,
                                           const std::string& RoutingKey)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::RemoveMessageFilter(const std::string& QueueName)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::RemoveMessageRouter(const std::string& SourceQueue,
                                              const std::string& TargetQueue)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::GetDeadLetterMessages(const std::string& QueueName,
                                                std::vector<MessagePtr>& OutMessages,
                                                uint32_t MaxCount)
{
    auto DeadLetterQueueName = GetDeadLetterQueueName(QueueName);
    auto* DeadLetterQueue = GetQueueData(DeadLetterQueueName);
    
    if (!DeadLetterQueue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::shared_lock<std::shared_mutex> Lock(DeadLetterQueue->Mutex);
    
    OutMessages.clear();
    uint32_t Count = 0;
    
    // 从死信队列中获取消息
    while (!DeadLetterQueue->DeadLetterMessages.empty() && Count < MaxCount)
    {
        OutMessages.push_back(DeadLetterQueue->DeadLetterMessages.front());
        DeadLetterQueue->DeadLetterMessages.pop();
        Count++;
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
          "获取死信消息: queue={}, count={}", QueueName, Count);
    
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::RequeueDeadLetterMessage(const std::string& QueueName,
                                                   MessageId MessageId)
{
    auto DeadLetterQueueName = GetDeadLetterQueueName(QueueName);
    auto* DeadLetterQueue = GetQueueData(DeadLetterQueueName);
    
    if (!DeadLetterQueue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::unique_lock<std::shared_mutex> Lock(DeadLetterQueue->Mutex);
    
    // 查找指定的死信消息
    std::queue<MessagePtr> TempQueue;
    MessagePtr TargetMessage = nullptr;
    
    while (!DeadLetterQueue->DeadLetterMessages.empty())
    {
        auto Message = DeadLetterQueue->DeadLetterMessages.front();
        DeadLetterQueue->DeadLetterMessages.pop();
        
        if (Message->Header.Id == MessageId)
        {
            TargetMessage = Message;
        }
        else
        {
            TempQueue.push(Message);
        }
    }
    
    // 恢复其他消息
    while (!TempQueue.empty())
    {
        DeadLetterQueue->DeadLetterMessages.push(TempQueue.front());
        TempQueue.pop();
    }
    
    if (!TargetMessage)
    {
        return QueueResult::MESSAGE_NOT_FOUND;
    }
    
    // 重置消息状态，重新加入原队列
    TargetMessage->Status = MessageStatus::PENDING;
    TargetMessage->Header.RetryCount = 0;
    TargetMessage->Header.NextRetryTime = 0;
    TargetMessage->Header.DeadLetterReasonValue = DeadLetterReason::UNKNOWN;
    TargetMessage->LastModifiedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count();
    
    // 添加到原队列
    auto* OriginalQueue = GetQueueData(QueueName);
    if (OriginalQueue)
    {
        OriginalQueue->AddMessage(TargetMessage);
        DeadLetterQueue->Stats.DeadLetterMessages--;
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
              "死信消息重新入队: queue={}, messageId={}", QueueName, MessageId);
        
        NotifyEvent(QueueName, "MessageRequeued", "Dead letter message requeued");
    }
    
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::PurgeDeadLetterQueue(const std::string& QueueName)
{
    auto DeadLetterQueueName = GetDeadLetterQueueName(QueueName);
    auto* DeadLetterQueue = GetQueueData(DeadLetterQueueName);
    
    if (!DeadLetterQueue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::unique_lock<std::shared_mutex> Lock(DeadLetterQueue->Mutex);
    
    uint32_t PurgedCount = 0;
    while (!DeadLetterQueue->DeadLetterMessages.empty())
    {
        DeadLetterQueue->DeadLetterMessages.pop();
        PurgedCount++;
    }
    
    DeadLetterQueue->Stats.DeadLetterMessages = 0;
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
          "清空死信队列: queue={}, purgedCount={}", QueueName, PurgedCount);
    
    NotifyEvent(QueueName, "DeadLetterQueuePurged", "Dead letter queue purged");
    
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::GetTopicStats(const std::string& TopicName, QueueStats& OutStats) const
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::GetGlobalStats(QueueStats& OutStats) const
{
    return QueueResult::SUCCESS;
}
std::vector<MessagePtr> MessageQueue::GetPendingMessages(const std::string& QueueName,
                                                         uint32_t MaxCount) const
{
    return {};
}
QueueResult MessageQueue::SaveToDisk()
{
    if (!Initialized.load() || !PersistenceMgr)
    {
        return QueueResult::INTERNAL_ERROR;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "开始保存消息队列数据到磁盘");

    // 保存所有队列数据
    {
        std::shared_lock<std::shared_mutex> Lock(QueuesMutex);
        for (const auto& [QueueName, QueueData] : Queues)
        {
            auto Result = PersistenceMgr->SaveQueue(QueueName, QueueData->Config, QueueData->Stats);
            if (Result != QueueResult::SUCCESS)
            {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "保存队列失败 queue={} code={}", QueueName, static_cast<int>(Result));
                return Result;
            }
        }
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "消息队列数据保存到磁盘完成");
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::LoadFromDisk()
{
    if (!Initialized.load() || !PersistenceMgr)
    {
        return QueueResult::INTERNAL_ERROR;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "开始从磁盘加载消息队列数据");

    // 获取所有持久化的队列
    auto PersistedQueues = PersistenceMgr->ListPersistedQueues();
    
    for (const auto& QueueName : PersistedQueues)
    {
        QueueConfig Config;
        QueueStats Stats;
        
        auto Result = PersistenceMgr->LoadQueue(QueueName, Config, Stats);
        if (Result != QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "加载队列失败 queue={} code={}", QueueName, static_cast<int>(Result));
            continue;
        }

        // 创建队列
        Result = CreateQueue(Config);
        if (Result != QueueResult::SUCCESS)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "创建队列失败 queue={} code={}", QueueName, static_cast<int>(Result));
            continue;
        }

        // 加载队列中的所有消息
        std::vector<MessagePtr> Messages;
        Result = PersistenceMgr->LoadAllMessages(QueueName, Messages);
        if (Result == QueueResult::SUCCESS)
        {
            auto QueueData = GetQueueData(QueueName);
            if (QueueData)
            {
                for (const auto& Message : Messages)
                {
                    QueueData->AddMessage(Message);
                }
            }
        }
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "从磁盘加载消息队列数据完成");
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::EnablePersistence(const std::string& QueueName, PersistenceMode Mode)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::DisablePersistence(const std::string& QueueName)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::EnableReplication(const std::vector<std::string>& ReplicaNodes)
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::DisableReplication()
{
    return QueueResult::SUCCESS;
}
bool MessageQueue::IsReplicationEnabled() const
{
    return false;
}
QueueResult MessageQueue::SyncWithReplicas()
{
    return QueueResult::SUCCESS;
}
void MessageQueue::SetQueueEventHandler(QueueEventHandler Handler)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    EventHandler = Handler;
}
void MessageQueue::SetErrorHandler(ErrorHandler Handler)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    ErrorHandlerFunc = Handler;
}
void MessageQueue::RemoveAllHandlers()
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    EventHandler = nullptr;
    ErrorHandlerFunc = nullptr;
}
QueueResult MessageQueue::SetGlobalConfig(const std::string& Key, const std::string& Value)
{
    {
        std::unique_lock<std::shared_mutex> lock(ConfigMutex);
        GlobalConfig[Key] = Value;
    }

    // 动态应用部分配置
    if (Key == "metrics.interval.ms")
    {
        try
        {
            uint32_t ms = static_cast<uint32_t>(std::stoul(Value));
            if (ms < 100) ms = 100; // 下限保护
            {
                std::lock_guard<std::mutex> lk(MetricsMonitorMutex);
                MetricsIntervalMs = ms;
            }
            MetricsMonitorCondition.notify_all();
        }
        catch (...)
        {
            return QueueResult::INVALID_PARAMETER;
        }
    }
    else if (Key == "metrics.window.ms")
    {
        try
        {
            uint32_t ms = static_cast<uint32_t>(std::stoul(Value));
            if (ms < 1000) ms = 1000; // 至少1秒窗口
            {
                std::lock_guard<std::mutex> lk(MetricsMonitorMutex);
                MetricsWindowMs = ms;
            }
        }
        catch (...)
        {
            return QueueResult::INVALID_PARAMETER;
        }
    }
    else if (Key == "metrics.latency.capacity")
    {
        try
        {
            size_t cap = static_cast<size_t>(std::stoull(Value));
            if (cap < 32) cap = 32; // 下限
            // 为所有队列应用新的容量
            std::shared_lock<std::shared_mutex> ql(QueuesMutex);
            for (auto& pair : Queues)
            {
                auto* q = pair.second.get();
                std::unique_lock<std::shared_mutex> lq(q->Mutex);
                q->LatencyCapacity = cap;
                if (q->LatencySamplesMs.size() > cap)
                {
                    q->LatencySamplesMs.erase(q->LatencySamplesMs.begin(),
                                               q->LatencySamplesMs.begin() + (q->LatencySamplesMs.size() - cap));
                }
            }
        }
        catch (...)
        {
            return QueueResult::INVALID_PARAMETER;
        }
    }
    else if (Key == "cluster.shards")
    {
        try
        {
            ShardCount = static_cast<uint32_t>(std::stoul(Value));
            RebuildShardRing();
        }
        catch (...) { return QueueResult::INVALID_PARAMETER; }
    }
    else if (Key == "cluster.shard.vnodes")
    {
        try
        {
            ShardVirtualNodes = static_cast<uint32_t>(std::stoul(Value));
            RebuildShardRing();
        }
        catch (...) { return QueueResult::INVALID_PARAMETER; }
    }
    else if (Key == "cluster.heartbeat.flap.prob")
    {
        try
        {
            double Prob = std::stod(Value);
            if (Prob < 0.0) Prob = 0.0; if (Prob > 1.0) Prob = 1.0;
            std::lock_guard<std::mutex> Lk(HeartbeatMutex);
            HeartbeatFlapProbability = Prob;
        }
        catch (...) { return QueueResult::INVALID_PARAMETER; }
    }
    else if (Key == "replication.min.acks")
    {
        try
        {
            uint32_t Acks = static_cast<uint32_t>(std::stoul(Value));
            MinReplicationAcks = Acks;
        }
        catch (...) { return QueueResult::INVALID_PARAMETER; }
    }

    return QueueResult::SUCCESS;
}
std::string MessageQueue::GetGlobalConfig(const std::string& Key) const
{
    std::shared_lock<std::shared_mutex> lock(ConfigMutex);
    auto it = GlobalConfig.find(Key);
    if (it == GlobalConfig.end()) return "";
    return it->second;
}
QueueResult MessageQueue::FlushAll()
{
    return QueueResult::SUCCESS;
}
QueueResult MessageQueue::CompactQueues()
{
    return QueueResult::SUCCESS;
}
std::vector<std::string> MessageQueue::GetQueueDiagnostics(const std::string& QueueName) const
{
    return {};
}
QueueResult MessageQueue::ValidateQueue(const std::string& QueueName)
{
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::RouteMessage(const std::string& SourceQueue, MessagePtr Message)
{
    if (!Message)
    {
        return QueueResult::INVALID_PARAMETER;
    }

    // 选择路由键：优先使用消息属性 partition_key，否则使用队列名
    std::string RouteKey = SourceQueue;
    auto ItKey = Message->Header.Properties.find("partition_key");
    if (ItKey != Message->Header.Properties.end() && !ItKey->second.empty())
    {
        RouteKey = ItKey->second;
    }

    // 通过一致性哈希获得目标节点（单机：shard-i）
    const std::string NodeId = ShardRing.GetNode(RouteKey);

    // 尝试解析分片索引
    int ShardIndex = -1;
    if (!NodeId.empty())
    {
        auto Pos = NodeId.rfind('-');
        if (Pos != std::string::npos)
        {
            try { ShardIndex = std::stoi(NodeId.substr(Pos + 1)); } catch (...) { ShardIndex = -1; }
        }
    }

    // 增强路由容错：主失效自动回退与重试幂等
    std::string SelectedNodeId = NodeId;
    std::string SelectedRole = "UNKNOWN";
    bool SelectedHealthy = true;
    uint32_t RetryCount = 0;
    const uint32_t MaxRetries = 3; // 最大重试次数
    
    if (ShardIndex >= 0)
    {
        std::shared_lock<std::shared_mutex> ClusterLock(ClusterMutex);
        if (static_cast<size_t>(ShardIndex) < Cluster.Shards.size())
        {
            const auto& Replicas = Cluster.Shards[static_cast<size_t>(ShardIndex)].Replicas;
            
            // 第一优先级：健康Leader
            for (const auto& R : Replicas)
            {
                if (R.Role == ReplicaRole::LEADER && R.Healthy)
                {
                    SelectedNodeId = R.NodeId;
                    SelectedRole = "LEADER";
                    SelectedHealthy = true;
                    break;
                }
            }
            
            // 第二优先级：健康Follower（如果Leader不可用）
            if (SelectedRole != "LEADER")
            {
                for (const auto& R : Replicas)
                {
                    if (R.Healthy)
                    {
                        SelectedNodeId = R.NodeId;
                        SelectedRole = (R.Role == ReplicaRole::FOLLOWER ? "FOLLOWER" : "UNKNOWN");
                        SelectedHealthy = true;
                        break;
                    }
                }
            }
            
            // 第三优先级：任何可用副本（如果都不健康，选择第一个）
            if (!SelectedHealthy && !Replicas.empty())
            {
                SelectedNodeId = Replicas[0].NodeId;
                SelectedRole = (Replicas[0].Role == ReplicaRole::LEADER ? "LEADER" : 
                              Replicas[0].Role == ReplicaRole::FOLLOWER ? "FOLLOWER" : "UNKNOWN");
                SelectedHealthy = false;
            }
            
            // 记录重试信息到消息属性
            auto RetryIt = Message->Header.Properties.find("routing_retry_count");
            if (RetryIt != Message->Header.Properties.end())
            {
                try { RetryCount = static_cast<uint32_t>(std::stoul(RetryIt->second)); } catch (...) { RetryCount = 0; }
            }
            RetryCount++;
            Message->Header.Properties["routing_retry_count"] = std::to_string(RetryCount);
        }
    }

    // 注入路由信息到属性（便于后续副本/HA接入）
    Message->Header.Properties["routed_node"] = SelectedNodeId;
    if (ShardIndex >= 0)
    {
        Message->Header.Properties["routed_shard"] = std::to_string(ShardIndex);
    }
    if (!SelectedRole.empty())
    {
        Message->Header.Properties["routed_role"] = SelectedRole;
    }
    Message->Header.Properties["routed_healthy"] = SelectedHealthy ? "true" : "false";
    Message->Header.Properties["routing_attempt"] = std::to_string(RetryCount);

    // 结构化日志记录（包含重试信息）
    {
        Helianthus::Common::LogFields F;
        F.AddField("queue", SourceQueue);
        F.AddField("message_id", static_cast<uint64_t>(Message->Header.Id));
        F.AddField("route_key", RouteKey);
        F.AddField("node", SelectedNodeId);
        F.AddField("shard", ShardIndex);
        F.AddField("role", SelectedRole);
        F.AddField("healthy", SelectedHealthy);
        F.AddField("retry_count", static_cast<uint32_t>(RetryCount));
        F.AddField("max_retries", MaxRetries);
        
        std::string LogLevel = "INFO";
        if (RetryCount > 1)
        {
            LogLevel = "WARN";
            F.AddField("fallback_used", true);
        }
        if (!SelectedHealthy)
        {
            LogLevel = "ERROR";
            F.AddField("unhealthy_node", true);
        }
        
        Helianthus::Common::StructuredLogger::Log(
            LogLevel == "INFO" ? Helianthus::Common::StructuredLogLevel::INFO :
            LogLevel == "WARN" ? Helianthus::Common::StructuredLogLevel::WARN :
            Helianthus::Common::StructuredLogLevel::ERROR,
            "MQ",
            "Message routed",
            F,
            __FILE__, __LINE__, SPDLOG_FUNCTION);
    }

    return QueueResult::SUCCESS;
}
const MessageQueue::TopicData* MessageQueue::GetTopicData(const std::string& TopicName) const
{
    std::shared_lock<std::shared_mutex> Lock(TopicsMutex);
    auto It = Topics.find(TopicName);
    return (It != Topics.end()) ? It->second.get() : nullptr;
}

std::string MessageQueue::GetDeadLetterQueueName(const std::string& QueueName) const
{
    return QueueName + "_DLQ";
}

QueueResult MessageQueue::EnsureDeadLetterQueue(const std::string& QueueName)
{
    auto DeadLetterQueueName = GetDeadLetterQueueName(QueueName);
    
    // 检查死信队列是否已存在
    if (QueueExists(DeadLetterQueueName))
    {
        return QueueResult::SUCCESS;
    }
    
    // 创建死信队列配置
    QueueConfig DeadLetterConfig;
    DeadLetterConfig.Name = DeadLetterQueueName;
    DeadLetterConfig.Type = QueueType::DEAD_LETTER;
    DeadLetterConfig.Persistence = PersistenceMode::DISK_PERSISTENT;  // 死信队列默认持久化
    DeadLetterConfig.MaxSize = 10000;  // 死信队列可以存储更多消息
    DeadLetterConfig.MaxSizeBytes = 1024 * 1024 * 1024;  // 1GB
    DeadLetterConfig.MessageTtlMs = 86400000;  // 24小时
    DeadLetterConfig.EnableDeadLetter = false;  // 死信队列本身不再有死信队列
    DeadLetterConfig.EnablePriority = false;
    DeadLetterConfig.EnableBatching = false;
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
          "创建死信队列: {}", DeadLetterQueueName);
    {
        Helianthus::Common::LogFields f; 
        f.AddField("queue", QueueName);
        f.AddField("dlq", DeadLetterQueueName);
        Helianthus::Common::StructuredLogger::Log(
            Helianthus::Common::StructuredLogLevel::INFO,
            "MQ",
            "Dead letter queue created",
            f,
            __FILE__, __LINE__, SPDLOG_FUNCTION);
    }
    
    return CreateQueue(DeadLetterConfig);
}

// DLQ监控方法实现
QueueResult MessageQueue::GetDeadLetterQueueStats(const std::string& QueueName, 
                                                  DeadLetterQueueStats& OutStats) const
{
    auto DeadLetterQueueName = GetDeadLetterQueueName(QueueName);
    const auto* DeadLetterQueue = GetQueueData(DeadLetterQueueName);
    
    if (!DeadLetterQueue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::shared_lock<std::shared_mutex> Lock(DeadLetterQueue->Mutex);
    
    OutStats.QueueName = QueueName;
    OutStats.DeadLetterQueueName = DeadLetterQueueName;
    OutStats.TotalDeadLetterMessages = DeadLetterQueue->Stats.DeadLetterMessages;
    OutStats.CurrentDeadLetterMessages = DeadLetterQueue->DeadLetterMessages.size();
    OutStats.ExpiredMessages = DeadLetterQueue->Stats.ExpiredMessages;
    OutStats.MaxRetriesExceededMessages = DeadLetterQueue->Stats.DeadLetterMessages - 
                                          DeadLetterQueue->Stats.ExpiredMessages - 
                                          DeadLetterQueue->Stats.RejectedMessages;
    OutStats.RejectedMessages = DeadLetterQueue->Stats.RejectedMessages;
    OutStats.LastDeadLetterTime = DeadLetterQueue->Stats.LastMessageTime;
    OutStats.CreatedTime = DeadLetterQueue->Stats.CreatedTime;
    
    // 计算死信率
    const auto* OriginalQueue = GetQueueData(QueueName);
    if (OriginalQueue)
    {
        std::shared_lock<std::shared_mutex> OriginalLock(OriginalQueue->Mutex);
        if (OriginalQueue->Stats.TotalMessages > 0)
        {
            OutStats.DeadLetterRate = static_cast<double>(OutStats.TotalDeadLetterMessages) / 
                                      OriginalQueue->Stats.TotalMessages;
        }
    }
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAllDeadLetterQueueStats(std::vector<DeadLetterQueueStats>& OutStats) const
{
    OutStats.clear();
    std::shared_lock<std::shared_mutex> Lock(QueuesMutex);
    
    for (const auto& [QueueName, Queue] : Queues)
    {
        if (Queue->Config.Type == QueueType::DEAD_LETTER)
        {
            continue; // 跳过死信队列本身
        }
        
        DeadLetterQueueStats Stats;
        auto Result = GetDeadLetterQueueStats(QueueName, Stats);
        if (Result == QueueResult::SUCCESS)
        {
            OutStats.push_back(Stats);
        }
    }
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::SetDeadLetterAlertConfig(const std::string& QueueName, 
                                                   const DeadLetterAlertConfig& Config)
{
    std::unique_lock<std::shared_mutex> Lock(DeadLetterMonitorMutex);
    DeadLetterAlertConfigs[QueueName] = Config;
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
          "设置DLQ告警配置: queue={}, maxCount={}, maxRate={}", 
          QueueName, Config.MaxDeadLetterMessages, Config.MaxDeadLetterRate);
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetDeadLetterAlertConfig(const std::string& QueueName, 
                                                   DeadLetterAlertConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> Lock(DeadLetterMonitorMutex);
    auto It = DeadLetterAlertConfigs.find(QueueName);
    if (It == DeadLetterAlertConfigs.end())
    {
        // 返回默认配置
        OutConfig = DeadLetterAlertConfig{};
        return QueueResult::SUCCESS;
    }
    
    OutConfig = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetActiveDeadLetterAlerts(const std::string& QueueName,
                                                   std::vector<DeadLetterAlert>& OutAlerts) const
{
    std::shared_lock<std::shared_mutex> Lock(DeadLetterMonitorMutex);
    auto It = ActiveDeadLetterAlerts.find(QueueName);
    if (It == ActiveDeadLetterAlerts.end())
    {
        OutAlerts.clear();
        return QueueResult::SUCCESS;
    }
    
    OutAlerts = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAllActiveDeadLetterAlerts(std::vector<DeadLetterAlert>& OutAlerts) const
{
    OutAlerts.clear();
    std::shared_lock<std::shared_mutex> Lock(DeadLetterMonitorMutex);
    
    for (const auto& [QueueName, Alerts] : ActiveDeadLetterAlerts)
    {
        OutAlerts.insert(OutAlerts.end(), Alerts.begin(), Alerts.end());
    }
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ClearDeadLetterAlert(const std::string& QueueName, 
                                               DeadLetterAlertType AlertType)
{
    ClearDeadLetterAlertInternal(QueueName, AlertType);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ClearAllDeadLetterAlerts(const std::string& QueueName)
{
    std::unique_lock<std::shared_mutex> Lock(DeadLetterMonitorMutex);
    ActiveDeadLetterAlerts.erase(QueueName);
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, 
          "清除所有DLQ告警: queue={}", QueueName);
    
    return QueueResult::SUCCESS;
}

void MessageQueue::SetDeadLetterAlertHandler(DeadLetterAlertHandler Handler)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    DeadLetterAlertHandlerFunc = Handler;
}

void MessageQueue::SetDeadLetterStatsHandler(DeadLetterStatsHandler Handler)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    DeadLetterStatsHandlerFunc = Handler;
}

// DLQ监控内部方法实现
void MessageQueue::ProcessDeadLetterMonitoring()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "启动DLQ监控线程");
    
    while (!StopDeadLetterMonitor.load())
    {
        // 检查DLQ告警
        {
            std::shared_lock<std::shared_mutex> Lock(DeadLetterMonitorMutex);
            for (const auto& [QueueName, Config] : DeadLetterAlertConfigs)
            {
                CheckDeadLetterAlerts(QueueName);
            }
        }
        
        // 等待下一次检查，使用较短的等待时间以便及时响应停止信号
        std::unique_lock<std::mutex> WaitLock(HandlersMutex);
        if (DeadLetterMonitorCondition.wait_for(WaitLock, 
            std::chrono::milliseconds(1000), // 改为1秒检查一次，提高响应性
            [this] { return StopDeadLetterMonitor.load(); }))
        {
            // 停止信号被触发，立即退出
            break;
        }
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "DLQ监控线程停止");
}

void MessageQueue::CheckDeadLetterAlerts(const std::string& QueueName)
{
    DeadLetterQueueStats Stats;
    auto Result = GetDeadLetterQueueStats(QueueName, Stats);
    if (Result != QueueResult::SUCCESS)
    {
        return;
    }
    
    DeadLetterAlertConfig Config;
    Result = GetDeadLetterAlertConfig(QueueName, Config);
    if (Result != QueueResult::SUCCESS)
    {
        return;
    }
    
    // 检查死信数量告警
    if (Config.EnableDeadLetterCountAlert && 
        Stats.CurrentDeadLetterMessages > Config.MaxDeadLetterMessages)
    {
        const_cast<MessageQueue*>(this)->TriggerDeadLetterAlert(QueueName, DeadLetterAlertType::DEAD_LETTER_COUNT_EXCEEDED,
                              "死信队列消息数量超过阈值",
                              Stats.CurrentDeadLetterMessages, Config.MaxDeadLetterMessages);
    }
    
    // 检查死信率告警
    if (Config.EnableDeadLetterRateAlert && 
        Stats.DeadLetterRate > Config.MaxDeadLetterRate)
    {
        const_cast<MessageQueue*>(this)->TriggerDeadLetterAlert(QueueName, DeadLetterAlertType::DEAD_LETTER_RATE_EXCEEDED,
                              "死信率超过阈值",
                              0, 0, Stats.DeadLetterRate, Config.MaxDeadLetterRate);
    }
    
    // 检查死信队列已满告警
    auto* DeadLetterQueue = const_cast<MessageQueue*>(this)->GetQueueData(Stats.DeadLetterQueueName);
    if (DeadLetterQueue && DeadLetterQueue->DeadLetterMessages.size() >= DeadLetterQueue->Config.MaxSize)
    {
        const_cast<MessageQueue*>(this)->TriggerDeadLetterAlert(QueueName, DeadLetterAlertType::DEAD_LETTER_QUEUE_FULL,
                              "死信队列已满");
    }
}

void MessageQueue::UpdateDeadLetterStats(const std::string& QueueName, DeadLetterReason Reason)
{
    auto DeadLetterQueueName = GetDeadLetterQueueName(QueueName);
    auto* DeadLetterQueue = GetQueueData(DeadLetterQueueName);
    if (!DeadLetterQueue)
    {
        return;
    }
    
    std::unique_lock<std::shared_mutex> Lock(DeadLetterQueue->Mutex);
    
    switch (Reason)
    {
        case DeadLetterReason::EXPIRED:
            DeadLetterQueue->Stats.ExpiredMessages++;
            break;
        case DeadLetterReason::MAX_RETRIES_EXCEEDED:
            // 这个在RejectMessage中已经处理
            break;
        case DeadLetterReason::REJECTED:
            DeadLetterQueue->Stats.RejectedMessages++;
            break;
        default:
            break;
    }
    
    DeadLetterQueue->Stats.DeadLetterMessages++;
    DeadLetterQueue->Stats.LastMessageTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void MessageQueue::TriggerDeadLetterAlert(const std::string& QueueName, 
                                          DeadLetterAlertType AlertType,
                                          const std::string& AlertMessage,
                                          uint64_t CurrentValue,
                                          uint64_t ThresholdValue,
                                          double CurrentRate,
                                          double ThresholdRate)
{
    DeadLetterAlert Alert;
    Alert.Type = AlertType;
    Alert.QueueName = QueueName;
    Alert.DeadLetterQueueName = GetDeadLetterQueueName(QueueName);
    Alert.AlertMessage = AlertMessage;
    Alert.CurrentValue = CurrentValue;
    Alert.ThresholdValue = ThresholdValue;
    Alert.CurrentRate = CurrentRate;
    Alert.ThresholdRate = ThresholdRate;
    Alert.AlertTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    Alert.IsActive = true;
    
    // 添加到活跃告警列表
    std::unique_lock<std::shared_mutex> Lock(DeadLetterMonitorMutex);
    ActiveDeadLetterAlerts[QueueName].push_back(Alert);
    
    // 调用告警处理器
    if (DeadLetterAlertHandlerFunc)
    {
        DeadLetterAlertHandlerFunc(Alert);
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, 
          "DLQ告警触发: queue={}, type={}, message={}", 
          QueueName, static_cast<int>(AlertType), AlertMessage);
    {
        Helianthus::Common::LogFields f;
        f.AddField("queue", QueueName);
        f.AddField("type", static_cast<int32_t>(static_cast<int>(AlertType)));
        f.AddField("current_value", static_cast<uint64_t>(CurrentValue));
        f.AddField("threshold_value", static_cast<uint64_t>(ThresholdValue));
        f.AddField("current_rate", CurrentRate);
        f.AddField("threshold_rate", ThresholdRate);
        Helianthus::Common::StructuredLogger::Log(
            Helianthus::Common::StructuredLogLevel::WARN,
            "MQ",
            AlertMessage,
            f,
            __FILE__, __LINE__, SPDLOG_FUNCTION);
    }
}

void MessageQueue::ClearDeadLetterAlertInternal(const std::string& QueueName, 
                                                DeadLetterAlertType AlertType)
{
    std::unique_lock<std::shared_mutex> Lock(DeadLetterMonitorMutex);
    auto It = ActiveDeadLetterAlerts.find(QueueName);
    if (It == ActiveDeadLetterAlerts.end())
    {
        return;
    }
    
    auto& Alerts = It->second;
    Alerts.erase(std::remove_if(Alerts.begin(), Alerts.end(),
        [AlertType](const DeadLetterAlert& Alert) {
            return Alert.Type == AlertType;
        }), Alerts.end());
    
    if (Alerts.empty())
    {
        ActiveDeadLetterAlerts.erase(It);
    }
}

// 指标窗口辅助
void MessageQueue::RecordEnqueueEvent(QueueData* Queue)
{
    if (!Queue) return;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    Queue->EnqueueTimestamps.push_back(now);
}

void MessageQueue::RecordDequeueEvent(QueueData* Queue)
{
    if (!Queue) return;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    Queue->DequeueTimestamps.push_back(now);
}

void MessageQueue::RecordLatencySample(QueueData* Queue, double Ms)
{
    if (!Queue) return;
    if (Queue->LatencySamplesMs.size() >= Queue->LatencyCapacity)
    {
        // 简单环形：移除前半以控制开销
        Queue->LatencySamplesMs.erase(Queue->LatencySamplesMs.begin(),
                                      Queue->LatencySamplesMs.begin() + Queue->LatencySamplesMs.size() / 2);
    }
    Queue->LatencySamplesMs.push_back(Ms);
}

void MessageQueue::TrimOld(std::deque<MessageTimestamp>& TsDeque, MessageTimestamp Now, uint32_t WindowMs)
{
    const auto cutoff = Now - WindowMs;
    while (!TsDeque.empty() && TsDeque.front() < cutoff)
    {
        TsDeque.pop_front();
    }
}

double MessageQueue::ComputeRatePerSec(const std::deque<MessageTimestamp>& TsDeque, MessageTimestamp Now, uint32_t WindowMs)
{
    if (TsDeque.empty()) return 0.0;
    const double windowSec = static_cast<double>(WindowMs) / 1000.0;
    return static_cast<double>(TsDeque.size()) / windowSec;
}

void MessageQueue::ComputePercentiles(const std::vector<double>& Samples, double& P50, double& P95)
{
    if (Samples.empty()) { P50 = 0.0; P95 = 0.0; return; }
    std::vector<double> tmp = Samples;
    std::sort(tmp.begin(), tmp.end());
    auto idx50 = static_cast<size_t>(std::clamp(static_cast<long>(std::llround(0.5 * (tmp.size() - 1))), 0L, static_cast<long>(tmp.size()-1)));
    auto idx95 = static_cast<size_t>(std::clamp(static_cast<long>(std::llround(0.95 * (tmp.size() - 1))), 0L, static_cast<long>(tmp.size()-1)));
    P50 = tmp[idx50];
    P95 = tmp[idx95];
}

QueueResult MessageQueue::GetAllQueueMetrics(std::vector<QueueMetrics>& OutMetrics) const
{
    OutMetrics.clear();
    std::shared_lock<std::shared_mutex> Lock(QueuesMutex);
    for (const auto& [Name, Queue] : Queues)
    {
        QueueMetrics M;
        if (GetQueueMetrics(Name, M) == QueueResult::SUCCESS)
        {
            OutMetrics.push_back(M);
        }
    }
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetQueueMetrics(const std::string& QueueName, QueueMetrics& OutMetrics) const
{
    const auto* Queue = GetQueueData(QueueName);
    if (!Queue)
    {
        return QueueResult::QUEUE_NOT_FOUND;
    }

    std::shared_lock<std::shared_mutex> Lock(Queue->Mutex);
    OutMetrics.QueueName = QueueName;
    OutMetrics.PendingMessages = static_cast<uint64_t>(Queue->GetMessageCount());
    OutMetrics.TotalMessages = Queue->Stats.TotalMessages;
    OutMetrics.ProcessedMessages = Queue->Stats.ProcessedMessages;
    OutMetrics.DeadLetterMessages = Queue->Stats.DeadLetterMessages;
    OutMetrics.RetriedMessages = Queue->Stats.RetriedMessages;

    // 计算窗口内速率与分位
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    TrimOld(const_cast<std::deque<MessageTimestamp>&>(Queue->EnqueueTimestamps), now, MetricsWindowMs);
    TrimOld(const_cast<std::deque<MessageTimestamp>&>(Queue->DequeueTimestamps), now, MetricsWindowMs);
    OutMetrics.EnqueueRate = ComputeRatePerSec(Queue->EnqueueTimestamps, now, MetricsWindowMs);
    OutMetrics.DequeueRate = ComputeRatePerSec(Queue->DequeueTimestamps, now, MetricsWindowMs);
    ComputePercentiles(Queue->LatencySamplesMs, OutMetrics.P50LatencyMs, OutMetrics.P95LatencyMs);

    OutMetrics.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return QueueResult::SUCCESS;
}

void MessageQueue::ProcessMetricsMonitoring()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "启动指标监控线程");
    for (;;)
    {
        std::unique_lock<std::mutex> lock(MetricsMonitorMutex);
        if (MetricsMonitorCondition.wait_for(lock, std::chrono::milliseconds(MetricsIntervalMs), [this]{ return StopMetricsMonitor.load(); }))
        {
            break;
        }
        lock.unlock();

        // 聚合并输出指标
        std::vector<QueueMetrics> metricsList;
        if (GetAllQueueMetrics(metricsList) == QueueResult::SUCCESS)
        {
            for (const auto& m : metricsList)
            {
                Helianthus::Common::LogFields f;
                f.AddField("queue", m.QueueName);
                f.AddField("pending", static_cast<uint64_t>(m.PendingMessages));
                f.AddField("total", static_cast<uint64_t>(m.TotalMessages));
                f.AddField("processed", static_cast<uint64_t>(m.ProcessedMessages));
                f.AddField("dead_letter", static_cast<uint64_t>(m.DeadLetterMessages));
                f.AddField("retried", static_cast<uint64_t>(m.RetriedMessages));
                f.AddField("enq_rate", m.EnqueueRate);
                f.AddField("deq_rate", m.DequeueRate);
                f.AddField("p50_ms", m.P50LatencyMs);
                f.AddField("p95_ms", m.P95LatencyMs);
                Helianthus::Common::StructuredLogger::Log(
                    Helianthus::Common::StructuredLogLevel::INFO,
                    "MQ",
                    "Queue metrics",
                    f,
                    __FILE__, __LINE__, SPDLOG_FUNCTION);
            }
        }

        // 分片/副本状态指标（聚合）
        uint32_t Shards = 0;
        uint32_t Leaders = 0;
        uint32_t HealthyReplicas = 0;
        uint64_t TotalWalLen = 0;
        uint64_t TotalFollowerApplied = 0;
        uint64_t TotalReplicationLag = 0;
        {
            std::shared_lock<std::shared_mutex> Lock(ClusterMutex);
            Shards = static_cast<uint32_t>(Cluster.Shards.size());
            for (const auto& S : Cluster.Shards)
            {
                for (const auto& R : S.Replicas)
                {
                    if (R.Role == ReplicaRole::LEADER) Leaders++;
                    if (R.Healthy) HealthyReplicas++;
                }
            }
            // 复制滞后度聚合（基于 WAL 占位）
            for (size_t i = 0; i < WalPerShard.size(); ++i)
            {
                const auto& Wal = WalPerShard[i];
                const uint64_t LeaderLen = static_cast<uint64_t>(Wal.Entries.size());
                TotalWalLen += LeaderLen;
                uint64_t MaxApplied = 0;
                for (const auto& kv : Wal.FollowerAppliedIndex)
                {
                    MaxApplied = std::max(MaxApplied, kv.second);
                }
                TotalFollowerApplied += MaxApplied;
                if (LeaderLen > MaxApplied)
                {
                    TotalReplicationLag += (LeaderLen - MaxApplied);
                }
            }
        }
        {
            Helianthus::Common::LogFields F;
            F.AddField("shards", Shards);
            F.AddField("leaders", Leaders);
            F.AddField("healthy_replicas", HealthyReplicas);
            F.AddField("wal_total_len", static_cast<uint64_t>(TotalWalLen));
            F.AddField("followers_max_applied", static_cast<uint64_t>(TotalFollowerApplied));
            F.AddField("replication_total_lag", static_cast<uint64_t>(TotalReplicationLag));
            Helianthus::Common::StructuredLogger::Log(
                Helianthus::Common::StructuredLogLevel::INFO,
                "MQ",
                "Cluster metrics",
                F,
                __FILE__, __LINE__, SPDLOG_FUNCTION);
        }
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "指标监控线程停止");
}

void MessageQueue::RebuildShardRing()
{
    ShardRing.Clear();
    // 单机模拟：使用本地"节点"为 shard-i
    for (uint32_t I = 0; I < std::max(ShardCount, 1u); ++I)
    {
        const std::string NodeId = std::string("shard-") + std::to_string(I);
        ShardRing.AddNode(NodeId, std::max(ShardVirtualNodes, 1u));
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "一致性哈希环构建完成: shards={} vnodes/node={} total_nodes={}",
          ShardCount, ShardVirtualNodes, ShardRing.Size());
}

// 集群/分片/副本接口实现
QueueResult MessageQueue::SetClusterConfig(const ClusterConfig& Config)
{
    {
        std::unique_lock<std::shared_mutex> Lock(ClusterMutex);
        Cluster = Config;
        // 同步 ShardCount 与 HashRing（按 Shards 大小）
        ShardCount = static_cast<uint32_t>(Cluster.Shards.size() > 0 ? Cluster.Shards.size() : ShardCount);
    }
    RebuildShardRing();
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetClusterConfig(ClusterConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> Lock(ClusterMutex);
    OutConfig = Cluster;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetShardForKey(const std::string& Key, ShardId& OutShardId, std::string& OutNodeId) const
{
    const std::string NodeId = ShardRing.GetNode(Key);
    if (NodeId.empty()) return QueueResult::INTERNAL_ERROR;
    // 解析 shard index
    int ShardIndex = -1;
    auto Pos = NodeId.rfind('-');
    if (Pos != std::string::npos)
    {
        try { ShardIndex = std::stoi(NodeId.substr(Pos + 1)); } catch (...) { ShardIndex = -1; }
    }
    if (ShardIndex < 0) return QueueResult::INTERNAL_ERROR;
    OutShardId = static_cast<ShardId>(ShardIndex);
    OutNodeId = NodeId;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetShardReplicas(ShardId Shard, std::vector<ReplicaInfo>& OutReplicas) const
{
    std::shared_lock<std::shared_mutex> Lock(ClusterMutex);
    if (Shard >= Cluster.Shards.size()) return QueueResult::QUEUE_NOT_FOUND;
    OutReplicas = Cluster.Shards[Shard].Replicas;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::SetNodeHealth(const std::string& NodeId, bool Healthy)
{
    std::unique_lock<std::shared_mutex> Lock(ClusterMutex);
    for (auto& Shard : Cluster.Shards)
    {
        for (auto& Rep : Shard.Replicas)
        {
            if (Rep.NodeId == NodeId)
            {
                Rep.Healthy = Healthy;
            }
        }
    }
    return QueueResult::SUCCESS;
}

void MessageQueue::ProcessHeartbeat()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "启动心跳线程");
    for (;;)
    {
        std::unique_lock<std::mutex> Lk(HeartbeatMutex);
        if (HeartbeatCondition.wait_for(Lk, std::chrono::milliseconds(HeartbeatIntervalMs), [this]{ return StopHeartbeat.load(); }))
        {
            break;
        }
        Lk.unlock();

        // 扫描并随机模拟健康波动
        {
            std::unique_lock<std::shared_mutex> Lock(ClusterMutex);
            if (!Cluster.Shards.empty())
            {
                // 简单伪随机：基于时间种子与自增扰动，避免全局rand()
                const auto NowNs = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                uint64_t Seed = static_cast<uint64_t>(NowNs);
                auto NextBool = [&Seed]() {
                    // xorshift64*
                    Seed ^= Seed >> 12; Seed ^= Seed << 25; Seed ^= Seed >> 27; 
                    uint64_t R = Seed * 2685821657736338717ULL; 
                    return (R & 0xFFFFFFFFULL);
                };
                for (auto& Shard : Cluster.Shards)
                {
                    // 记录当前Leader是否健康
                    std::string CurrentLeader;
                    bool LeaderHealthy = false;
                    for (auto& Replica : Shard.Replicas)
                    {
                        // 以 HeartbeatFlapProbability 概率翻转健康状态
                        uint32_t RandomValue = static_cast<uint32_t>(NextBool());
                        // 将0..UINT32映射到[0,1)
                        const double Uniform = static_cast<double>(RandomValue) / static_cast<double>(0xFFFFFFFFu);
                        if (Uniform < HeartbeatFlapProbability)
                        {
                            Replica.Healthy = !Replica.Healthy;
                        }
                        if (Replica.Role == ReplicaRole::LEADER) { CurrentLeader = Replica.NodeId; LeaderHealthy = Replica.Healthy; }
                    }

                    // 简单主选举：Leader不健康则选择第一个健康Follower接管
                    if (!LeaderHealthy)
                    {
                        std::string TakeoverNode;
                        for (auto& Replica : Shard.Replicas)
                        {
                            if (Replica.NodeId != CurrentLeader && Replica.Healthy)
                            {
                                TakeoverNode = Replica.NodeId;
                                break;
                            }
                        }
                        if (!TakeoverNode.empty())
                        {
                            // 执行切换
                            for (auto& Replica : Shard.Replicas)
                            {
                                if (Replica.NodeId == CurrentLeader) Replica.Role = ReplicaRole::FOLLOWER;
                                if (Replica.NodeId == TakeoverNode) Replica.Role = ReplicaRole::LEADER;
                            }
                            // 回调（在持锁内先收集参数，出锁后触发）
                            std::string OldLeader = CurrentLeader;
                            std::string NewLeader = TakeoverNode;
                            ShardId ShardIdValue = Shard.Id;
                            // 出锁后回调
                            Lock.unlock();
                            if (FailoverCb) FailoverCb(ShardIdValue, OldLeader, NewLeader);
                            if (LeaderChangedCb) LeaderChangedCb(ShardIdValue, OldLeader, NewLeader);
                            // 重新加锁以继续循环
                            Lock.lock();
                        }
                    }
                }
            }
        }

        // 模拟Follower重放推进：将Follower应用位点逐步逼近Leader日志长度
        {
            std::unique_lock<std::shared_mutex> Lock(ClusterMutex);
            for (size_t S = 0; S < WalPerShard.size(); ++S)
            {
                auto& Wal = WalPerShard[S];
                const uint64_t LeaderLen = static_cast<uint64_t>(Wal.Entries.size());
                if (LeaderLen == 0) continue;
                for (auto& Pair : Wal.FollowerAppliedIndex)
                {
                    uint64_t& Applied = Pair.second;
                    if (Applied < LeaderLen)
                    {
                        uint64_t Gap = LeaderLen - Applied;
                        uint64_t Step = (Gap >= 3 ? 3 : (Gap >= 2 ? 2 : 1));
                        Applied = std::min(LeaderLen, Applied + Step);
                    }
                }
            }
        }

        // 输出心跳日志（演示用）
        std::vector<std::pair<std::string, bool>> NodeHealthList;
        {
            std::shared_lock<std::shared_mutex> Lock(ClusterMutex);
            for (const auto& Shard : Cluster.Shards)
            {
                for (const auto& Replica : Shard.Replicas)
                {
                    NodeHealthList.emplace_back(Replica.NodeId, Replica.Healthy);
                }
            }
        }
        if (!NodeHealthList.empty())
        {
            Helianthus::Common::LogFields F;
            F.AddField("nodes", static_cast<uint64_t>(NodeHealthList.size()));
            Helianthus::Common::StructuredLogger::Log(
                Helianthus::Common::StructuredLogLevel::INFO,
                "MQ",
                "Heartbeat tick",
                F,
                __FILE__, __LINE__, SPDLOG_FUNCTION);
        }
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "心跳线程停止");
}

QueueResult MessageQueue::GetClusterShardStatuses(std::vector<ShardInfo>& OutShards) const
{
    std::shared_lock<std::shared_mutex> Lock(ClusterMutex);
    OutShards = Cluster.Shards;
    return QueueResult::SUCCESS;
}

uint32_t MessageQueue::SimulateReplication(uint32_t ShardIndex, MessageId Id)
{
    std::shared_lock<std::shared_mutex> Lock(ClusterMutex);
    if (ShardIndex >= Cluster.Shards.size()) return 0;
    const auto& Shard = Cluster.Shards[ShardIndex];
    // 简化：对所有健康Follower计为ACK，最多不超过 MinReplicationAcks
    uint32_t Acked = 0;
    for (const auto& Replica : Shard.Replicas)
    {
        if (Replica.Role == ReplicaRole::FOLLOWER && Replica.Healthy)
        {
            Acked++;
            if (Acked >= MinReplicationAcks) break;
        }
    }
    return Acked;
}

QueueResult MessageQueue::PromoteToLeader(ShardId Shard, const std::string& NodeId)
{
    std::unique_lock<std::shared_mutex> Lock(ClusterMutex);
    if (Shard >= Cluster.Shards.size()) return QueueResult::QUEUE_NOT_FOUND;
    auto& Replicas = Cluster.Shards[Shard].Replicas;
    std::string OldLeader;
    for (auto& R : Replicas) {
        if (R.Role == ReplicaRole::LEADER) { OldLeader = R.NodeId; R.Role = ReplicaRole::FOLLOWER; }
    }
    bool Found = false;
    for (auto& R : Replicas) {
        if (R.NodeId == NodeId) { R.Role = ReplicaRole::LEADER; Found = true; break; }
    }
    if (!Found) return QueueResult::QUEUE_NOT_FOUND;
    Lock.unlock();
    if (LeaderChangedCb) LeaderChangedCb(Shard, OldLeader, NodeId);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::DemoteToFollower(ShardId Shard, const std::string& NodeId)
{
    std::unique_lock<std::shared_mutex> Lock(ClusterMutex);
    if (Shard >= Cluster.Shards.size()) return QueueResult::QUEUE_NOT_FOUND;
    auto& Replicas = Cluster.Shards[Shard].Replicas;
    bool Found = false;
    for (auto& R : Replicas) {
        if (R.NodeId == NodeId) { R.Role = ReplicaRole::FOLLOWER; Found = true; break; }
    }
    return Found ? QueueResult::SUCCESS : QueueResult::QUEUE_NOT_FOUND;
}

QueueResult MessageQueue::GetCurrentLeader(ShardId Shard, std::string& OutNodeId) const
{
    std::shared_lock<std::shared_mutex> Lock(ClusterMutex);
    if (Shard >= Cluster.Shards.size()) return QueueResult::QUEUE_NOT_FOUND;
    for (const auto& R : Cluster.Shards[Shard].Replicas) {
        if (R.Role == ReplicaRole::LEADER) { OutNodeId = R.NodeId; return QueueResult::SUCCESS; }
    }
    OutNodeId.clear();
    return QueueResult::QUEUE_NOT_FOUND;
}

void MessageQueue::SetLeaderChangeHandler(LeaderChangeHandler Handler)
{
    std::lock_guard<std::mutex> Lk(HandlersMutex);
    LeaderChangedCb = Handler;
}

void MessageQueue::SetFailoverHandler(FailoverHandler Handler)
{
    std::lock_guard<std::mutex> Lk(HandlersMutex);
    FailoverCb = Handler;
}

// 事务管理实现
TransactionId MessageQueue::GenerateTransactionId()
{
    return NextTransactionId++;
}

TransactionId MessageQueue::BeginTransaction(const std::string& Description, uint32_t TimeoutMs)
{
    std::unique_lock<std::shared_mutex> Lock(TransactionsMutex);
    
    TransactionId Id = GenerateTransactionId();
    Transaction Transaction;
    Transaction.Id = Id;
    Transaction.Status = TransactionStatus::PENDING;
    Transaction.Description = Description;
    Transaction.TimeoutMs = TimeoutMs;
    Transaction.StartTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    Transaction.IsDistributed = false;
    
    Transactions[Id] = Transaction;
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始事务: id={}, description={}, timeout={}ms", 
          Id, Description, TimeoutMs);
    
    return Id;
}

QueueResult MessageQueue::CommitTransaction(TransactionId Id)
{
    std::unique_lock<std::shared_mutex> Lock(TransactionsMutex);
    
    auto It = Transactions.find(Id);
    if (It == Transactions.end())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务不存在: id={}", Id);
        return QueueResult::INVALID_PARAMETER;
    }
    
    Transaction& Transaction = It->second;
    if (Transaction.Status != TransactionStatus::PENDING)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务状态不正确: id={}, status={}", 
              Id, static_cast<int>(Transaction.Status));
        return QueueResult::INVALID_STATE;
    }
    
    // 执行所有操作
    bool AllSuccess = true;
    std::string ErrorMessage;
    
    for (const auto& Operation : Transaction.Operations)
    {
        auto Result = ExecuteTransactionOperation(Operation);
        if (Result != QueueResult::SUCCESS)
        {
            AllSuccess = false;
            ErrorMessage = "操作执行失败: " + std::to_string(static_cast<int>(Result));
            break;
        }
    }
    
    // 更新事务状态
    Transaction.EndTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (AllSuccess)
    {
        Transaction.Status = TransactionStatus::COMMITTED;
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "事务提交成功: id={}", Id);
    }
    else
    {
        Transaction.Status = TransactionStatus::FAILED;
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务提交失败: id={}, error={}", Id, ErrorMessage);
    }
    
    // 更新统计信息
    UpdateTransactionStats(Transaction);
    
    // 通知回调
    NotifyTransactionCommit(Id, AllSuccess, ErrorMessage);
    
    return AllSuccess ? QueueResult::SUCCESS : QueueResult::OPERATION_FAILED;
}

QueueResult MessageQueue::RollbackTransaction(TransactionId Id, const std::string& Reason)
{
    std::unique_lock<std::shared_mutex> Lock(TransactionsMutex);
    
    auto It = Transactions.find(Id);
    if (It == Transactions.end())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务不存在: id={}", Id);
        return QueueResult::INVALID_PARAMETER;
    }
    
    Transaction& Transaction = It->second;
    if (Transaction.Status != TransactionStatus::PENDING)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务状态不正确: id={}, status={}", 
              Id, static_cast<int>(Transaction.Status));
        return QueueResult::INVALID_STATE;
    }
    
    // 回滚所有操作
    auto Result = RollbackTransactionOperations(Id);
    
    // 更新事务状态
    Transaction.EndTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    Transaction.Status = TransactionStatus::ROLLED_BACK;
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "事务回滚: id={}, reason={}", Id, Reason);
    
    // 更新统计信息
    UpdateTransactionStats(Transaction);
    
    // 通知回调
    NotifyTransactionRollback(Id, Reason);
    
    return Result;
}

QueueResult MessageQueue::AbortTransaction(TransactionId Id, const std::string& Reason)
{
    return RollbackTransaction(Id, Reason);
}

QueueResult MessageQueue::SendMessageInTransaction(TransactionId Id, const std::string& QueueName, MessagePtr Message)
{
    if (!Message)
    {
        return QueueResult::INVALID_PARAMETER;
    }
    
    TransactionOperation Operation;
    Operation.Type = TransactionOperationType::SEND_MESSAGE;
    Operation.QueueName = QueueName;
    Operation.Message = Message;
    Operation.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return AddTransactionOperation(Id, Operation);
}

QueueResult MessageQueue::AcknowledgeMessageInTransaction(TransactionId Id, const std::string& QueueName, MessageId MessageId)
{
    TransactionOperation Operation;
    Operation.Type = TransactionOperationType::ACKNOWLEDGE;
    Operation.QueueName = QueueName;
    Operation.TargetMessageId = MessageId;
    Operation.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return AddTransactionOperation(Id, Operation);
}

QueueResult MessageQueue::RejectMessageInTransaction(TransactionId Id, const std::string& QueueName, MessageId MessageId, const std::string& Reason)
{
    TransactionOperation Operation;
    Operation.Type = TransactionOperationType::REJECT_MESSAGE;
    Operation.QueueName = QueueName;
    Operation.TargetMessageId = MessageId;
    Operation.ErrorMessage = Reason;
    Operation.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return AddTransactionOperation(Id, Operation);
}

QueueResult MessageQueue::CreateQueueInTransaction(TransactionId Id, const QueueConfig& Config)
{
    TransactionOperation Operation;
    Operation.Type = TransactionOperationType::CREATE_QUEUE;
    Operation.QueueName = Config.Name;
    Operation.TargetQueueConfig = Config;
    Operation.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return AddTransactionOperation(Id, Operation);
}

QueueResult MessageQueue::DeleteQueueInTransaction(TransactionId Id, const std::string& QueueName)
{
    TransactionOperation Operation;
    Operation.Type = TransactionOperationType::DELETE_QUEUE;
    Operation.QueueName = QueueName;
    Operation.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return AddTransactionOperation(Id, Operation);
}

QueueResult MessageQueue::AddTransactionOperation(TransactionId Id, const TransactionOperation& Operation)
{
    std::shared_lock<std::shared_mutex> Lock(TransactionsMutex);
    
    auto It = Transactions.find(Id);
    if (It == Transactions.end())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务不存在: id={}", Id);
        return QueueResult::INVALID_PARAMETER;
    }
    
    Transaction& Transaction = It->second;
    if (Transaction.Status != TransactionStatus::PENDING)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务状态不正确: id={}, status={}", 
              Id, static_cast<int>(Transaction.Status));
        return QueueResult::INVALID_STATE;
    }
    
    Transaction.Operations.push_back(Operation);
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "添加事务操作: id={}, type={}, queue={}", 
          Id, static_cast<int>(Operation.Type), Operation.QueueName);
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ExecuteTransactionOperation(const TransactionOperation& Operation)
{
    switch (Operation.Type)
    {
        case TransactionOperationType::SEND_MESSAGE:
            return SendMessage(Operation.QueueName, Operation.Message);
            
        case TransactionOperationType::ACKNOWLEDGE:
            return AcknowledgeMessage(Operation.QueueName, Operation.TargetMessageId);
            
        case TransactionOperationType::REJECT_MESSAGE:
            return RejectMessage(Operation.QueueName, Operation.TargetMessageId, true); // 默认重新入队
            
        case TransactionOperationType::CREATE_QUEUE:
            return CreateQueue(Operation.TargetQueueConfig);
            
        case TransactionOperationType::DELETE_QUEUE:
            return DeleteQueue(Operation.QueueName);
            
        default:
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "未知的事务操作类型: {}", static_cast<int>(Operation.Type));
            return QueueResult::INVALID_PARAMETER;
    }
}

QueueResult MessageQueue::RollbackTransactionOperations(TransactionId Id)
{
    // 这里实现回滚逻辑
    // 对于消息发送，可以标记为待删除
    // 对于消息确认/拒收，可以恢复消息状态
    // 对于队列创建/删除，可以反向操作
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "回滚事务操作: id={}", Id);
    
    // 简化实现：记录回滚操作
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetTransactionStatus(TransactionId Id, TransactionStatus& Status)
{
    std::shared_lock<std::shared_mutex> Lock(TransactionsMutex);
    
    auto It = Transactions.find(Id);
    if (It == Transactions.end())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    
    Status = It->second.Status;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetTransactionInfo(TransactionId Id, Transaction& Info)
{
    std::shared_lock<std::shared_mutex> Lock(TransactionsMutex);
    
    auto It = Transactions.find(Id);
    if (It == Transactions.end())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    
    Info = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetTransactionStats(TransactionStats& Stats)
{
    std::lock_guard<std::mutex> Lock(TransactionStatsMutex);
    Stats = TransactionStatsData;
    return QueueResult::SUCCESS;
}

void MessageQueue::SetTransactionCommitHandler(TransactionCommitHandler Handler)
{
    TransactionCommitHandlerFunc = Handler;
}

void MessageQueue::SetTransactionRollbackHandler(TransactionRollbackHandler Handler)
{
    TransactionRollbackHandlerFunc = Handler;
}

void MessageQueue::SetTransactionTimeoutHandler(TransactionTimeoutHandler Handler)
{
    TransactionTimeoutHandlerFunc = Handler;
}

void MessageQueue::UpdateTransactionStats(const Transaction& Transaction)
{
    std::lock_guard<std::mutex> Lock(TransactionStatsMutex);
    
    TransactionStatsData.TotalTransactions++;
    
    switch (Transaction.Status)
    {
        case TransactionStatus::COMMITTED:
            TransactionStatsData.CommittedTransactions++;
            break;
        case TransactionStatus::ROLLED_BACK:
            TransactionStatsData.RolledBackTransactions++;
            break;
        case TransactionStatus::TIMEOUT:
            TransactionStatsData.TimeoutTransactions++;
            break;
        case TransactionStatus::FAILED:
            TransactionStatsData.FailedTransactions++;
            break;
        default:
            break;
    }
    
    // 计算成功率
    if (TransactionStatsData.TotalTransactions > 0)
    {
        TransactionStatsData.SuccessRate = static_cast<double>(TransactionStatsData.CommittedTransactions) / TransactionStatsData.TotalTransactions;
        TransactionStatsData.RollbackRate = static_cast<double>(TransactionStatsData.RolledBackTransactions) / TransactionStatsData.TotalTransactions;
        TransactionStatsData.TimeoutRate = static_cast<double>(TransactionStatsData.TimeoutTransactions) / TransactionStatsData.TotalTransactions;
    }
    
    // 计算平均时间
    if (Transaction.EndTime > Transaction.StartTime)
    {
        uint64_t Duration = Transaction.EndTime - Transaction.StartTime;
        if (Transaction.Status == TransactionStatus::COMMITTED)
        {
            TransactionStatsData.AverageCommitTimeMs = (TransactionStatsData.AverageCommitTimeMs + Duration) / 2.0;
        }
        else if (Transaction.Status == TransactionStatus::ROLLED_BACK)
        {
            TransactionStatsData.AverageRollbackTimeMs = (TransactionStatsData.AverageRollbackTimeMs + Duration) / 2.0;
        }
    }
    
    TransactionStatsData.LastUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void MessageQueue::NotifyTransactionCommit(TransactionId Id, bool Success, const std::string& ErrorMessage)
{
    if (TransactionCommitHandlerFunc)
    {
        TransactionCommitHandlerFunc(Id, Success, ErrorMessage);
    }
}

void MessageQueue::NotifyTransactionRollback(TransactionId Id, const std::string& Reason)
{
    if (TransactionRollbackHandlerFunc)
    {
        TransactionRollbackHandlerFunc(Id, Reason);
    }
}

void MessageQueue::NotifyTransactionTimeout(TransactionId Id)
{
    if (TransactionTimeoutHandlerFunc)
    {
        TransactionTimeoutHandlerFunc(Id);
    }
}

void MessageQueue::ProcessTransactionTimeouts()
{
    while (!StopTransactionTimeout)
    {
        std::unique_lock<std::mutex> Lock(TransactionTimeoutMutex);
        TransactionTimeoutCondition.wait_for(Lock, std::chrono::seconds(1));
        
        if (StopTransactionTimeout)
            break;
        
        auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        std::vector<TransactionId> TimeoutTransactions;
        
        {
            std::shared_lock<std::shared_mutex> TransactionsLock(TransactionsMutex);
            for (const auto& Pair : Transactions)
            {
                const Transaction& Transaction = Pair.second;
                if (Transaction.Status == TransactionStatus::PENDING && 
                    Transaction.StartTime + Transaction.TimeoutMs < Now)
                {
                    TimeoutTransactions.push_back(Transaction.Id);
                }
            }
        }
        
        // 处理超时事务
        for (TransactionId Id : TimeoutTransactions)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "事务超时: id={}", Id);
            
            {
                std::unique_lock<std::shared_mutex> TransactionsLock(TransactionsMutex);
                auto It = Transactions.find(Id);
                if (It != Transactions.end())
                {
                    It->second.Status = TransactionStatus::TIMEOUT;
                    It->second.EndTime = Now;
                    UpdateTransactionStats(It->second);
                }
            }
            
            NotifyTransactionTimeout(Id);
        }
    }
}

// 分布式事务支持实现
QueueResult MessageQueue::BeginDistributedTransaction(const std::string& CoordinatorId, const std::string& Description, uint32_t TimeoutMs)
{
    std::unique_lock<std::shared_mutex> Lock(TransactionsMutex);
    
    TransactionId Id = GenerateTransactionId();
    Transaction Transaction;
    Transaction.Id = Id;
    Transaction.Status = TransactionStatus::PENDING;
    Transaction.Description = Description;
    Transaction.TimeoutMs = TimeoutMs;
    Transaction.StartTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    Transaction.IsDistributed = true;
    Transaction.CoordinatorId = CoordinatorId;
    
    Transactions[Id] = Transaction;
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始分布式事务: id={}, coordinator={}, description={}, timeout={}ms", 
          Id, CoordinatorId, Description, TimeoutMs);
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::PrepareTransaction(TransactionId Id)
{
    std::unique_lock<std::shared_mutex> Lock(TransactionsMutex);
    
    auto It = Transactions.find(Id);
    if (It == Transactions.end())
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务不存在: id={}", Id);
        return QueueResult::TRANSACTION_NOT_FOUND;
    }
    
    Transaction& Transaction = It->second;
    if (Transaction.Status != TransactionStatus::PENDING)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务状态不正确: id={}, status={}", 
              Id, static_cast<int>(Transaction.Status));
        return QueueResult::INVALID_STATE;
    }
    
    // 检查所有操作是否可以执行
    bool AllCanPrepare = true;
    std::string ErrorMessage;
    
    for (const auto& Operation : Transaction.Operations)
    {
        // 这里可以添加预检查逻辑
        // 例如：检查队列是否存在、消息是否有效等
    }
    
    if (AllCanPrepare)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "事务准备就绪: id={}", Id);
        return QueueResult::SUCCESS;
    }
    else
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "事务准备失败: id={}, error={}", Id, ErrorMessage);
        return QueueResult::OPERATION_FAILED;
    }
}

QueueResult MessageQueue::CommitDistributedTransaction(TransactionId Id)
{
    // 分布式事务提交，需要两阶段提交协议
    // 这里简化实现，直接调用普通提交
    return CommitTransaction(Id);
}

QueueResult MessageQueue::RollbackDistributedTransaction(TransactionId Id, const std::string& Reason)
{
    // 分布式事务回滚
    return RollbackTransaction(Id, Reason);
}

// 压缩和加密管理实现
QueueResult MessageQueue::SetCompressionConfig(const std::string& QueueName, const CompressionConfig& Config)
{
    std::unique_lock<std::shared_mutex> Lock(CompressionConfigsMutex);
    CompressionConfigs[QueueName] = Config;
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "设置压缩配置: queue={}, algorithm={}, level={}, min_size={}", 
          QueueName, static_cast<int>(Config.Algorithm), Config.Level, Config.MinSize);
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetCompressionConfig(const std::string& QueueName, CompressionConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> Lock(CompressionConfigsMutex);
    
    auto It = CompressionConfigs.find(QueueName);
    if (It == CompressionConfigs.end())
    {
        OutConfig = CompressionConfig{};
        return QueueResult::SUCCESS;
    }
    
    OutConfig = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::SetEncryptionConfig(const std::string& QueueName, const EncryptionConfig& Config)
{
    std::unique_lock<std::shared_mutex> Lock(EncryptionConfigsMutex);
    EncryptionConfigs[QueueName] = Config;
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "设置加密配置: queue={}, algorithm={}, auto_encrypt={}", 
          QueueName, static_cast<int>(Config.Algorithm), Config.EnableAutoEncryption);
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetEncryptionConfig(const std::string& QueueName, EncryptionConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> Lock(EncryptionConfigsMutex);
    
    auto It = EncryptionConfigs.find(QueueName);
    if (It == EncryptionConfigs.end())
    {
        OutConfig = EncryptionConfig{};
        return QueueResult::SUCCESS;
    }
    
    OutConfig = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetCompressionStats(const std::string& QueueName, CompressionStats& OutStats) const
{
    std::lock_guard<std::mutex> Lock(CompressionStatsMutex);
    
    auto It = CompressionStatsData.find(QueueName);
    if (It == CompressionStatsData.end())
    {
        OutStats = CompressionStats{};
        return QueueResult::SUCCESS;
    }
    
    OutStats = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAllCompressionStats(std::vector<CompressionStats>& OutStats) const
{
    std::lock_guard<std::mutex> Lock(CompressionStatsMutex);
    
    OutStats.clear();
    for (const auto& Pair : CompressionStatsData)
    {
        OutStats.push_back(Pair.second);
    }
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetEncryptionStats(const std::string& QueueName, EncryptionStats& OutStats) const
{
    std::lock_guard<std::mutex> Lock(EncryptionStatsMutex);
    
    auto It = EncryptionStatsData.find(QueueName);
    if (It == EncryptionStatsData.end())
    {
        OutStats = EncryptionStats{};
        return QueueResult::SUCCESS;
    }
    
    OutStats = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAllEncryptionStats(std::vector<EncryptionStats>& OutStats) const
{
    std::lock_guard<std::mutex> Lock(EncryptionStatsMutex);
    
    OutStats.clear();
    for (const auto& Pair : EncryptionStatsData)
    {
        OutStats.push_back(Pair.second);
    }
    
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::CompressMessage(MessagePtr Message, CompressionAlgorithm Algorithm)
{
    if (!Message || Message->Payload.Data.empty())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "压缩消息: algorithm={}, size={}", 
          static_cast<int>(Algorithm), Message->Payload.Data.size());
    
    // 简化实现，直接返回成功
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::DecompressMessage(MessagePtr Message)
{
    if (!Message || Message->Payload.Data.empty())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "解压消息: size={}", Message->Payload.Data.size());
    
    // 简化实现，直接返回成功
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::EncryptMessage(MessagePtr Message, EncryptionAlgorithm Algorithm)
{
    if (!Message || Message->Payload.Data.empty())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "加密消息: algorithm={}, size={}", 
          static_cast<int>(Algorithm), Message->Payload.Data.size());
    
    // 简化实现，直接返回成功
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::DecryptMessage(MessagePtr Message)
{
    if (!Message || Message->Payload.Data.empty())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "解密消息: size={}", Message->Payload.Data.size());
    
    // 简化实现，直接返回成功
    return QueueResult::SUCCESS;
}

// 监控告警管理实现 - 简化版本
QueueResult MessageQueue::SetAlertConfig(const AlertConfig& Config)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "设置告警配置: type={}, queue={}", 
          static_cast<int>(Config.Type), Config.QueueName);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAlertConfig(AlertType Type, const std::string& QueueName, AlertConfig& OutConfig) const
{
    OutConfig = AlertConfig{};
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAllAlertConfigs(std::vector<AlertConfig>& OutConfigs) const
{
    OutConfigs.clear();
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::DeleteAlertConfig(AlertType Type, const std::string& QueueName)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "删除告警配置: type={}, queue={}", 
          static_cast<int>(Type), QueueName);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetActiveAlerts(std::vector<Alert>& OutAlerts) const
{
    OutAlerts.clear();
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAlertHistory(uint32_t Limit, std::vector<Alert>& OutAlerts) const
{
    OutAlerts.clear();
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAlertStats(AlertStats& OutStats) const
{
    OutStats = AlertStats{};
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::AcknowledgeAlert(uint64_t AlertId)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "确认告警: id={}", AlertId);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ResolveAlert(uint64_t AlertId, const std::string& ResolutionNote)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "解决告警: id={}, note={}", AlertId, ResolutionNote);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ClearAllAlerts()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "清空所有告警");
    return QueueResult::SUCCESS;
}

void MessageQueue::SetAlertHandler(AlertHandler Handler)
{
    AlertHandlerFunc = Handler;
}

void MessageQueue::SetAlertConfigHandler(AlertConfigHandler Handler)
{
    AlertConfigHandlerFunc = Handler;
}

// 告警相关辅助方法 - 简化版本
uint64_t MessageQueue::GenerateAlertId()
{
    static std::atomic<uint64_t> NextAlertId{1};
    return NextAlertId++;
}

std::string MessageQueue::MakeAlertConfigKey(AlertType Type, const std::string& QueueName)
{
    return std::to_string(static_cast<int>(Type)) + "_" + QueueName;
}

void MessageQueue::ProcessAlertMonitoring()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "启动告警监控线程");

    // 周期性检查间隔（秒）
    constexpr auto kCheckInterval = std::chrono::seconds(5);

    while (!StopAlertMonitor)
    {
        // 等待或被唤醒（Shutdown 时唤醒）
        {
            std::unique_lock<std::mutex> lock(AlertMonitorMutex);
            AlertMonitorCondition.wait_for(lock, kCheckInterval, [this]() { return StopAlertMonitor.load(); });
        }
        if (StopAlertMonitor)
        {
            break;
        }

        // 周期性执行检查
        try { CheckQueueAlerts(); } catch (...) {}
        try { CheckSystemAlerts(); } catch (...) {}
        try { ProcessBatchTimeout(); } catch (...) {}
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "告警监控线程停止");
}

void MessageQueue::CheckQueueAlerts()
{
    // 简化实现
}

void MessageQueue::CheckSystemAlerts()
{
    // 简化实现
}

void MessageQueue::TriggerAlert(const AlertConfig& Config, double CurrentValue, const std::string& Message, const std::string& Details)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "触发告警: type={}, level={}, message={}", 
          static_cast<int>(Config.Type), static_cast<int>(Config.Level), Message);
}

void MessageQueue::ResolveAlertInternal(uint64_t AlertId, const std::string& ResolutionNote)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "解决告警: id={}", AlertId);
}

void MessageQueue::UpdateAlertStats(const Alert& Alert, bool IsNew)
{
    // 简化实现
}

void MessageQueue::NotifyAlert(const Alert& Alert)
{
    if (AlertHandlerFunc)
    {
        try
        {
            AlertHandlerFunc(Alert);
        }
        catch (const std::exception& e)
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "告警处理器异常: {}", e.what());
        }
    }
}

// 性能优化管理实现 - 简化版本
QueueResult MessageQueue::SetMemoryPoolConfig(const MemoryPoolConfig& Config)
{
    {
        std::unique_lock<std::shared_mutex> lock(MemoryPoolConfigMutex);
        MemoryPoolConfigData = Config;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "设置内存池配置: initial_size={}, max_size={}, block_size={}", 
          Config.InitialSize, Config.MaxSize, Config.BlockSize);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetMemoryPoolConfig(MemoryPoolConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> lock(MemoryPoolConfigMutex);
    OutConfig = MemoryPoolConfigData;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::SetBufferConfig(const BufferConfig& Config)
{
    {
        std::unique_lock<std::shared_mutex> lock(BufferConfigMutex);
        BufferConfigData = Config;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "设置缓冲区配置: initial_capacity={}, max_capacity={}, batching={} size={} timeout_ms={} zero_copy={}",
          Config.InitialCapacity, Config.MaxCapacity, Config.EnableBatching, Config.BatchSize, Config.BatchTimeoutMs, Config.EnableZeroCopy);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetBufferConfig(BufferConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> lock(BufferConfigMutex);
    OutConfig = BufferConfigData;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetPerformanceStats(PerformanceStats& OutStats) const
{
    std::lock_guard<std::mutex> guard(PerformanceStatsMutex);
    OutStats = PerformanceStatsData;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ResetPerformanceStats()
{
    {
        std::lock_guard<std::mutex> guard(PerformanceStatsMutex);
        PerformanceStatsData = PerformanceStats{};
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "重置性能统计");
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::AllocateFromPool(size_t Size, void*& OutPtr)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> guard(MemoryPoolMutex);
    OutPtr = nullptr;

    // 尝试从内存池按块分配
    MemoryBlock* block = FindFreeBlock(Size);
    if (block)
    {
        MarkBlockAsUsed(block);
        OutPtr = block->Data;
        {
            std::lock_guard<std::mutex> s(PerformanceStatsMutex);
            PerformanceStatsData.MemoryPoolHits++;
        }
    }
    else
    {
        // 回退到系统分配
        OutPtr = std::malloc(Size);
        {
            std::lock_guard<std::mutex> s(PerformanceStatsMutex);
            PerformanceStatsData.MemoryPoolMisses++;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    UpdatePerformanceStats("allocation", ms, Size);

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Verbose, "从内存池分配: size={}, ptr={}, hit={}",
          Size, OutPtr, block != nullptr);
    return OutPtr ? QueueResult::SUCCESS : QueueResult::OPERATION_FAILED;
}

QueueResult MessageQueue::DeallocateToPool(void* Ptr, size_t Size)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    bool returnedToPool = false;
    {
        std::lock_guard<std::mutex> guard(MemoryPoolMutex);
        for (auto* block : MemoryPoolBlocks)
        {
            if (block->Data == Ptr)
            {
                MarkBlockAsFree(block);
                returnedToPool = true;
                break;
            }
        }
    }
    if (!returnedToPool)
    {
        std::free(Ptr);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    UpdatePerformanceStats("deallocation", ms, Size);

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Verbose, "释放到{}: ptr={}, size={}", returnedToPool ? "内存池" : "系统", Ptr, Size);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::CompactMemoryPool()
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "内存池压缩");
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::CreateZeroCopyBuffer(const void* Data, size_t Size, ZeroCopyBuffer& OutBuffer)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    OutBuffer.Data = const_cast<void*>(Data);
    OutBuffer.Size = Size;
    OutBuffer.Capacity = Size;
    OutBuffer.IsOwned = false;
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    UpdatePerformanceStats("zero_copy", ms, Size);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Verbose, "创建零拷贝缓冲区: size={}", Size);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::ReleaseZeroCopyBuffer(ZeroCopyBuffer& Buffer)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "释放零拷贝缓冲区");
    Buffer.Data = nullptr;
    Buffer.Size = 0;
    Buffer.Capacity = 0;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::SendMessageZeroCopy(const std::string& QueueName, const ZeroCopyBuffer& Buffer)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    auto msg = std::make_shared<Message>();
    // 真正零拷贝：直接引用外部数据，不复制
    msg->Payload.SetExternal(Buffer.Data, Buffer.Size, /*Owned*/ false, /*Deallocator*/ nullptr);
    auto res = SendMessage(QueueName, msg);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    UpdatePerformanceStats("zero_copy", ms, Buffer.Size);
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Verbose, "零拷贝发送消息: queue={}, size={}, code={}", QueueName, Buffer.Size, static_cast<int>(res));
    return res;
}

QueueResult MessageQueue::CreateBatch(uint32_t& OutBatchId)
{
    OutBatchId = NextBatchId++;
    BatchMessage batch;
    batch.BatchId = OutBatchId;
    batch.CreateTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    {
        std::shared_lock<std::shared_mutex> lock(BufferConfigMutex);
        batch.ExpireTime = batch.CreateTime + BufferConfigData.BatchTimeoutMs;
    }
    {
        std::lock_guard<std::mutex> guard(BatchesMutex);
        ActiveBatches[OutBatchId] = batch;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建批处理: id={}", OutBatchId);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::CreateBatchForQueue(const std::string& QueueName, uint32_t& OutBatchId)
{
    auto r = CreateBatch(OutBatchId);
    if (r != QueueResult::SUCCESS)
    {
        return r;
    }
    {
        std::lock_guard<std::mutex> guard(BatchesMutex);
        auto it = ActiveBatches.find(OutBatchId);
        if (it != ActiveBatches.end())
        {
            it->second.QueueName = QueueName;
        }
    }
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::AddToBatch(uint32_t BatchId, MessagePtr Message)
{
    std::shared_lock<std::shared_mutex> cfgLock(BufferConfigMutex);
    const bool enableBatching = BufferConfigData.EnableBatching;
    const uint32_t batchSize = BufferConfigData.BatchSize;
    cfgLock.unlock();

    std::lock_guard<std::mutex> guard(BatchesMutex);
    auto it = ActiveBatches.find(BatchId);
    if (it == ActiveBatches.end())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    auto& batch = it->second;
    batch.Messages.push_back(std::move(Message));
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Verbose, "添加到批处理: batch_id={}, count={}", BatchId, batch.Messages.size());

    if (enableBatching && batch.Messages.size() >= batchSize)
    {
        // 达到阈值自动提交
        return CommitBatch(BatchId);
    }
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::CommitBatch(uint32_t BatchId)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    std::vector<MessagePtr> messages;
    std::string queueName;
    {
        std::lock_guard<std::mutex> guard(BatchesMutex);
        auto it = ActiveBatches.find(BatchId);
        if (it == ActiveBatches.end())
        {
            return QueueResult::INVALID_PARAMETER;
        }
        messages = std::move(it->second.Messages);
        queueName = it->second.QueueName;
        ActiveBatches.erase(it);
    }

    QueueResult res = QueueResult::SUCCESS;
    if (!messages.empty())
    {
        if (!queueName.empty())
        {
            res = SendBatchMessages(queueName, messages);
        }
        else
        {
            // 回退到逐条发送（保持兼容）
            for (auto& m : messages)
            {
                auto r1 = SendMessage("", m);
                if (r1 != QueueResult::SUCCESS)
                {
                    res = r1;
                }
            }
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    UpdatePerformanceStats("batch", ms, messages.size());
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "提交批处理: id={}, messages={}", BatchId, messages.size());
    return res;
}

QueueResult MessageQueue::AbortBatch(uint32_t BatchId)
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "中止批处理: id={}", BatchId);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetBatchInfo(uint32_t BatchId, BatchMessage& OutBatch) const
{
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "获取批处理信息: id={}", BatchId);
    return QueueResult::SUCCESS;
}

// 性能优化相关辅助方法 - 基础实现
void MessageQueue::InitializeMemoryPool()
{
    std::lock_guard<std::mutex> guard(MemoryPoolMutex);

    // 避免重复初始化
    if (MemoryPoolData != nullptr)
    {
        return;
    }

    // 读取配置（默认值已在 MemoryPoolConfigData 中）
    MemoryPoolSize = MemoryPoolConfigData.InitialSize;
    MemoryPoolData = std::malloc(MemoryPoolSize);
    if (!MemoryPoolData)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "内存池初始化失败: 分配 {} 字节失败", MemoryPoolSize);
        return;
    }

    // 切分为固定大小块
    const size_t blockSize = std::max<size_t>(MemoryPoolConfigData.BlockSize, 64);
    const size_t blockCount = MemoryPoolSize / blockSize;
    MemoryPoolBlocks.reserve(blockCount);
    FreeBlocks.reserve(blockCount);

    char* base = static_cast<char*>(MemoryPoolData);
    for (size_t i = 0; i < blockCount; ++i)
    {
        auto* block = new MemoryBlock();
        block->Data = base + i * blockSize;
        block->Size = blockSize;
        block->IsUsed = false;
        block->Next = nullptr;
        block->AllocTime = 0;
        MemoryPoolBlocks.push_back(block);
        FreeBlocks.push_back(block);
    }

    MemoryPoolUsed = 0;
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "内存池初始化完成: size={} bytes, block_size={}, blocks={}",
          MemoryPoolSize, blockSize, blockCount);
}

void MessageQueue::CleanupMemoryPool()
{
    std::lock_guard<std::mutex> guard(MemoryPoolMutex);
    for (auto* block : MemoryPoolBlocks)
    {
        delete block;
    }
    MemoryPoolBlocks.clear();
    FreeBlocks.clear();

    if (MemoryPoolData)
    {
        std::free(MemoryPoolData);
        MemoryPoolData = nullptr;
    }
    MemoryPoolSize = 0;
    MemoryPoolUsed = 0;
}

MemoryBlock* MessageQueue::FindFreeBlock(size_t Size)
{
    // 简单的首适配：按块大小分配，不支持大块拼接（后续可扩展）
    for (auto* block : FreeBlocks)
    {
        if (!block->IsUsed && block->Size >= Size)
        {
            return block;
        }
    }
    return nullptr;
}

void MessageQueue::MarkBlockAsUsed(MemoryBlock* Block)
{
    if (Block == nullptr || Block->IsUsed)
    {
        return;
    }
    Block->IsUsed = true;
    Block->AllocTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    // 从空闲表移除
    auto it = std::find(FreeBlocks.begin(), FreeBlocks.end(), Block);
    if (it != FreeBlocks.end())
    {
        FreeBlocks.erase(it);
    }
    MemoryPoolUsed += Block->Size;
}

void MessageQueue::MarkBlockAsFree(MemoryBlock* Block)
{
    if (Block == nullptr || !Block->IsUsed)
    {
        return;
    }
    Block->IsUsed = false;
    Block->AllocTime = 0;
    FreeBlocks.push_back(Block);
    if (MemoryPoolUsed >= Block->Size)
    {
        MemoryPoolUsed -= Block->Size;
    }
}

void MessageQueue::UpdatePerformanceStats(const std::string& Operation, double TimeMs, size_t Size)
{
    std::lock_guard<std::mutex> guard(PerformanceStatsMutex);
    auto& s = PerformanceStatsData;
    if (Operation == "allocation")
    {
        s.TotalAllocations++;
        s.TotalBytesAllocated += Size;
        s.CurrentBytesAllocated += Size;
        s.PeakBytesAllocated = std::max(s.PeakBytesAllocated, s.CurrentBytesAllocated);
        s.AverageAllocationTimeMs = (s.AverageAllocationTimeMs + TimeMs) / 2.0;
    }
    else if (Operation == "deallocation")
    {
        s.TotalDeallocations++;
        if (s.CurrentBytesAllocated >= Size)
        {
            s.CurrentBytesAllocated -= Size;
        }
        s.AverageDeallocationTimeMs = (s.AverageDeallocationTimeMs + TimeMs) / 2.0;
    }
    else if (Operation == "zero_copy")
    {
        s.ZeroCopyOperations++;
        s.AverageZeroCopyTimeMs = (s.AverageZeroCopyTimeMs + TimeMs) / 2.0;
    }
    else if (Operation == "batch")
    {
        s.BatchOperations++;
        s.AverageBatchTimeMs = (s.AverageBatchTimeMs + TimeMs) / 2.0;
    }

    const uint64_t totalReq = s.MemoryPoolHits + s.MemoryPoolMisses;
    if (totalReq > 0)
    {
        s.MemoryPoolHitRate = static_cast<double>(s.MemoryPoolHits) / static_cast<double>(totalReq);
    }
    s.LastUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
}

void MessageQueue::ProcessBatchTimeout()
{
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    std::lock_guard<std::mutex> guard(BatchesMutex);
    std::vector<uint32_t> expired;
    for (const auto& kv : ActiveBatches)
    {
        const auto& batch = kv.second;
        if (batch.ExpireTime > 0 && batch.ExpireTime <= now)
        {
            expired.push_back(kv.first);
        }
    }
    for (auto id : expired)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "批处理超时: id={}", id);
        ActiveBatches.erase(id);
    }
}

void MessageQueue::CompressBatch(BatchMessage& Batch)
{
    // 预留：后续接入真实压缩。当前仅计算大小
    Batch.OriginalSize = 0;
    for (const auto& m : Batch.Messages)
    {
        Batch.OriginalSize += m ? m->Payload.Data.size() : 0;
    }
    Batch.CompressedSize = Batch.OriginalSize;
    Batch.IsCompressed = false;
}

void MessageQueue::DecompressBatch(BatchMessage& Batch)
{
    // 预留
    Batch.IsCompressed = false;
}
}  // namespace Helianthus::MessageQueue