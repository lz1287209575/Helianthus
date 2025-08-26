#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <map>

#include "IMessageQueue.h"
#include "MessagePersistence.h"

namespace Helianthus::MessageQueue
{
/**
 * @brief 高性能消息队列实现
 *
 * 支持多种消息传递模式：
 * - 点对点队列（生产者-消费者）
 * - 发布订阅模式（发布者-订阅者）
 * - 广播消息
 * - 优先级队列
 * - 延迟消息
 * - 死信队列
 */
class MessageQueue : public IMessageQueue
{
public:
    MessageQueue();
    ~MessageQueue() override;

    // 禁用拷贝构造和赋值
    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;

    // 生命周期管理
    QueueResult Initialize() override;
    void Shutdown() override;
    bool IsInitialized() const override;

    // 队列管理
    QueueResult CreateQueue(const QueueConfig& Config) override;
    QueueResult DeleteQueue(const std::string& QueueName) override;
    QueueResult PurgeQueue(const std::string& QueueName) override;
    bool QueueExists(const std::string& QueueName) const override;
    std::vector<std::string> ListQueues() const override;
    QueueResult GetQueueInfo(const std::string& QueueName, QueueConfig& OutConfig) const override;
    QueueResult UpdateQueueConfig(const std::string& QueueName, const QueueConfig& Config) override;

    // 主题管理（发布订阅）
    QueueResult CreateTopic(const TopicConfig& Config) override;
    QueueResult DeleteTopic(const std::string& TopicName) override;
    bool TopicExists(const std::string& TopicName) const override;
    std::vector<std::string> ListTopics() const override;
    QueueResult GetTopicInfo(const std::string& TopicName, TopicConfig& OutConfig) const override;

    // 消息发送（生产者）
    QueueResult SendMessage(const std::string& QueueName, MessagePtr Message) override;
    QueueResult SendMessageAsync(const std::string& QueueName,
                                 MessagePtr Message,
                                 AcknowledgeHandler Handler = nullptr) override;
    QueueResult SendBatchMessages(const std::string& QueueName,
                                  const std::vector<MessagePtr>& Messages) override;
    std::future<QueueResult> SendMessageFuture(const std::string& QueueName,
                                               MessagePtr Message) override;

    // 消息接收（消费者）
    QueueResult ReceiveMessage(const std::string& QueueName,
                               MessagePtr& OutMessage,
                               uint32_t TimeoutMs = 0) override;
    QueueResult ReceiveBatchMessages(const std::string& QueueName,
                                     std::vector<MessagePtr>& OutMessages,
                                     uint32_t MaxCount = 10,
                                     uint32_t TimeoutMs = 1000) override;
    QueueResult PeekMessage(const std::string& QueueName, MessagePtr& OutMessage) override;

    // 消息确认
    QueueResult AcknowledgeMessage(const std::string& QueueName, MessageId MessageId) override;
    QueueResult
    RejectMessage(const std::string& QueueName, MessageId MessageId, bool Requeue = true) override;
    QueueResult AcknowledgeBatch(const std::string& QueueName,
                                 const std::vector<MessageId>& MessageIds) override;

    // 消费者管理
    QueueResult RegisterConsumer(const std::string& QueueName,
                                 const ConsumerConfig& Config,
                                 MessageHandler Handler) override;
    QueueResult RegisterBatchConsumer(const std::string& QueueName,
                                      const ConsumerConfig& Config,
                                      BatchMessageHandler Handler) override;
    QueueResult UnregisterConsumer(const std::string& QueueName,
                                   const std::string& ConsumerId) override;
    std::vector<std::string> GetActiveConsumers(const std::string& QueueName) const override;

    // 生产者管理
    QueueResult RegisterProducer(const std::string& QueueName,
                                 const ProducerConfig& Config) override;
    QueueResult UnregisterProducer(const std::string& QueueName,
                                   const std::string& ProducerId) override;
    std::vector<std::string> GetActiveProducers(const std::string& QueueName) const override;

    // 发布订阅
    QueueResult PublishMessage(const std::string& TopicName, MessagePtr Message) override;
    QueueResult PublishBatchMessages(const std::string& TopicName,
                                     const std::vector<MessagePtr>& Messages) override;
    QueueResult Subscribe(const std::string& TopicName,
                          const std::string& SubscriberId,
                          MessageHandler Handler) override;
    QueueResult Unsubscribe(const std::string& TopicName, const std::string& SubscriberId) override;
    std::vector<std::string> GetActiveSubscribers(const std::string& TopicName) const override;

    // 广播消息
    QueueResult BroadcastMessage(MessagePtr Message) override;
    QueueResult BroadcastToQueues(const std::vector<std::string>& QueueNames,
                                  MessagePtr Message) override;
    QueueResult BroadcastToTopics(const std::vector<std::string>& TopicNames,
                                  MessagePtr Message) override;

    // 延迟和定时消息
    QueueResult
    ScheduleMessage(const std::string& QueueName, MessagePtr Message, uint32_t DelayMs) override;
    QueueResult ScheduleRecurringMessage(const std::string& QueueName,
                                         MessagePtr Message,
                                         uint32_t IntervalMs,
                                         uint32_t Count = 0) override;
    QueueResult CancelScheduledMessage(MessageId MessageId) override;

    // 消息过滤和路由
    QueueResult SetMessageFilter(const std::string& QueueName,
                                 const std::string& FilterExpression) override;
    QueueResult SetMessageRouter(const std::string& SourceQueue,
                                 const std::string& TargetQueue,
                                 const std::string& RoutingKey) override;
    QueueResult RemoveMessageFilter(const std::string& QueueName) override;
    QueueResult RemoveMessageRouter(const std::string& SourceQueue,
                                    const std::string& TargetQueue) override;

    // 死信队列管理
    QueueResult GetDeadLetterMessages(const std::string& QueueName,
                                      std::vector<MessagePtr>& OutMessages,
                                      uint32_t MaxCount = 100) override;
    QueueResult RequeueDeadLetterMessage(const std::string& QueueName,
                                         MessageId MessageId) override;
    QueueResult PurgeDeadLetterQueue(const std::string& QueueName) override;

    // 统计和监控
    QueueResult GetQueueStats(const std::string& QueueName, QueueStats& OutStats) const override;
    QueueResult GetTopicStats(const std::string& TopicName, QueueStats& OutStats) const override;
    QueueResult GetGlobalStats(QueueStats& OutStats) const override;
    std::vector<MessagePtr> GetPendingMessages(const std::string& QueueName,
                                               uint32_t MaxCount = 100) const override;

    // DLQ监控
    QueueResult GetDeadLetterQueueStats(const std::string& QueueName, 
                                        DeadLetterQueueStats& OutStats) const override;
    QueueResult GetAllDeadLetterQueueStats(std::vector<DeadLetterQueueStats>& OutStats) const override;
    QueueResult SetDeadLetterAlertConfig(const std::string& QueueName, 
                                         const DeadLetterAlertConfig& Config) override;
    QueueResult GetDeadLetterAlertConfig(const std::string& QueueName, 
                                         DeadLetterAlertConfig& OutConfig) const override;
    QueueResult GetActiveDeadLetterAlerts(const std::string& QueueName,
                                         std::vector<DeadLetterAlert>& OutAlerts) const override;
    QueueResult GetAllActiveDeadLetterAlerts(std::vector<DeadLetterAlert>& OutAlerts) const override;
    QueueResult ClearDeadLetterAlert(const std::string& QueueName, 
                                     DeadLetterAlertType AlertType) override;
    QueueResult ClearAllDeadLetterAlerts(const std::string& QueueName) override;
    void SetDeadLetterAlertHandler(DeadLetterAlertHandler Handler) override;
    void SetDeadLetterStatsHandler(DeadLetterStatsHandler Handler) override;

    // 持久化管理
    QueueResult SaveToDisk() override;
    QueueResult LoadFromDisk() override;
    QueueResult EnablePersistence(const std::string& QueueName, PersistenceMode Mode) override;
    QueueResult DisablePersistence(const std::string& QueueName) override;

    // 集群和复制
    QueueResult EnableReplication(const std::vector<std::string>& ReplicaNodes) override;
    QueueResult DisableReplication() override;
    bool IsReplicationEnabled() const override;
    QueueResult SyncWithReplicas() override;

    // 事件回调
    void SetQueueEventHandler(QueueEventHandler Handler) override;
    void SetErrorHandler(ErrorHandler Handler) override;
    void RemoveAllHandlers() override;

    // 配置和调优
    QueueResult SetGlobalConfig(const std::string& Key, const std::string& Value) override;
    std::string GetGlobalConfig(const std::string& Key) const override;
    QueueResult FlushAll() override;
    QueueResult CompactQueues() override;

    // 调试和诊断
    std::string GetQueueInfo() const override;
    std::vector<std::string> GetQueueDiagnostics(const std::string& QueueName) const override;
    QueueResult ValidateQueue(const std::string& QueueName) override;

    // 指标查询
    virtual QueueResult GetQueueMetrics(const std::string& QueueName, QueueMetrics& OutMetrics) const override;
    virtual QueueResult GetAllQueueMetrics(std::vector<QueueMetrics>& OutMetrics) const override;

    // 集群/分片/副本接口
    QueueResult SetClusterConfig(const ClusterConfig& Config) override;
    QueueResult GetClusterConfig(ClusterConfig& OutConfig) const override;
    QueueResult GetShardForKey(const std::string& Key, ShardId& OutShardId, std::string& OutNodeId) const override;
    QueueResult GetShardReplicas(ShardId Shard, std::vector<ReplicaInfo>& OutReplicas) const override;
    QueueResult SetNodeHealth(const std::string& NodeId, bool Healthy) override;
    // 分片状态查询（导出每分片详细状态）
    QueueResult GetClusterShardStatuses(std::vector<ShardInfo>& OutShards) const override;

    // 基础HA：主选举/降级/查询/回调
    QueueResult PromoteToLeader(ShardId Shard, const std::string& NodeId) override;
    QueueResult DemoteToFollower(ShardId Shard, const std::string& NodeId) override;
    QueueResult GetCurrentLeader(ShardId Shard, std::string& OutNodeId) const override;
    void SetLeaderChangeHandler(LeaderChangeHandler Handler) override;
    void SetFailoverHandler(FailoverHandler Handler) override;

private:
    // 一致性哈希分片路由
    class ConsistentHashRing
    {
    public:
        void Clear() { Ring.clear(); }
        void AddNode(const std::string& NodeId, uint32_t VirtualNodes)
        {
            for (uint32_t i = 0; i < VirtualNodes; ++i)
            {
                std::string VirtualNodeKey = NodeId + "#" + std::to_string(i);
                uint32_t Hash = static_cast<uint32_t>(std::hash<std::string>{}(VirtualNodeKey));
                Ring.emplace(Hash, NodeId);
            }
        }
        std::string GetNode(const std::string& Key) const
        {
            if (Ring.empty()) return {};
            uint32_t Hash = static_cast<uint32_t>(std::hash<std::string>{}(Key));
            auto It = Ring.lower_bound(Hash);
            if (It == Ring.end()) return Ring.begin()->second;
            return It->second;
        }
        size_t Size() const { return Ring.size(); }
    private:
        std::map<uint32_t, std::string> Ring;
    };

    void RebuildShardRing();

    ConsistentHashRing ShardRing;
    uint32_t ShardCount = 1;
    uint32_t ShardVirtualNodes = 128;

    // 集群配置与元数据
    ClusterConfig Cluster;
    mutable std::shared_mutex ClusterMutex;

    // WAL 占位：每分片一份日志与Follower位点
    struct WalEntry
    {
        uint64_t Index = 0;
        MessageId Id = 0;
        std::string Queue;
        MessageTimestamp Timestamp = 0;
    };
    struct ShardWal
    {
        std::vector<WalEntry> Entries;
        std::unordered_map<std::string, uint64_t> FollowerAppliedIndex; // NodeId -> last applied
    };
    std::vector<ShardWal> WalPerShard;

    // 内部数据结构
    struct QueueData
    {
        QueueConfig Config;
        std::queue<MessagePtr> Messages;
        std::priority_queue<MessagePtr,
                            std::vector<MessagePtr>,
                            std::function<bool(const MessagePtr&, const MessagePtr&)>>
            PriorityMessages;
        std::queue<MessagePtr> DeadLetterMessages;
        std::unordered_map<MessageId, MessagePtr> PendingAcknowledgments;
        std::unordered_map<std::string, ConsumerConfig> Consumers;
        std::unordered_map<std::string, ProducerConfig> Producers;
        std::unordered_map<std::string, MessageHandler> ConsumerHandlers;
        std::unordered_map<std::string, BatchMessageHandler> BatchConsumerHandlers;
        QueueStats Stats;
        mutable std::shared_mutex Mutex;
        std::condition_variable_any NotifyCondition;
        std::string MessageFilter;
        std::unordered_map<std::string, std::string> MessageRouters;

        // 指标滑动窗口（最近窗口秒内的事件时间戳与时延样本）
        std::deque<MessageTimestamp> EnqueueTimestamps;
        std::deque<MessageTimestamp> DequeueTimestamps;
        std::vector<double> LatencySamplesMs; // 环形缓冲上限
        size_t LatencyCapacity = 512;
        MessageTimestamp LastMetricsSampleTs = 0;

        QueueData(const QueueConfig& Config);
        bool ShouldUsePriorityQueue() const;
        void AddMessage(MessagePtr Message);
        MessagePtr GetNextMessage();
        bool IsEmpty() const;
        size_t GetMessageCount() const;
    };

    struct TopicData
    {
        TopicConfig Config;
        std::unordered_map<std::string, MessageHandler> Subscribers;
        std::vector<MessagePtr> RecentMessages;
        QueueStats Stats;
        mutable std::shared_mutex Mutex;

        TopicData(const TopicConfig& Config);
    };

    struct ScheduledMessage
    {
        MessagePtr Message;
        std::string QueueName;
        MessageTimestamp ExecuteTime;
        uint32_t IntervalMs;
        uint32_t RemainingCount;
        bool IsRecurring;

        ScheduledMessage(MessagePtr Msg,
                         const std::string& Queue,
                         MessageTimestamp ExecTime,
                         uint32_t Interval = 0,
                         uint32_t Count = 0);
    };

    // 内部状态
    std::atomic<bool> Initialized;
    std::atomic<bool> ShuttingDown;

    // 队列和主题存储
    std::unordered_map<std::string, std::unique_ptr<QueueData>> Queues;
    std::unordered_map<std::string, std::unique_ptr<TopicData>> Topics;
    mutable std::shared_mutex QueuesMutex;
    mutable std::shared_mutex TopicsMutex;

    // 消息ID生成
    std::atomic<MessageId> NextMessageId;

    // 调度消息管理
    std::vector<ScheduledMessage> ScheduledMessages;
    std::mutex ScheduledMessagesMutex;
    std::thread SchedulerThread;
    std::condition_variable SchedulerCondition;

    // 消费者工作线程
    std::vector<std::thread> ConsumerThreads;
    std::atomic<bool> StopConsumerThreads;

    // 事件处理
    QueueEventHandler EventHandler;
    ErrorHandler ErrorHandlerFunc;
    std::mutex HandlersMutex;

    // DLQ监控
    std::unordered_map<std::string, DeadLetterAlertConfig> DeadLetterAlertConfigs;
    std::unordered_map<std::string, std::vector<DeadLetterAlert>> ActiveDeadLetterAlerts;
    DeadLetterAlertHandler DeadLetterAlertHandlerFunc;
    DeadLetterStatsHandler DeadLetterStatsHandlerFunc;
    std::thread DeadLetterMonitorThread;
    std::atomic<bool> StopDeadLetterMonitor;
    std::condition_variable DeadLetterMonitorCondition;
    mutable std::shared_mutex DeadLetterMonitorMutex;

    // 指标监控线程
    std::thread MetricsMonitorThread;
    std::atomic<bool> StopMetricsMonitor{false};
    std::condition_variable MetricsMonitorCondition;
    mutable std::mutex MetricsMonitorMutex;
    uint32_t MetricsIntervalMs = 5000; // 默认5秒

    // 轻量心跳占位线程
    std::thread HeartbeatThread;
    std::atomic<bool> StopHeartbeat{false};
    std::condition_variable HeartbeatCondition;
    mutable std::mutex HeartbeatMutex;
    uint32_t HeartbeatIntervalMs = 3000; // 默认3秒
    double HeartbeatFlapProbability = 0.30; // 提高至30%几率模拟健康波动（演示）

    // HA回调
    LeaderChangeHandler LeaderChangedCb;
    FailoverHandler FailoverCb;

    // 全局配置
    std::unordered_map<std::string, std::string> GlobalConfig;
    mutable std::shared_mutex ConfigMutex;

    // 统计信息
    QueueStats GlobalStats;
    mutable std::shared_mutex StatsMutex;

    // 持久化管理器
    std::unique_ptr<PersistenceManager> PersistenceMgr;
    PersistenceConfig PersistenceSettings;

    // 内部方法
    MessageId GenerateMessageId();
    QueueData* GetQueueData(const std::string& QueueName);
    const QueueData* GetQueueData(const std::string& QueueName) const;
    TopicData* GetTopicData(const std::string& TopicName);
    const TopicData* GetTopicData(const std::string& TopicName) const;

    void ProcessConsumerQueue(const std::string& QueueName);
    void ProcessScheduledMessages();
    void
    NotifyEvent(const std::string& QueueName, const std::string& Event, const std::string& Details);
    void NotifyError(QueueResult Result, const std::string& ErrorMessage);

    bool ValidateMessage(MessagePtr Message);
    bool ValidateQueueConfig(const QueueConfig& Config);
    bool ValidateTopicConfig(const TopicConfig& Config);
    bool IsMessageExpired(MessagePtr Message);
    void MoveToDeadLetter(const std::string& QueueName, MessagePtr Message, DeadLetterReason Reason);

    QueueResult DeliverMessageToConsumers(const std::string& QueueName, MessagePtr Message);
    QueueResult DeliverMessageToSubscribers(const std::string& TopicName, MessagePtr Message);

    void
    UpdateQueueStats(const std::string& QueueName, bool MessageSent, bool MessageReceived = false);
    void UpdateTopicStats(const std::string& TopicName, bool MessagePublished);
    void UpdateGlobalStats(bool MessageProcessed, bool MessageFailed = false);

    // 指标窗口统计辅助
    uint32_t MetricsWindowMs = 60000; // 60秒窗口（可配置）
    void RecordEnqueueEvent(QueueData* Queue);
    void RecordDequeueEvent(QueueData* Queue);
    void RecordLatencySample(QueueData* Queue, double Ms);
    static void TrimOld(std::deque<MessageTimestamp>& TsDeque, MessageTimestamp Now, uint32_t WindowMs);
    static double ComputeRatePerSec(const std::deque<MessageTimestamp>& TsDeque, MessageTimestamp Now, uint32_t WindowMs);
    static void ComputePercentiles(const std::vector<double>& Samples, double& P50, double& P95);

    std::string GetDeadLetterQueueName(const std::string& QueueName) const;
    QueueResult EnsureDeadLetterQueue(const std::string& QueueName);

    // DLQ监控内部方法
    void ProcessDeadLetterMonitoring();
    void CheckDeadLetterAlerts(const std::string& QueueName);
    void UpdateDeadLetterStats(const std::string& QueueName, DeadLetterReason Reason);
    void TriggerDeadLetterAlert(const std::string& QueueName, DeadLetterAlertType AlertType, 
                                const std::string& AlertMessage, uint64_t CurrentValue = 0, 
                                uint64_t ThresholdValue = 0, double CurrentRate = 0.0, 
                                double ThresholdRate = 0.0);
    void ClearDeadLetterAlertInternal(const std::string& QueueName, DeadLetterAlertType AlertType);

    // 指标监控内部方法
    void ProcessMetricsMonitoring();

    // 心跳占位内部方法
    void ProcessHeartbeat();
    // 模拟副本复制（占位）：Leader 将消息复制到 Followers，返回获得的ACK数量
    uint32_t SimulateReplication(uint32_t ShardIndex, MessageId Id);
    // 最小复制ACK数（占位，可通过全局配置调整）
    uint32_t MinReplicationAcks = 0;
    // 复制统计（占位）
    std::atomic<uint64_t> ReplicationEvents{0};
    std::atomic<uint64_t> ReplicationAcksTotal{0};

    bool EvaluateMessageFilter(const std::string& Filter, MessagePtr Message);
    QueueResult RouteMessage(const std::string& SourceQueue, MessagePtr Message);
};

}  // namespace Helianthus::MessageQueue